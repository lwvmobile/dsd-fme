#ifndef DSP_RESAMPLER_H
#define DSP_RESAMPLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of demod_state structure */
struct demod_state;

/**
 * Design windowed-sinc low-pass prototype for polyphase upfirdn (runs at L*Fs_in).
 * Taps are stored phase-major with stride L (k*L + phase). The function allocates
 * aligned storage for taps and history inside the provided demod_state and
 * initializes the resampler bookkeeping fields.
 *
 * @param s Demodulator state to receive resampler taps/history.
 * @param L Upsampling factor.
 * @param M Downsampling factor.
 */
void
resamp_design(struct demod_state* s, int L, int M);

/**
 * Process one block using polyphase upfirdn with history.
 *
 * @param s      Demodulator state containing resampler state.
 * @param in     Pointer to input samples.
 * @param in_len Number of input samples.
 * @param out    Pointer to output buffer (sized to hold produced samples).
 * @return Number of output samples written.
 */
int
resamp_process_block(struct demod_state* s, const int16_t* in, int in_len, int16_t* out);

#ifdef __cplusplus
}
#endif

#endif /* DSP_RESAMPLER_H */


