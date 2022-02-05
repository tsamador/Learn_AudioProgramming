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
#include "portsf/breakpoints.h"

/* set size of multi-channel frame-buffer */
#define NFRAMES (1024)

/* TODO define program argument list, excluding flags */
enum {ARG_PROGNAME, ARG_OUTFILE, ARG_TYPE, ARG_DUR, ARG_SRATE, ARG_AMP, ARG_FREQ,ARG_NARGS};

enum {WAVE_SINE, WAVE_TRIANGLE, WAVE_SQUARE, WAVE_SAWUP, WAVE_SAWDOWN, WAVE_NTYPES};

/*Function pointer to the correct tick function */
typedef double (*tickfunc)(OSCIL* osc, double freq);

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
	int wave_type;
	tickfunc tick;

	/* BreakPoint stream for amplitude */
	break_stream* ampstream = NULL;
	FILE* amplitude_file = NULL;
	unsigned long break_amp_size = 0;
	double minval, maxval;

	/* Breakpoint stream for frequency */

	break_stream* freqstream = NULL;
	FILE* frequency_file = NULL;
	unsigned long break_freq_size = 0;


	/* TODO: define an output frame buffer if channel width different 	*/
	
/* STAGE 2 */	

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
			"usage: siggen outfile wavetype duration(s) samplerate amplitude frequency\n"
            " \t Where wavetype = \n"
            " \t    0 = Sine Wave\n"
            " \t    1 = Triangle Wave\n"
            " \t    2 = Square Wave\n"
            " \t    3 = Sawtooth Up Wave\n"
			" \t    4 = Sawtooth Down Wave\n"
			" \t amplitude can either be a constant or filename of a breakpoint file\n"
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

	wave_type = atoi(argv[ARG_TYPE]);
	if(wave_type < WAVE_SINE || wave_type >= WAVE_NTYPES)
	{
		puts("Invalid option for wave type");
		return 1;
	}

    sample_rate = atoi(argv[ARG_SRATE]);
    if( sample_rate < 0)
    {
        puts("Sample Rate must be postive");
        return 1;
    }

	/* basically, instead of doing complicated command line arguments,
	assume the user entered a breakpoint file, if it fails to open
	then treat it like a number */
	amplitude_file = fopen(argv[ARG_AMP], "r");
	if(!amplitude_file)
	{
		amplitude = atof(argv[ARG_AMP]);
    	if(amplitude < 0.0 || amplitude > 1.0)
    	{
        	puts("Amplitude must be positive");
        	return 1;
    	}
	}
	else
	{
		ampstream = new_breakpoint_stream(amplitude_file, sample_rate, &break_amp_size);

		if(!bps_getminmax(ampstream, &minval, &maxval))
		{
			printf("Error finding minimum and maximum values in breakpoint file %s", argv[ARG_AMP]);
			error++;
			goto exit;
		}

		if(minval < 0.0 || minval > 1.0 || maxval < 0.0 || maxval >1.0)
		{
			puts("Error: amplitude value out of range");
			error++;
			goto exit;
		}
	}
    
	/* Do the same for frequency */
	frequency_file = fopen(argv[ARG_FREQ], "r");
	if(!frequency_file)
	{
		frequency = atof(argv[ARG_FREQ]);
    	if(frequency < 0.0 )
    	{
        	puts("Frequency must be positive");
        	return 1;
    	}
	}
	else
	{
		freqstream = new_breakpoint_stream(frequency_file, sample_rate, &break_freq_size);

		if(!bps_getminmax(freqstream, &minval, &maxval))
		{
			printf("Error finding minimum and maximum values in breakpoint file %s", argv[ARG_AMP]);
			error++;
			goto exit;
		}

		if(minval < 0.0 || maxval < 0.0)
		{
			puts("Error: frequency value out of range");
			error++;
			goto exit;
		}
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

	switch (wave_type)
	{
		case WAVE_SINE:
		{
			tick = sine_tick;
		}
		break;
		case WAVE_TRIANGLE:
		{
			tick = triangle_tick;
		}
		break;
		case WAVE_SQUARE:
		{
			tick = square_tick;
		}
		break;
		case WAVE_SAWUP:
		{
			tick = saw_upward_tick;
		}
		break;
		case WAVE_SAWDOWN:
		{
			tick = saw_downward_tick;
		}
		break;
	}

/* STAGE 5 */	
	printf("processing....\n");								
	/* TODO: init any loop-related variables */
	for(i = 0; i < nbufs; i++)
    {
        if(i == nbufs-1)
        {
            nframes = remainder;
        }

        for( j = 0; j < nframes;j++)
        {
			if(ampstream)
			{
				amplitude = breakpoints_stream_tick(ampstream);
				printf("Current amplitude value: %f \n", amplitude);
			}
			
			if(freqstream)
			{
				frequency = breakpoints_stream_tick(freqstream);
				printf("Current Frequency value: %f", frequency);
			}
			outframe[j] = (float)(amplitude * tick(osc, frequency));
        }

        if(psf_sndWriteFloatFrames(ofd, outframe, nframes)!= nframes)
        {
            puts("Error writing to outfile\n");
            error++;
            break;
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
	if(ampstream)
	{
		bps_freepoints(ampstream);
		free(ampstream);
	}
	if(amplitude_file)
	{
		if(fclose(amplitude_file))
			puts("Error closing breakpoint file");
	}
	/*TODO: cleanup any other resources */

	psf_finish();
	return error;
}
