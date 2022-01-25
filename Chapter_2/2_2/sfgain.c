#include <portsf.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

enum {
    ARG_PROGNAME,
    ARG_INFILE,
    ARG_OUTFILE,
    ARG_AMPFACE,
    ARG_NARGS
};

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
    PSF_PROPS props;
    long framesread, totalread;

    /* init all resource vars to default states */
    int ifd = -1, ofd = -1; /* input file and output file IDS */
    int error = 0;
    int i;
    psf_format outformat = PSF_FMT_UNKNOWN;
    PSF_CHPEAK* peaks = NULL;
    float* frame = NULL;
    float amplitude_factor, scalefac;
    double dbval, inpeak = 0.0;

    printf("\nSFGAIN: Change level of soundfile\n");

    if(argc < ARG_NARGS)
    {
        printf("insufficient arguments. \nusage: ./sf2float <infile> <outfile> <dbval>\n");
        return 1;
    }

    //Get Amplitude Factor from command line
    dbval = atof(argv[3]);
    if(dbval >= 0.0)
    {
        printf("Error: decibal value must be positive\n");
        return 1;
    }
    amplitude_factor = (float) pow(10.0, dbval/20.0);

    //Initialize the library
    if(psf_init())
    {
        puts("Unable to start up portsf\n");
        return 1;
    }
    
    //Open our infile
    ifd = psf_sndOpen(argv[ARG_INFILE], &props, 0);

    if(ifd < 0 )
    {
        printf("Error: unable to open file infile %s\n", argv[ARG_INFILE]);
        return 1;
    }

    /* allocate space for sample buffer */
    frame = (float*)malloc(FRAMES_PER_WRITE * (props.chans * sizeof(float))); // Buffer to hold our data for processing/writing

    
    //Find the peak value of our infile
    if(psf_sndReadPeaks(ifd, peaks, NULL) > 0) // If our file has data for the peak values
    {
        long i;
        for(i = 0; i < props.chans; i++)
        {
            if(peaks[i].val > inpeak)
                inpeak = peaks[i].val;
        }
    }
    else //Otherwise, find the peak value ourselves.
    {
        framesread = psf_sndReadFloatFrames(ifd, frame, FRAMES_PER_WRITE);
        while(framesread > 0)
        {
            double thispeak;
            int blocksize = props.chans * framesread;
            thispeak = maxsamp(frame,blocksize);
            if(thispeak > inpeak)
            {
                inpeak = thispeak;
            }
            framesread = psf_sndReadFloatFrames(ifd, frame, FRAMES_PER_WRITE);
        }

        /* Now rewind the file for copying */

        if(psf_sndSeek(ifd, 0, PSF_SEEK_SET) < 0)
        {
            printf( "Error: unable to rewind infile\n");
            error++;
            goto exit;
        }
    }

    if(inpeak == 0.0)
    {
        printf("infile is silent! Outfile not created. \n");
        goto exit;
    }
    /* check outfile extension is one we know about */
    outformat = psf_getFormatExt(argv[ARG_OUTFILE]);

    if(outformat == PSF_FMT_UNKNOWN)
    {
        printf("outfile name %s has unknown format. \n Use any of .wav .aiff. .aif .afc .aifc\n", argv[ARG_OUTFILE]);
        error++;
        goto exit;
    }
    props.format = outformat;
    ofd = psf_sndCreate(argv[2], &props, 0,0,PSF_CREATE_RDWR);

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
    peaks = (PSF_CHPEAK*) malloc(props.chans* sizeof(PSF_CHPEAK));

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
    scalefac = (float)(amplitude_factor / inpeak);
    while(framesread == FRAMES_PER_WRITE){

        /* Audio Programming Book Exercise 2.1.2
        Add a progress meter to show that the copying is happningfor large files.
        */
        if (totalread % (props.srate * 100) == 0)
        {
            printf("Copying to file... %ld samples copied\n", totalread);
        }
        totalread += FRAMES_PER_WRITE;

        //Do processing on frames here
        for(i = 0; i < props.chans*FRAMES_PER_WRITE; i++)
        {
            frame[i] *= scalefac;
        }
        if(psf_sndWriteFloatFrames(ofd,frame,FRAMES_PER_WRITE) != FRAMES_PER_WRITE) /* Write to our outfile in this line */
        {
            puts("Error Writing to outfile \n");
            error++;
            break;
        }
        framesread = psf_sndReadFloatFrames(ifd,frame,FRAMES_PER_WRITE);
    }
    //If the samplerate is not divisible by the number of Frames we write, then we will have some leftover frames
    //We need to add on after our main loop.
    if(framesread > 0)
    {
        printf("Frames leftover %ld \n", framesread);
        psf_sndWriteFloatFrames(ofd, frame, framesread);
        totalread += framesread;
    }

    if(framesread < 0) {
        printf("Error reading infile. Outfile is incomplete. \n");
        error++;
    }
    else
    {
        printf("Done. %ld sample frames copied to %s \n", totalread, argv[ARG_OUTFILE]);

    }

    /* Report PEAK values to user */
    if(psf_sndReadPeaks(ofd, peaks,NULL) > 0){
        long i;
        double peaktime;
        printf("PEAK information: \n");
        for(i = 0; i < props.chans; i++) 
        {
            peaktime = (double) peaks[i].pos / props.srate;
            printf("CH %ld: \t%.4f at %.4f secs\n", i+1, peaks[i].val, peaktime);
        }

    
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



*/