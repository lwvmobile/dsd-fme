#ifndef DSP_DEMOD_PIPELINE_H
#define DSP_DEMOD_PIPELINE_H

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of demod_state structure */
struct demod_state;

/**
 * Boxcar low-pass and decimate by step (no wraparound).
 * Length must be a multiple of step.
 *
 * @param signal2 In/out buffer of samples.
 * @param len     Length of input buffer.
 * @param step    Decimation factor.
 * @return New length after decimation.
 */
int
low_pass_simple(int16_t* signal2, int len, int step);

/**
 * Simple square window FIR on real samples with decimation to rate_out2.
 *
 * @param s Demodulator state (uses result buffer and decimation state).
 */
void
low_pass_real(struct demod_state* s);

/**
 * Deferred low-pass: sums and decimates with saturation on writeback.
 *
 * @param d Demodulator state (uses lowpassed buffer and decimation state).
 */
void
low_pass(struct demod_state* d);

/**
 * Fifth-order half-band-like decimator operating on a single real sequence.
 * Caller applies this separately to I and Q streams. Uses 6-tap state in
 * `hist` and writes decimated output in-place.
 *
 * @param data   In/out real data buffer (single channel).
 * @param length Input length (elements), processed in-place.
 * @param hist   Persistent history buffer of length >= 6.
 */
void
fifth_order(int16_t* data, int length, int16_t* hist);

/**
 * FIR filter with symmetric 9-tap coefficients (phase-saving implementation).
 *
 * @param data   In/out data buffer (interleaved step of 2 assumed).
 * @param length Number of input samples.
 * @param fir    Coefficient array (expects layout for length 9).
 * @param hist   History buffer used across calls.
 */
void
generic_fir(int16_t* data, int length, int* fir, int16_t* hist);

/**
 * Perform FM discriminator on interleaved low-passed I/Q to produce audio PCM.
 * Uses the active discriminator configured in fm->discriminator.
 *
 * @param fm Demodulator state (uses lowpassed as input, writes to result).
 */
void
fm_demod(struct demod_state* fm);

/**
 * Pass-through demodulator: copies low-passed samples to output unchanged.
 *
 * @param fm Demodulator state (copies lowpassed to result).
 */
void
raw_demod(struct demod_state* fm);

/**
 * Apply post-demod deemphasis IIR filter with Q15 coefficient.
 *
 * @param fm Demodulator state (reads/writes result, updates deemph_avg).
 */
void
deemph_filter(struct demod_state* fm);

/**
 * Apply a simple DC blocking (leaky integrator high-pass) filter to audio.
 *
 * @param fm Demodulator state (reads/writes result, updates dc_avg).
 */
void
dc_block_filter(struct demod_state* fm);

/**
 * Apply a simple one-pole low-pass filter to audio.
 *
 * @param fm Demodulator state (reads/writes result, updates audio_lpf_state).
 */
void
audio_lpf_filter(struct demod_state* fm);

/**
 * Calculate mean power (squared RMS) with DC bias removed.
 *
 * @param samples Input samples buffer.
 * @param len     Number of samples.
 * @param step    Step size for sampling.
 * @return Mean power (squared RMS) with DC bias removed.
 */
long int
mean_power(int16_t* samples, int len, int step);

/**
 * Full demodulation pipeline for one block.
 * Applies decimation (HB cascade or legacy), optional FLL and timing
 * correction, followed by the configured discriminator and post-processing.
 *
 * @param d Demodulator state (consumes lowpassed, produces result).
 */
void
full_demod(struct demod_state* d);

#ifdef __cplusplus
}
#endif

#endif /* DSP_DEMOD_PIPELINE_H */
