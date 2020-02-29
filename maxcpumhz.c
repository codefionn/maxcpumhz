/* Printing cpu MHz using /proc/cpuinfo
 * Copyright (C) 2020  Fionn Langhans
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> // sleep

static const char * CPUMHZ = "cpu MHz";

static void writetobuffer(size_t *len, size_t *buflen, char * buffer, char c) {
	// reserve more buffer memory if necessary
	if (*len == *buflen) {
		*buflen *= 2;
		buffer = (char*) realloc((void*) buffer, *buflen);
	}
	// write char
	buffer[(*len)++] = (char) c;
}

static void readcpuinfo(size_t *filelen, size_t *buflen, char * buffer) {
	FILE *file = fopen("/proc/cpuinfo", "r");
	if (!file) {
		fprintf(stderr, "Error reading /proc/cpuinfo");
		exit(1);
		return;
	}

	int c;
	char last[8];
	last[7] = '\0';
	size_t lasti = 0;
	while ((c = fgetc(file)) != EOF) {
		if (lasti == 7) {
			// shift to left by 1
			for (size_t i = 1; i < 7; ++i) {
				last[i-1] = last[i];
			}
			// last to newly read character
			last[6] = (char) c;

			if (strcmp(last, CPUMHZ))
				continue;
		} else {
			last[lasti++] = c;
			if (!(lasti == 7 && !strcmp(last, CPUMHZ)))
				continue;
		}
		// clear
		memset((void*) last, 0, 7);
		lasti =  0;

		// real till digit
		while (!isdigit(c = fgetc(file)) && c != EOF);
		if (c == EOF)
			continue;

		do {
			writetobuffer(filelen, buflen, buffer, c);
		} while ((c = fgetc(file)) != '\n' && c != EOF);
		writetobuffer(filelen, buflen, buffer, '\n');
	}

	fclose(file);
}

static void printcpumax(const size_t len, const char * strings) {
	double max = 0.0;
	for (size_t i = 0; i < len; ++i) {
		const char * startptr = strings + i;
		while (strings[i] != '\n')
			++i;
		// i points to '\n'
		double mhz = strtod(startptr, NULL);
		if (max < mhz)
			max = mhz;
	}

	printf("%.2lf", max);
}

typedef struct _cpuclock {
	size_t nodeid;
	double clock;
} cpuclock;

static int comparecpuclock(const void *a, const void *b) {
	const cpuclock *ca = a;
	const cpuclock *cb = b;
	if (ca->clock == cb->clock)
		return 0;
	return ca->clock < cb->clock ? -1 : 1;
}

static void printcpuall(const size_t len, const char * strings) {
	size_t cpucount = 0;
	for (size_t i = 0; i < len; ++i) {
		if (strings[i] == '\n')
			++cpucount;
	}

	cpuclock * cpuclocks = (cpuclock*) malloc(sizeof(cpuclock) * cpucount);
	size_t nodeid = 0;
	for (size_t i = 0; i < len; ++i) {
		cpuclocks[nodeid].nodeid = nodeid;
		const char * startptr = strings + i;
		while (strings[i] != '\n')
			++i;
		// i points to '\n'
		double mhz = strtod(startptr, NULL);
		cpuclocks[nodeid].clock = mhz;
		// select next node point
		++nodeid;
	}

	qsort((void*) cpuclocks, cpucount, sizeof(cpuclock), comparecpuclock);
	for (size_t i = 0; i < cpucount; ++i) {
		if (i != 0)
			printf(", ");

		printf("%.2lf", cpuclocks[i].clock);
	}
}

static void printcpuinfo(size_t *filelen, size_t *buflen, char * buffer, bool displayall) {
	*filelen = 0; // reset read file contents
	readcpuinfo(filelen, buflen, buffer);
	if (displayall)
		printcpuall(*filelen, buffer);
	else
		printcpumax(*filelen, buffer);

	fflush(stdout);
}

int main(int argsc, char * argsv[]) {
	bool repeat = false, displayall = false;

	printf("maxcpumhz  Copyright (C) 2020  Fionn Langhans\n"
    		"This program comes with ABSOLUTELY NO WARRANTY;\n"
    		"This is free software, and you are welcome to redistribute it\n"
    		"under certain conditions;\n\n");

	if (argsc == 2) {
		if (strchr(argsv[1], 'r'))
			repeat = true;
		if (strchr(argsv[1], 'a'))
			displayall = true;
		if (!strcmp(argsv[1], "help") || !strcmp(argsv[1], "-h") || !strcmp(argsv[1], "--help")) {
			puts("cpumaxmhz: Display clock speed of processor threads\n"
					"\n"
					"ra - Repeat every 1 second, display all clock speeds\n"
					"r - Repeat every 1 second, display max\n"
					"a - Output one time, display all clock speeds\n"
					"No argument - Output one time, display max");
			return 0;
		}
	}

	size_t filelen = 0;
	size_t buflen = 1024 * 10; // 10k reserved
	char * buffer = (char*) malloc(buflen);

	printcpuinfo(&filelen, &buflen, buffer, displayall);

	while (repeat) {
		sleep(1);
		putchar('\r');
		printcpuinfo(&filelen, &buflen, buffer, displayall);
	}

	putchar('\n');

	free((void*) buffer);
}
