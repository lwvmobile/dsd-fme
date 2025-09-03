/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
 * Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
 * Copyright (C) 2013 by Elias Oenal <EliasOenal@gmail.com>
 * Copyright (C) 2014 by Kyle Keen <keenerd@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <atomic>
#include <vector>
#include <rtl-sdr.h>
#include "dsd.h"

#define DEFAULT_SAMPLE_RATE		48000
#define DEFAULT_BUF_LENGTH		(1 * 16384)
#define MAXIMUM_OVERSAMPLE		16
#define MAXIMUM_BUF_LENGTH		(MAXIMUM_OVERSAMPLE * DEFAULT_BUF_LENGTH)
#define AUTO_GAIN			-100
#define BUFFER_DUMP		4096

#define FREQUENCIES_LIMIT		  1000

static int lcm_post[17] = {1,1,1,3,1,5,3,7,1,9,5,11,3,13,7,15,1};
static int ACTUAL_BUF_LENGTH;

static const double kPi = 3.14159265358979323846;

/* =====================
   Vectorization helpers and alignment
   ===================== */
#if defined(__GNUC__) || defined(__clang__)
#define DSD_FME_PRAGMA(x) _Pragma(#x)
#define DSD_FME_IVDEP DSD_FME_PRAGMA(GCC ivdep)
template <typename T>
static inline T* assume_aligned_ptr(T* p, size_t /*align_unused*/) {
    return (T*)__builtin_assume_aligned(p, 64);
}
#else
#define DSD_FME_IVDEP
template <typename T>
static inline T* assume_aligned_ptr(T* p, size_t /*align_unused*/) {
    return p;
}
#endif
#ifndef DSD_FME_ALIGN
#define DSD_FME_ALIGN 64
#endif

static int *atan_lut = NULL;
static int atan_lut_size = 131072; /* 512 KB */
static int atan_lut_coef = 8;
static pthread_once_t atan_lut_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t atan_lut_mutex = PTHREAD_MUTEX_INITIALIZER;

/* =====================
   Saturating helpers
   ===================== */
static inline int16_t sat16(int32_t x)
{
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (int16_t)x;
}

/* =====================
   Half-band FIR decimator (2:1) - Q15 taps
   ===================== */

/* Runtime flag (default enabled). Set DSD_FME_HB_DECIM=0 to use legacy decimator */
static int use_halfband_decimator = 1;

/* 15-tap half-band low-pass coefficients, Q15 scaled.
   Odd-indexed taps are zero; center tap is 0.5 (16384). The remaining even taps
   sum to 0.5 to yield unity DC gain. Coefficients are symmetric. */
#define HB_TAPS 15
#define HB_HALF ((HB_TAPS - 1) / 2)
static const int16_t hb_q15_taps[HB_TAPS] = {
	-108,    0,  1800,    0,  -500,    0,  7000, 16384,
	 7000,   0,  -500,    0,  1800,    0,   -108
};

/* Decimate one real channel by 2 using half-band FIR with persistent left history.
   - in:       pointer to real samples
   - in_len:   number of real samples
   - out:      pointer to output buffer (size >= in_len/2)
   - hist:     persistent history of length HB_TAPS-1 (left wing)
   Returns number of output samples written (in_len/2). */
static inline int hb_decim2_real(const int16_t *in, int in_len, int16_t *out, int16_t *hist)
{
	const int hist_len = HB_TAPS - 1;
	/* Pad right side by repeating last sample to avoid needing future context */
	int16_t last = (in_len > 0) ? in[in_len - 1] : 0;
	/* For simplicity, operate via a small ringless window into a temp view using hist + in + right pad (virtually). */
	int out_len = in_len >> 1; /* floor */
	for (int n = 0; n < out_len; n++) {
		int center_idx = hist_len + (n << 1); /* position in the concatenated [hist | in] domain */
		int64_t acc = 0;
		/* Convolution around center: taps indexed 0..HB_TAPS-1 */
		for (int t = 0; t < HB_TAPS; t++) {
			int src_idx = center_idx - HB_HALF + t;
			int16_t x;
			if (src_idx < hist_len) {
				x = hist[src_idx];
			} else {
				int rel = src_idx - hist_len;
				x = (rel < in_len) ? in[rel] : last;
			}
			acc += (int32_t)hb_q15_taps[t] * (int32_t)x;
		}
		/* Q15 -> Q0 with rounding */
		acc += (1 << 14);
		int32_t y = (int32_t)(acc >> 15);
		out[n] = sat16(y);
	}
	/* Update history with the last (HB_TAPS-1) input samples for next call */
	if (in_len >= hist_len) {
		memcpy(hist, in + (in_len - hist_len), (size_t)hist_len * sizeof(int16_t));
	} else {
		/* Not enough samples: keep tail of previous hist and append current */
		int need = hist_len - in_len;
		if (need > 0) {
			/* shift left existing hist */
			memmove(hist, hist + in_len, (size_t)need * sizeof(int16_t));
		}
		memcpy(hist + need, in, (size_t)in_len * sizeof(int16_t));
	}
	return out_len;
}

/* One 2:1 decimation stage on interleaved I/Q, using per-stage histories. */
static inline int hb_decim2_complex_stage(const int16_t *in_iq, int in_iq_len,
	int16_t *out_iq, int16_t *hist_i, int16_t *hist_q)
{
	/* in_iq_len is count of interleaved samples (I,Q,I,Q,...) */
	int ch_len = in_iq_len >> 1; /* samples per channel */
	if (ch_len <= 0) {
		return 0;
	}
	/* Deinterleave into contiguous I and Q working buffers */
	std::vector<int16_t> i_buf;
	std::vector<int16_t> q_buf;
	i_buf.resize((size_t)ch_len);
	q_buf.resize((size_t)ch_len);
	for (int k = 0, j = 0; j < in_iq_len; j += 2, k++) {
		i_buf[(size_t)k] = in_iq[(size_t)j];
		q_buf[(size_t)k] = in_iq[(size_t)j + 1];
	}
	/* Output per channel */
	int out_ch_len;
	{
		/* Reuse i_buf as input; produce into temporary then interleave */
		std::vector<int16_t> i_out;
		std::vector<int16_t> q_out;
		i_out.resize((size_t)(ch_len >> 1));
		q_out.resize((size_t)(ch_len >> 1));
		int ilen = hb_decim2_real(i_buf.data(), ch_len, i_out.data(), hist_i);
		int qlen = hb_decim2_real(q_buf.data(), ch_len, q_out.data(), hist_q);
		out_ch_len = (ilen < qlen) ? ilen : qlen;
		/* Interleave back */
		for (int n = 0; n < out_ch_len; n++) {
			out_iq[(size_t)(2*n)]     = i_out[(size_t)n];
			out_iq[(size_t)(2*n + 1)] = q_out[(size_t)n];
		}
	}
	return out_ch_len << 1; /* interleaved sample count */
}

static void atan_lut_once_init(void)
{
	int i;
	atan_lut = static_cast<int*>(malloc(atan_lut_size * sizeof(int)));
	if (atan_lut == NULL) {
		return;
	}
	for (i = 0; i < atan_lut_size; i++) {
		atan_lut[i] = (int) (atan((double) i / (1<<atan_lut_coef)) / kPi * (1<<14));
	}
}

//UDP -- keep for compatibility reasons
#include <netinet/in.h>
#include <arpa/inet.h>
static pthread_t socket_freq;

int rtl_bandwidth;
int bandwidth_multiplier;
int bandwidth_divisor = 48000; //divide bandwidth by this to get multiplier for the for j loop to queue.push

short int volume_multiplier;
short int port;

struct dongle_state
{
	int      exit_flag;
	pthread_t thread;
	rtlsdr_dev_t *dev;
	int      dev_index;
	uint32_t freq;
	uint32_t rate;
	int      gain;
	uint32_t buf_len;
	int      ppm_error;
	int      offset_tuning;
	int      direct_sampling;
	int      mute;
	struct demod_state *demod_target;
};

struct demod_state
{
	int      exit_flag;
	pthread_t thread;
	int16_t  *lowpassed;
	/* Double-buffered input for callback→demod handoff */
	alignas(DSD_FME_ALIGN) int16_t  input_buffers[2][MAXIMUM_BUF_LENGTH];
	std::atomic<int> write_buf_index;
	std::atomic<int> ready_buf_index;
	std::atomic<uint32_t> input_len[2];
	int      lp_len;
	int16_t  lp_i_hist[10][6];
	int16_t  lp_q_hist[10][6];
	alignas(DSD_FME_ALIGN) int16_t  result[MAXIMUM_BUF_LENGTH];
	int16_t  droop_i_hist[9];
	int16_t  droop_q_hist[9];
	int      result_len;
	int      rate_in;
	int      rate_out;
	int      rate_out2;
	int      now_r, now_j;
	int      pre_r, pre_j;
	int      prev_index;
	int      downsample;    /* min 1, max 256 */
	int      post_downsample;
	int      output_scale;
	int      squelch_level, conseq_squelch, squelch_hits, terminate_on_squelch;
	/* Incremental, decimated RMS squelch estimator (power-domain, sqrt-free) */
	int64_t  squelch_running_power;
	int      squelch_decim_stride;
	int      squelch_decim_phase;
	int      squelch_window;
	int      downsample_passes;
	int      comp_fir_size;
	int      custom_atan;
	int      deemph, deemph_a;
	int      deemph_avg;
	int      now_lpr;
	int      prev_lpr_index;
	int      dc_block, dc_avg;
	/* Half-band decimator state */
	int16_t  hb_workbuf[MAXIMUM_BUF_LENGTH];
	int16_t  hb_hist_i[10][HB_TAPS-1];
	int16_t  hb_hist_q[10][HB_TAPS-1];
	int      (*discriminator)(int, int, int, int);
	void     (*mode_demod)(struct demod_state*);
	pthread_cond_t ready;
	pthread_mutex_t ready_m;
	struct output_state *output_target;
};

struct output_state
{
	int      rate;
	int16_t  *buffer;
	size_t   capacity;
	std::atomic<size_t> head;
	std::atomic<size_t> tail;
	pthread_cond_t ready;
	pthread_cond_t space;
	pthread_mutex_t ready_m;
};

struct controller_state
{
	int      exit_flag;
	pthread_t thread;
	uint32_t freqs[FREQUENCIES_LIMIT];
	int      freq_len;
	int      freq_now;
	int      edge;
	int      wb_mode;
	pthread_cond_t hop;
	pthread_mutex_t hop_m;
};

struct dongle_state dongle;
struct demod_state demod;
struct output_state output;
struct controller_state controller;

#define safe_cond_signal(n, m) pthread_mutex_lock(m); pthread_cond_signal(n); pthread_mutex_unlock(m)
#define safe_cond_wait(n, m) pthread_mutex_lock(m); pthread_cond_wait(n, m); pthread_mutex_unlock(m)

/* =====================
   Thread Scheduling Helpers (optional realtime/affinity)
   ===================== */

static void maybe_set_thread_realtime_and_affinity(const char *role)
{
    const char *enable = getenv("DSD_FME_RT_SCHED");
    if (!enable || enable[0] != '1') {
        return;
    }

    /* Optional: role-specific priority (1..99) for SCHED_FIFO */
    int policy = SCHED_FIFO;
    struct sched_param sp;
    int pmax = sched_get_priority_max(policy);
    int pmin = sched_get_priority_min(policy);
    int def = (pmax > 10) ? (pmax - 10) : pmax; /* default near top, but safe */
    char envname[64];

    sp.sched_priority = def;
    if (role) {
        /* e.g., DSD_FME_RT_PRIO_DEMOD, DSD_FME_RT_PRIO_DONGLE */
        snprintf(envname, sizeof(envname), "DSD_FME_RT_PRIO_%s", role);
        const char *prio_str = getenv(envname);
        if (prio_str && prio_str[0] != '\0') {
            int pr = atoi(prio_str);
            if (pr < pmin) pr = pmin;
            if (pr > pmax) pr = pmax;
            sp.sched_priority = pr;
        }
    }

    if (pthread_setschedparam(pthread_self(), policy, &sp) != 0) {
        fprintf(stderr, "WARNING: Failed to set %s thread to SCHED_FIFO (needs CAP_SYS_NICE).\n", role ? role : "RT");
    } else {
        fprintf(stderr, "%s thread SCHED_FIFO priority set to %d.\n", role ? role : "RT", sp.sched_priority);
    }

    /* Optional: role-specific CPU affinity: DSD_FME_CPU_DEMOD / DSD_FME_CPU_DONGLE */
    if (role) {
        snprintf(envname, sizeof(envname), "DSD_FME_CPU_%s", role);
        const char *cpu_str = getenv(envname);
        if (cpu_str && cpu_str[0] != '\0') {
            int cpu = atoi(cpu_str);
            if (cpu >= 0) {
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET((unsigned)cpu, &cpuset);
                if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0) {
                    fprintf(stderr, "WARNING: Failed to set CPU affinity for %s thread to CPU %d.\n", role, cpu);
                } else {
                    fprintf(stderr, "%s thread pinned to CPU %d.\n", role, cpu);
                }
            }
        }
    }
}

/* =====================
   SPSC Ring Buffer (output path)
   ===================== */

static inline size_t ring_used(const struct output_state *o)
{
    size_t h = o->head.load();
    size_t t = o->tail.load();
    if (h >= t) return h - t;
    return o->capacity - (t - h);
}

static inline size_t ring_free(const struct output_state *o)
{
    return (o->capacity - 1) - ring_used(o);
}

static inline int ring_is_empty(const struct output_state *o)
{
    return o->head.load() == o->tail.load();
}

static inline void ring_clear(struct output_state *o)
{
    o->tail.store(0);
    o->head.store(0);
}

/* Write up to count samples, blocking until space is available. Signals data availability after writes. */
static void ring_write(struct output_state *o, const int16_t *data, size_t count)
{
    while (count > 0 && !exitflag) {
        size_t free_sp = ring_free(o);
        if (free_sp == 0) {
            /* Wait for space */
            safe_cond_wait(&o->space, &o->ready_m);
            continue;
        }
        size_t write_now = (count < free_sp) ? count : free_sp;
        size_t h = o->head.load();
        size_t first = o->capacity - h;
        if (first > write_now) first = write_now;
        memcpy(o->buffer + h, data, first * sizeof(int16_t));
        if (write_now > first) {
            memcpy(o->buffer, data + first, (write_now - first) * sizeof(int16_t));
            h = write_now - first;
        } else {
            h += first;
            if (h == o->capacity) h = 0;
        }
        o->head.store(h);
        data += write_now;
        count -= write_now;
    }
}

/* Same as ring_write but does not signal; caller decides when to signal */
static void ring_write_no_signal(struct output_state *o, const int16_t *data, size_t count)
{
    while (count > 0 && !exitflag) {
        size_t free_sp = ring_free(o);
        if (free_sp == 0) {
            safe_cond_wait(&o->space, &o->ready_m);
            continue;
        }
        size_t write_now = (count < free_sp) ? count : free_sp;
        size_t h = o->head.load();
        size_t first = o->capacity - h;
        if (first > write_now) first = write_now;
        memcpy(o->buffer + h, data, first * sizeof(int16_t));
        if (write_now > first) {
            memcpy(o->buffer, data + first, (write_now - first) * sizeof(int16_t));
            h = write_now - first;
        } else {
            h += first;
            if (h == o->capacity) h = 0;
        }
        o->head.store(h);
        data += write_now;
        count -= write_now;
    }
}

/* Read one sample, returns 0 on success, -1 on exit */
static int ring_read_one(struct output_state *o, int16_t *out)
{
    while (ring_is_empty(o)) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 10L * 1000000L; /* 10ms */
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += ts.tv_nsec / 1000000000L;
            ts.tv_nsec = ts.tv_nsec % 1000000000L;
        }
        pthread_mutex_lock(&o->ready_m);
        pthread_cond_timedwait(&o->ready, &o->ready_m, &ts);
        pthread_mutex_unlock(&o->ready_m);
        if (exitflag) return -1;
    }
    size_t t = o->tail.load();
    *out = o->buffer[t];
    t += 1;
    if (t == o->capacity) t = 0;
    o->tail.store(t);
    /* Signal space available for producer */
    safe_cond_signal(&o->space, &o->ready_m);
    return 0;
}

/* Read up to max_count samples into out. Blocks until at least one sample is available or exit. Returns
   number of samples read (>=1) or -1 on exit. Signals producer space once after the batch. */
static int ring_read_batch(struct output_state *o, int16_t *out, size_t max_count)
{
    if (max_count == 0) return 0;
    while (ring_is_empty(o)) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 10L * 1000000L; /* 10ms */
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += ts.tv_nsec / 1000000000L;
            ts.tv_nsec = ts.tv_nsec % 1000000000L;
        }
        pthread_mutex_lock(&o->ready_m);
        pthread_cond_timedwait(&o->ready, &o->ready_m, &ts);
        pthread_mutex_unlock(&o->ready_m);
        if (exitflag) return -1;
    }

    size_t available = ring_used(o);
    size_t read_now = (max_count < available) ? max_count : available;
    size_t t = o->tail.load();
    size_t first = o->capacity - t;
    if (first > read_now) first = read_now;
    memcpy(out, o->buffer + t, first * sizeof(int16_t));
    t += first;
    if (t == o->capacity) t = 0;
    if (read_now > first) {
        memcpy(out + first, o->buffer, (read_now - first) * sizeof(int16_t));
        t = read_now - first;
    }
    o->tail.store(t);
    /* Signal space available once for the whole batch */
    safe_cond_signal(&o->space, &o->ready_m);
    return (int)read_now;
}

/* {length, coef, coef, coef}  and scaled by 2^15
   for now, only length 9, optimal way to get +85% bandwidth */
#define CIC_TABLE_MAX 10
int cic_9_tables[][10] = {
	{0,},
	{9, -156,  -97, 2798, -15489, 61019, -15489, 2798,  -97, -156},
	{9, -128, -568, 5593, -24125, 74126, -24125, 5593, -568, -128},
	{9, -129, -639, 6187, -26281, 77511, -26281, 6187, -639, -129},
	{9, -122, -612, 6082, -26353, 77818, -26353, 6082, -612, -122},
	{9, -120, -602, 6015, -26269, 77757, -26269, 6015, -602, -120},
	{9, -120, -582, 5951, -26128, 77542, -26128, 5951, -582, -120},
	{9, -119, -580, 5931, -26094, 77505, -26094, 5931, -580, -119},
	{9, -119, -578, 5921, -26077, 77484, -26077, 5921, -578, -119},
	{9, -119, -577, 5917, -26067, 77473, -26067, 5917, -577, -119},
	{9, -199, -362, 5303, -25505, 77489, -25505, 5303, -362, -199},
};

void rotate_90(unsigned char *buf, uint32_t len)
/* 90 rotation is 1+0j, 0+1j, -1+0j, 0-1j
   or [0, 1, -3, 2, -4, -5, 7, -6] */
{
	uint32_t i;
	unsigned char tmp;
	for (i=0; i<len; i+=8) {
		/* uint8_t negation = 255 - x */
		tmp = 255 - buf[i+3];
		buf[i+3] = buf[i+2];
		buf[i+2] = tmp;

		buf[i+4] = 255 - buf[i+4];
		buf[i+5] = 255 - buf[i+5];

		tmp = 255 - buf[i+6];
		buf[i+6] = buf[i+7];
		buf[i+7] = tmp;
	}
}

void low_pass(struct demod_state *d)
/* simple square window FIR */
{
	int i=0, i2=0;
	while (i < d->lp_len) {
		d->now_r += d->lowpassed[i];
		d->now_j += d->lowpassed[i+1];
		i += 2;
		d->prev_index++;
		if (d->prev_index < d->downsample) {
			continue;
		}
		/* Saturate accumulated sums when writing back to int16 */
		d->lowpassed[i2]   = sat16(d->now_r);
		d->lowpassed[i2+1] = sat16(d->now_j);
		d->prev_index = 0;
		d->now_r = 0;
		d->now_j = 0;
		i2 += 2;
	}
	d->lp_len = i2;
}

int low_pass_simple(int16_t *signal2, int len, int step)
// no wrap around, length must be multiple of step
{
	int i, i2, sum;
	for(i=0; i < len; i+=step) {
		sum = 0;
		for(i2=0; i2<step; i2++) {
			sum += (int)signal2[i + i2];
		}
		//signal2[i/step] = (int16_t)(sum / step);
		/* Saturate accumulated sum on write */
		signal2[i/step] = sat16(sum);
	}
	signal2[i/step + 1] = signal2[i/step];
	return len / step;
}

void low_pass_real(struct demod_state *s)
/* simple square window FIR */
// add support for upsampling?
{
	int i=0, i2=0;
	int16_t *r = assume_aligned_ptr(s->result, DSD_FME_ALIGN);
	int fast = (int)s->rate_out;
	int slow = s->rate_out2;
	/* Precompute fixed-point reciprocal of decimation factor to avoid per-sample division */
	int decim = (slow != 0) ? (fast / slow) : 1;
	if (decim < 1) decim = 1;
	const int kShiftLPR = 15; /* Q15 reciprocal */
	int recip_decim_q = (1 << kShiftLPR) / decim;
	DSD_FME_IVDEP
	while (i < s->result_len) {
		s->now_lpr += r[i];
		i++;
		s->prev_lpr_index += slow;
		if (s->prev_lpr_index < fast) {
			continue;
		}
		/* Multiply by reciprocal and shift instead of dividing by (fast/slow) */
		int64_t scaled = ((int64_t)s->now_lpr * recip_decim_q);
		r[i2] = (int16_t)(scaled >> kShiftLPR);
		s->prev_lpr_index -= fast;
		s->now_lpr = 0;
		i2 += 1;
	}
	s->result_len = i2;
}

void fifth_order(int16_t *data, int length, int16_t *hist)
/* for half of interleaved data */
{
	int i;
	int16_t a, b, c, d, e, f;
	a = hist[1];
	b = hist[2];
	c = hist[3];
	d = hist[4];
	e = hist[5];
	f = data[0];
	/* a downsample should improve resolution, so don't fully shift */
	data[0] = (a + (b+e)*5 + (c+d)*10 + f) >> 4;
	for (i=4; i<length; i+=4) {
		a = c;
		b = d;
		c = e;
		d = f;
		e = data[i-2];
		f = data[i];
		data[i/2] = (a + (b+e)*5 + (c+d)*10 + f) >> 4;
	}
	/* archive */
	hist[0] = a;
	hist[1] = b;
	hist[2] = c;
	hist[3] = d;
	hist[4] = e;
	hist[5] = f;
}

void generic_fir(int16_t *data, int length, int *fir, int16_t *hist)
/* Okay, not at all generic.  Assumes length 9, fix that eventually. */
{
	int d, temp, sum;
	for (d=0; d<length; d+=2) {
		temp = data[d];
		sum = 0;
		sum += (hist[0] + hist[8]) * fir[1];
		sum += (hist[1] + hist[7]) * fir[2];
		sum += (hist[2] + hist[6]) * fir[3];
		sum += (hist[3] + hist[5]) * fir[4];
		sum +=            hist[4]  * fir[5];
		data[d] = sum >> 15 ;
		hist[0] = hist[1];
		hist[1] = hist[2];
		hist[2] = hist[3];
		hist[3] = hist[4];
		hist[4] = hist[5];
		hist[5] = hist[6];
		hist[6] = hist[7];
		hist[7] = hist[8];
		hist[8] = temp;
	}
}

// define our own complex math ops because ARMv5 has no hardware float
void multiply(int ar, int aj, int br, int bj, int *cr, int *cj)
{
	*cr = ar*br - aj*bj;
	*cj = aj*br + ar*bj;
}

/* 64-bit safe complex multiply to prevent overflow in discriminator math */
static inline void multiply64(int ar, int aj, int br, int bj, int64_t *cr, int64_t *cj)
{
	*cr = (int64_t)ar * (int64_t)br - (int64_t)aj * (int64_t)bj;
	*cj = (int64_t)aj * (int64_t)br + (int64_t)ar * (int64_t)bj;
}

int polar_discriminant(int ar, int aj, int br, int bj)
{
	int64_t cr, cj;
	double angle;
	multiply64(ar, aj, br, -bj, &cr, &cj);
	angle = atan2((double)cj, (double)cr);
	return (int)(angle / kPi * (1<<14));
}

int fast_atan2(int y, int x)
/* pre scaled for int16 */
{
	int yabs, angle;
	int pi4=(1<<12), pi34=3*(1<<12);  // note pi = 1<<14
	if (x==0 && y==0) {
		return 0;
	}
	yabs = y;
	if (yabs < 0) {
		yabs = -yabs;
	}
	if (x >= 0) {
		angle = pi4  - pi4 * (x-yabs) / (x+yabs);
	} else {
		angle = pi34 - pi4 * (x+yabs) / (yabs-x);
	}
	if (y < 0) {
		return -angle;
	}
	return angle;
}

/* 64-bit safe version of fast atan2 to avoid overflow in intermediate math */
int fast_atan2_64(int64_t y, int64_t x)
/* pre scaled for int16, returns angle scaled so that pi == 1<<14 */
{
	int angle;
	int pi4=(1<<12), pi34=3*(1<<12);  /* note: pi = 1<<14 */
	int64_t yabs;
	if (x == 0 && y == 0) {
		return 0;
	}
	yabs = y;
	if (yabs < 0) {
		yabs = -yabs;
	}
	if (x >= 0) {
		/* denominator (x + yabs) cannot be zero here unless x==y==0 handled above */
		angle = (int)(pi4  - ( (int64_t)pi4 * (x - yabs) ) / (x + yabs));
	} else {
		/* denominator (yabs - x) > 0 */
		angle = (int)(pi34 - ( (int64_t)pi4 * (x + yabs) ) / (yabs - x));
	}
	if (y < 0) {
		return -angle;
	}
	return angle;
}

int polar_disc_fast(int ar, int aj, int br, int bj)
{
	int64_t cr, cj;
	multiply64(ar, aj, br, -bj, &cr, &cj);
	return fast_atan2_64(cj, cr);
}

int atan_lut_init(void)
{
	/* Thread-safe, idempotent initialization */
	pthread_once(&atan_lut_once, atan_lut_once_init);
	if (atan_lut != NULL) {
		return 0;
	}
	/* If LUT was freed after once, allow re-init guarded by mutex */
	pthread_mutex_lock(&atan_lut_mutex);
	if (atan_lut == NULL) {
		atan_lut_once_init();
	}
	pthread_mutex_unlock(&atan_lut_mutex);
	return (atan_lut != NULL) ? 0 : -1;
}

void atan_lut_free(void)
{
	pthread_mutex_lock(&atan_lut_mutex);
	if (atan_lut != NULL) {
		free(atan_lut);
		atan_lut = NULL;
	}
	pthread_mutex_unlock(&atan_lut_mutex);
}

int polar_disc_lut(int ar, int aj, int br, int bj)
{
	int64_t cr, cj;
	int64_t x, x_abs;

	/* Ensure LUT is available; fall back if allocation failed */
	atan_lut_init();
	if (atan_lut == NULL) {
		multiply64(ar, aj, br, -bj, &cr, &cj);
		return fast_atan2_64(cj, cr);
	}

	multiply64(ar, aj, br, -bj, &cr, &cj);

	/* special cases */
	if (cr == 0 || cj == 0) {
		if (cr == 0 && cj == 0)
			{return 0;}
		if (cr == 0 && cj > 0)
			{return 1 << 13;}
		if (cr == 0 && cj < 0)
			{return -(1 << 13);}
		if (cj == 0 && cr > 0)
			{return 0;}
		if (cj == 0 && cr < 0)
			{return (1 << 14) - 1;}
	}

	/* real range -32768 - 32768 use 64x range -> absolute maximum: 2097152 */
	x = ((int64_t)cj << atan_lut_coef) / cr;
	x_abs = (x < 0) ? -x : x;

	if (x_abs >= (int64_t)atan_lut_size) {
		/* Preserve quadrant using both cr and cj signs */
		if (cr < 0) {
			return (cj >= 0) ? ((1 << 14) - 1) : (-(1 << 14) + 1);
		} else {
			return (cj >= 0) ? (1 << 13) : -(1 << 13);
		}
	}

	if (x > 0) {
		int val = (cj > 0) ? atan_lut[(int)x] : (atan_lut[(int)x] - (1<<14));
		if (val == (1 << 14)) { val = (1 << 14) - 1; }
		if (val == -(1 << 14)) { val = -(1 << 14) + 1; }
		return val;
	} else {
		int val = (cj > 0) ? ((1<<14) - atan_lut[(int)(-x)]) : (-atan_lut[(int)(-x)]);
		if (val == (1 << 14)) { val = (1 << 14) - 1; }
		if (val == -(1 << 14)) { val = -(1 << 14) + 1; }
		return val;
	}

	return 0;
}

void fm_demod(struct demod_state *fm)
{
	int i, pcm;
	int16_t *lp = assume_aligned_ptr(fm->lowpassed, DSD_FME_ALIGN);
	int16_t *res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
	/* Use selected discriminator from the very first sample */
	pcm = fm->discriminator(lp[0], lp[1], fm->pre_r, fm->pre_j);
	res[0] = (int16_t)pcm;
	DSD_FME_IVDEP
	for (i = 2; i < (fm->lp_len-1); i += 2) {
		pcm = fm->discriminator(lp[i], lp[i+1], lp[i-2], lp[i-1]);
		res[i/2] = (int16_t)pcm;
	}
	fm->pre_r = lp[fm->lp_len - 2];
	fm->pre_j = lp[fm->lp_len - 1];
	fm->result_len = fm->lp_len/2;
}

void raw_demod(struct demod_state *fm)
{
	int i;
	for (i = 0; i < fm->lp_len; i++) {
		fm->result[i] = (int16_t)fm->lowpassed[i];
	}
	fm->result_len = fm->lp_len;
}

void deemph_filter(struct demod_state *fm)
{
	int avg = fm->deemph_avg; /* per-instance state */
	int i, d;
	int16_t *res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
	/* Precompute fixed-point reciprocal of deemphasis constant to avoid per-sample division */
	const int kShiftDeemph = 15; /* Q15 */
	int a = fm->deemph_a;
	if (a <= 0) a = 1;
	int recip_q = (1 << kShiftDeemph) / a;
	/* Single-pole IIR: avg += (x - avg) * (1 - a) with fixed-point scaling */
	DSD_FME_IVDEP
	for (i = 0; i < fm->result_len; i++) {
		d = res[i] - avg;
		int64_t delta = (int64_t)d * recip_q;
		/* symmetric rounding */
		if (d > 0) {
			delta += (1LL << (kShiftDeemph - 1));
		} else if (d < 0) {
			delta -= (1LL << (kShiftDeemph - 1));
		}
		avg += (int)(delta >> kShiftDeemph);
		res[i] = (int16_t)avg;
	}
	fm->deemph_avg = avg; /* write back state */
}

void dc_block_filter(struct demod_state *fm)
{
	int i;
	/* Leaky integrator high-pass: dc += (x - dc) >> k; y = x - dc */
	int dc = fm->dc_avg;
	const int k = 11; /* cutoff ~ Fs / 2^k (k in 10..12) */
	int16_t *res = assume_aligned_ptr(fm->result, DSD_FME_ALIGN);
	DSD_FME_IVDEP
	for (i = 0; i < fm->result_len; i++) {
		int x = (int)res[i];
		dc += (x - dc) >> k;
		int y = x - dc;
		res[i] = sat16(y);
	}
	fm->dc_avg = dc;
}

long int rms(int16_t *samples, int len, int step)
/* largely lifted from rtl_power */
{
	int i;
	long int rms;
	long p, t, s;
	double dc, err;

	p = t = 0L;
	for (i=0; i<len; i+=step) {
		s = (long)samples[i];
		t += s;
		p += s * s;
	}
	/* correct for dc offset in squares */
	dc = (double)(t*step) / (double)len;
	err = t * 2 * dc - dc * dc * len;

	rms = (long int)sqrt((p-err) / len);
	//going with a value that's easy to figure out when its done the thing
	if (rms < 0){ rms = 999; }
	return rms;
}

void full_demod(struct demod_state *d)
{
	int i, ds_p;
	ds_p = d->downsample_passes;
	if (ds_p) {
		/* Choose decimator: half-band cascade (default) or legacy path */
		if (use_halfband_decimator) {
			/* Apply ds_p stages of 2:1 half-band decimation on interleaved lowpassed */
			int in_len = d->lp_len;
			int16_t *src = d->lowpassed;
			int16_t *dst = d->hb_workbuf;
			for (i = 0; i < ds_p; i++) {
				int out_len = hb_decim2_complex_stage(src, in_len, dst, d->hb_hist_i[i], d->hb_hist_q[i]);
				/* Next stage uses previous output as input */
				src = dst;
				in_len = out_len;
				/* swap buffers for next stage to avoid overwrite if needed */
				dst = (src == d->hb_workbuf) ? d->lowpassed : d->hb_workbuf;
			}
			/* Final output resides in 'src' with length in_len */
			if (d->lowpassed != src) {
				memcpy(d->lowpassed, src, (size_t)in_len * sizeof(int16_t));
			}
			d->lp_len = in_len;
			/* No droop compensation for half-band cascade */
		} else {
			for (i=0; i < ds_p; i++) {
				fifth_order(d->lowpassed,   (d->lp_len >> i), d->lp_i_hist[i]);
				fifth_order(d->lowpassed+1, (d->lp_len >> i) - 1, d->lp_q_hist[i]);
			}
			d->lp_len = d->lp_len >> ds_p;
			/* droop compensation */
			if (d->comp_fir_size == 9 && ds_p <= CIC_TABLE_MAX) {
				generic_fir(d->lowpassed, d->lp_len,
					cic_9_tables[ds_p], d->droop_i_hist);
				generic_fir(d->lowpassed+1, d->lp_len-1,
					cic_9_tables[ds_p], d->droop_q_hist);
			}
		}
	} else {
		low_pass(d);
	}
	/* power squelch (sqrt-free): compare mean power to squared threshold */
	if (d->squelch_level) {
		/* Decimated block power estimate (no DC correction; EMA smooths) */
		int stride = (d->squelch_decim_stride > 0) ? d->squelch_decim_stride : 16;
		int phase = d->squelch_decim_phase;
		int64_t p = 0;
		int count = 0;
		for (int j = phase; j < d->lp_len; j += stride) {
			int64_t s2 = (int64_t)d->lowpassed[j];
			p += s2 * s2;
			count++;
		}
		/* Advance phase to sample different positions next block */
		if (stride > 0) {
			int adv = d->lp_len % stride;
			d->squelch_decim_phase = (phase + adv) % stride;
		}
		if (count > 0) {
			int64_t block_mean = p / count;
			if (d->squelch_running_power == 0) {
				/* Initialize on first measurement to avoid long ramp */
				d->squelch_running_power = block_mean;
			} else {
				/* EMA: running += (block_mean - running) / window */
				int w = (d->squelch_window > 0) ? d->squelch_window : 2048;
				int shift = 0;
				/* approximate log2(window), prefer power-of-two windows */
				while ((1 << shift) < w && shift < 30) { shift++; }
				int64_t delta = (block_mean - d->squelch_running_power);
				d->squelch_running_power += (delta >> shift);
			}
		}
		int64_t thr2 = (int64_t)d->squelch_level * (int64_t)d->squelch_level;
		if (d->squelch_running_power < thr2) {
			d->squelch_hits++;
			for (i=0; i<d->lp_len; i++) {
				d->lowpassed[i] = 0;
			}
		} else {
			d->squelch_hits = 0;
		}
	}
	d->mode_demod(d);  /* lowpassed -> result */
	if (d->mode_demod == &raw_demod) {
		return;
	}
	/* todo, fm noise squelch */
	// use nicer filter here too?
	if (d->post_downsample > 1) {
		d->result_len = low_pass_simple(d->result, d->result_len, d->post_downsample);}
	if (d->deemph) {
		deemph_filter(d);}
	if (d->dc_block) {
		dc_block_filter(d);}
	if (d->rate_out2 > 0) {
		low_pass_real(d);
		//arbitrary_resample(d->result, d->result, d->result_len, d->result_len * d->rate_out2 / d->rate_out);
	}
}

static void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx)
{
	int i;
	struct dongle_state *s = static_cast<dongle_state*>(ctx);
	struct demod_state *d = s->demod_target;
	/* One-time: ensure the USB callback thread gets RT scheduling/affinity if enabled */
	{
		static std::atomic<int> usb_sched_applied{0};
		int expected = 0;
		if (usb_sched_applied.compare_exchange_strong(expected, 1)) {
			maybe_set_thread_realtime_and_affinity("USB");
		}
	}

	if (exitflag) {
		return;}
	if (!ctx) {
		return;}
	if (s->mute) {
		for (i=0; i<s->mute; i++) {
			buf[i] = 127;}
		s->mute = 0;
	}
	if (!s->offset_tuning) {
		rotate_90(buf, len);}
	/* Write directly into the current write buffer */
	int wb = d->write_buf_index.load();
	int16_t *dst = d->input_buffers[wb];
	for (i=0; i<(int)len; i++) {
		dst[i] = (int16_t)buf[i] - 127;
	}
	d->input_len[wb].store(len);
	/* Flip buffers atomically: the buffer we just wrote becomes ready */
	d->ready_buf_index.store(wb);
	d->write_buf_index.store(wb ^ 1);
	safe_cond_signal(&d->ready, &d->ready_m);
}

static void *dongle_thread_fn(void *arg)
{
	struct dongle_state *s = static_cast<dongle_state*>(arg);
	maybe_set_thread_realtime_and_affinity("DONGLE");
	rtlsdr_read_async(s->dev, rtlsdr_callback, s, 16, s->buf_len);
	return 0;
}

static void *demod_thread_fn(void *arg)
{
	struct demod_state *d = static_cast<demod_state*>(arg);
	struct output_state *o = d->output_target;
	maybe_set_thread_realtime_and_affinity("DEMOD");
	while (!exitflag) {
		safe_cond_wait(&d->ready, &d->ready_m);
		/* Consume from last fully-written input buffer */
		int rb = d->ready_buf_index.load();
		d->lowpassed = d->input_buffers[rb];
		d->lp_len = (int)d->input_len[rb].load();
		full_demod(d);
		if (d->exit_flag) {
			exitflag = 1;
		}
		if (d->squelch_level && d->squelch_hits > d->conseq_squelch) {
			d->squelch_hits = d->conseq_squelch + 1;  /* hair trigger */
			safe_cond_signal(&controller.hop, &controller.hop_m);
			continue;
		}
		/* Write demod block to SPSC ring. If upsampling (bandwidth_multiplier > 1),
		   linearly interpolate between adjacent samples instead of duplicating. */
		if (bandwidth_multiplier <= 1) {
			ring_write_no_signal(o, d->result, (size_t)d->result_len);
		} else {
			const int M = bandwidth_multiplier;
			const int N = d->result_len;
			if (N <= 0) {
				/* nothing to write */
			} else if (N == 1) {
				/* Degenerate case: only one sample, replicate M times */
				std::vector<int16_t> tmp(M);
				for (int m = 0; m < M; m++) tmp[m] = d->result[0];
				ring_write_no_signal(o, tmp.data(), (size_t)M);
			} else {
				/* N >= 2: perform linear interpolation between successive samples */
				const size_t up_len = (size_t)N * (size_t)M;
				std::vector<int16_t> upsampled;
				upsampled.resize(up_len);
				for (int n = 0; n < N - 1; n++) {
					int16_t x0 = d->result[n];
					int16_t x1 = d->result[n + 1];
					int32_t dx = (int32_t)x1 - (int32_t)x0;
					for (int m = 0; m < M; m++) {
						int32_t interp = (int32_t)x0 + (dx * m) / M;
						upsampled[(size_t)n * (size_t)M + (size_t)m] = (int16_t)interp;
					}
				}
				/* Last original sample maps to the last position */
				upsampled[(size_t)(N - 1) * (size_t)M] = d->result[N - 1];
				ring_write_no_signal(o, upsampled.data(), up_len);
			}
		}
		safe_cond_signal(&o->ready, &o->ready_m);
	}
	return 0;
}

int nearest_gain(rtlsdr_dev_t *dev, int target_gain)
{
	int i, r, err1, err2, count, nearest;
	int* gains;
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf (stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	count = rtlsdr_get_tuner_gains(dev, NULL);
	if (count <= 0) {
		return 0;
	}
	gains = static_cast<int*>(malloc(sizeof(int) * count));
	count = rtlsdr_get_tuner_gains(dev, gains);
	nearest = gains[0];
	for (i=0; i<count; i++) {
		err1 = abs(target_gain - nearest);
		err2 = abs(target_gain - gains[i]);
		if (err2 < err1) {
			nearest = gains[i];
		}
	}
	free(gains);
	return nearest;
}

int verbose_set_frequency(rtlsdr_dev_t *dev, uint32_t frequency)
{
	int r;
	r = rtlsdr_set_center_freq(dev, frequency);
	if (r < 0) {
		fprintf (stderr, " (WARNING: Failed to set Center Frequency). \n");
	} else {
		fprintf (stderr, " (Center Frequency: %u Hz.) \n", frequency);
	}
	return r;
}

int verbose_set_sample_rate(rtlsdr_dev_t *dev, uint32_t samp_rate)
{
	int r;
	r = rtlsdr_set_sample_rate(dev, samp_rate);
	if (r < 0) {
		fprintf (stderr, "WARNING: Failed to set sample rate.\n");
	} else {
		fprintf (stderr, "Sampling at %u S/s.\n", samp_rate);
	}
	return r;
}

int verbose_direct_sampling(rtlsdr_dev_t *dev, int on)
{
	int r;
	r = rtlsdr_set_direct_sampling(dev, on);
	if (r != 0) {
		fprintf (stderr, "WARNING: Failed to set direct sampling mode.\n");
		return r;
	}
	if (on == 0) {
		fprintf (stderr, "Direct sampling mode disabled.\n");}
	if (on == 1) {
		fprintf (stderr, "Enabled direct sampling mode, input 1/I.\n");}
	if (on == 2) {
		fprintf (stderr, "Enabled direct sampling mode, input 2/Q.\n");}
	return r;
}

int verbose_offset_tuning(rtlsdr_dev_t *dev)
{
	int r;
	r = rtlsdr_set_offset_tuning(dev, 1);
	if (r != 0) {
		fprintf (stderr, "WARNING: Failed to set offset tuning.\n");
	} else {
		fprintf (stderr, "Offset tuning mode enabled.\n");
	}
	return r;
}

int verbose_auto_gain(rtlsdr_dev_t *dev)
{
	int r;
	r = rtlsdr_set_tuner_gain_mode(dev, 0);
	if (r != 0) {
		fprintf (stderr, "WARNING: Failed to set tuner gain.\n");
	} else {
		fprintf (stderr, "Tuner gain set to automatic.\n");
	}
	return r;
}

int verbose_gain_set(rtlsdr_dev_t *dev, int gain)
{
	int r;
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf (stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	r = rtlsdr_set_tuner_gain(dev, gain);
	if (r != 0) {
		fprintf (stderr, "WARNING: Failed to set tuner gain.\n");
	} else {
		fprintf (stderr, "Tuner gain set to %0.2f dB.\n", gain/10.0);
	}
	return r;
}

int verbose_ppm_set(rtlsdr_dev_t *dev, int ppm_error)
{
	int r;
	// if (ppm_error == 0) {
	// 	return 0;}
	r = rtlsdr_set_freq_correction(dev, ppm_error);
	if (r < 0) {
		fprintf (stderr, "WARNING: Failed to set ppm error.\n");
	} else {
		fprintf (stderr, "Tuner error set to %i ppm.\n", ppm_error);
	}
	return r;
}

int verbose_reset_buffer(rtlsdr_dev_t *dev)
{
	int r;
	r = rtlsdr_reset_buffer(dev);
	if (r < 0) {
		fprintf (stderr, "WARNING: Failed to reset buffers.\n");}
	return r;
}

static void optimal_settings(int freq, int rate)
{
	UNUSED(rate);

	// giant ball of hacks
	// seems unable to do a single pass, 2:1
	int capture_freq, capture_rate;
	struct dongle_state *d = &dongle;
	struct demod_state *dm = &demod;
	struct controller_state *cs = &controller;
	dm->downsample = (1000000 / dm->rate_in) + 1; //dm->rate_in is the rtl_bandwidth value
	if (dm->downsample_passes) {
		dm->downsample_passes = (int)log2(dm->downsample) + 1;
		dm->downsample = 1 << dm->downsample_passes;
	}
	capture_freq = freq;
	capture_rate = dm->downsample * dm->rate_in; //
	if (!d->offset_tuning) {
		capture_freq = freq + capture_rate/4;} //
	capture_freq += cs->edge * dm->rate_in / 2;
	dm->output_scale = (1<<15) / (128 * dm->downsample);
	if (dm->output_scale < 1) {
		dm->output_scale = 1;}
	if (dm->mode_demod == &fm_demod) {
		dm->output_scale = 1;}
	d->freq = (uint32_t)capture_freq;
	d->rate = (uint32_t)capture_rate;
	// fprintf (stderr, "Capture Frequency: %i Rate: %i \n", capture_freq, capture_rate);
}

static void *controller_thread_fn(void *arg)
{
	// thoughts for multiple dongles
	// might be no good using a controller thread if retune/rate blocks
	int i;
	struct controller_state *s = static_cast<controller_state*>(arg);

	if (s->wb_mode) {
		for (i=0; i < s->freq_len; i++) {
			s->freqs[i] += 16000;}
	}

	/* set up primary channel */
	optimal_settings(s->freqs[0], demod.rate_in);
	if (dongle.direct_sampling) {
		verbose_direct_sampling(dongle.dev, 1);}
	if (dongle.offset_tuning) {
		verbose_offset_tuning(dongle.dev);}

	/* Set the frequency */
	verbose_set_frequency(dongle.dev, dongle.freq);
	fprintf (stderr, "Oversampling input by: %ix.\n", demod.downsample);
	fprintf (stderr, "Oversampling output by: %ix.\n", demod.post_downsample);
	fprintf (stderr, "Buffer size: %0.2fms\n",
		1000 * 0.5 * (float)ACTUAL_BUF_LENGTH / (float)dongle.rate);

	/* Set the sample rate */
	verbose_set_sample_rate(dongle.dev, dongle.rate);
	fprintf (stderr, "Output at %u Hz.\n", demod.rate_in/demod.post_downsample);

	while (!exitflag) {
		safe_cond_wait(&s->hop, &s->hop_m);
		if (s->freq_len <= 1) {
			continue;}
		/* hacky hopping */
		s->freq_now = (s->freq_now + 1) % s->freq_len;
		optimal_settings(s->freqs[s->freq_now], demod.rate_in);
		rtlsdr_set_center_freq(dongle.dev, dongle.freq);
		dongle.mute = BUFFER_DUMP;
	}
	return 0;
}

void dongle_init(struct dongle_state *s)
{
	s->rate = rtl_bandwidth;
	s->gain = AUTO_GAIN; // tenths of a dB
	s->mute = 0;
	s->direct_sampling = 0;
	s->offset_tuning = 0; //E4000 tuners only
	s->demod_target = &demod;
}

void demod_init_analog(struct demod_state *s)
{
	s->rate_in = rtl_bandwidth;
	s->rate_out = rtl_bandwidth;
	s->squelch_level = 0;
	s->conseq_squelch = 10;
	s->terminate_on_squelch = 0;
	s->squelch_hits = 11;
	s->downsample_passes = 1; //
	s->comp_fir_size = 0;
	s->prev_index = 0;
	s->post_downsample = 1;  //1 -- once this works, default = 4 -- doesn't work on the official rtl-sdr source code either
	s->custom_atan = 2;
	s->deemph = 1; //
	s->rate_out2 = rtl_bandwidth;  // -1 flag for disabled -- this enables low_pass_real, seems to work okay
	s->mode_demod = &fm_demod;
	s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
	s->prev_lpr_index = 0;
	s->deemph_a = 0; //
	s->deemph_avg = 0;
	s->now_lpr = 0;
	s->dc_block = 1; //
	s->dc_avg = 0;
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
	/* Double-buffer init */
	s->write_buf_index.store(0);
	s->ready_buf_index.store(1);
	s->input_len[0].store(0);
	s->input_len[1].store(0);
	s->lowpassed = s->input_buffers[s->ready_buf_index.load()];
	s->lp_len = 0;
	pthread_cond_init(&s->ready, NULL);
	pthread_mutex_init(&s->ready_m, NULL);
	s->output_target = &output;
	if (s->custom_atan == 2 && atan_lut == NULL) { atan_lut_init(); }
	/* set discriminator function pointer */
	s->discriminator = (s->custom_atan == 0) ? &polar_discriminant :
		(s->custom_atan == 1) ? &polar_disc_fast : &polar_disc_lut;
}

void demod_init_ro2(struct demod_state *s)
{
	s->rate_in = rtl_bandwidth;
	s->rate_out = rtl_bandwidth;
	s->squelch_level = 0;
	s->conseq_squelch = 10;
	s->terminate_on_squelch = 0;
	s->squelch_hits = 11;
	s->downsample_passes = 0;
	s->comp_fir_size = 0;
	s->prev_index = 0;
	s->post_downsample = 1;  //1 -- once this works, default = 4 -- doesn't work on the official rtl-sdr source code either
	s->custom_atan = 2;
	s->deemph = 0;
	s->rate_out2 = rtl_bandwidth;  // -1 flag for disabled -- this enables low_pass_real, seems to work okay
	s->mode_demod = &fm_demod;
	s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
	s->prev_lpr_index = 0;
	s->deemph_a = 0;
	s->deemph_avg = 0;
	s->now_lpr = 0;
	s->dc_block = 1; //enabling by default, but offset tuning is also enabled, so center spike shouldn't be an issue
	s->dc_avg = 0;
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
	/* Double-buffer init */
	s->write_buf_index.store(0);
	s->ready_buf_index.store(1);
	s->input_len[0].store(0);
	s->input_len[1].store(0);
	s->lowpassed = s->input_buffers[s->ready_buf_index.load()];
	s->lp_len = 0;
	pthread_cond_init(&s->ready, NULL);
	pthread_mutex_init(&s->ready_m, NULL);
	s->output_target = &output;
	if (s->custom_atan == 2 && atan_lut == NULL) { atan_lut_init(); }
	/* set discriminator function pointer */
	s->discriminator = (s->custom_atan == 0) ? &polar_discriminant :
		(s->custom_atan == 1) ? &polar_disc_fast : &polar_disc_lut;
}

void demod_init(struct demod_state *s)
{
	s->rate_in = rtl_bandwidth;
	s->rate_out = rtl_bandwidth;
	s->squelch_level = 0;
	s->conseq_squelch = 10;
	s->terminate_on_squelch = 0;
	s->squelch_hits = 11;
	s->downsample_passes = 0;
	s->comp_fir_size = 0;
	s->prev_index = 0;
	s->post_downsample = 1;  //1 -- once this works, default = 4 -- doesn't work on the official rtl-sdr source code either
	s->custom_atan = 2;
	s->deemph = 0;
	s->rate_out2 = -1;  // -1 flag for disabled -- this enables low_pass_real, seems to work okay
	s->mode_demod = &fm_demod;
	s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
	s->prev_lpr_index = 0;
	s->deemph_a = 0;
	s->deemph_avg = 0;
	s->now_lpr = 0;
	s->dc_block = 1; //enabling by default, but offset tuning is also enabled, so center spike shouldn't be an issue
	s->dc_avg = 0;
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
	/* Double-buffer init */
	s->write_buf_index.store(0);
	s->ready_buf_index.store(1);
	s->input_len[0].store(0);
	s->input_len[1].store(0);
	s->lowpassed = s->input_buffers[s->ready_buf_index.load()];
	s->lp_len = 0;
	pthread_cond_init(&s->ready, NULL);
	pthread_mutex_init(&s->ready_m, NULL);
	s->output_target = &output;
	if (s->custom_atan == 2 && atan_lut == NULL) { atan_lut_init(); }
	/* set discriminator function pointer */
	s->discriminator = (s->custom_atan == 0) ? &polar_discriminant :
		(s->custom_atan == 1) ? &polar_disc_fast : &polar_disc_lut;
}

void demod_cleanup(struct demod_state *s)
{
	pthread_cond_destroy(&s->ready);
	pthread_mutex_destroy(&s->ready_m);
}

void output_init(struct output_state *s)
{
	s->rate = rtl_bandwidth;
	pthread_cond_init(&s->ready, NULL);
	pthread_cond_init(&s->space, NULL);
	pthread_mutex_init(&s->ready_m, NULL);
	/* Allocate SPSC ring buffer */
	s->capacity = (size_t)(MAXIMUM_BUF_LENGTH * 8);
	/* Try aligned allocation for better vectorized copies; fall back if unavailable */
	{
		void *mem_ptr = NULL;
#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L)
		if (posix_memalign(&mem_ptr, DSD_FME_ALIGN, s->capacity * sizeof(int16_t)) != 0) {
			mem_ptr = malloc(s->capacity * sizeof(int16_t));
		}
#else
		mem_ptr = malloc(s->capacity * sizeof(int16_t));
#endif
		s->buffer = static_cast<int16_t*>(mem_ptr);
	}
	s->head.store(0);
	s->tail.store(0);
}

void output_cleanup(struct output_state *s)
{
	pthread_cond_destroy(&s->ready);
	pthread_cond_destroy(&s->space);
	pthread_mutex_destroy(&s->ready_m);
	if (s->buffer) { free(s->buffer); s->buffer = NULL; }
}

void controller_init(struct controller_state *s)
{
	s->freqs[0] = 446000000;
	s->freq_len = 0;
	s->edge = 0;
	s->wb_mode = 0;
	pthread_cond_init(&s->hop, NULL);
	pthread_mutex_init(&s->hop_m, NULL);
}

void controller_cleanup(struct controller_state *s)
{
	pthread_cond_destroy(&s->hop);
	pthread_mutex_destroy(&s->hop_m);
}

void sanity_checks(void)
{
	if (controller.freq_len == 0) {
		fprintf (stderr, "Please specify a frequency.\n");
		exit(1);
	}

	if (controller.freq_len >= FREQUENCIES_LIMIT) {
		fprintf (stderr, "Too many channels, maximum %i.\n", FREQUENCIES_LIMIT);
		exit(1);
	}

	if (controller.freq_len > 1 && demod.squelch_level == 0) {
		fprintf (stderr, "Please specify a squelch level.  Required for scanning multiple frequencies.\n");
		exit(1);
	}

}

//UDP remote stuff
static unsigned int chars_to_int(unsigned char* buf) {

	int i;
	unsigned int val = 0;

	for(i=1; i<5; i++) {
		val = val | ((buf[i]) << ((i-1)*8));
	}

	return val;
}

static void *socket_thread_fn(void *arg) {
  UNUSED(arg);

  int n;
  int sockfd;
  unsigned char buffer[5];
  struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (sockfd < 0) {
  	perror("ERROR opening socket");
  }

	bzero((char *) &serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *) &serv_addr,  sizeof(serv_addr)) < 0) {
		perror("ERROR on binding");
	}

	bzero(buffer,5);

	fprintf (stderr, "Main socket started! :-) Tuning enabled on UDP/%d \n", port);

	int new_freq;

	while((n = read(sockfd,buffer,5)) != 0) {
		if(buffer[0] == 0) {
			new_freq = chars_to_int(buffer);
			dongle.freq = new_freq;
			optimal_settings(new_freq, demod.rate_in);
			rtlsdr_set_center_freq(dongle.dev, dongle.freq);
			fprintf (stderr, "\nTuning to: %d [Hz] \n", new_freq);
		}


	}

	close(sockfd);
	return 0;
}
//UDP stuff end

void rtlsdr_sighandler()
{
	fprintf (stderr, "Signal caught, exiting!\n");
	rtlsdr_cancel_async(dongle.dev);
}

void open_rtlsdr_stream(dsd_opts *opts)
{
  int r;
	rtl_bandwidth =  opts->rtl_bandwidth * 1000; //reverted back to straight value
	bandwidth_multiplier = (bandwidth_divisor / rtl_bandwidth);
	volume_multiplier = 1; //moved to external handling to be more dynamic

	//this needs to be initted first, then we set the parameters
  dongle_init(&dongle);
	//init with low pass if decoding P25 or EDACS/Provoice
	if (opts->frame_p25p1 == 1 || opts->frame_p25p2 == 1 || opts->frame_provoice == 1)
  	demod_init_ro2(&demod);
	else if (opts->analog_only == 1 || opts->m17encoder == 1)
		demod_init_analog(&demod);
	else demod_init(&demod);
  output_init(&output);
  controller_init(&controller);

	/* Read optional environment flag for half-band decimator */
	{
		const char *hb = getenv("DSD_FME_HB_DECIM");
		if (hb && hb[0] != '\0') {
			int v = atoi(hb);
			use_halfband_decimator = (v != 0);
		}
	}

	if (opts->rtlsdr_center_freq > 0) {
		controller.freqs[controller.freq_len] = opts->rtlsdr_center_freq;
		controller.freq_len++;
	}

	if (opts->rtlsdr_ppm_error != 0) {
		dongle.ppm_error = opts->rtlsdr_ppm_error;
		fprintf (stderr, "Setting RTL PPM Error Set to %d\n", opts->rtlsdr_ppm_error);
	}

	dongle.dev_index = opts->rtl_dev_index;
	// demod.squelch_level = opts->rtl_squelch_level; //no longer used here, used in framesync vc rms value under select conditions
	fprintf (stderr, "Setting RTL Bandwidth to %d Hz\n", rtl_bandwidth);
	// fprintf (stderr, "Setting RTL Sample Multiplier to %d\n", bandwidth_multiplier);
	fprintf (stderr, "Setting RTL RMS Squelch Level to %d\n", opts->rtl_squelch_level);
	if (opts->rtl_udp_port != 0) port = opts->rtl_udp_port; //set this here, only open socket thread if set
	if (opts->rtl_gain_value > 0) {
		dongle.gain = opts->rtl_gain_value * 10; //multiple by ten to make it consitent with the way rtl_fm works
	}

  /* quadruple sample_rate to limit to Δθ to ±π/2 */
	demod.rate_in *= demod.post_downsample;

  if (!output.rate) output.rate = demod.rate_out;

  sanity_checks();

	if (controller.freq_len > 1) demod.terminate_on_squelch = 0;

  ACTUAL_BUF_LENGTH = lcm_post[demod.post_downsample] * DEFAULT_BUF_LENGTH;
  /* Ensure async read uses a valid, explicit buffer length */
  dongle.buf_len = (uint32_t)ACTUAL_BUF_LENGTH;

  r = rtlsdr_open(&dongle.dev, (uint32_t)dongle.dev_index);
  if (r < 0)
  {
    fprintf (stderr, "Failed to open rtlsdr device %d.\n", dongle.dev_index);
    exit(1);
  } else {
		fprintf (stderr, "Using RTLSDR Device Index: %d. \n", dongle.dev_index);
	}

  if (demod.deemph) {
		demod.deemph_a = (int)round(1.0/((1.0-exp(-1.0/(demod.rate_out * 75e-6)))));
	}

  /* Set the tuner gain */
	if (dongle.gain == AUTO_GAIN) {
		verbose_auto_gain(dongle.dev);
		fprintf (stderr, "Setting RTL Autogain. \n");
	} else {
		dongle.gain = nearest_gain(dongle.dev, dongle.gain);
		verbose_gain_set(dongle.dev, dongle.gain);
		// fprintf (stderr, "Setting RTL Nearest Gain to %d. \n", dongle.gain); //seems to be working now
	}

  verbose_ppm_set(dongle.dev, dongle.ppm_error);

  /* Reset endpoint before we start reading from it (mandatory) */
	verbose_reset_buffer(dongle.dev);

  pthread_create(&controller.thread, NULL, controller_thread_fn, (void*)(&controller));
  usleep(100000);
  pthread_create(&demod.thread, NULL, demod_thread_fn, (void*)(&demod));
  pthread_create(&dongle.thread, NULL, dongle_thread_fn, (void*)(&dongle));
	//only create socket thread IF user specified (for legacy uses), else don't use it
	if (port != 0) pthread_create(&socket_freq, NULL, socket_thread_fn, (void *)(&controller));
}

void cleanup_rtlsdr_stream()
{
	fprintf (stderr, "cleaning up...\n");
  rtlsdr_cancel_async(dongle.dev);
  pthread_join(dongle.thread, NULL);
  safe_cond_signal(&demod.ready, &demod.ready_m);
  pthread_join(demod.thread, NULL);
  safe_cond_signal(&output.ready, &output.ready_m);
  safe_cond_signal(&controller.hop, &controller.hop_m);
  pthread_join(controller.thread, NULL);

  //dongle_cleanup(&dongle);
  demod_cleanup(&demod);
  output_cleanup(&output);
  controller_cleanup(&controller);

	/* free LUT memory if allocated */
	atan_lut_free();

  rtlsdr_close(dongle.dev);
}

/* Batched consumer API: read up to count samples with fewer wakeups/locks.
   Returns number of samples read (>=1) or -1 on exit. Applies volume scaling. */
int get_rtlsdr_samples(int16_t *out, size_t count, dsd_opts * opts, dsd_state * state)
{
	UNUSED(state);
	if (count == 0) return 0;

	/* If PPM Error is Manually Changed, change it here once per batch */
	if (opts->rtlsdr_ppm_error != dongle.ppm_error)
	{
		dongle.ppm_error = opts->rtlsdr_ppm_error;
		verbose_ppm_set(dongle.dev, dongle.ppm_error);
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

//original for safe keeping
// void get_rtlsdr_sample(int16_t *sample, dsd_opts * opts, dsd_state * state)
// {
// 	if (output.queue.empty())
// 	{
// 		safe_cond_wait(&output.ready, &output.ready_m);
// 	}
// 	pthread_rwlock_wrlock(&output.rw);
// 	*sample = output.queue.front() * volume_multiplier;
// 	output.queue.pop();
// 	pthread_rwlock_unlock(&output.rw);
// }

//find way to modify this function to allow hopping (tuning) while squelched and send 0 sample?
int get_rtlsdr_sample(int16_t *sample, dsd_opts * opts, dsd_state * state)
{
	/* Delegate to batched API for a single sample */
	int ret = get_rtlsdr_samples(sample, 1, opts, state);
	if (ret < 0) return -1;
	return 0;
}

//function may lag since it isn't running as its own thread
void rtl_dev_tune(dsd_opts * opts, long int frequency)
{
	int r;
	if (opts->payload == 1)
		fprintf (stderr, "\nTuning to %lu Hz.", frequency);
	dongle.freq = opts->rtlsdr_center_freq = frequency;
	optimal_settings(dongle.freq, demod.rate_in);
	if (opts->payload == 1)
		fprintf (stderr, " (Center Frequency: %u Hz.) \n", dongle.freq);
	r = rtlsdr_set_center_freq(dongle.dev, dongle.freq);
	if (r < 0)
		fprintf (stderr, " (WARNING: Failed to set Center Frequency %u). \n", dongle.freq);

	rtl_clean_queue();

}

//return RMS value (root means square) power level -- used as soft squelch inside of framesync
long int rtl_return_rms()
{
	long int sr = 0;
	// #ifdef __arm__
	// sr = 100;
	// #else
	//debug -- on main machine, lp_len is around 6420, so this probably contributes to very high CPU usage
	// fprintf (stderr, "LP_LEN: %d \n", demod.lp_len);
	//I've found that just using a sample size of 160 will give us a good approximation without killing the CPU
	// sr = rms(demod.lowpassed, demod.lp_len, 1);
	sr = rms(demod.lowpassed, 160, 1); //I wonder what a reasonable value would be for #2 (input len) there
	// #endif
	return (sr);
}

//simple function to clear the rtl sample queue when tuning and during other events (ncurses menu open/close)
void rtl_clean_queue()
{
	/* Clear the entire ring to prevent sample 'lag' */
	ring_clear(&output);
	/* Wake producer waiting for space */
	safe_cond_signal(&output.space, &output.ready_m);
}