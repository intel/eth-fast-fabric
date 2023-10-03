#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2020-2023, Intel Corporation
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

# optional override of defaults
if [ -f /etc/eth-tools/ethfastfabric.conf ]
then
	. /etc/eth-tools/ethfastfabric.conf
fi

. /usr/lib/eth-tools/ethfastfabric.conf.def

. /usr/lib/eth-tools/ff_funcs

trap "exit 1" SIGHUP SIGTERM SIGINT

readonly BASENAME="$(basename $0)"

Usage_full()
{
	echo "Usage: $BASENAME [-Cp] [-f hostfile] [-F switchesfile] [-h 'hosts']" >&2
	echo "                     [-H 'switches']" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   -C - Performs a ping against switches. Default is hosts." >&2
	echo "   -p - Pings all hosts/switches in parallel." >&2
	echo "   -f hostfile - Specifies the file with hosts in cluster. Default is " >&2
	echo "        $CONFIG_DIR/$FF_PRD_NAME/hosts." >&2
	echo "   -F switchesfile - Specifies the file with switches in cluster. Default is " >&2
	echo "        $CONFIG_DIR/$FF_PRD_NAME/switches." >&2
	echo "   -h hosts - Specifies the list of hosts to ping." >&2
	echo "   -H switches - Specifies the list of switches to ping." >&2
	echo " Environment:" >&2
	echo "   HOSTS - List of hosts, used if -h option not supplied." >&2
	echo "   SWITCHES - List of switches, used if -H option not supplied." >&2
	echo "   HOSTS_FILE - File containing list of hosts, used in absence of -f and -h." >&2
	echo "   SWITCHES_FILE - File containing list of switches, used in absence of -F and -H." >&2
	echo "   FF_MAX_PARALLEL - When -p option is used, maximum concurrent operations are performed." >&2
	echo "Examples:">&2
	echo "   $BASENAME" >&2
	echo "   $BASENAME -h 'arwen elrond'" >&2
	echo "   HOSTS='arwen elrond' $BASENAME" >&2
	echo "   $BASENAME -C" >&2
	echo "   $BASENAME -C -H 'switch1 switch2'" >&2
	echo "   SWITCHES='switch1 switch2' $BASENAME -C" >&2
	exit 0
}

Usage()
{
	echo "Usage: $BASENAME [-Cp] [-f hostfile] [-F switchesfile]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   -C - Performs a ping against switches. Default is hosts." >&2
	echo "   -p - Pings all hosts/switches in parallel." >&2
	echo "   -f hostfile - Specifies the file with hosts in cluster. Default is " >&2
	echo "        $CONFIG_DIR/$FF_PRD_NAME/hosts." >&2
	echo "   -F switchesfile - Specifies the file with switches in cluster. Default is " >&2
	echo "        $CONFIG_DIR/$FF_PRD_NAME/switches." >&2
	echo "Examples:">&2
	echo "   $BASENAME" >&2
	echo "   $BASENAME -C" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

ping_dest()
{
	# $1 is the destination to ping
	ping_host $1
	if [ $? != 0 ]
	then
		echo "$1: doesn't ping"
	else
		echo "$1: is alive"
	fi
}

popt=n
host=0
switches=0
while getopts Cf:F:h:H:p param
do
	case $param in
	C)
		switches=1;;
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
	p)
		popt=y;;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -gt 0 ]
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
if [ $switches -eq 0 ]
then
	check_host_args $BASENAME
	DESTS="$HOSTS"
else
	check_switches_args $BASENAME
	DESTS="$SWITCHES"
fi

running=0
for dest in $DESTS
do
	if [ $switches -ne 0 ]
	then
		dest=`strip_chassis_slots "$dest"`
	fi
	if [ "$popt" = "y" ]
	then
		if [ $running -ge $FF_MAX_PARALLEL ]
		then
			wait
			running=0
		fi
		ping_dest $dest &
		running=$(($running + 1))
	else
		ping_dest $dest
	fi
done
wait
