
#include <stdio.h>
#include <math.h>

#define SAMPLING_RATE 44100

#define NUM_SECONDS 3
#define NUM_SAMPLES (NUM_SECONDS * SAMPLING_RATE)
#define PI M_PI
#define FREQUENCY 440


int main()
{
    int i ;
    for(i = 0;i < NUM_SAMPLES; i++)
    {
        float sample;
        sample = sin(2 * PI * FREQUENCY * i /SAMPLING_RATE);
        printf("%f\n", sample);
    }
}