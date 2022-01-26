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


int main(int argc, char* argv[])
{
    PSF_PROPS inprops;
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
