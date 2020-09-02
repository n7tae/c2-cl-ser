/*---------------------------------------------------------------------------*\

  FILE........: c2dec.c
  AUTHOR......: David Rowe
  DATE CREATED: 23/8/2010

  Decodes a file of bits to a file of raw speech samples using codec2.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2010 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <memory>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "codec2.h"
#include "dump.h"

#define NONE          0  /* no bit errors                          */
#define UNIFORM       1  /* random bit errors                      */
#define TWO_STATE     2  /* Two state error model                  */
#define UNIFORM_RANGE 3  /* random bit errors over a certain range */

void print_help(const struct option *long_options, int num_opts, char* argv[]);

int main(int argc, char *argv[])
{
	int            mode=0;
	FILE          *fin;
	FILE          *fout;
	FILE          *fber = NULL;
	int            nsam, nbit, nbyte, i, byte, frames, bits_proc, bit_errors, error_mode;
	int            nstart_bit, nend_bit, bit_rate;
	int            state, next_state;
	float          ber, r, burst_length, burst_period, burst_timer, ber_est;
	unsigned char  mask;
	int            natural, softdec, bit, ret, bitperchar;
#ifdef DUMP
	int dump;
#endif
	int     report_energy;
	FILE   *f_ratek = NULL;
	float  *user_ratek;
	int     K;
	CCodec2 cc2;

	const char *opt_string = "h:";
	struct option long_options[] =
	{
		{ "ber", required_argument, NULL, 0 },
		{ "startbit", required_argument, NULL, 0 },
		{ "endbit", required_argument, NULL, 0 },
		{ "berfile", required_argument, NULL, 0 },
		{ "natural", no_argument, &natural, 1 },
		{ "softdec", no_argument, &softdec, 1 },
		{ "bitperchar", no_argument, &bitperchar, 1 },
#ifdef DUMP
		{ "dump", required_argument, &dump, 1 },
#endif
		{ "energy", no_argument, NULL, 0 },
		{ "mlfeat", required_argument, NULL, 0 },
		{ "loadcb", required_argument, NULL, 0 },
		{ "loadratek", required_argument, NULL, 0 },
		{ "nopf", no_argument, NULL, 0 },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, 0 }
	};
	int num_opts=sizeof(long_options)/sizeof(struct option);

	if (argc < 4)
		print_help(long_options, num_opts, argv);

	if (strcmp(argv[2], "-")  == 0) fin = stdin;
	else if ( (fin = fopen(argv[2],"rb")) == NULL )
	{
		fprintf(stderr, "Error opening input bit file: %s: %s.\n", argv[2], strerror(errno));
		exit(1);
	}

	if (strcmp(argv[3], "-") == 0) fout = stdout;
	else if ( (fout = fopen(argv[3],"wb")) == NULL )
	{
		fprintf(stderr, "Error opening output speech file: %s: %s.\n", argv[3], strerror(errno));
		exit(1);
	}

	// If we got here, we need to honor the command line mode
	if (strcmp(argv[1],"3200") == 0)
		mode = CODEC2_MODE_3200;
	else if (strcmp(argv[1],"2400") == 0)
		mode = CODEC2_MODE_2400;
	else if (strcmp(argv[1],"1600") == 0)
		mode = CODEC2_MODE_1600;
	else if (strcmp(argv[1],"1400") == 0)
		mode = CODEC2_MODE_1400;
	else if (strcmp(argv[1],"1300") == 0)
		mode = CODEC2_MODE_1300;
	else if (strcmp(argv[1],"1200") == 0)
		mode = CODEC2_MODE_1200;
	else if (strcmp(argv[1],"700C") == 0)
		mode = CODEC2_MODE_700C;
	else if (strcmp(argv[1],"450") == 0)
		mode = CODEC2_MODE_450;
	else if (strcmp(argv[1],"450PWB") == 0)
		mode = CODEC2_MODE_450PWB;
	else
	{
		fprintf(stderr, "Error in mode: %s.  Must be 3200, 2400, 1600, 1400, 1300, 1200, 700C, 450, or 450PWB\n", argv[1]);
		exit(1);
	}
	bit_rate = atoi(argv[1]);

	error_mode = NONE;
	ber = 0.0;
	burst_length = burst_period = 0.0;
	burst_timer = 0.0;
	natural = softdec = bitperchar = 0;
	report_energy = 0;
#ifdef DUMP
	dump = 0;
#endif

	if (cc2.codec2_create(mode))
		return 1;
	nsam = cc2.codec2_samples_per_frame();
	nbit = cc2.codec2_bits_per_frame();
	short buf[nsam];
	nbyte = (nbit + 7) / 8;
	unsigned char bits[nbyte];
	float softdec_bits[nbit];
	char bitperchar_bits[nbit];
	frames = bit_errors = bits_proc = 0;
	nstart_bit = 0;
	nend_bit = nbit-1;

	while(1)
	{
		int option_index = 0;
		int opt = getopt_long(argc, argv, opt_string, long_options, &option_index);
		if (opt == -1)
			break;

		switch (opt)
		{
		case 0:
			if(strcmp(long_options[option_index].name, "ber") == 0)
			{
				ber = atof(optarg);
				error_mode = UNIFORM;
			}
			else if(strcmp(long_options[option_index].name, "startbit") == 0)
			{
				nstart_bit = atoi(optarg);
			}
			else if(strcmp(long_options[option_index].name, "endbit") == 0)
			{
				nend_bit = atoi(optarg);
			}
			else if(strcmp(long_options[option_index].name, "berfile") == 0)
			{
				if ((fber = fopen(optarg,"wt")) == NULL)
				{
					fprintf(stderr, "Error opening BER file: %s %s.\n",
							optarg, strerror(errno));
					exit(1);
				}

			}
#ifdef DUMP
			else if(strcmp(long_options[option_index].name, "dump") == 0)
			{
				if (dump)
					dump_on(optarg);
			}
#endif
			else if (strcmp(long_options[option_index].name, "energy") == 0)
			{
				report_energy = 1;
			}
			else if (strcmp(long_options[option_index].name, "loadcb") == 0)
			{
				/* load VQ stage (700C only) */
				//fprintf(stderr, "%s\n", optarg+1);
				cc2.codec2_load_codebook(atoi(optarg)-1, argv[optind]);
			}
			else if (strcmp(long_options[option_index].name, "loadratek") == 0)
			{
				/* load rate K vectors (by passing quantisation) for 700C VQ tests */
				fprintf(stderr, "%s\n", optarg);
				f_ratek = fopen(optarg, "rb");
				assert(f_ratek != NULL);
				user_ratek = cc2.codec2_enable_user_ratek(&K);
			}
			else if (strcmp(long_options[option_index].name, "nopf") == 0)
			{
				cc2.codec2_700c_post_filter(0);
			}
			else if (strcmp(long_options[option_index].name, "mlfeat") == 0)
			{
				/* dump machine learning features (700C only) */
				cc2.codec2_open_mlfeat(optarg, NULL);
			}
			break;

		case 'h':
			print_help(long_options, num_opts, argv);
			break;

		default:
			/* This will never be reached */
			break;
		}
	}
	assert(nend_bit <= nbit);
	cc2.codec2_set_natural_or_gray(!natural);
	//printf("%d %d\n", nstart_bit, nend_bit);

	//fprintf(stderr, "softdec: %d natural: %d\n", softdec, natural);
	if (softdec)
	{
		ret = (fread(softdec_bits, sizeof(float), nbit, fin) == (size_t)nbit);
	}
	if (bitperchar)
	{
		ret = (fread(bitperchar_bits, sizeof(char), nbit, fin) == (size_t)nbit);
	}
	if (!softdec && !bitperchar)
	{
		ret = (fread(bits, sizeof(char), nbyte, fin) == (size_t)nbyte);
	}

	while(ret)
	{
		frames++;

		// apply bit errors, MSB of byte 0 is bit 0 in frame, only works in packed mode

		if ((error_mode == UNIFORM) || (error_mode == UNIFORM_RANGE))
		{
			assert(softdec == 0);
			for(i=nstart_bit; i<nend_bit+1; i++)
			{
				r = (float)rand()/RAND_MAX;
				if (r < ber)
				{
					byte = i/8;
					//printf("nbyte %d nbit %d i %d byte %d bits[%d] 0x%0x ", nbyte, nbit, i, byte, byte, bits[byte]);
					mask = 1 << (7 - i + byte*8);
					bits[byte] ^= mask;
					//printf("shift: %d mask: 0x%0x bits[%d] 0x%0x\n", 7 - i + byte*8, mask, byte, bits[byte] );
					bit_errors++;
				}
				bits_proc++;
			}
		}

		if (error_mode == TWO_STATE)
		{
			assert(softdec == 0);
			burst_timer += (float)nbit/bit_rate;
			fprintf(stderr, "burst_timer: %f  state: %d\n", burst_timer, state);

			next_state = state;
			switch(state)
			{
			case 0:

				/* clear channel state - no bit errors */

				if (burst_timer > (burst_period - burst_length))
					next_state = 1;
				break;

			case 1:

				/* burst error state - 50% bit error rate */

				for(i=nstart_bit; i<nend_bit+1; i++)
				{
					r = (float)rand()/RAND_MAX;
					if (r < 0.5)
					{
						byte = i/8;
						bits[byte] ^= 1 << (7 - i + byte*8);
						bit_errors++;
					}
					bits_proc++;
				}

				if (burst_timer > burst_period)
				{
					burst_timer = 0.0;
					next_state = 0;
				}
				break;

			}

			state = next_state;
		}

		if (fber != NULL)
		{
			if (fread(&ber_est, sizeof(float), 1, fber) != 1)
			{
				fprintf(stderr, "ran out of BER estimates!\n");
				exit(1);
			}
			//fprintf(stderr, "ber_est: %f\n", ber_est);
		}
		else
			ber_est = 0.0;

		if (softdec)
		{
			/* pack bits, MSB received first  */

			bit = 7;
			byte = 0;
			memset(bits, 0, nbyte);
			for(i=0; i<nbit; i++)
			{
				bits[byte] |= ((softdec_bits[i] < 0.0) << bit);
				bit--;
				if (bit < 0)
				{
					bit = 7;
					byte++;
				}
			}
			cc2.codec2_set_softdec(softdec_bits);
		}

		if (bitperchar)
		{
			/* pack bits, MSB received first  */

			bit = 7;
			byte = 0;
			memset(bits, 0, nbyte);
			for(i=0; i<nbit; i++)
			{
				bits[byte] |= bitperchar_bits[i] << bit;
				bit--;
				if (bit < 0)
				{
					bit = 7;
					byte++;
				}
			}
		}

		if (report_energy)
			fprintf(stderr, "Energy: %1.3f\n", cc2.codec2_get_energy(bits));

		if (f_ratek != NULL)
			ret = fread(user_ratek, sizeof(float), K, f_ratek);

		cc2.codec2_decode_ber(buf, bits, ber_est);
		fwrite(buf, sizeof(short), nsam, fout);

		//if this is in a pipeline, we probably don't want the usual
		//buffering to occur

		if (fout == stdout) fflush(stdout);
		if (fin == stdin) fflush(stdin);

		if (softdec)
		{
			ret = (fread(softdec_bits, sizeof(float), nbit, fin) == (size_t)nbit);
		}
		if (bitperchar)
		{
			ret = (fread(bitperchar_bits, sizeof(char), nbit, fin) == (size_t)nbit);
		}
		if (!softdec && !bitperchar)
		{
			ret = (fread(bits, sizeof(char), nbyte, fin) == (size_t)nbyte);
		}
	}

	if (error_mode)
		fprintf(stderr, "actual BER: %1.3f\n", (float)bit_errors/bits_proc);

	cc2.codec2_destroy();

	fclose(fin);
	fclose(fout);
	if (fber != NULL)
	{
		fclose(fber);
	}

	return 0;
}

void print_help(const struct option* long_options, int num_opts, char* argv[])
{
	int i;
	const char *option_parameters;
	fprintf(stderr, "\nc2dec - Codec 2 decoder and bit error simulation program\n"
			"usage: %s 3200|2400|1600|1400|1300|1200|700C|450|450PWB InputFile OutputRawFile [OPTIONS]\n\n"
			"Options:\n", argv[0]);
	for(i=0; i<num_opts-1; i++)
	{
		if(long_options[i].has_arg == no_argument)
		{
			option_parameters="";
		}
		else if (strcmp("ber", long_options[i].name) == 0)
		{
			option_parameters = " BER";
		}
		else if (strcmp("startbit", long_options[i].name) == 0)
		{
			option_parameters = " startBit";
		}
		else if (strcmp("endbit", long_options[i].name) == 0)
		{
			option_parameters = " endBit";
		}
		else if (strcmp("berfile", long_options[i].name) == 0)
		{
			option_parameters = " berFileName";
		}
		else if (strcmp("dump", long_options[i].name) == 0)
		{
			option_parameters = " dumpFilePrefix";
		}
		else if (strcmp("lspEWov", long_options[i].name) == 0)
		{
			option_parameters = " featureFileName";
		}
		else
		{
			option_parameters = " <UNDOCUMENTED parameter>";
		}
		fprintf(stderr, "\t--%s%s\n", long_options[i].name, option_parameters);
	}
	exit(1);
}
