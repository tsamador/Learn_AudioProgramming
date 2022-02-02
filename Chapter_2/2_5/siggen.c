/* Copyright (c) 2009 Richard Dobson

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/
/* main.c :  generic soundfile processing main function*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <portsf.h>
#include "wave.h"

/* set size of multi-channel frame-buffer */
#define NFRAMES (1024)

/* TODO define program argument list, excluding flags */
enum {ARG_PROGNAME, ARG_OUTFILE, ARG_DUR, ARG_SRATE, ARG_AMP, ARG_FREQ,ARG_NARGS};

int main(int argc, char* argv[])
{
/* STAGE 1 */	
	PSF_PROPS inprops,outprops;									
	long framesread;	
	/* init all dynamic resources to default states */
	int ifd = -1,ofd = -1;
	int error = 0;
    unsigned int i, j;
	PSF_CHPEAK* peaks = NULL;	
	psf_format outformat =  PSF_FMT_UNKNOWN;
	unsigned long nframes = NFRAMES;
	float* inframe = NULL;
	float* outframe = NULL;
    unsigned long nbufs, outframes, remainder;
    int sample_rate;
    double duration, amplitude, frequency;
    OSCIL* osc;


	/* TODO: define an output frame buffer if channel width different 	*/
	
/* STAGE 2 */	
	printf("MAIN: generic process\n");						

	/* process any optional flags: remove this block if none used! */
	if(argc > 1){
		char flag;
		while(argv[1][0] == '-'){
			flag = argv[1][1];
			switch(flag){
			/*TODO: handle any  flag arguments here */
			case('\0'):
				printf("Error: missing flag name\n");
				return 1;
			default:
				break;
			}
			argc--;
			argv++;
		}
	}

	/* check rest of commandline */
	if(argc < ARG_NARGS){
		printf("insufficient arguments.\n"
			/* TODO: add required usage message */
			"usage: siggen outfile duration samplerate amplitude frequency\n"
			);
		return 1;
	}

    /* Validate all command line arguments */
    duration = atof(argv[ARG_DUR]);
    if(duration < 0.0)
    {
        puts("Duration must be positive");
        return 1;
    }

    sample_rate = atoi(argv[ARG_SRATE]);
    if( sample_rate < 0)
    {
        puts("Sample Rate must be postive");
        return 1;
    }

    amplitude = atof(argv[ARG_AMP]);
    if(amplitude < 0.0)
    {
        puts("Amplitude must be positive");
        return 1;
    }

    frequency = atof (argv[ARG_FREQ]);
    if(frequency < 0.0)
    {
        puts("frequency must be positve");
        return 1;
    }

    // fill out our outfiles properties.
    outprops.srate = sample_rate;
    outprops.chans = 1;
    outprops.samptype = PSF_SAMP_16;
    outprops.chformat = STDWAVE;

	/*  always startup portsf */
	if(psf_init()){
		printf("unable to start portsf\n");
		return 1;
	}
/* STAGE 3 */																							
	
    //determine required memory for requested outfile
    outframes = (unsigned long) (duration * outprops.srate + 0.5);
    nbufs = outframes/ nframes;
    remainder = outframes - nbufs * nframes;
    if(remainder > 0)
        nbufs++;



	/* allocate space for  sample frame buffer ...*/
	outframe = (float*) malloc(nframes * sizeof(float));
	if(outframe==NULL){
		puts("No memory!\n");
		error++;
		goto exit;
	}

	/* check file extension of outfile name, so we use correct output file format*/
	outformat = psf_getFormatExt(argv[ARG_OUTFILE]);
	if(outformat == PSF_FMT_UNKNOWN){
		printf("outfile name %s has unknown format.\n"
			"Use any of .wav, .aiff, .aif, .afc, .aifc\n",argv[ARG_OUTFILE]);
		error++;
		goto exit;
	}
    outprops.format = outformat;
	
/* STAGE 4 */												
	/* TODO: any other argument processing and setup of variables, 		     
	   output buffer, etc., before creating outfile
	*/

	/* handle outfile */
	/* TODO:  make any changes to outprops here  */

	peaks  =  (PSF_CHPEAK*) malloc(outprops.chans * sizeof(PSF_CHPEAK));
	if(peaks == NULL){
		puts("No memory!\n");
		error++;
		goto exit;
	}

	/* TODO: if outchans != inchans, allocate outframe, and modify main loop accordingly */

	ofd = psf_sndCreate(argv[ARG_OUTFILE],&outprops,0,0,PSF_CREATE_RDWR);
	if(ofd < 0){
        printf("\n%d\n", ofd);
		printf("Error: unable to create outfile %s\n",argv[ARG_OUTFILE]);
		error++;
		goto exit;
	}
    osc = oscil();
    InitOscillator(osc, sample_rate);
    
/* STAGE 5 */	
	printf("processing....\n");								
	/* TODO: init any loop-related variables */
	for(i = 0; i < nbufs; i++)
    {
        if(i==nbufs-1)
        {
            nframes = remainder;
        }

        for( j = 0; j < nframes;j++)
        {
            outframe[j] = (float) (amplitude * sine_tick(osc, frequency));
            if(psf_sndWriteFloatFrames(ofd, outframe, nframes)!= nframes)
            {
                puts("Error writing to outfile\n");
                error++;
                break;
            }
        }
    }

	if(framesread < 0)	{
		printf("Error reading infile. Outfile is incomplete.\n");
		error++;
	}
	else
		printf("Done: %d errors\n",error);
/* STAGE 6 */		
	/* report PEAK values to user */							
	/*
    if(psf_sndReadPeaks(ofd,peaks,NULL) > 0){
		long i;
		double peaktime;
		printf("PEAK information:\n");	
			for(i=0;i < inprops.chans;i++){
				peaktime = (double) peaks[i].pos / (double) inprops.srate;
				printf("CH %ld:\t%.4f at %.4f secs\n", i+1, peaks[i].val, peaktime);
			}
	}
    */
/* STAGE 7 */	
	/* do all cleanup  */    									
exit:	 	
	if(ofd >= 0)
		if(psf_sndClose(ofd))
			printf("%s: Warning: error closing outfile %s\n",argv[ARG_PROGNAME],argv[ARG_OUTFILE]);
	if(inframe)
		free(inframe);
	if(peaks)
		free(peaks);
	/*TODO: cleanup any other resources */

	psf_finish();
	return error;
}
