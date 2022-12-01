/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2021, Intel Corporation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

** END_ICS_COPYRIGHT7   ****************************************/

/* [ICS VERSION STRING: unknown] */

#include "port_num_gen.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define BASENAME "ethportnum"
#define MAX_NAME 32
#define MAX_PORTS 256

void Usage() {
	fprintf(stderr, "Usage: %s file\n", BASENAME);
	fprintf(stderr, "\twhere 'file' is Port Id file with each line represents one port id\n");
	fprintf(stderr, "Example:\n");
	fprintf(stderr, "\t%s port_id.txt\n", BASENAME);
}

boolean check_filename(char* name) {
	if (!name || !name[0]) {
		fprintf(stderr, "%s: ERROR - Empty input file name.\n", BASENAME);
		return FALSE;
	}
	if (strlen(name) >= PATH_MAX) {
		fprintf(stderr, "%s: ERROR - Input file name is too long.\n", BASENAME);
		return FALSE;
	}

	struct stat res;

	if (stat(name, &res)) {
		fprintf(stderr, "%s: ERROR - Cannot access '%s': %s\n",
		                BASENAME, name, strerror(errno));
		return FALSE;
	}

	if (!S_ISREG(res.st_mode) && !S_ISLNK(res.st_mode) && !S_ISFIFO(res.st_mode) ) {
		fprintf(stderr, "%s: ERROR - File '%s' is not a regular file, symbolic link or named pipe\n",
		                BASENAME, name);
		return FALSE;
	}

	if (S_ISREG(res.st_mode) && res.st_size > (200L << 20)) {
		fprintf(stderr, "%s: ERROR - File '%s' exceeds 200 Mb\n", BASENAME, name);
		return FALSE;
	}

	return TRUE;
}

// internal tool used by ethxlattopology
int main(int argc, char** argv) {
	if (argc != 2) {
		Usage();
		exit(1);
	}

	char* filename = argv[1];
	if (!check_filename(filename)) {
		Usage();
		exit(2);
	}

	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "%s: ERROR - Failed to open '%s': %s.\n",
                                                BASENAME, filename, strerror(errno));
                exit(3);
	}

	pn_gen_t model;
	pn_gen_init(&model);
	char buffer[MAX_NAME+2]; // one for new line, one for null termination
	size_t line_len = 0;
	boolean skipping = FALSE;

	char (*ports)[MAX_NAME+1] = calloc(MAX_PORTS, MAX_NAME+1);
	if (ports == NULL) {
		fprintf(stderr, "%s: ERROR - Cannot allocate memory\n", BASENAME);
		exit(3);
	}

	int count = 0;
	while (NULL != fgets(buffer, sizeof(buffer), fp) && count < MAX_PORTS) {
		line_len = strlen(buffer);
		if (line_len == 0)
			continue;
		// ignore long line
		if (buffer[line_len - 1] != '\n') {
			skipping = TRUE;
			continue;
		}
		if (skipping) {
			skipping = FALSE;
			continue;
		}
		if (buffer[0] == '#' || buffer[0] == '\n') {
			continue; // ignore comments or empty lines
		}
		// remove '\n'
		if (buffer[line_len - 1] == '\n') {
			buffer[line_len - 1] = '\0';
		}

		snprintf(ports[count++], MAX_NAME+1, "%s", buffer);
		pn_gen_register(&model, buffer);
	}
	fclose(fp);
	if (count == MAX_PORTS) {
		fprintf(stderr, "%s: WARNING - Only first %d items processed\n", BASENAME, MAX_PORTS);
	}

	int i;
	for (i = 0; i< count; i++) {
		printf("%d:%s\n", pn_gen_get_port(&model, ports[i]), ports[i]);
	}

	free(ports);
	pn_gen_cleanup(&model);
	return 0;
}
