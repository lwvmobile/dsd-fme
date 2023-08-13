

#include "dsd.h"

#include "p25p1_hdu.h"
#include "p25p1_heuristics.h"


void
processTDU (dsd_opts* opts, dsd_state* state)
{
    AnalogSignal analog_signal_array[14];
    int status_count;

    // we skip the status dibits that occur every 36 symbols
    // the first IMBE frame starts 14 symbols before next status
    // so we start counter at 36-14-1 = 21
    status_count = 21;

    // Next 14 dibits should be zeros
    read_zeros(opts, state, analog_signal_array, 28, &status_count, 1);

    // Next we should find an status dibit
    if (status_count != 35) {
        fprintf (stderr, "%s", KRED);
        fprintf (stderr, "*** SYNC ERROR\n");
        fprintf (stderr, "%s", KNRM);
    }

    // trailing status symbol
    {
        int status;
        status = getDibit (opts, state) + '0';
        // TODO: do something useful with the status bits...
        UNUSED(status);
    }
    
    //reset some strings -- since its a tdu, blank out any call strings, only want during actual call
    sprintf (state->call_string[0], "%s", "                     "); //21 spaces
    sprintf (state->call_string[1], "%s", "                     "); //21 spaces

    //reset gain
    if (opts->floating_point == 1)
        state->aout_gain = opts->audio_gain;

    //zero out MI, key, alg
    state->payload_miP = 0;
    state->payload_algid = 0;
    state->payload_keyid = 0;
    
}
