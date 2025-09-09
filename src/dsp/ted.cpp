#include "dsp/ted.h"

void ted_init_state(ted_state_t* state) {
    state->mu_q20 = 0;
}

void gardner_timing_adjust(const ted_config_t* config, ted_state_t* state,
                          int16_t* x, int* N, int16_t* y) {
    if (!config->enabled || config->sps <= 1) {
        return;
    }

    /* Guard: run TED only when we're near symbol rate to keep CPU low.
       Skip when samples-per-symbol is very high unless explicitly forced. */
    int sps = config->sps;
    if (sps > 12 && !config->force) {
        return;
    }

    int mu = state->mu_q20;     /* Q20 */
    int gain = config->gain_q20; /* Q20 */
    const int buf_len = *N;
    int out_n = 0;
    const int one = (1 << 20);

    int mu_nom = one / (sps > 0 ? sps : 1); /* Q20 increment per complex sample */

    for (int n = 0; n + 3 < buf_len; n += 2) {
        int a = n;     /* base complex sample index */
        int b = n + 2; /* next complex sample index */
        if (b + 1 >= buf_len) {
            break;
        }

        int frac = mu & (one - 1); /* 0..one-1 */
        int inv = one - frac;

        /* Linear interpolation between x[a] and x[b] (complex) */
        int32_t ar = x[a];
        int32_t aj = x[a + 1];
        int32_t br = x[b];
        int32_t bj = x[b + 1];
        int32_t ir = (int32_t)(((int64_t)inv * ar + (int64_t)frac * br) >> 20);
        int32_t ij = (int32_t)(((int64_t)inv * aj + (int64_t)frac * bj) >> 20);
        y[out_n++] = (int16_t)ir;
        y[out_n++] = (int16_t)ij;

        /* Gardner error using previous and next symbol-spaced samples */
        int km1 = a - 2;
        if (km1 < 0) {
            km1 = 0;
        }
        int kp1 = b + 2;
        if (kp1 + 1 >= buf_len) {
            kp1 = b;
        }

        int16_t xr1 = x[kp1];
        int16_t xj1 = x[kp1 + 1];
        int16_t xrm = x[km1];
        int16_t xjm = x[km1 + 1];
        int16_t dr = xr1 - xrm;
        int16_t dj = xj1 - xjm;
        int32_t e = (int32_t)dr * (int32_t)ir + (int32_t)dj * (int32_t)ij; /* Q0 */

        /* Update fractional phase: nominal advance + small correction */
        int64_t corr = ((int64_t)gain * (int64_t)e) >> 15; /* scale */
        mu += mu_nom + (int)corr;

        /* Wrap mu to [0, one) */
        if (mu >= one) {
            mu -= one;
        }
        if (mu < 0) {
            mu += one;
        }
    }

    if (out_n >= 2) {
        /* Copy timing-adjusted samples back to input buffer */
        for (int i = 0; i < out_n; i++) {
            x[i] = y[i];
        }
        *N = out_n;
    }

    state->mu_q20 = mu;
}
