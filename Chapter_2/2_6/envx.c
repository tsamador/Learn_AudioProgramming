#include <portsf.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "portsf/breakpoints.h"

/* envx.c:
extract amplitude envelope from mono soundfile */

enum {
    ARG_PROGNAME,
    ARG_INFILE,
    ARG_OUTFILE,
    ARG_NARGS
};

typedef struct pan_position_t {
    double left;
    double right;
} pan_position;


#define FRAMES_PER_WRITE 1024
#define DEFAULT_WINDOW_MSECS 15
pan_position constpowerpan(double position);
double maxsamp(float*buf, unsigned long blocksize);

int main(int argc, char* argv[])
{
    PSF_PROPS inprops;
    long framesread, totalread;
    /* init all resource vars to default states */
    int ifd = -1;  /* input file and output file IDS */
    int error = 0;
    int i;
    float* frame = NULL;
    double win_duration = DEFAULT_WINDOW_MSECS; /*default of the window in msecs */
    unsigned long winsize;
    double break_time;
    FILE* fp = NULL;
    unsigned long npoints = 0;

    /* TODO: Add Description of program */
    printf("\nenvcs: Extract enbelope information from a soundfile\n");

    /* If the users specifies a particular window duration */

    if(argc > 1) {
        char flag; 
        while(argv[1][0] == '-')
        {
            flag = argv[1][0];
            switch(flag)
            {
                case '\0':
                    printf("Error: missing flag name\n");
                    return 1;
                case 'w':
                    win_duration = atof(&argv[1][2]);
                    if(win_duration <= 0.0) {
                        printf("bad value for window duration, must be positive\n");
                        return 1;
                    }
                    break;
                default:
                    break;
            }
            argc--;
            argv++;
        }
    }
    if(argc < ARG_NARGS)
    {
        printf("insufficient arguments. \nusage: ./envcs [-wN] <infile> <outfile.brk> \n"
                "\t -wN set extraction window size to N msecs\n"
                "           (default: 15)\n");
        return 1;
    }

    //Initialize the library
    if(psf_init())
    {
        puts("Unable to start up portsf\n");
        return 1;
    }

    //TODO: Open and Verify Infile for this application
    ifd = psf_sndOpen(argv[ARG_INFILE], &inprops, 0);

    if(ifd < 0 )
    {
        printf("Error: unable to open file infile %s\n", argv[ARG_INFILE]);
        return 1;
    }

    if(inprops.chans > 1)
    {
        printf("soundfile contains %d channels: must be mono\n", inprops.chans);
    }


    /* Open the breakpoint file */
    fp = fopen(argv[ARG_OUTFILE], "w");
    if(fp == NULL)
    {
        printf("envx: unable to create breakpoint file %s\n", argv[ARG_OUTFILE]);
        error++;
        goto exit;
    }

    /* set buffersize to the required envelope window size */
    win_duration /= 1000.0; //Convert to seconds
    winsize = (unsigned long)(win_duration * inprops.srate); /* Winsize is how many frames we are going to read at a time */

    /* allocate space for sample buffer */
    frame = (float*)malloc(winsize*sizeof(float)); // Buffer to hold our data for processing/writing

    if(frame == NULL) {
        puts("No memory!\n");
        error++;
        goto exit;
    }

    break_time = 0.0;
    npoints = 0;
    while((framesread = psf_sndReadFloatFrames(ifd, frame, winsize)) > 0)
    {
        double amp; 
        amp = maxsamp(frame, framesread);
        if(fprintf(fp, "%f\t%f", break_time, amp) < 2)
        {
            printf("Failed to write to breakpoint file");
            error++;
            break;
        }
        npoints++;
        break_time += win_duration;
    }    
    
    if(framesread < 0)
    {
        printf("error reading infile. Outfile is incomplete.\n");
        error++;
    }

    /*do all the cleanup */
exit:
    if(ifd >= 0)
    {
        psf_sndClose(ifd);
    }
    if(frame)
    {
        free(frame);
    }
    
    if(fp)
    {
        if(fclose(fp))
        {
            printf("envx: failed to close output file %s\n"
                    , argv[ARG_OUTFILE]);
        }
    }

    //Clean up the library
    psf_finish();
    return error;
}

pan_position constpowerpan(double position)
{
    pan_position pos;
    /* pi/2: 1/4 cycle of a sinusoid */
    const double piover2 = 4.0 * atan(1.0) * 0.5;
    const double root2over2 = sqrt(2.0) * 0.5;

    /* scale position to fit the pi/2 range */
    double thispos = position * piover2;

    /* each channel uses a 1.4 of a cycle */
    double angle = thispos * 0.5;

    pos.left = root2over2 * (cos(angle) - sin(angle));
    pos.right = root2over2 * (cos(angle) + sin(angle));
    return pos;
}

void print_file_properties(PSF_PROPS props, char* filename)
{
    printf("Config info for %s\n", filename);
    printf("Num of channels %d\n", props.chans);
    /* TODO(Tanner): Change these to print correct enum */
    printf("Channel Format %d\n", props.chformat);
    printf("Format %d\n", props.format);
    printf("Sample Type %d \n", props.samptype);
    printf("Sample Rate %d \n", props.srate);
}

/* Finds the max sample value in a buffer */
double maxsamp(float*buf, unsigned long blocksize)
{
    double absval, peak = 0.0;
    unsigned long i;

    for(i = 0; i < blocksize; i++)
    {
        absval = fabs(buf[i]);
        if(absval > peak)
            peak = absval;
    }
    return peak;
}

/*  Notes

We are trying to extract "envelope" data, which is essentially
a representation of how the waveform changes over time (Breakpoint file)

We want to try to minimize the data we collect, if we collect too much 
we end up just reproducing the waveform itself, if we collect too little
we may corrupt the waveform.

As a rule of thumb, a 15 millisecond window is sufficient to capture the 
envelope of most souds - around 66 points per second 
*/