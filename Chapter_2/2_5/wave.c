
#include "wave.h"

OSCIL* oscil()
{
    OSCIL* osc = (OSCIL*)malloc(sizeof(OSCIL));
    if(!osc)
        return 0;
    return osc;

}

void InitOscillator(OSCIL* osc, unsigned long sample_rate)
{
    osc->two_pi_over_sample_rate = TWOPI/ (double) sample_rate;
    osc->current_frequency = 0.0;
    osc->current_phase = 0.0;
    osc->incr = 0.0;    
}

double sine_tick(OSCIL* osc, double freq)
{
    double val;

    val = sin(osc->current_phase);
    printf("Value: %lf\n", val);
    if(osc->current_frequency != freq)
    {
        osc->current_frequency = freq;
        osc->incr = osc->two_pi_over_sample_rate * freq;
    }

    osc->current_phase += osc->incr;
    if(osc->current_phase >= TWOPI)
        osc->current_phase -= TWOPI;
    if(osc->current_phase < 0.0)
        osc->current_phase += TWOPI;
    return val;
    
}


