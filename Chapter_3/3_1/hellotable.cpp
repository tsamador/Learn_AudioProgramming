
#include <stdio.h>
#include <math.h>

#define SAMPLING_RATE 44100
#define TABLE_LEN 512

#define SINE 0
#define SQUARE 1
#define SAW 2
#define TRIANGLE 3

#define NUM_SAMPLES (NUM_SECONDS * SAMPLING_RATE)
#define PI M_PI
#define FREQUENCY 440

float table[TABLE_LEN]; //Table to be filled up with samples 

#ifdef REALTIME /*uses Tiny Audio Library */
#include "tinyAudioLib.h"


int main()
{
    int waveform;
    float frequency, duration;

    printf("Enter the frequency of the wave to output in HZ");
    scanf("%f", &frequency);

    printf("\nEnter the duration of the tone in seconds and press ENTER: ");
    scanf("%f", &duration);

    do
    {
        printf("Enter the Waveform you wish to output and press ENTER:\n"
            "\t 0 = Sine\n"
            "\t 1 = Square\n"
            "\t 2 = Sawtooth\n"
            "\t 3 = Triangle\n");
        scanf("%d", waveform);

        if(waveform < 0 || waveform > 3)
        {
            puts("Wrong Number for waveform, must be between 0 and 3");
        }

    }while(waveform < 0 || waveform > 3);

    switch(waveform)
    {
        case SINE:
        {
            fill_sine();
        } break;
        case SQUARE:
        {
            fill_square();
        } break;
        case SAW:
        {
            fill_saw();
        } break;
        case TRIANGLE:
        {
            fill_triangle();
        } break;
    }

    
    init();
    /* ------------------ Synthesis Engine Start ------------ */
    {
        int j;
        double sample_increment = frequency * TABLE_LEN / SAMPLING_RATE;
        double phase = 0;
        float sample;

        for(j = 0; j < duration * SAMPLING_RATE; j++)
        {
            sample = table[(long)phase];
            outsample(sample);
            phase += sample_increment;
            if(phase > TABLE_LEN)
            {
                phase -= TABLE_LEN;
            }
        }
    }

    /* --------------------- Sythesis Engine end -------------- */
    cleanup();
    printf('end of Process \n');
}

void fill_sine()
{
    int j;
    for(j = 0; j < TABLE_LEN; j++)
    {
        table[j] = (float) sin(2 * PI *j/TABLE_LEN);
    }
}

void fill_square()
{
    int j;
    for(j = 0; j < TABLE_LEN/2; j++)
    {
        table[j] = 1;
    }
    for(j = TABLE_LEN/2; j < TABLE_LEN; j++)
    {
        table[j] = -1;
    }

}

void fill_saw()
{
    int j;
    for(j = 0; j < TABLE_LEN; j++)
    {
        table[j] = 1 - (2 * (float) j / (float) TABLE_LEN);
    }
}

void fill_triangle()
{
    int j = 0;
    for(j = 0; j < TABLE_LEN/2; j++)
    {
        table[j] = 2 * (float) j / (float) (TABLE_LEN/2) - 1;
    }
    for( j = TABLE_LEN/2; j < TABLE_LEN; j++)
    {
        table[j] = 1 - 2 * (float) j / (float) (TABLE_LEN/2) - 1;
    }
}