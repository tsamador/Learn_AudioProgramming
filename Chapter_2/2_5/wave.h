#ifndef WAVE_H
#define WAVE_H

#define M_PI 3.1415926535897932
#define TWOPI (2.0 * M_PI)

#include <math.h>
#include <stdlib.h>
#include <stdio.h>


/*Definition for an oscillator type */
typedef struct t_oscil 
{
    double two_pi_over_sample_rate; //Often used constant
    double current_frequency;
    double current_phase;
    double incr;

} OSCIL;

OSCIL* oscil();
void InitOscillator(OSCIL* osc, unsigned long sample_rate);
double sine_tick(OSCIL* osc, double freq);

#endif