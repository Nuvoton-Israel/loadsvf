/* Copyright (c) 2018, Nuvoton Corporation */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/time.h>
#include "jtag.h"

#define JTAG_FREQ	(10 * 1000000)
#define LOG_LEVEL	2
#define JTAG_MODE_SPI	0
#define JTAG_MODE_GPIO	1
int loglevel = LOG_LEVEL;
unsigned int frequency = 0;
int step = 0;
unsigned long total_runtest_time = 0;

static int mode = JTAG_MODE_SPI;  /* default transfer mode */
char *svf_path = NULL;
char *jtag_dev = NULL;
static JTAG_Handler* jtag_handler = NULL;
extern int handle_svf_command(JTAG_Handler* state, char *filename);

void DBG_log(int level, const char *format, ...)
{
	if (level < loglevel)
		return;

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
}

void showUsage(char **argv)
{
	fprintf(stderr, "Usage: %s [option(s)]\n", argv[0]);
	fprintf(stderr, "  -d <device>   jtag device\n");
	fprintf(stderr, "  -l <level>    log level\n");
	fprintf(stderr, "  -m <mode>     transfer mode\n");
	fprintf(stderr, "                (1: PSPI mode)\n");
	fprintf(stderr, "                (0: GPIO mode)\n");
	fprintf(stderr, "  -f <freq>     force running at frequency (Mhz)\n");
	fprintf(stderr, "                for PSPI mode\n");
	fprintf(stderr, "  -s <filepath> load svf file\n");
	fprintf(stderr, "  -g            run svf command line by line\n\n");
}

void process_command_line(int argc, char **argv)
{
	int c = 0;
	int v;
	while ((c = getopt(argc, argv, "m:f:l:s:d:g")) != -1) {
		switch (c) {
		case 'l': {
			loglevel = atoi(optarg);
			break;
		}
		case 'm': {
			v = atoi(optarg);
			if (v >= 0 && v <= 1) {
				if (v == 1)
					mode = JTAG_MODE_SPI;
				else if (v == 0)
					mode = JTAG_MODE_GPIO;
			}
			break;
		}
		case 'f': {
			v = atoi(optarg);
			if (v > 0 && v <= MAX_FREQ) {
				frequency = v * 1000000;
			}
			break;
		}
		case 'g': {
			step = 1;
			break;
		}
		case 'd': {
			jtag_dev = malloc(strlen(optarg) + 1);
			strcpy(jtag_dev, optarg);
			break;
		}
		case 's': {
			svf_path = malloc(strlen(optarg) + 1);
			strcpy(svf_path, optarg);
			break;
		}
		default:  // h, ?, and other
			showUsage(argv);
			exit(EXIT_SUCCESS);
		}
	}
	if (optind < argc) {
		fprintf(stderr, "invalid non-option argument(s)\n");
		showUsage(argv);
		exit(EXIT_SUCCESS);
	}
}

JTAG_Handler* JTAG_initialize(void)
{
	int enable;
	JTAG_Handler* state = (JTAG_Handler*)malloc(sizeof(JTAG_Handler));
	if (!state)
		return NULL;

	state->tap_state = JtagTLR;

	state->JTAG_driver_handle = open(jtag_dev, O_RDWR);
	if (state->JTAG_driver_handle == -1) {
		fprintf(stderr, "Can't open %s\n", jtag_dev);
		goto err;
	}

	/* set frequency */
	if (frequency > 0) {
		if (JTAG_set_clock_frequency(state->JTAG_driver_handle, frequency) != ST_OK) {
			fprintf(stderr, "Unable to set the TAP frequency!\n");
			goto err;
		}
	}

	/* set transfer mode */
	if (JTAG_set_mode(state->JTAG_driver_handle, mode) != ST_OK) {
		fprintf(stderr, "Failed to set JTAG mode to master.\n");
		goto err;
	}

	if (JTAG_set_tap_state(state, JtagTLR) != ST_OK ||
			JTAG_set_tap_state(state, JtagRTI) != ST_OK) {
		fprintf(stderr, "Failed to set initial TAP state.\n");
		goto err;
	}

	return state;
err:
	free(state);
	return NULL;
}

int main(int argc, char **argv)
{
	struct  timeval start, end;
	unsigned long diff;
	int ret = 0;

	process_command_line(argc, argv);
	if (!svf_path || !jtag_dev) {
		showUsage(argv);
		return 0;
	}

	jtag_handler = JTAG_initialize();
	if (!jtag_handler) {
		fprintf(stderr, "Failed to initialize JTAG\n");
		goto err;
	}

	gettimeofday(&start,NULL);
	ret = handle_svf_command(jtag_handler, svf_path);
	fprintf(stderr, "handle_svf_command %d\n", ret);
	if (ret) {
		fprintf(stderr, "Failed to handle svf command\n");
		goto err;
	}
	gettimeofday(&end,NULL);
	diff = 1000 * (end.tv_sec-start.tv_sec)+ (end.tv_usec-start.tv_usec) / 1000;
	printf("loading time is %ld ms\n",diff);
	//printf("total runtest time is %ld ms\n", total_runtest_time / 1000);

err:
	free(svf_path);
	free(jtag_dev);
	return ret;
}
