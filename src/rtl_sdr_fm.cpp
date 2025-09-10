/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 *
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
 * Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
 * Copyright (C) 2013 by Elias Oenal <EliasOenal@gmail.com>
 * Copyright (C) 2014 by Kyle Keen <keenerd@gmail.com>
 * Copyright (C) 2025 by arancormonk <180709949+arancormonk@users.noreply.github.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <atomic>
#include <math.h>
#include <pthread.h>
#include <rtl-sdr.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "dsd.h"
#include "dsp/demod_pipeline.h"
#include "dsp/demod_state.h"
#include "dsp/fll.h"
#include "dsp/math_utils.h"
#include "dsp/polar_disc.h"
#include "dsp/resampler.h"
#include "dsp/ted.h"
#include "io/rtl_device.h"
#include "io/udp_control.h"
#include "runtime/config.h"
#include "runtime/input_ring.h"
#include "runtime/log.h"
#include "runtime/mem.h"
#include "runtime/ring.h"
#include "runtime/rt_sched.h"
#include "runtime/worker_pool.h"

#ifdef __cplusplus
extern "C" {
#endif
/* Forward declarations for internal helpers used by shims */
void dsd_rtl_stream_clear_output(void);
long int dsd_rtl_stream_return_pwr(void);
unsigned int dsd_rtl_stream_output_rate(void);
#ifdef __cplusplus
}
#endif

#define DEFAULT_SAMPLE_RATE      48000
#define DEFAULT_BUF_LENGTH       (1 * 16384)
#define MAXIMUM_OVERSAMPLE       16
#define MAXIMUM_BUF_LENGTH       (MAXIMUM_OVERSAMPLE * DEFAULT_BUF_LENGTH)
#define AUTO_GAIN                -100
#define BUFFER_DUMP              4096

#define FREQUENCIES_LIMIT        1000

/* Clamp for bandwidth upsampling multiplier to avoid extreme expansion */
#define MAX_BANDWIDTH_MULTIPLIER 8

static int lcm_post[17] = {1, 1, 1, 3, 1, 5, 3, 7, 1, 9, 5, 11, 3, 13, 7, 15, 1};
static int ACTUAL_BUF_LENGTH;

static const double kPi = 3.14159265358979323846;

#if defined(__GNUC__) || defined(__clang__)
#define DSD_FME_PRAGMA(x) _Pragma(#x)
#define DSD_FME_IVDEP     DSD_FME_PRAGMA(GCC ivdep)

/**
 * Hint that a pointer is aligned to a compile-time boundary for vectorization.
 *
 * This is a lightweight wrapper over compiler intrinsics to improve
 * auto-vectorization by promising the compiler that the pointer meets the
 * specified alignment. Use with care and only when the alignment guarantee
 * is actually met.
 *
 * @tparam T Element type of the pointer.
 * @param p  Pointer to memory that is at least `align_unused` aligned.
 * @param align_unused Alignment in bytes (ignored at runtime; for readability).
 * @return Pointer `p` with alignment assumption applied.
 */
template <typename T>
static inline T*
assume_aligned_ptr(T* p, size_t /*align_unused*/) {
    return (T*)__builtin_assume_aligned(p, 64);
}

template <typename T>
static inline const T*
assume_aligned_ptr(const T* p, size_t /*align_unused*/) {
    return (const T*)__builtin_assume_aligned(p, 64);
}
#else
#define DSD_FME_IVDEP

/**
 * See aligned variant: noop fallback when compiler does not support alignment
 * assumptions.
 * @tparam T Element type of the pointer.
 * @param p  Pointer to return as-is.
 * @param align_unused Unused parameter for signature compatibility.
 * @return Pointer `p` unchanged.
 */
template <typename T>
static inline T*
assume_aligned_ptr(T* p, size_t /*align_unused*/) {
    return p;
}

template <typename T>
static inline const T*
assume_aligned_ptr(const T* p, size_t /*align_unused*/) {
    return p;
}
#endif
#ifndef DSD_FME_ALIGN
#define DSD_FME_ALIGN 64
#endif

/* Compiler-friendly restrict qualifier */
#if defined(__GNUC__) || defined(__clang__)
#define DSD_FME_RESTRICT __restrict__
#else
#define DSD_FME_RESTRICT
#endif

int fll_lut_enabled = 0; /* DSD_FME_FLL_LUT (0 default: use fast approx) */
/* Debug/compat toggles via env */
static int combine_rotate_enabled = 1;      /* DSD_FME_COMBINE_ROT (1 default) */
static int upsample_fixedpoint_enabled = 1; /* DSD_FME_UPSAMPLE_FP (1 default) */

/* Runtime flag (default enabled). Set DSD_FME_HB_DECIM=0 to use legacy decimator */
int use_halfband_decimator = 1;

// UDP control handle
static struct udp_control* g_udp_ctrl = NULL;

int rtl_bandwidth;
int bandwidth_multiplier;
int bandwidth_divisor =
    48000; // multiplier = bandwidth_divisor / rtl_bandwidth; clamped to [1, MAX_BANDWIDTH_MULTIPLIER]

short int volume_multiplier;
uint16_t port;

struct dongle_state {
    int exit_flag;
    pthread_t thread;
    rtlsdr_dev_t* dev;
    int dev_index;
    uint32_t freq;
    uint32_t rate;
    int gain;
    uint32_t buf_len;
    int ppm_error;
    int offset_tuning;
    int direct_sampling;
    std::atomic<int> mute;
    struct demod_state* demod_target;
};

struct demod_mt_worker_arg {
    struct demod_state* s;
    int id;
};

struct controller_state {
    int exit_flag;
    pthread_t thread;
    uint32_t freqs[FREQUENCIES_LIMIT];
    int freq_len;
    int freq_now;
    int edge;
    int wb_mode;
    pthread_cond_t hop;
    pthread_mutex_t hop_m;
};

struct rtl_device* rtl_device_handle = NULL;
struct dongle_state dongle;
struct demod_state demod;
struct output_state output;
struct controller_state controller;
static struct input_ring_state input_ring;

struct RtlSdrInternals {
    struct rtl_device* device;
    struct dongle_state* dongle;
    struct demod_state* demod;
    struct output_state* output;
    struct controller_state* controller;
    struct input_ring_state* input_ring;
    struct udp_control** udp_ctrl_ptr;
    const DsdFmeRuntimeConfig* cfg;
};

static struct RtlSdrInternals* g_stream = NULL;

/**
 * Complex multiply using 32-bit intermediates (suitable for small magnitudes).
 * Defined for platforms lacking hardware float.
 *
 * @param ar Real part of a.
 * @param aj Imag part of a.
 * @param br Real part of b.
 * @param bj Imag part of b.
 * @param cr [out] Real part of result.
 * @param cj [out] Imag part of result.
 */
void
multiply(int ar, int aj, int br, int bj, int* cr, int* cj) {
    *cr = ar * br - aj * bj;
    *cj = aj * br + ar * bj;
}

/**
 * Complex multiply using 64-bit intermediates to prevent overflow.
 *
 * @param ar Real part of a.
 * @param aj Imag part of a.
 * @param br Real part of b.
 * @param bj Imag part of b.
 * @param cr [out] Real part of result (int64).
 * @param cj [out] Imag part of result (int64).
 */
static inline void
multiply64(int ar, int aj, int br, int bj, int64_t* cr, int64_t* cj) {
    *cr = (int64_t)ar * (int64_t)br - (int64_t)aj * (int64_t)bj;
    *cj = (int64_t)aj * (int64_t)br + (int64_t)ar * (int64_t)bj;
}

/**
 * Demodulation worker: consume input ring, run pipeline, and produce audio.
 *
 * Reads baseband I/Q blocks from the input ring, invokes the full demodulation
 * pipeline, and writes audio samples to the output ring with optional
 * resampling or legacy upsampling. Runs until global exit flag is set.
 *
 * @param arg Pointer to `demod_state`.
 * @return NULL on exit.
 */
static void*
demod_thread_fn(void* arg) {
    struct demod_state* d = static_cast<demod_state*>(arg);
    struct output_state* o = d->output_target;
    maybe_set_thread_realtime_and_affinity("DEMOD");
    while (!exitflag) {
        /* Read a block from input ring */
        int got = input_ring_read_block(&input_ring, d->input_cb_buf, MAXIMUM_BUF_LENGTH);
        if (got <= 0) {
            continue;
        }
        d->lowpassed = d->input_cb_buf;
        d->lp_len = got;
        full_demod(d);
        if (d->exit_flag) {
            exitflag = 1;
        }
        if (d->squelch_level && d->squelch_hits > d->conseq_squelch) {
            d->squelch_hits = d->conseq_squelch + 1; /* hair trigger */
            safe_cond_signal(&controller.hop, &controller.hop_m);
            continue;
        }
        if (d->resamp_enabled) {
            int out_n = resamp_process_block(d, d->result, d->result_len, d->resamp_outbuf);
            if (out_n > 0) {
                ring_write_signal_on_empty_transition(o, d->resamp_outbuf, (size_t)out_n);
            }
        } else {
            /* When resampler is disabled, pass-through. */
            if (d->result_len > 0) {
                ring_write_signal_on_empty_transition(o, d->result, (size_t)d->result_len);
            }
        }
        /* Signaling occurs only when the ring transitions from empty to non-empty. */
    }
    return 0;
}

/**
 * Compute and stage tuner/demodulator capture settings based on the
 * requested center frequency and current demod configuration. The actual
 * device programming occurs elsewhere after these fields are updated.
 *
 * @param freq Desired RF center frequency in Hz.
 * @param rate Current input sample rate (unused).
 */
static void
optimal_settings(int freq, int rate) {
    UNUSED(rate);

    int capture_freq, capture_rate;
    struct dongle_state* d = &dongle;
    struct demod_state* dm = &demod;
    struct controller_state* cs = &controller;
    dm->downsample = (1000000 / dm->rate_in) + 1;
    if (dm->downsample_passes) {
        int ds = dm->downsample;
        if (ds <= 1) {
            dm->downsample_passes = 0;
            dm->downsample = 1;
        } else {
#if defined(__GNUC__) || defined(__clang__)
            int floor_log2 = 31 - __builtin_clz(ds);
#else
            int floor_log2 = 0;
            {
                int t = ds;
                while (t >>= 1) {
                    floor_log2++;
                }
            }
#endif
            int is_pow2 = (ds & (ds - 1)) == 0;
            int passes = is_pow2 ? floor_log2 : (floor_log2 + 1);
            dm->downsample_passes = passes;
            dm->downsample = 1 << passes;
        }
    }
    capture_freq = freq;
    capture_rate = dm->downsample * dm->rate_in; // input capture rate
    /* Apply fs/4 shift for zero-IF DC spur avoidance when offset_tuning is disabled. */
    if (!d->offset_tuning) {
        capture_freq = freq + capture_rate / 4;
    }
    capture_freq += cs->edge * dm->rate_in / 2;
    dm->output_scale = (1 << 15) / (128 * dm->downsample);
    if (dm->output_scale < 1) {
        dm->output_scale = 1;
    }
    if (dm->mode_demod == &fm_demod) {
        dm->output_scale = 1;
    }
    d->freq = (uint32_t)capture_freq;
    d->rate = (uint32_t)capture_rate;
}

/**
 * Program device to new center frequency and sample rate using a
 * single, consistent path. Applies fs/4 shift when offset_tuning is off.
 *
 * @param center_freq_hz Desired RF center frequency in Hz.
 */
static void
apply_capture_settings(uint32_t center_freq_hz) {
    optimal_settings((int)center_freq_hz, demod.rate_in);
    rtl_device_set_frequency(rtl_device_handle, dongle.freq);
    rtl_device_set_sample_rate(rtl_device_handle, dongle.rate);
}

/**
 * Controller worker: scans/hops through configured center frequencies.
 *
 * Programs tuner frequency/sample rate according to current optimal settings
 * and hops when signaled by the demod path (e.g., squelch-triggered).
 *
 * @param arg Pointer to `controller_state`.
 * @return NULL on exit.
 */
static void*
controller_thread_fn(void* arg) {
    int i;
    struct controller_state* s = static_cast<controller_state*>(arg);

    if (s->wb_mode) {
        for (i = 0; i < s->freq_len; i++) {
            s->freqs[i] += 16000;
        }
    }

    /* set up primary channel */
    optimal_settings(s->freqs[0], demod.rate_in);
    if (dongle.direct_sampling) {
        rtl_device_set_direct_sampling(rtl_device_handle, 1);
    }
    if (dongle.offset_tuning) {
        rtl_device_set_offset_tuning(rtl_device_handle);
    }

    /* Set the frequency */
    rtl_device_set_frequency(rtl_device_handle, dongle.freq);
    LOG_INFO("Oversampling input by: %ix.\n", demod.downsample);
    LOG_INFO("Oversampling output by: %ix.\n", demod.post_downsample);
    LOG_INFO("Buffer size: %0.2fms\n", 1000 * 0.5 * (float)ACTUAL_BUF_LENGTH / (float)dongle.rate);

    /* Set the sample rate */
    rtl_device_set_sample_rate(rtl_device_handle, dongle.rate);
    LOG_INFO("Output at %u Hz.\n", demod.rate_in / demod.post_downsample);

    while (!exitflag) {
        safe_cond_wait(&s->hop, &s->hop_m);
        if (s->freq_len <= 1) {
            continue;
        }
        s->freq_now = (s->freq_now + 1) % s->freq_len;
        apply_capture_settings((uint32_t)s->freqs[s->freq_now]);
        rtl_device_mute(rtl_device_handle, BUFFER_DUMP);
    }
    return 0;
}

/**
 * Initialize dongle (RTL-SDR source) state with default parameters.
 *
 * @param s Dongle state to initialize.
 */
void
dongle_init(struct dongle_state* s) {
    s->rate = rtl_bandwidth;
    s->gain = AUTO_GAIN; // tenths of a dB
    s->mute = 0;
    s->direct_sampling = 0;
    s->offset_tuning = 0; //E4000 tuners only
    s->demod_target = &demod;
}

typedef enum { DEMOD_DIGITAL = 0, DEMOD_ANALOG = 1, DEMOD_RO2 = 2 } DemodMode;

typedef struct {
    int deemph_default;
} DemodInitParams;

/**
 * Initialize demodulator state using a unified entrypoint.
 *
 * Sets common defaults and applies mode-specific adjustments for digital,
 * analog, or RO2 operation. This centralizes prior duplicated init logic.
 *
 * @param s Demodulator state to initialize.
 * @param mode Demodulation mode selector (digital, analog, or RO2).
 * @param p Optional initialization parameters (e.g., default deemphasis).
 */
static void
demod_init_mode(struct demod_state* s, DemodMode mode, const DemodInitParams* p) {
    /* Common defaults */
    s->rate_in = rtl_bandwidth;
    s->rate_out = rtl_bandwidth;
    s->squelch_level = 0;
    s->conseq_squelch = 10;
    s->terminate_on_squelch = 0;
    s->squelch_hits = 11;
    s->downsample_passes = 0;
    s->comp_fir_size = 0;
    s->prev_index = 0;
    s->post_downsample = 1;
    s->custom_atan = 2;
    s->deemph = 0;
    s->rate_out2 = -1;
    s->mode_demod = &fm_demod;
    s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
    s->prev_lpr_index = 0;
    s->deemph_a = 0;
    s->deemph_avg = 0;
    /* Audio LPF defaults */
    s->audio_lpf_enable = 0;
    s->audio_lpf_alpha = 0;
    s->audio_lpf_state = 0;
    s->now_lpr = 0;
    s->dc_block = 1;
    s->dc_avg = 0;
    /* Resampler defaults */
    s->resamp_enabled = 0;
    s->resamp_target_hz = 0;
    s->resamp_L = 1;
    s->resamp_M = 1;
    s->resamp_phase = 0;
    s->resamp_taps_len = 0;
    s->resamp_taps_per_phase = 0;
    s->resamp_taps = NULL;
    s->resamp_hist = NULL;
    /* FLL/TED defaults */
    s->fll_enabled = 0;
    s->fll_alpha_q15 = 0;
    s->fll_beta_q15 = 0;
    s->fll_freq_q15 = 0;
    s->fll_phase_q15 = 0;
    s->fll_prev_r = 0;
    s->fll_prev_j = 0;
    s->ted_enabled = 0;
    s->ted_gain_q20 = 0;
    s->ted_sps = 0;
    s->ted_mu_q20 = 0;
    /* Initialize FLL and TED module states */
    fll_init_state(&s->fll_state);
    ted_init_state(&s->ted_state);
    /* Squelch estimator init */
    s->squelch_running_power = 0;
    s->squelch_decim_stride = 16;
    s->squelch_decim_phase = 0;
    s->squelch_window = 2048;
    /* HB decimator histories */
    for (int st = 0; st < 10; st++) {
        memset(s->hb_hist_i[st], 0, sizeof(s->hb_hist_i[st]));
        memset(s->hb_hist_q[st], 0, sizeof(s->hb_hist_q[st]));
    }
    /* Legacy CIC histories used by fifth_order path */
    for (int st = 0; st < 10; st++) {
        memset(s->lp_i_hist[st], 0, sizeof(s->lp_i_hist[st]));
        memset(s->lp_q_hist[st], 0, sizeof(s->lp_q_hist[st]));
    }
    /* Input ring does not require double-buffer init */
    s->lowpassed = s->input_cb_buf;
    s->lp_len = 0;
    pthread_cond_init(&s->ready, NULL);
    pthread_mutex_init(&s->ready_m, NULL);
    s->output_target = &output;

    /* Mode-specific adjustments */
    if (mode == DEMOD_ANALOG) {
        s->downsample_passes = 1;
        s->comp_fir_size = 9;
        s->custom_atan = 1;
        s->deemph = 1;
        s->rate_out2 = rtl_bandwidth;
    } else if (mode == DEMOD_RO2) {
        s->downsample_passes = 0;
        s->comp_fir_size = 0;
        s->custom_atan = 2;
        s->deemph = 0;
        s->rate_out2 = rtl_bandwidth;
    } else {
        /* DEMOD_DIGITAL default already set */
        s->rate_out2 = -1;
    }

    if (p && p->deemph_default >= 0) {
        s->deemph = p->deemph_default;
    }

    if (s->custom_atan == 2) {
        atan_lut_init();
    }
    /* set discriminator function pointer */
    s->discriminator = (s->custom_atan == 0)   ? &polar_discriminant
                       : (s->custom_atan == 1) ? &polar_disc_fast
                                               : &polar_disc_lut;
    /* Init minimal worker pool (env-gated) */
    demod_mt_init(s);
}

/**
 * Initialize demodulator state for analog FM path.
 *
 * Applies analog-specific defaults (e.g., deemphasis enabled, FIR size,
 * and downsample passes) on top of common initialization.
 *
 * @param s Demodulator state to initialize.
 */
void
demod_init_analog(struct demod_state* s) {
    s->rate_in = rtl_bandwidth;
    s->rate_out = rtl_bandwidth;
    s->squelch_level = 0;
    s->conseq_squelch = 10;
    s->terminate_on_squelch = 0;
    s->squelch_hits = 11;
    s->downsample_passes = 1; //
    s->comp_fir_size = 9;
    s->prev_index = 0;
    s->post_downsample =
        1; //1 -- once this works, default = 4 -- doesn't work on the official rtl-sdr source code either
    s->custom_atan = 1;
    s->deemph = 1;                //
    s->rate_out2 = rtl_bandwidth; // -1 flag for disabled -- this enables low_pass_real, seems to work okay
    s->mode_demod = &fm_demod;
    s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
    s->prev_lpr_index = 0;
    s->deemph_a = 0; //
    s->deemph_avg = 0;
    /* Audio LPF defaults */
    s->audio_lpf_enable = 0;
    s->audio_lpf_alpha = 0;
    s->audio_lpf_state = 0;
    s->now_lpr = 0;
    s->dc_block = 1; //
    s->dc_avg = 0;
    /* Resampler defaults */
    s->resamp_enabled = 0;
    s->resamp_target_hz = 0;
    s->resamp_L = 1;
    s->resamp_M = 1;
    s->resamp_phase = 0;
    s->resamp_taps_len = 0;
    s->resamp_taps_per_phase = 0;
    s->resamp_taps = NULL;
    s->resamp_hist = NULL;
    /* FLL/TED defaults */
    s->fll_enabled = 0;
    s->fll_alpha_q15 = 0;
    s->fll_beta_q15 = 0;
    s->fll_freq_q15 = 0;
    s->fll_phase_q15 = 0;
    s->fll_prev_r = 0;
    s->fll_prev_j = 0;
    s->ted_enabled = 0;
    s->ted_gain_q20 = 0;
    s->ted_sps = 0;
    s->ted_mu_q20 = 0;
    /* Initialize FLL and TED module states */
    fll_init_state(&s->fll_state);
    ted_init_state(&s->ted_state);
    /* Squelch estimator init */
    s->squelch_running_power = 0;
    s->squelch_decim_stride = 16; /* evaluate 1/16th samples for low CPU */
    s->squelch_decim_phase = 0;
    s->squelch_window = 2048; /* EMA window ~2048 samples */
    /* HB decimator histories */
    for (int st = 0; st < 10; st++) {
        memset(s->hb_hist_i[st], 0, sizeof(s->hb_hist_i[st]));
        memset(s->hb_hist_q[st], 0, sizeof(s->hb_hist_q[st]));
    }
    /* Legacy CIC histories used by fifth_order path */
    for (int st = 0; st < 10; st++) {
        memset(s->lp_i_hist[st], 0, sizeof(s->lp_i_hist[st]));
        memset(s->lp_q_hist[st], 0, sizeof(s->lp_q_hist[st]));
    }
    /* Input ring does not require double-buffer init */
    s->lowpassed = s->input_cb_buf;
    s->lp_len = 0;
    pthread_cond_init(&s->ready, NULL);
    pthread_mutex_init(&s->ready_m, NULL);
    s->output_target = &output;
    if (s->custom_atan == 2) {
        atan_lut_init();
    }
    /* set discriminator function pointer */
    /* custom_atan mapping:
	   0 -> polar_discriminant (double atan2; slow, highest accuracy)
	   1 -> polar_disc_fast    (int64 fast_atan2 approximation)
	   2 -> polar_disc_lut     (LUT-based atan2 approximation)
	*/
    s->discriminator = (s->custom_atan == 0)   ? &polar_discriminant
                       : (s->custom_atan == 1) ? &polar_disc_fast
                                               : &polar_disc_lut;
    /* Init minimal worker pool (env-gated) */
    demod_mt_init(s);
}

/**
 * Initialize demodulator state for RO2 path (no CIC, LUT atan by default).
 *
 * @param s Demodulator state to initialize.
 */
void
demod_init_ro2(struct demod_state* s) {
    s->rate_in = rtl_bandwidth;
    s->rate_out = rtl_bandwidth;
    s->squelch_level = 0;
    s->conseq_squelch = 10;
    s->terminate_on_squelch = 0;
    s->squelch_hits = 11;
    s->downsample_passes = 0;
    s->comp_fir_size = 0;
    s->prev_index = 0;
    s->post_downsample =
        1; //1 -- once this works, default = 4 -- doesn't work on the official rtl-sdr source code either
    s->custom_atan = 2;
    s->deemph = 0;
    s->rate_out2 = rtl_bandwidth; // -1 flag for disabled -- this enables low_pass_real, seems to work okay
    s->mode_demod = &fm_demod;
    s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
    s->prev_lpr_index = 0;
    s->deemph_a = 0;
    s->deemph_avg = 0;
    /* Audio LPF defaults */
    s->audio_lpf_enable = 0;
    s->audio_lpf_alpha = 0;
    s->audio_lpf_state = 0;
    s->now_lpr = 0;
    s->dc_block = 1; //enabling by default, but offset tuning is also enabled, so center spike shouldn't be an issue
    s->dc_avg = 0;
    /* Resampler defaults */
    s->resamp_enabled = 0;
    s->resamp_target_hz = 0;
    s->resamp_L = 1;
    s->resamp_M = 1;
    s->resamp_phase = 0;
    s->resamp_taps_len = 0;
    s->resamp_taps_per_phase = 0;
    s->resamp_taps = NULL;
    s->resamp_hist = NULL;
    /* FLL/TED defaults */
    s->fll_enabled = 0;
    s->fll_alpha_q15 = 0;
    s->fll_beta_q15 = 0;
    s->fll_freq_q15 = 0;
    s->fll_phase_q15 = 0;
    s->fll_prev_r = 0;
    s->fll_prev_j = 0;
    s->ted_enabled = 0;
    s->ted_gain_q20 = 0;
    s->ted_sps = 0;
    s->ted_mu_q20 = 0;
    /* Initialize FLL and TED module states */
    fll_init_state(&s->fll_state);
    ted_init_state(&s->ted_state);
    /* Squelch estimator init */
    s->squelch_running_power = 0;
    s->squelch_decim_stride = 16;
    s->squelch_decim_phase = 0;
    s->squelch_window = 2048;
    /* HB decimator histories */
    for (int st = 0; st < 10; st++) {
        memset(s->hb_hist_i[st], 0, sizeof(s->hb_hist_i[st]));
        memset(s->hb_hist_q[st], 0, sizeof(s->hb_hist_q[st]));
    }
    /* Legacy CIC histories used by fifth_order path */
    for (int st = 0; st < 10; st++) {
        memset(s->lp_i_hist[st], 0, sizeof(s->lp_i_hist[st]));
        memset(s->lp_q_hist[st], 0, sizeof(s->lp_q_hist[st]));
    }
    /* Input ring does not require double-buffer init */
    s->lowpassed = s->input_cb_buf;
    s->lp_len = 0;
    pthread_cond_init(&s->ready, NULL);
    pthread_mutex_init(&s->ready_m, NULL);
    s->output_target = &output;
    if (s->custom_atan == 2) {
        atan_lut_init();
    }
    /* set discriminator function pointer */
    s->discriminator = (s->custom_atan == 0)   ? &polar_discriminant
                       : (s->custom_atan == 1) ? &polar_disc_fast
                                               : &polar_disc_lut;
    /* Init minimal worker pool (env-gated) */
    demod_mt_init(s);
}

/**
 * Initialize demodulator state for default digital path.
 *
 * @param s Demodulator state to initialize.
 */
void
demod_init(struct demod_state* s) {
    s->rate_in = rtl_bandwidth;
    s->rate_out = rtl_bandwidth;
    s->squelch_level = 0;
    s->conseq_squelch = 10;
    s->terminate_on_squelch = 0;
    s->squelch_hits = 11;
    s->downsample_passes = 0;
    s->comp_fir_size = 0;
    s->prev_index = 0;
    s->post_downsample =
        1; //1 -- once this works, default = 4 -- doesn't work on the official rtl-sdr source code either
    s->custom_atan = 2;
    s->deemph = 0;
    s->rate_out2 = -1; // -1 flag for disabled -- this enables low_pass_real, seems to work okay
    s->mode_demod = &fm_demod;
    s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
    s->prev_lpr_index = 0;
    s->deemph_a = 0;
    s->deemph_avg = 0;
    /* Audio LPF defaults */
    s->audio_lpf_enable = 0;
    s->audio_lpf_alpha = 0;
    s->audio_lpf_state = 0;
    s->now_lpr = 0;
    s->dc_block = 1; //enabling by default, but offset tuning is also enabled, so center spike shouldn't be an issue
    s->dc_avg = 0;
    /* Resampler defaults */
    s->resamp_enabled = 0;
    s->resamp_target_hz = 0;
    s->resamp_L = 1;
    s->resamp_M = 1;
    s->resamp_phase = 0;
    s->resamp_taps_len = 0;
    s->resamp_taps_per_phase = 0;
    s->resamp_taps = NULL;
    s->resamp_hist = NULL;
    /* FLL/TED defaults */
    s->fll_enabled = 0;
    s->fll_alpha_q15 = 0;
    s->fll_beta_q15 = 0;
    s->fll_freq_q15 = 0;
    s->fll_phase_q15 = 0;
    s->fll_prev_r = 0;
    s->fll_prev_j = 0;
    s->ted_enabled = 0;
    s->ted_gain_q20 = 0;
    s->ted_sps = 0;
    s->ted_mu_q20 = 0;
    /* Initialize FLL and TED module states */
    fll_init_state(&s->fll_state);
    ted_init_state(&s->ted_state);
    /* Squelch estimator init */
    s->squelch_running_power = 0;
    s->squelch_decim_stride = 16;
    s->squelch_decim_phase = 0;
    s->squelch_window = 2048;
    /* HB decimator histories */
    for (int st = 0; st < 10; st++) {
        memset(s->hb_hist_i[st], 0, sizeof(s->hb_hist_i[st]));
        memset(s->hb_hist_q[st], 0, sizeof(s->hb_hist_q[st]));
    }
    /* Legacy CIC histories used by fifth_order path */
    for (int st = 0; st < 10; st++) {
        memset(s->lp_i_hist[st], 0, sizeof(s->lp_i_hist[st]));
        memset(s->lp_q_hist[st], 0, sizeof(s->lp_q_hist[st]));
    }
    /* Input ring does not require double-buffer init */
    s->lowpassed = s->input_cb_buf;
    s->lp_len = 0;
    pthread_cond_init(&s->ready, NULL);
    pthread_mutex_init(&s->ready_m, NULL);
    s->output_target = &output;
    if (s->custom_atan == 2) {
        atan_lut_init();
    }
    /* set discriminator function pointer */
    s->discriminator = (s->custom_atan == 0)   ? &polar_discriminant
                       : (s->custom_atan == 1) ? &polar_disc_fast
                                               : &polar_disc_lut;
    /* Init minimal worker pool (env-gated) */
    demod_mt_init(s);
}

/**
 * Release resources owned by the demodulator state.
 *
 * @param s Demodulator state to clean up.
 */
void
demod_cleanup(struct demod_state* s) {
    pthread_cond_destroy(&s->ready);
    pthread_mutex_destroy(&s->ready_m);
    /* Destroy worker pool if enabled */
    demod_mt_destroy(s);
    /* Free resampler resources */
    if (s->resamp_taps) {
        dsd_fme_aligned_free(s->resamp_taps);
        s->resamp_taps = NULL;
    }
    if (s->resamp_hist) {
        dsd_fme_aligned_free(s->resamp_hist);
        s->resamp_hist = NULL;
    }
}

/**
 * Initialize output ring buffer and synchronization primitives.
 *
 * @param s Output state to initialize.
 */
void
output_init(struct output_state* s) {
    s->rate = rtl_bandwidth;
    pthread_cond_init(&s->ready, NULL);
    pthread_cond_init(&s->space, NULL);
    pthread_mutex_init(&s->ready_m, NULL);
    /* Allocate SPSC ring buffer */
    s->capacity = (size_t)(MAXIMUM_BUF_LENGTH * 8);
    /* Try aligned allocation for better vectorized copies; fall back if unavailable */
    {
        void* mem_ptr = dsd_fme_aligned_malloc(s->capacity * sizeof(int16_t));
        if (!mem_ptr) {
            LOG_ERROR("Failed to allocate output ring buffer (%zu samples).\n", s->capacity);
            /* Propagate by keeping buffer NULL; callers must detect before use */
            return;
        }
        s->buffer = static_cast<int16_t*>(mem_ptr);
    }
    s->head.store(0);
    s->tail.store(0);
    /* Metrics */
    s->write_timeouts.store(0);
    s->read_timeouts.store(0);
}

/**
 * Destroy output ring buffer and synchronization primitives.
 *
 * @param s Output state to clean up.
 */
void
output_cleanup(struct output_state* s) {
    pthread_cond_destroy(&s->ready);
    pthread_cond_destroy(&s->space);
    pthread_mutex_destroy(&s->ready_m);
    if (s->buffer) {
        dsd_fme_aligned_free(s->buffer);
        s->buffer = NULL;
    }
}

/**
 * Initialize controller state (frequency list and hop control).
 *
 * @param s Controller state to initialize.
 */
void
controller_init(struct controller_state* s) {
    s->freqs[0] = 446000000;
    s->freq_len = 0;
    s->edge = 0;
    s->wb_mode = 0;
    pthread_cond_init(&s->hop, NULL);
    pthread_mutex_init(&s->hop_m, NULL);
}

/**
 * Destroy controller synchronization primitives.
 *
 * @param s Controller state to clean up.
 */
void
controller_cleanup(struct controller_state* s) {
    pthread_cond_destroy(&s->hop);
    pthread_mutex_destroy(&s->hop_m);
}

/**
 * Validate runtime options and controller state prior to starting streams.
 * Exits the process with an error message if constraints are violated.
 */
void
sanity_checks(void) {
    if (controller.freq_len == 0) {
        LOG_ERROR("Please specify a frequency.\n");
        exit(1);
    }

    if (controller.freq_len >= FREQUENCIES_LIMIT) {
        LOG_ERROR("Too many channels, maximum %i.\n", FREQUENCIES_LIMIT);
        exit(1);
    }

    if (controller.freq_len > 1 && demod.squelch_level == 0) {
        LOG_ERROR("Please specify a squelch level.  Required for scanning multiple frequencies.\n");
        exit(1);
    }
}

/**
 * Signal handler to request RTL-SDR async cancel and exit.
 */
extern "C" void
rtlsdr_sighandler(void) {
    LOG_ERROR("Signal caught, exiting!\n");
    rtl_device_stop_async(rtl_device_handle);
}

/**
 * Apply runtime configuration flags and set up optional resampler/FLL/TED.
 *
 * Reads environment-backed runtime configuration and options, enables
 * or disables modules, and designs the rational resampler when requested.
 *
 * @param opts Decoder options.
 */
static void
configure_from_env_and_opts(dsd_opts* opts) {
    dsd_fme_config_init(opts);
    const DsdFmeRuntimeConfig* cfg = dsd_fme_get_config();
    if (!cfg) {
        return;
    }
    if (cfg->hb_decim_is_set) {
        use_halfband_decimator = (cfg->hb_decim != 0);
    }
    if (cfg->combine_rot_is_set) {
        combine_rotate_enabled = (cfg->combine_rot != 0);
    }
    if (cfg->upsample_fp_is_set) {
        upsample_fixedpoint_enabled = (cfg->upsample_fp != 0);
    }

    int enable_resamp = 1;
    int target = 48000;
    if (cfg->resamp_is_set) {
        enable_resamp = cfg->resamp_disable ? 0 : 1;
        target = cfg->resamp_target_hz > 0 ? cfg->resamp_target_hz : 48000;
    }
    if (enable_resamp) {
        demod.resamp_target_hz = target;
        int inRate = (demod.rate_out > 0) ? demod.rate_out : rtl_bandwidth;
        int g = gcd_int(inRate, target);
        int L = target / g;
        int M = inRate / g;
        if (L < 1) {
            L = 1;
        }
        if (M < 1) {
            M = 1;
        }
        int scale_num = L;
        int scale_den = M;
        int scale = (scale_den > 0) ? ((scale_num + scale_den - 1) / scale_den) : 1;
        if (scale > 4) {
            LOG_WARNING("Resampler ratio too large (L=%d,M=%d). Clamping not supported; disabling resampler.\n", L, M);
            demod.resamp_enabled = 0;
        } else {
            demod.resamp_enabled = 1;
            resamp_design(&demod, L, M);
            LOG_INFO("Rational resampler enabled: %d -> %d Hz (L=%d,M=%d).\n", inRate, target, L, M);
        }
    } else {
        demod.resamp_enabled = 0;
    }

    demod.fll_enabled = cfg->fll_is_set ? (cfg->fll_enable != 0) : 0;
    fll_lut_enabled = cfg->fll_lut_is_set ? (cfg->fll_lut_enable != 0) : fll_lut_enabled;
    demod.fll_alpha_q15 = cfg->fll_alpha_is_set ? cfg->fll_alpha_q15 : 50;
    demod.fll_beta_q15 = cfg->fll_beta_is_set ? cfg->fll_beta_q15 : 5;
    demod.fll_deadband_q14 = cfg->fll_deadband_is_set ? cfg->fll_deadband_q14 : 45;
    demod.fll_slew_max_q15 = cfg->fll_slew_is_set ? cfg->fll_slew_max_q15 : 64;
    demod.fll_freq_q15 = 0;
    demod.fll_phase_q15 = 0;
    demod.fll_prev_r = demod.fll_prev_j = 0;

    demod.ted_enabled = cfg->ted_is_set ? (cfg->ted_enable != 0) : 0;
    demod.ted_gain_q20 = cfg->ted_gain_is_set ? cfg->ted_gain_q20 : 64;
    demod.ted_sps = cfg->ted_sps_is_set ? cfg->ted_sps : 10;
    demod.ted_mu_q20 = 0;
    demod.ted_force = cfg->ted_force_is_set ? (cfg->ted_force != 0) : 0;
}

/**
 * Apply sensible defaults for digital vs analog modes when env not set.
 *
 * @param opts Decoder options.
 */
static void
select_defaults_for_mode(dsd_opts* opts) {
    int env_ted_set = dsd_fme_get_config()->ted_is_set;
    int env_fll_alpha_set = dsd_fme_get_config()->fll_alpha_is_set;
    int env_fll_beta_set = dsd_fme_get_config()->fll_beta_is_set;
    int env_ted_sps_set = dsd_fme_get_config()->ted_sps_is_set;
    int env_ted_gain_set = dsd_fme_get_config()->ted_gain_is_set;
    int digital_mode = (opts->frame_p25p1 == 1 || opts->frame_p25p2 == 1 || opts->frame_provoice == 1);
    if (digital_mode) {
        if (!env_ted_set) {
            demod.ted_enabled = 0;
        }
        if (!env_ted_sps_set) {
            int ds_passes = demod.downsample_passes;
            if (ds_passes < 0) {
                ds_passes = 0;
            }
            int denom = 1 << ds_passes;
            long long Fs_cx_ll = (long long)demod.rate_in * (long long)demod.post_downsample;
            int Fs_cx = (int)(Fs_cx_ll / (denom ? denom : 1));
            if (Fs_cx <= 0) {
                Fs_cx = (int)output.rate;
            }
            int sps = (Fs_cx + 2400) / 4800; /* round(Fs/4800) */
            if (sps < 2) {
                sps = 2;
            }
            demod.ted_sps = sps;
        }
        if (!env_ted_gain_set) {
            demod.ted_gain_q20 = 96;
        }
        if (!env_fll_alpha_set) {
            demod.fll_alpha_q15 = 150;
        }
        if (!env_fll_beta_set) {
            demod.fll_beta_q15 = 15;
        }
        if (!demod.fll_enabled && !dsd_fme_get_config()->fll_is_set) {
            demod.fll_enabled = 1;
        }
    } else {
        if (!env_ted_set) {
            demod.ted_enabled = 0;
        }
        if (!env_fll_alpha_set) {
            demod.fll_alpha_q15 = 50;
        }
        if (!env_fll_beta_set) {
            demod.fll_beta_q15 = 5;
        }
    }
}

/**
 * Seed initial device index, center frequency, gain and UDP port.
 *
 * @param opts Decoder options.
 */
static void
setup_initial_freq_and_rate(dsd_opts* opts) {
    if (opts->rtlsdr_center_freq > 0) {
        controller.freqs[controller.freq_len] = opts->rtlsdr_center_freq;
        controller.freq_len++;
    }
    if (opts->rtlsdr_ppm_error != 0) {
        dongle.ppm_error = opts->rtlsdr_ppm_error;
        LOG_INFO("Setting RTL PPM Error Set to %d\n", opts->rtlsdr_ppm_error);
    }
    dongle.dev_index = opts->rtl_dev_index;
    LOG_INFO("Setting RTL Bandwidth to %d Hz\n", rtl_bandwidth);
    LOG_INFO("Setting RTL Power Squelch Level to %d\n", opts->rtl_squelch_level);
    if (opts->rtl_udp_port != 0) {
        int p = opts->rtl_udp_port;
        if (p < 0) {
            p = 0;
        }
        if (p > 65535) {
            p = 65535;
        }
        port = (uint16_t)p;
    }
    if (opts->rtl_gain_value > 0) {
        dongle.gain = opts->rtl_gain_value * 10;
    }
}

/**
 * Launch controller/demod threads and start async device capture.
 */
static void
start_threads_and_async(void) {
    pthread_create(&controller.thread, NULL, controller_thread_fn, (void*)(&controller));
    pthread_create(&demod.thread, NULL, demod_thread_fn, (void*)(&demod));
    rtl_device_start_async(rtl_device_handle, (uint32_t)ACTUAL_BUF_LENGTH);
    if (port != 0) {
        g_udp_ctrl = udp_control_start(
            port,
            [](uint32_t new_freq_hz, void* /*user_data*/) {
                /* Single programming path: recompute and program */
                apply_capture_settings(new_freq_hz);
                rtl_device_mute(rtl_device_handle, BUFFER_DUMP);
            },
            NULL);
    }
}

/**
 * Initialize and open the RTL-SDR streaming pipeline, threads, and buffers.
 *
 * Configures device and demod state, validates options, allocates buffers,
 * programs initial capture settings (including fs/4 shift when appropriate),
 * and starts async capture plus worker threads.
 *
 * @param opts Decoder options used to configure the pipeline.
 * @return 0 on success, negative on error.
 */
extern "C" int
dsd_rtl_stream_open(dsd_opts* opts) {
    rtl_bandwidth = opts->rtl_bandwidth * 1000; //reverted back to straight value
    bandwidth_multiplier = (bandwidth_divisor / rtl_bandwidth);
    /* Guard multiplier to a safe range [1, MAX_BANDWIDTH_MULTIPLIER] */
    {
        int orig_mult = bandwidth_multiplier;
        if (bandwidth_multiplier < 1) {
            LOG_WARNING("bandwidth_multiplier computed as %d (divisor=%d, bandwidth=%d Hz). Clamping to 1.\n",
                        orig_mult, bandwidth_divisor, rtl_bandwidth);
            bandwidth_multiplier = 1;
        } else if (bandwidth_multiplier > MAX_BANDWIDTH_MULTIPLIER) {
            LOG_WARNING("bandwidth_multiplier computed as %d exceeds max %d (divisor=%d, bandwidth=%d Hz). "
                        "Clamping to %d.\n",
                        orig_mult, MAX_BANDWIDTH_MULTIPLIER, bandwidth_divisor, rtl_bandwidth,
                        MAX_BANDWIDTH_MULTIPLIER);
            bandwidth_multiplier = MAX_BANDWIDTH_MULTIPLIER;
        }
    }
    volume_multiplier = 1;

    dongle_init(&dongle);
    {
        DemodInitParams params = {0};
        if (opts->frame_p25p1 == 1 || opts->frame_p25p2 == 1 || opts->frame_provoice == 1) {
            demod_init_mode(&demod, DEMOD_RO2, &params);
        } else if (opts->analog_only == 1 || opts->m17encoder == 1) {
            params.deemph_default = 1;
            demod_init_mode(&demod, DEMOD_ANALOG, &params);
        } else {
            demod_init_mode(&demod, DEMOD_DIGITAL, &params);
        }
    }
    output_init(&output);
    if (!output.buffer) {
        LOG_ERROR("Output ring buffer allocation failed.\n");
        return -1;
    }
    /* Init input ring */
    {
        void* mem_ptr = dsd_fme_aligned_malloc((size_t)(MAXIMUM_BUF_LENGTH * 8) * sizeof(int16_t));
        if (!mem_ptr) {
            LOG_ERROR("Failed to allocate input ring buffer.\n");
            return -1;
        }
        input_ring.buffer = static_cast<int16_t*>(mem_ptr);
        input_ring.capacity = (size_t)(MAXIMUM_BUF_LENGTH * 8);
        input_ring.head.store(0);
        input_ring.tail.store(0);
        pthread_cond_init(&input_ring.ready, NULL);
        pthread_mutex_init(&input_ring.ready_m, NULL);
        /* Metrics */
        input_ring.producer_drops.store(0);
        input_ring.read_timeouts.store(0);
    }
    controller_init(&controller);

    /* Read optional environment flags (centralized) */
    configure_from_env_and_opts(opts);
    select_defaults_for_mode(opts);

    setup_initial_freq_and_rate(opts);

    /* quadruple sample_rate to limit to Δθ to ±π/2 */
    demod.rate_in *= demod.post_downsample;

    if (!output.rate) {
        output.rate = demod.rate_out;
    }

    {
        /* Validate inputs; require at least one frequency and squelch when scanning */
        if (controller.freq_len == 0) {
            LOG_ERROR("Please specify a frequency.\n");
            return -1;
        }
        if (controller.freq_len >= FREQUENCIES_LIMIT) {
            LOG_ERROR("Too many channels, maximum %i.\n", FREQUENCIES_LIMIT);
            return -1;
        }
        if (controller.freq_len > 1 && demod.squelch_level == 0) {
            LOG_ERROR("Please specify a squelch level.  Required for scanning multiple frequencies.\n");
            return -1;
        }
    }

    if (controller.freq_len > 1) {
        demod.terminate_on_squelch = 0;
    }

    ACTUAL_BUF_LENGTH = lcm_post[demod.post_downsample] * DEFAULT_BUF_LENGTH;
    /* Ensure async read uses a valid, explicit buffer length */
    dongle.buf_len = (uint32_t)ACTUAL_BUF_LENGTH;

    rtl_device_handle = rtl_device_create(dongle.dev_index, &input_ring, combine_rotate_enabled);
    if (!rtl_device_handle) {
        LOG_ERROR("Failed to open rtlsdr device %d.\n", dongle.dev_index);
        return -1;
    } else {
        LOG_INFO("Using RTLSDR Device Index: %d. \n", dongle.dev_index);
    }

    if (demod.deemph) {
        const DsdFmeRuntimeConfig* cfg = dsd_fme_get_config();
        double tau_s = 75e-6; /* default 75 microseconds */
        if (cfg && cfg->deemph_is_set) {
            if (cfg->deemph_mode == DSD_FME_DEEMPH_OFF) {
                demod.deemph = 0;
            } else if (cfg->deemph_mode == DSD_FME_DEEMPH_50) {
                tau_s = 50e-6;
            } else if (cfg->deemph_mode == DSD_FME_DEEMPH_NFM) {
                tau_s = 750e-6;
            } else if (cfg->deemph_mode == DSD_FME_DEEMPH_75) {
                tau_s = 75e-6;
            }
        }
        if (demod.deemph) {
            double Fs = (double)demod.rate_out;
            if (Fs < 1.0) {
                Fs = 1.0;
            }
            double a = exp(-1.0 / (Fs * tau_s));
            double alpha = 1.0 - a;
            int coef_q15 = (int)lrint(alpha * (double)(1 << 15));
            if (coef_q15 < 1) {
                coef_q15 = 1;
            }
            if (coef_q15 > ((1 << 15) - 1)) {
                coef_q15 = ((1 << 15) - 1);
            }
            demod.deemph_a = coef_q15;
        }
    }

    /* Configure optional post-demod audio LPF via env DSD_FME_AUDIO_LPF.
       Values:
       - off or 0: disabled (default)
       - NNNN: cutoff in Hz (approximate), e.g., 3000 or 5000.
       One-pole: y[n] = y[n-1] + alpha * (x[n] - y[n-1]),
       alpha ≈ 1 - exp(-2*pi*fc/Fs) in Q15. */
    {
        const DsdFmeRuntimeConfig* cfg = dsd_fme_get_config();
        demod.audio_lpf_enable = 0;
        demod.audio_lpf_alpha = 0;
        demod.audio_lpf_state = 0;
        if (cfg && cfg->audio_lpf_is_set && !cfg->audio_lpf_disable && cfg->audio_lpf_cutoff_hz > 0) {
            int cutoff_hz = cfg->audio_lpf_cutoff_hz;
            if (cutoff_hz < 100) {
                cutoff_hz = 100; /* guard */
            }
            double Fs = (double)demod.rate_out;
            if (Fs < 1.0) {
                Fs = 1.0;
            }
            double a = 1.0 - exp(-2.0 * kPi * (double)cutoff_hz / Fs);
            if (a < 0.0) {
                a = 0.0;
            }
            if (a > 1.0) {
                a = 1.0;
            }
            int alpha_q15 = (int)lrint(a * (double)(1 << 15));
            if (alpha_q15 < 1) {
                alpha_q15 = 1;
            }
            if (alpha_q15 > ((1 << 15) - 1)) {
                alpha_q15 = ((1 << 15) - 1);
            }
            demod.audio_lpf_alpha = alpha_q15;
            demod.audio_lpf_enable = 1;
            LOG_INFO("Audio LPF enabled: fc≈%d Hz, alpha_q15=%d\n", cutoff_hz, demod.audio_lpf_alpha);
        }
    }

    /* Set the tuner gain */
    rtl_device_set_gain(rtl_device_handle, dongle.gain);
    if (dongle.gain == AUTO_GAIN) {
        LOG_INFO("Setting RTL Autogain. \n");
    }

    rtl_device_set_ppm(rtl_device_handle, dongle.ppm_error);

    /* Program initial frequency and sample rate before starting async */
    if (controller.freq_len == 0) {
        controller.freqs[controller.freq_len] = 446000000;
        controller.freq_len++;
    }
    optimal_settings(controller.freqs[0], demod.rate_in);
    if (dongle.direct_sampling) {
        rtl_device_set_direct_sampling(rtl_device_handle, 1);
    }
    if (dongle.offset_tuning) {
        rtl_device_set_offset_tuning(rtl_device_handle);
    }
    apply_capture_settings((uint32_t)controller.freqs[0]);
    LOG_INFO("Oversampling input by: %ix.\n", demod.downsample);
    LOG_INFO("Oversampling output by: %ix.\n", demod.post_downsample);
    LOG_INFO("Buffer size: %0.2fms\n", 1000 * 0.5 * (float)ACTUAL_BUF_LENGTH / (float)dongle.rate);
    LOG_INFO("Output at %u Hz.\n", demod.rate_in / demod.post_downsample);

    /* Reset endpoint before we start reading from it (mandatory) */
    rtl_device_reset_buffer(rtl_device_handle);

    /* Start controller/demod threads and async */
    start_threads_and_async();

    /* If resampler is enabled, update output.rate for downstream consumers */
    if (demod.resamp_enabled && demod.resamp_target_hz > 0) {
        output.rate = demod.resamp_target_hz;
        LOG_INFO("Output rate set to %d Hz via resampler.\n", output.rate);
    } else {
        output.rate = demod.rate_out;
    }

    if (g_stream) {
        free(g_stream);
        g_stream = NULL;
    }
    g_stream = (struct RtlSdrInternals*)calloc(1, sizeof(struct RtlSdrInternals));
    if (g_stream) {
        g_stream->device = rtl_device_handle;
        g_stream->dongle = &dongle;
        g_stream->demod = &demod;
        g_stream->output = &output;
        g_stream->controller = &controller;
        g_stream->input_ring = &input_ring;
        g_stream->udp_ctrl_ptr = &g_udp_ctrl;
        g_stream->cfg = dsd_fme_get_config();
    }
    return 0;
}

/**
 * Stop threads, cleanup buffers/objects, and close the RTL-SDR stream.
 */
extern "C" void
dsd_rtl_stream_close(void) {
    LOG_INFO("cleaning up...\n");
    /* Log Phase 3 metrics before teardown */
    LOG_INFO("Output ring: write_timeouts=%llu read_timeouts=%llu\n", (unsigned long long)output.write_timeouts.load(),
             (unsigned long long)output.read_timeouts.load());
    LOG_INFO("Input ring: producer_drops=%llu read_timeouts=%llu\n",
             (unsigned long long)input_ring.producer_drops.load(), (unsigned long long)input_ring.read_timeouts.load());
    if (g_udp_ctrl) {
        udp_control_stop(g_udp_ctrl);
        g_udp_ctrl = NULL;
    }
    /* Request threads to exit and wake any waiters */
    exitflag = 1;
    safe_cond_signal(&input_ring.ready, &input_ring.ready_m);
    safe_cond_signal(&controller.hop, &controller.hop_m);
    rtl_device_stop_async(rtl_device_handle);
    safe_cond_signal(&demod.ready, &demod.ready_m);
    pthread_join(demod.thread, NULL);
    safe_cond_signal(&output.ready, &output.ready_m);
    pthread_join(controller.thread, NULL);

    demod_cleanup(&demod);
    output_cleanup(&output);
    controller_cleanup(&controller);

    /* free input ring */
    if (input_ring.buffer) {
        dsd_fme_aligned_free(input_ring.buffer);
        input_ring.buffer = NULL;
    }

    /* free LUT memory if allocated */
    atan_lut_free();

    rtl_device_destroy(rtl_device_handle);
    rtl_device_handle = NULL;

    if (g_stream) {
        free(g_stream);
        g_stream = NULL;
    }
}

/**
 * Batched consumer API: read up to count samples with fewer wakeups/locks.
 * Applies volume scaling.
 *
 * @param out   Destination buffer for audio samples.
 * @param count Maximum number of samples to read.
 * @param opts  Decoder options (used for runtime PPM changes).
 * @param state Decoder state (unused).
 * @return Number of samples read (>=1) or -1 on exit.
 */
extern "C" int
dsd_rtl_stream_read(int16_t* out, size_t count, dsd_opts* opts, dsd_state* state) {
    UNUSED(state);
    if (count == 0) {
        return 0;
    }

    /* If PPM Error is Manually Changed, change it here once per batch */
    if (opts->rtlsdr_ppm_error != dongle.ppm_error) {
        dongle.ppm_error = opts->rtlsdr_ppm_error;
        rtl_device_set_ppm(rtl_device_handle, dongle.ppm_error);
    }

    int got = ring_read_batch(&output, out, count);
    if (got <= 0) {
        return -1;
    }
    /* Apply volume scaling with saturation */
    for (int i = 0; i < got; i++) {
        int32_t y = (int32_t)out[i] * (int32_t)volume_multiplier;
        out[i] = sat16(y);
    }
    return got;
}

/**
 * Convenience wrapper to read a single sample via the batched API.
 *
 * @param sample Destination for one sample.
 * @param opts   Decoder options.
 * @param state  Decoder state (unused).
 * @return 0 on success, -1 on exit.
 */
/* single-sample helper removed */

/**
 * Return the current output audio sample rate in Hz.
 *
 * @return Output sample rate in Hz.
 */
extern "C" unsigned int
dsd_rtl_stream_output_rate(void) {
    return (unsigned int)output.rate;
}

/**
 * Tune RTL-SDR to a new center frequency, updating optimal settings.
 *
 * @param opts      Decoder options.
 * @param frequency Target center frequency in Hz.
 */
extern "C" int
dsd_rtl_stream_tune(dsd_opts* opts, long int frequency) {
    int r;
    if (opts->payload == 1) {
        LOG_INFO("\nTuning to %ld Hz.", frequency);
    }
    dongle.freq = opts->rtlsdr_center_freq = frequency;
    apply_capture_settings((uint32_t)dongle.freq);
    if (opts->payload == 1) {
        LOG_INFO(" (Center Frequency: %u Hz.) \n", dongle.freq);
    }
    {
        struct rtl_device* dev = rtl_device_handle;
        if (g_stream && g_stream->device) {
            dev = g_stream->device;
        }
        r = rtl_device_set_frequency(dev, dongle.freq);
        rtl_device_set_sample_rate(dev, dongle.rate);
    }
    if (r < 0) {
        LOG_WARNING(" (Failed to set Center Frequency %u). \n", dongle.freq);
        return r;
    }

    dsd_rtl_stream_clear_output();
    return 0;
}

/**
 * Return mean power approximation (RMS^2 proxy) for soft squelch decisions.
 * Uses a small fixed sample window for efficiency.
 *
 * @return Mean power value (approximate RMS squared).
 */
extern "C" long int
dsd_rtl_stream_return_pwr(void) {
    long int pwr = 0;
    int n = demod.lp_len;
    if (n > 160) {
        n = 160;
    }
    if (n < 0) {
        n = 0;
    }
    pwr = mean_power(demod.lowpassed, n, 1);
    return (pwr);
}

/**
 * Clear the output ring buffer and wake any waiting producer.
 */
extern "C" void
dsd_rtl_stream_clear_output(void) {
    /* Clear the entire ring to prevent sample 'lag' */
    {
        struct output_state* outp = &output;
        if (g_stream && g_stream->output) {
            outp = g_stream->output;
        }
        ring_clear(outp);
    }
    /* Wake producer waiting for space */
    {
        struct output_state* outp = &output;
        if (g_stream && g_stream->output) {
            outp = g_stream->output;
        }
        safe_cond_signal(&outp->space, &outp->ready_m);
    }
}