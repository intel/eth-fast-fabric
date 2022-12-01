/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2022, Intel Corporation

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include <dirent.h>

#define MAX_CNTR_FILENAME 1024

#define CMD "ethbw"

//struct ethtool_counters_s {
//	unsigned long tx_priority_0_xon_nic;
//	unsigned long tx_priority_0_xoff_nic;
//	unsigned long rx_priority_0_xon_nic;
//	unsigned long rx_priority_0_xoff_nic;
//	unsigned long rx_dropped;
//	unsigned long tx_dropped_link_down_nic;
//	unsigned long rx_dropped_link_down_nic;
//};

struct sysclass_counter_s {
	char *filename;
	unsigned long value;
};
	

unsigned int g_interval = 1;
#define MAX_INTERVAL 60	// make sure 100g will fit in 6 digits of MBs
unsigned int g_duration = UINT_MAX;

#define MAX_NICS 16
#define MB (1000 * 1000)
int g_num_nics=0;
struct nic_info_s {
	char *name;
	struct sysclass_counter_s ip4OutOctets;
	struct sysclass_counter_s ip4InOctets;
	struct sysclass_counter_s ip4InDiscards;
	struct sysclass_counter_s tcpRetransSegs;
	//struct ethtool_counters_s eth_ctrs;
} g_nics[MAX_NICS];

// "cnpHandled",
// "cnpIgnored",
// "cnpSent",
// "ip4InDiscards",
// "ip4InMcastOctets",
// "ip4InMcastPkts",
// "ip4InOctets",
// "ip4InPkts",
// "ip4InReasmRqd",
// "ip4InTruncatedPkts",
// "ip4OutMcastOctets",
// "ip4OutMcastPkts",
// "ip4OutNoRoutes",
// "ip4OutOctets",
// "ip4OutPkts",
// "ip4OutSegRqd",
// "ip6InDiscards",
// "ip6InMcastOctets",
// "ip6InMcastPkts",
// "ip6InOctets",
// "ip6InPkts",
// "ip6InReasmRqd",
// "ip6InTruncatedPkts",
// "ip6OutMcastOctets",
// "ip6OutMcastPkts",
// "ip6OutNoRoutes",
// "ip6OutOctets",
// "ip6OutPkts",
// "ip6OutSegRqd",
// "iwInRdmaReads",
// "iwInRdmaSends",
// "iwInRdmaWrites",
// "iwOutRdmaReads",
// "iwOutRdmaSends",
// "iwOutRdmaWrites",
// "iwRdmaBnd",
// "iwRdmaInv",
// "lifespan",
// "RxECNMrkd",
// "RxUDP",
// "rxVlanErrors",
// "tcpInOptErrors",
// "tcpInProtoErrors",
// "tcpInSegs",
// "tcpOutSegs",
// "tcpRetransSegs",
// "TxUDP",

// ANSI terminal sequences to change color
char bf_color_off[] = {27, '[', '0', 'm', 0};
char bf_color_red[] = {27, '[', '3', '1', 'm', 0};
char bf_color_green[] = {27, '[', '3', '2', 'm', 0};
char bf_color_yellow[] = {27, '[', '3', '3', 'm', 0};
char bf_color_blue[] = {27, '[', '3', '4', 'm', 0};
char bf_color_cyan[] = {27, '[', '3', '6', 'm', 0};
// ANSI terminal sequence to clear screen
char bf_clear_screen[] = {27, '[', 'H', 27, '[', '2', 'J', 0};

int init_sysclass_counter(const char *nic, const char *counter,
						 struct sysclass_counter_s *cntr)
{
	char filename[MAX_CNTR_FILENAME];

	snprintf(filename, MAX_CNTR_FILENAME, "/sys/class/infiniband/%s/ports/1/hw_counters/%s", nic, counter);
	cntr->filename = realpath(filename, NULL);
	if (cntr->filename && access(cntr->filename, R_OK) == 0) 
		return 0;

	// alternate directory name for some 3rd party drivers
	snprintf(filename, MAX_CNTR_FILENAME, "/sys/class/infiniband/%s/ports/1/counters/%s", nic, counter);
	cntr->filename = realpath(filename, NULL);
	if (!cntr->filename || access(cntr->filename, R_OK) != 0) {
		free(cntr->filename);
		cntr->filename = NULL;
		return -1;
	} else
		return 0;
}

int check_counters(const char *nic, struct nic_info_s *nic_info)
{
	if (init_sysclass_counter(nic, "ip4OutOctets", &nic_info->ip4OutOctets ) != 0)
		if (init_sysclass_counter(nic, "port_xmit_data", &nic_info->ip4OutOctets ) != 0)
			return -1;
	if (init_sysclass_counter(nic, "ip4InOctets", &nic_info->ip4InOctets) != 0)
		if (init_sysclass_counter(nic, "port_rcv_data", &nic_info->ip4InOctets) != 0)
			return -1;

	// These counters are unavailble on some 3rd party NICs
	if (init_sysclass_counter(nic, "ip4InDiscards", &nic_info->ip4InDiscards) != 0)
		fprintf(stderr, CMD ": Warning: NIC %s: lacks ip4InDiscards\n", nic);

	// newer CVL driver has changed name of this counter so try both names
	if (init_sysclass_counter(nic, "tcpRetransSegs", &nic_info->tcpRetransSegs) == 0) {
		return 0;
	}
	if (init_sysclass_counter(nic, "RetransSegs", &nic_info->tcpRetransSegs) != 0)
		fprintf(stderr, CMD ": Warning: NIC %s: lacks RetransSegs\n", nic);
	return 0;
}

void get_nic_names(char **nic_args)
{
	char *nic;
	struct dirent **namelist = NULL;
	int n=0, i=0;

	if (! nic_args) {
		// we will get NICs in sorted order
		n = scandir("/sys/class/infiniband/", &namelist, NULL, alphasort);
		if (n < 0) {
			perror(CMD ": Unable to get list of NICs");
			exit(1);
		}
		if (n == 0 || ! namelist) {
			fprintf(stderr, "No NICs found");
			exit(1);
		}
		nic = namelist[0]->d_name;
	} else {
		nic = *nic_args++;
	}
	while (nic) {
		if (! nic_args && nic[0] == '.') {
			// skip . and .. and any hidden files/dirs
		} else if (nic[0] == '.' || strchr(nic, '/')) {
			fprintf(stderr, CMD ": Skipping NIC %s: suspicious NIC name\n", nic);
		} else {
			if (g_num_nics >= MAX_NICS) {
				fprintf(stderr, CMD ": Too many NICs\n");
				exit(1);
			}
			if (0 != check_counters(nic, &g_nics[g_num_nics])) {
				fprintf(stderr, CMD ": Skipping NIC %s: lacks required counters\n", nic);
			} else {
				g_nics[g_num_nics].name=strdup(nic);
				g_num_nics++;
			}
		}

		if (! nic_args) {
			if (namelist)	/* klockwork */
				free(namelist[i]);
			i++;
			if (i < n && namelist)	/* klockwork wants namelist tested */
				nic = namelist[i]->d_name;
			else
				nic = NULL;
		} else {
			nic = *nic_args++;
		}
	}
	if (! g_num_nics) {
		fprintf(stderr, CMD ": No NICs found\n");
		exit(1);
	}

	if (! nic_args) {
		free(namelist);
	}
}

unsigned long get_counter(struct sysclass_counter_s *cntr)
{
	FILE *f;
	char value[1024];
	unsigned long ul;
	int r;
	char *end;

	if (! cntr->filename)	// counter unavailable
		return 0;

	f = fopen(cntr->filename, "r");
	if (! f) {
		fprintf(stderr, CMD ": Can't open NIC counter %s\n", cntr->filename);
		exit(1);
	}
	r = fread(value, 1, sizeof(value), f);
	if (r <= 0 || r >= sizeof(value)) {
		fprintf(stderr, CMD ": Unable to read value of %s\n", cntr->filename);
		exit(1);
	}
	value[r] = '\0';
	ul = strtoul(value, &end, 10);
	// can have trailing newline
	if (end == value /*|| *end != '\0'*/) {
		//fprintf(stderr, CMD ": Invalid value of %s: %s %p %p 0x%x\n", fn, value, value, end, *end);
		fprintf(stderr, CMD ": Invalid value of %s: %s\n", cntr->filename, value);
		exit(1);
	}
	fclose(f);
	return ul;
}

void heading(void)
{
	int i;
	for (i=0; i<g_num_nics; i++) {
		if (i && i%4 == 0)
			printf("\n");
		printf("NIC %d: %-11s ", i, g_nics[i].name);
	}
	printf("\n\n");

	printf("          ");
	for (i=0; i<g_num_nics; i++) {
		//printf("%11s ", g_nics[i].name);
		if (g_interval <= 8)
			printf("NIC%2d ", i);
		else
			printf(" NIC%2d ", i);
	}
	printf("\n");
}

void init_counters(void)
{
	int i;
	for (i=0; i<g_num_nics; i++) {
		g_nics[i].ip4OutOctets.value = get_counter(&g_nics[i].ip4OutOctets);
		g_nics[i].ip4InOctets.value = get_counter(&g_nics[i].ip4InOctets);
		g_nics[i].ip4InDiscards.value = get_counter(&g_nics[i].ip4InDiscards);
		g_nics[i].tcpRetransSegs.value = get_counter(&g_nics[i].tcpRetransSegs);
	}
}

unsigned long get_delta(struct sysclass_counter_s *cntr)
{
	unsigned long new_val = get_counter(cntr);
	unsigned long delta = new_val - cntr->value;
	cntr->value = new_val;
	return delta;
}

void show_counters(void)
{
	int i;
	time_t now;
	const struct tm *tm;
	static int last_hour=-1;	// force output on 1st call

	time(&now);
	tm = localtime(&now);
	if (! tm) {	// unexpected, probably out of memory, die
		perror(CMD ": Unable to get local time");
		exit(1);
	}
	if (tm->tm_hour != last_hour || g_interval >= (60*60))
		printf("%s", ctime(&now));
	last_hour = tm->tm_hour;
	printf("%02d:%02d", tm->tm_min, tm->tm_sec);
	printf(" xmt ");
	for (i=0; i<g_num_nics; i++) {
		unsigned long delta_out = get_delta(&g_nics[i].ip4OutOctets);
		unsigned long delta_re = get_delta(&g_nics[i].tcpRetransSegs);

		if (g_interval <= 8) {
			if (delta_re)
				printf("%s%5lu%s ", bf_color_red, delta_out / MB, bf_color_off);
			else
				printf("%5lu ", delta_out / MB);
		} else {
			if (delta_re)
				printf("%s%6lu%s ", bf_color_red, delta_out / MB, bf_color_off);
			else
				printf("%6lu ", delta_out / MB);
		}
	}
	printf("\n");

	printf("     ");
	printf(" rcv ");
	for (i=0; i<g_num_nics; i++) {
		unsigned long delta_in = get_delta(&g_nics[i].ip4InOctets);
		unsigned long delta_dis = get_delta(&g_nics[i].ip4InDiscards);

		if (g_interval <= 8) {
			if (delta_dis)
				printf("%s%5lu%s ", bf_color_red, delta_in / MB, bf_color_off);
			else
				printf("%5lu ", delta_in / MB);
		} else {
			if (delta_dis)
				printf("%s%6lu%s ", bf_color_red, delta_in / MB, bf_color_off);
			else
				printf("%6lu ", delta_in / MB);
		}
	}
	printf("\n");
}

void Usage(int exit_code)
{
	fprintf(stderr, "Usage: " CMD " [-i seconds] [-d seconds] [nic ... ]\n");
	fprintf(stderr, "    -i/--interval seconds     - interval at which bandwidth will be shown\n");
	fprintf(stderr, "                                Values of 1-60 allowed. Default 1\n");
	fprintf(stderr, "    -d/--duration seconds     - duration to monitor for.  Default is 'infinite'\n");
	fprintf(stderr, "Where each nic specified is an RDMA nic name\n");
	fprintf(stderr, "If no nics are specified, all RDMA nics will be monitored\n");
	fprintf(stderr, "\nfor example:\n");
	fprintf(stderr, "   ethbw\n");
	fprintf(stderr, "   ethbw irdma1 irdma3\n");
	fprintf(stderr, "   ethbw -i 2 -d 300 irdma1 irdma3\n");
	exit(exit_code);
}

// command line options
struct option options[] = {
	{ "help", no_argument, NULL, '$' }, // use an invalid option character
	{ "interval", required_argument, NULL, 'i' },
	{ "duration", required_argument, NULL, 'd' },
	{ 0 }
};


int main(int argc, char **argv)
{
	int c, index;
	unsigned long temp;
	char *endptr;
	time_t end;

	while (-1 != (c = getopt_long(argc, argv, "i:d:", options, &index)))
    {
		switch (c) {
		case '$':
			Usage(0);
			break;
		case 'i':
			errno = 0;
			temp = strtoul(optarg, &endptr, 0);
			if (temp > UINT_MAX || temp > MAX_INTERVAL || errno || ! endptr || *endptr != '\0') {
				fprintf(stderr, CMD ": Invalid interval: %s\n", optarg);
				Usage(2);
			}
			g_interval = (unsigned int)temp;
			break;
		case 'd':
			if (strcmp(optarg, "infinite") == 0) {
				temp = UINT_MAX;
			} else {
				errno = 0;
				temp = strtoul(optarg, &endptr, 0);
				if (temp > UINT_MAX || errno || ! endptr || *endptr != '\0') {
					fprintf(stderr, CMD ": Invalid duration: %s\n", optarg);
					Usage(2);
				}
			}
			g_duration = (unsigned int)temp;
			break;
		default:
			//fprintf(stderr, CMD ": Invalid option -%c\n", c);
			Usage(2);
			break;
		}   // switch
	}  // while
	if (optind < argc)
	{
		get_nic_names(&argv[optind]);
	} else {
		get_nic_names(NULL);
	}
	heading();
	init_counters();
	end = time(NULL) + g_duration;
	do {
		sleep(g_interval);
		show_counters();
	} while (g_duration == UINT_MAX || time(NULL) < end);
	return 0;
}
