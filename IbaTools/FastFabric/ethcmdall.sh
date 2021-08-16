#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015-2020, Intel Corporation
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Intel Corporation nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# END_ICS_COPYRIGHT8   ****************************************

# [ICS VERSION STRING: unknown]
# run a command on all hosts or switches

# optional override of defaults
if [ -f /etc/eth-tools/ethfastfabric.conf ]
then
	. /etc/eth-tools/ethfastfabric.conf
fi

. /usr/lib/eth-tools/ethfastfabric.conf.def

TOOLSDIR=${TOOLSDIR:-/usr/lib/eth-tools}
BINDIR=${BINDIR:-/usr/sbin}

. $TOOLSDIR/ff_funcs

trap "exit 1" SIGHUP SIGTERM SIGINT

readonly BASENAME="$(basename $0)"

Usage_full()
{
#	echo "Usage: $BASENAME [-CpqPS] [-f hostfile] [-F switchesfile] [-h 'hosts']" >&2
#	echo "                    [-H 'switches'] [-u user] [-m 'marker'] [-T timelimit]" >&2
#	echo "                    'cmd'" >&2
	echo "Usage: $BASENAME [-pqP] [-f hostfile] [-h 'hosts'] [-u user] "  >&2
	echo "                    [-T timelimit] 'cmd'" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
#	echo "   -C - perform command against switches, default is hosts" >&2
#	echo "   -p - run command in parallel on all hosts/switches" >&2
	echo "   -p - run command in parallel on all hosts" >&2
	echo "   -q - quiet mode, don't show command to execute" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
#	echo "   -F switchesfile - file with switches in cluster" >&2
#	echo "           default is $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
	echo "   -h hosts - list of hosts to execute command on" >&2
#	echo "   -H switches - list of switches to execute command on" >&2
	echo "   -u user - user to perform cmd as" >&2
	echo "           for hosts default is current user code" >&2
#	echo "           for switches default is admin" >&2
#	echo "   -S - securely prompt for password for user on switches" >&2
#	echo "   -m 'marker' - marker for end of switch command output" >&2
#	echo "           if omitted defaults to switch command prompt" >&2
#	echo "           this may be a regular expression" >&2
	echo "   -T timelimit - timelimit in seconds when running host commands" >&2
	echo "           default is -1 (infinite)" >&2
#	echo "   -P      output hostname/switch name as prefix to each output line" >&2
	echo "   -P      output hostname as prefix to each output line" >&2
	echo "           this can make script processing of output easier" >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
#	echo "   SWITCHES - list of switches, used if -C used and -H and -F options not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
#	echo "   SWITCHES_FILE - file containing list of switches, used in absence of -F and -H" >&2
	echo "   FF_MAX_PARALLEL - when -p option is used, maximum concurrent operations" >&2
	echo "   FF_SERIALIZE_OUTPUT - serialize output of parallel operations (yes or no)" >&2
#	echo "   FF_SWITCH_LOGIN_METHOD - how to login to switch: telnet or ssh" >&2
#	echo "   FF_SWITCH_ADMIN_PASSWORD - password for switch, used in absence of -S" >&2
	echo "for example:" >&2
	echo "  Operations on hosts" >&2
	echo "   $BASENAME date" >&2
	echo "   $BASENAME 'uname -a'" >&2
	echo "   $BASENAME -h 'elrond arwen' date" >&2
	echo "   HOSTS='elrond arwen' $BASENAME date" >&2
#	echo "  Operations on switches" >&2
#	echo "   $BASENAME -C 'ismPortStats -noprompt'" >&2
#	echo "   $BASENAME -C -H 'switch1 switch2' 'ismPortStats -noprompt'" >&2
#	echo "   SWITCHES='switch1 switch2' $BASENAME -C 'ismPortStats -noprompt'" >&2
	exit 0
}

Usage()
{
#	echo "Usage: $BASENAME [-Cpq] [-f hostfile] [-F switchesfile] [-u user] [-S]" >&2
#	echo "              [-T timelimit] [-P] 'cmd'" >&2
	echo "Usage: $BASENAME [-pq] [-f hostfile] [-u user] [-T timelimit] [-P] 'cmd'" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
#	echo "   -C - perform command against switches, default is hosts" >&2
#	echo "   -p - run command in parallel on all hosts/switches" >&2
	echo "   -p - run command in parallel on all hosts" >&2
	echo "   -q - quiet mode, don't show command to execute" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
#	echo "   -F switchesfile - file with switches in cluster" >&2
#	echo "           default is $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
	echo "   -u user - user to perform cmd as" >&2
	echo "           for hosts default is current user code" >&2
#	echo "           for switches default is admin, this is ignored" >&2
#	echo "   -S - securely prompt for password for user on switches" >&2
	echo "   -T timelimit - timelimit in seconds when running host commands" >&2
	echo "           default is -1 (infinite)" >&2
#	echo "   -P - output hostname/switch name as prefix to each output line" >&2
	echo "   -P - output hostname as prefix to each output line" >&2
	echo "           this can make script processing of output easier" >&2
	echo "for example:" >&2
	echo "  Operations on hosts" >&2
	echo "   $BASENAME date" >&2
	echo "   $BASENAME 'uname -a'" >&2
#	echo "  Operations on switches" >&2
#	echo "   $BASENAME -C 'ismPortStats -noprompt'" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

user=`id -u -n`
uopt=n
quiet=0
host=0
switches=0
parallel=0
Sopt=n
marker=""
timelimit="-1"
output_prefix=0
#while getopts Cqph:H:f:F:u:Sm:T:P param
while getopts qph:f:u:m:T:P param
do
	case $param in
	C)
		switches=1;;
	q)
		quiet=1;;
	p)
		parallel=1;;
	h)
		host=1
		HOSTS="$OPTARG";;
	H)
		switches=1
		SWITCHES="$OPTARG";;
	f)
		host=1
		HOSTS_FILE="$OPTARG";;
	F)
		switches=1
		SWITCHES_FILE="$OPTARG";;
	u)
		uopt=y
		user="$OPTARG";;
	S)
		Sopt=y;;
	m)
		marker="$OPTARG";;
	T)
		timelimit="$OPTARG";;
	P)
		output_prefix=1;;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))

if [ $# -ne 1 ]
then
	Usage
fi
if [[ $(($switches+$host)) -gt 1 ]]
then
	echo "$BASENAME: conflicting arguments, hosts and switches both specified" >&2
	Usage
fi
if [[ $(($switches+$host)) -eq 0 ]]
then
	host=1
fi
if [ x"$marker" != "x" -a $switches -eq 0 ]
then
	echo "$BASENAME: -m option only applicable to switches, ignored" >&2
fi
if [ "$timelimit" -le 0 ]
then
	timelimit=0	# some old versions of expect mistakenly treat -1 as a option
fi

export TEST_MAX_PARALLEL="$FF_MAX_PARALLEL"
export TEST_TIMEOUT_MULT="$FF_TIMEOUT_MULT"
export CFG_LOGIN_METHOD="$FF_LOGIN_METHOD"
export CFG_PASSWORD="$FF_PASSWORD"
export CFG_ROOTPASS="$FF_ROOTPASS"
export CFG_SWITCH_LOGIN_METHOD="$FF_SWITCH_LOGIN_METHOD"
export CFG_SWITCH_ADMIN_PASSWORD="$FF_SWITCH_ADMIN_PASSWORD"
export TEST_SERIALIZE_OUTPUT="$FF_SERIALIZE_OUTPUT"

if [ $switches -eq 0 ]
then
	check_host_args $BASENAME
	if [ $parallel -eq 0 ]
	then
		tclproc=hosts_run_cmd
	else
		tclproc=hosts_parallel_run_cmd
	fi
	$TOOLSDIR/tcl_proc $tclproc "$HOSTS" "$user" "$1" $quiet $timelimit $output_prefix
else
	if [ "$uopt" = n ]
	then
		user=admin
	fi
	check_switches_args $BASENAME
	if [ "$Sopt" = y ]
	then
		echo -n "Password for $user on all switches: " > /dev/tty
		stty -echo < /dev/tty > /dev/tty
		password=
		read password < /dev/tty
		stty echo < /dev/tty > /dev/tty
		echo > /dev/tty
		export CFG_SWITCH_ADMIN_PASSWORD="$password"
	fi
	# pardon my spelling need plural chassis that is distinct from singular
	if [ $parallel -eq 0 ]
	then
		tclproc=chassises_run_cmd
	else
		tclproc=chassises_parallel_run_cmd
	fi
	$TOOLSDIR/tcl_proc $tclproc "$SWITCHES" "$user" "$1" $quiet "$marker" "$output_prefix"
fi
