#include <portsf.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "portsf/breakpoints.h"


/*
sfenv.c 

Goal is to apply an amplitude evelope defined in a breakpoint
file to a source soundfile.
*/
enum {
    ARG_PROGNAME,
    ARG_INFILE,
    ARG_OUTFILE,
    ARG_BRKFILE,
    ARG_NARGS
};

typedef struct pan_position_t {
    double left;
    double right;
} pan_position;

pan_position simple_pan(double position)
{
    pan_position pos;

    position *= 0.5;
    pos.left = position - 0.5;
    pos.right = position + 0.5;

    return pos;
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

const unsigned long FRAMES_PER_WRITE = 1024;


/*
sfgain.c takes an infile and a copies it to an outfile
but with a reduced amplitude
*/
int main(int argc, char* argv[])
{
    PSF_PROPS inprops, outprops;
    pan_position thispos;
    long framesread, totalread;

    /* init all resource vars to default states */
    int ifd = -1, ofd = -1; /* input file and output file IDS */
    int error = 0;
    int i;
    psf_format outformat = PSF_FMT_UNKNOWN;
    PSF_CHPEAK* peaks = NULL;
    float* frame = NULL;
    float* outframe = NULL;
    float amplitude_factor, scalefac;
    double pos, inpeak = 0.0;

    double timeincr, sampletime;

    /* Variables for Break Point processing */
    breakpoint leftpoint, rightpoint;
    unsigned long ileft = 0 , iright = 1;
    double incr, curpos, frac, width, height; 
    int more_points = 1;

    FILE* fp = NULL;
    unsigned long size;
    breakpoint* points = NULL;
    printf("\nSFENV: Change level of soundfile\n");

    if(argc < ARG_NARGS)
    {
        printf("insufficient arguments. \nusage: ./sf2float <infile> <outfile> <posfile.brk>\n");
        return 1;
    }

    //Read breakpoint file and verify it 
    fp = fopen(argv[ARG_BRKFILE], "r");

    if(fp == NULL) 
    {
        printf("error: unable to pen breakpoint file %s\n", argv[ARG_BRKFILE]);
        error++;
        goto exit;
    }

    points = get_breakpoints(fp, &size);

    if(points == NULL)
    {
        printf("No breakpoints read. \n");
        error++;
        goto exit;
    }  

    if(size < 2) 
    {
        printf("Error: at least two breakpoints required\n");
        free(points);
        fclose(fp);
        return 1;
    }

    //We require breakpoints to start from 0 */
    if(points[0].time != 0.0) 
    {
        printf("error in breakpoint date, frist time must be 0.0\n");
        error++;
        goto exit;
    }
     if(!inrange(points, -1.0, 1.0, size))
     {
         printf("Error in breakpoint file, values out of range -1 to +1\n");
         error++;
         goto exit;
     }



    //Initialize the library
    if(psf_init())
    {
        puts("Unable to start up portsf\n");
        return 1;
    }
    
    //Open our infile
    ifd = psf_sndOpen(argv[ARG_INFILE], &inprops, 0);

    if(ifd < 0 )
    {
        printf("Error: unable to open file infile %s\n", argv[ARG_INFILE]);
        return 1;
    }

    if(inprops.chans != 1)
    {
        puts("Error: infile must be mono. \n");
        error++;
        goto exit;
    }

    outprops = inprops;
    outprops.chans = 2;

    /* allocate space for sample buffer */
    frame = (float*)malloc(FRAMES_PER_WRITE * (inprops.chans * sizeof(float))); // Buffer to hold our data for processing/writing

    /* allocate space for outframe */
    outframe = (float*)malloc(FRAMES_PER_WRITE * (outprops.chans * sizeof(float)));

    /* check outfile extension is one we know about */
    outformat = psf_getFormatExt(argv[ARG_OUTFILE]);

    if(outformat == PSF_FMT_UNKNOWN)
    {
        printf("outfile name %s has unknown format. \n Use any of .wav .aiff. .aif .afc .aifc\n", argv[ARG_OUTFILE]);
        error++;
        goto exit;
    }
    
    ofd = psf_sndCreate(argv[2], &outprops, 0,0,PSF_CREATE_RDWR);

    if(ofd < 0)
    {
        printf("Error: unable to create outfile %s\n", argv[ARG_OUTFILE]);
        error++;
        goto exit;
    }

    if(frame == NULL) {
        puts("No memory!\n");
        error++;
        goto exit;
    }

    /* allocate space for PEAK info */
    peaks = (PSF_CHPEAK*) malloc(inprops.chans* sizeof(PSF_CHPEAK));

    if(peaks == NULL) {
        puts("No memory!\n");
        error++;
        goto exit;
    }

    puts("copying... \n");
    
    /* Audio Programming book, Exercise 2.1.1
    modify this program to use multiple frames instead of doing one frame at a time 
    */
    framesread = psf_sndReadFloatFrames(ifd, frame, FRAMES_PER_WRITE);
    totalread = 0;

    timeincr = 1.0 / inprops.srate;
    sampletime = 0.0;
    ileft = 0;
    iright = 1;
    leftpoint = points[ileft];
    rightpoint = points[iright];
    width = rightpoint.time - leftpoint.time;
    height = rightpoint.value - leftpoint.value;
    more_points = 1;

    while(framesread > 0){

        int out_i = 0;
        double stereopos;
        totalread += framesread;

        //Do processing on frames here
        for(i = 0; i < inprops.chans*framesread; i++)
        {
            // 2_4 Deals with improving the runtime of this function
            stereopos = val_at_brktime(points, size, sampletime);
            thispos = constpowerpan(stereopos); //TODO(Tanner): Document what simple_pan does again
            outframe[out_i++] = (float)(frame[i] * thispos.left);
            outframe[out_i++] = (float)(frame[i] * thispos.right);
            sampletime += timeincr;
        }

        if(psf_sndWriteFloatFrames(ofd,outframe,framesread) != framesread   ) /* Write to our outfile in this line */
        {
            puts("Error Writing to outfile \n");
            error++;
            break;
        }
        framesread = psf_sndReadFloatFrames(ifd,frame,FRAMES_PER_WRITE);
    }

    if(framesread < 0) {
        printf("Error reading infile. Outfile is incomplete. \n");
        error++;
    }
    else
    {
        printf("Done. %ld sample frames copied to %s \n", totalread, argv[ARG_OUTFILE]);
    }

    /*do all the cleanup */
exit:
    if(ifd >= 0)
    {
        psf_sndClose(ifd);
    }
    if(ofd >= 0)
    {
        psf_sndClose(ofd);
    }
    if(frame)
    {
        free(frame);
    }
    if(outframe)
    {
        free(outframe);
    }
    if(peaks)
    {
        free(peaks);
    }

    //Clean up the library
    psf_finish();
    return error;
}

/* Notes:

An Envelop describes some relatively slowly time varying property
of a sound i.e amplitude. To extract the envelopt from a soundfile, 
we must perform the revers process, track the amplitude trend of the
signal, which changes far more slowly than the waveform.

Extractec envelope data lies within the positive "normalized" range from 
0 to 1.0, can be applied to other soundfiles, to reproduce the dynamics
of the primary source. We use linear interpolation to generate a stream of 
amplitude values, whiche are multiplied with the audio samples; the combination
of these two procesdures is usually termed envelope following.

*/