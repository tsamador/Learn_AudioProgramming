#include <portsf.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "portsf/breakpoints.h"

enum {
    ARG_PROGNAME,
    ARG_INFILE,
    ARG_OUTFILE,
    ARG_PANPOS,
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

    printf("\nSFGAIN: Change level of soundfile\n");

    if(argc < ARG_NARGS)
    {
        printf("insufficient arguments. \nusage: ./sf2float <infile> <outfile> <panpos>\n");
        return 1;
    }

    // Get the pan position from the command line
    pos = (float) atof(argv[ARG_PANPOS]);
    if(pos < -1.0 || pos > 1.0)
    {
        puts("Error: panpos value out of range -1 to +1\n");
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
    
    thispos = simple_pan(pos);

    while(framesread > 0){

        int out_i = 0;
        totalread += framesread;
        //Do processing on frames here
        for(i = 0; i < inprops.chans*framesread; i++)
        {
            outframe[out_i++] = (float)(frame[i] * thispos.left);
            outframe[out_i++] = (float)(frame[i] * thispos.right);
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


*/