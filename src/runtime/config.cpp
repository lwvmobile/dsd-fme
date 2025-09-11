/*
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

/**
 * @file
 * @brief Runtime configuration parser for environment-derived settings.
 *
 * Parses environment variables into a typed `DsdFmeRuntimeConfig` and exposes
 * an immutable accessor. Intended to be called early during application init.
 */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "dsd.h" /* for dsd_opts fields used in precedence */
#include "runtime/config.h"

static DsdFmeRuntimeConfig g_config;
static int g_config_inited = 0;

/**
 * @brief Check whether an environment string is set and non-empty.
 *
 * @param v Environment value string pointer (may be NULL).
 * @return 1 if set and non-empty; otherwise 0.
 */
static int
env_is_set(const char* v) {
    return v && v[0] != '\0';
}

/**
 * @brief Convert environment string to integer with fallback.
 *
 * @param v Environment value string (may be NULL or empty).
 * @param fallback Fallback integer when `v` is unset or empty.
 * @return Parsed integer value or `fallback` when not set.
 */
static int
env_as_int(const char* v, int fallback) {
    return env_is_set(v) ? atoi(v) : fallback;
}

/**
 * @brief Parse environment variables and initialize the runtime configuration.
 *
 * Precedence note: future CLI/opts may override env values; currently opts
 * are not applied beyond presence for future extension.
 *
 * @param opts Decoder options for potential precedence overrides.
 * @note Safe to call multiple times; the most recent call wins.
 */
void
dsd_fme_config_init(const dsd_opts* opts) {
    (void)opts; /* precedence hook reserved for future CLI/opts overrides */

    DsdFmeRuntimeConfig c;
    memset(&c, 0, sizeof(c));

    /* HB_DECIM */
    const char* hb = getenv("DSD_FME_HB_DECIM");
    c.hb_decim_is_set = env_is_set(hb);
    c.hb_decim = c.hb_decim_is_set ? (atoi(hb) != 0) : 1;

    /* COMBINE_ROT */
    const char* cr = getenv("DSD_FME_COMBINE_ROT");
    c.combine_rot_is_set = env_is_set(cr);
    c.combine_rot = c.combine_rot_is_set ? (atoi(cr) != 0) : 1;

    /* UPSAMPLE_FP */
    const char* ufp = getenv("DSD_FME_UPSAMPLE_FP");
    c.upsample_fp_is_set = env_is_set(ufp);
    c.upsample_fp = c.upsample_fp_is_set ? (atoi(ufp) != 0) : 1;

    /* RESAMP */
    const char* rs = getenv("DSD_FME_RESAMP");
    c.resamp_is_set = env_is_set(rs);
    c.resamp_disable = 0;
    c.resamp_target_hz = 48000;
    if (c.resamp_is_set) {
        if (strcasecmp(rs, "off") == 0 || strcmp(rs, "0") == 0) {
            c.resamp_disable = 1;
        } else {
            int v = atoi(rs);
            if (v > 0) {
                c.resamp_target_hz = v;
            }
        }
    }

    /* FLL */
    const char* fll = getenv("DSD_FME_FLL");
    c.fll_is_set = env_is_set(fll);
    c.fll_enable = (c.fll_is_set && fll[0] == '1') ? 1 : 0; /* may be overridden by mode later */

    const char* flut = getenv("DSD_FME_FLL_LUT");
    c.fll_lut_is_set = env_is_set(flut);
    c.fll_lut_enable = (c.fll_lut_is_set && flut[0] == '1') ? 1 : 0;

    const char* fa = getenv("DSD_FME_FLL_ALPHA");
    const char* fb = getenv("DSD_FME_FLL_BETA");
    const char* fdb = getenv("DSD_FME_FLL_DEADBAND");
    const char* fsl = getenv("DSD_FME_FLL_SLEW");
    c.fll_alpha_is_set = env_is_set(fa);
    c.fll_beta_is_set = env_is_set(fb);
    c.fll_deadband_is_set = env_is_set(fdb);
    c.fll_slew_is_set = env_is_set(fsl);
    c.fll_alpha_q15 = c.fll_alpha_is_set ? atoi(fa) : 50;
    c.fll_beta_q15 = c.fll_beta_is_set ? atoi(fb) : 5;
    c.fll_deadband_q14 = c.fll_deadband_is_set ? atoi(fdb) : 45;
    c.fll_slew_max_q15 = c.fll_slew_is_set ? atoi(fsl) : 64;

    /* TED */
    const char* ted = getenv("DSD_FME_TED");
    const char* tg = getenv("DSD_FME_TED_GAIN");
    const char* ts = getenv("DSD_FME_TED_SPS");
    const char* tf = getenv("DSD_FME_TED_FORCE");
    c.ted_is_set = env_is_set(ted);
    c.ted_enable = (c.ted_is_set && ted[0] == '1') ? 1 : 0;
    c.ted_gain_is_set = env_is_set(tg);
    c.ted_gain_q20 = c.ted_gain_is_set ? atoi(tg) : 64;
    c.ted_sps_is_set = env_is_set(ts);
    c.ted_sps = c.ted_sps_is_set ? atoi(ts) : 10;
    c.ted_force_is_set = env_is_set(tf);
    c.ted_force = (c.ted_force_is_set && tf[0] == '1') ? 1 : 0;

    /* Deemphasis */
    const char* deemph = getenv("DSD_FME_DEEMPH");
    c.deemph_is_set = env_is_set(deemph);
    c.deemph_mode = DSD_FME_DEEMPH_UNSET;
    if (c.deemph_is_set) {
        if (strcasecmp(deemph, "off") == 0 || strcmp(deemph, "0") == 0) {
            c.deemph_mode = DSD_FME_DEEMPH_OFF;
        } else if (strcmp(deemph, "50") == 0) {
            c.deemph_mode = DSD_FME_DEEMPH_50;
        } else if (strcmp(deemph, "75") == 0) {
            c.deemph_mode = DSD_FME_DEEMPH_75;
        } else if (strcasecmp(deemph, "nfm") == 0) {
            c.deemph_mode = DSD_FME_DEEMPH_NFM;
        }
    }

    /* Audio LPF */
    const char* alpf = getenv("DSD_FME_AUDIO_LPF");
    c.audio_lpf_is_set = env_is_set(alpf);
    c.audio_lpf_disable = 0;
    c.audio_lpf_cutoff_hz = 0;
    if (c.audio_lpf_is_set) {
        if (strcasecmp(alpf, "off") == 0 || strcmp(alpf, "0") == 0) {
            c.audio_lpf_disable = 1;
        } else {
            int cutoff = atoi(alpf);
            if (cutoff > 0) {
                c.audio_lpf_cutoff_hz = cutoff;
            }
        }
    }

    /* MT (intra-block worker pool) */
    const char* mt = getenv("DSD_FME_MT");
    c.mt_is_set = env_is_set(mt);
    c.mt_enable = (c.mt_is_set && mt[0] == '1') ? 1 : 0;

    /* Disable fs/4 capture shift */
    const char* dfs4 = getenv("DSD_FME_DISABLE_FS4_SHIFT");
    c.fs4_shift_disable_is_set = env_is_set(dfs4);
    c.fs4_shift_disable = (c.fs4_shift_disable_is_set && dfs4[0] == '1') ? 1 : 0;

    g_config = c;
    g_config_inited = 1;
}

/**
 * @brief Get immutable pointer to the current runtime configuration, or NULL if
 * initialization has not been performed.
 *
 * @return Pointer to config or NULL.
 */
const DsdFmeRuntimeConfig*
dsd_fme_get_config(void) {
    return g_config_inited ? &g_config : NULL;
}
