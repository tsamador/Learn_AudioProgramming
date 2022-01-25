#include <portsf.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "portsf/breakpoints.h"

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


    FILE* fp = NULL;
    unsigned long size;
    breakpoint* points = NULL;
    printf("\nSFPAN: Change level of soundfile\n");

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

    while(framesread > 0){

        int out_i = 0;
        double stereopos;
        totalread += framesread;

        //Do processing on frames here
        for(i = 0; i < inprops.chans*framesread; i++)
        {
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

/*
    NOTE(Tanner): Should eventually move this to OneNote

    Opening a SoundFile (Read Only)
    int psf_open(const char *path, PSF_PROPS *props, int rescale)

    path = File path of the file
    props = return of the properties of the file.
    rescale = 1 if you want to rescale, 0 otherwise
    
    Cannot be used to create a file, only open an existing.
    int return value will be a number that identifies the file
    for future functions. Any value below zero is an error code.
    
    rescale - it is possible for files to have values over 1.0,
    which would cause clipping, we can rescale the file so no clipping
    occurs. Requires that the file has a PEAK chunk, which 
    records the position and value of highest sample.


    Opening a Soundfile for writing
    int psf_sndCreate(const char *path, const PSF_PROPS *props, int clip_flots, int minheader, int mode)

    path and Props same as above except props is now an input
    clip_flot = range of writing values, e.g -1.0 to 1.0, typically always 1.
    related to the PEAK chunck from before.

    minheader - 1 and the file is created swith smallest possible header.
    , just required formant and data chuncks. Ideally, should always
    be 0.

    mode - set read/write access of file. In this library it is alwasys PST_CREATE_RDWR

    There are functions to read the peak data, but this is rarely used, but useful to note
    Pg. 200 of Audio Programming Book.

    The Working unit in portsf is the multi-channel sample frame, 

    Amplitude Vs Loudness
    loudnessDb = 20 * log10(amplitude);

    amplitude = 10^(loudnessDB/20)]

    (Insert picture from page) 211/238

    Basically, amplitude and loudness or logarithmically associated, But a Decibel
    value does not meccesarily correspond to a specific amplitude value. Amplitude is 
    an absolute value, representing the voltage in an analog system.

    Decibel on the otherhand, it a ratio. In a digital waveform, the reference level is the
    digital peak, 1.0 in the case of floating point samples, or 32768 in the case of 16-bit values
    . This reference level is defined as 0 decibelsm so all levels below are expressed
    in negative decibel values.

    <Above picture shows table of relationship between these two numbers

    Takeaway from this is that raw amplitude might not make huge differences in 
    the dB value,

    Normalization:

    Sometimes the requirement is not to change level by a certain amount,
    but to change it to a certain level, Common practice in audio production is
    to "normalize" a track to a standard level.

    in the sfgain.c program we accept a decibel level (less than 0) and normalize
    the output file while scaling it. This involves calculating the amplitude 
    factor from the decibel level, finding the peak value in our audio file 
    and using that to create our scalefactor

    Stereo Panning:


    Breakpoints:
    
    Breakpoints are a simple format to describe a change over time, they 
    are defined as follows

    0.0 0.0
    2.0 1.0
    4.5 0.0

    Where the left column is the time marker, and the right is the value,
    these can be read in using functions from breakpoints.c and then used 
    to linearly interpolated the wave between the two values to produce a 
    change.

    <Picture from page 230/257>

    Say we want to know the value at a time that is not represented by a 
    breakpoint value, we can use the following equations.

    The breakpoint prior to the time we're looking for is our "left" time, and 
    the breakpoint after is our "Right Time", wa can find a ration between the time

    a = time - timeleft
    b = timeright - timeleft 

    ratio = a/b

    We can apply this ratio to the span values to find the required value

    c = valueright - valueleft // distance between 
    target_distance = c * fraction
    target_value = valueleft + target_distance

    there are two special cases to consider:

    1) Two spans have the same time, causing a divide by zero error. General
    solution is to return the right-hand span valu

    2) The requested time is after the final breakpoint, I.e the file is 10 seconds
    long but breakpoints only go to the 5 second mark, in this case we just return the
    final value for the rest of the file.

    the function val_at_brktime in the breakpoints.c file performs this operation.

    We simply need to calculate the time of each sample. The distance between samples
    is constant and it is equal to the reciprocal of the sample rate.

    double timeincr = 1.0 / inprops.srate;

    and then we just need to incriment a sampletime variable by timeincr each sample
    sampletime += timeincr;
    
    we can use this to get the breakpoint value

    stereopos = get_val_brkpt(...);
    
    and then use this position to get the panning values

    pos = sample_pan(stereopos);

    With this method of panning, we may notice a couple of things:

     - Sound tends to e pulled into the left or the right speaker.
     - The sound seems to recede ( move away from the listener) when the input file
       is panned centrally.
     - The level seems to fall slightly when the sound is panned centrally.

    This is because intensity (dB) is measured by reading the square of the amplitude,
    if we want to find intensity of a pair of sources, we need to sum the square root
    of the squares of the sum of each amplitude:

    P = sqrt(pow(amp1, 2) + pow(amp2, 2))
    in our original version of sfpan, this equation would not add up to 1.

    <Insert picture of amplitude equation pg234/261>

    To fix this, instead of a linearly interpolation we us a constant-power panning function
    this function is constpower and replaces simple pan
    <Picture of constant power function>
*/