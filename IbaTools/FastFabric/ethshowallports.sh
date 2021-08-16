#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015-2017, Intel Corporation
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
readonly TL_DIR=/usr/lib/eth-tools

Usage_full()
{
	echo "Usage: $BASENAME [-f hostfile] [-h 'hosts']" >&2
#	echo "Usage: $BASENAME [-C] [-f hostfile] [-F switchesfile]" >&2
#	echo "                    [-h 'hosts'] [-H 'switches'] [-S]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
#	echo "   -C - perform operation against switches, default is hosts" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
#	echo "   -F switchesfile - file with switches in cluster" >&2
#	echo "           default is $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
	echo "   -h hosts - list of hosts to show ports for" >&2
#	echo "   -H switches - list of switches to show ports for" >&2
#	echo "   -S - securely prompt for password for admin on switches" >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
#	echo "   SWITCHES - list of switches, used if -H option not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
#	echo "   SWITCHES_FILE - file containing list of switches, used in absence of -F and -H" >&2
#	echo "   FF_SWITCH_LOGIN_METHOD - how to login to switch: telnet or ssh" >&2
#	echo "   FF_SWITCH_ADMIN_PASSWORD - admin password for switch, used in absence of -S" >&2
	echo "example:">&2
	echo "   $BASENAME" >&2
	echo "   $BASENAME -h 'elrond arwen'" >&2
	echo "   HOSTS='elrond arwen' $BASENAME" >&2
#	echo "   $BASENAME -C" >&2
#	echo "   $BASENAME -H 'switch1 switch2'" >&2
#	echo "   SWITCHES='switch1 switch2' $BASENAME -C" >&2
	exit 0
}


Usage()
{
	echo "Usage: $BASENAME [-f hostfile]" >&2
#	echo "Usage: $BASENAME [-C] [-f hostfile] [-F switchesfile] [-S]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
#	echo "   -C - perform operation against switches, default is hosts" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
#	echo "   -F switchesfile - file with switches in cluster" >&2
#	echo "           default is $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
#	echo "   -S - securely prompt for password for admin on switches" >&2
	echo "example:">&2
	echo "   $BASENAME" >&2
#	echo "   $BASENAME -C" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

host=0
switches=0
Sopt=n
while getopts f:h: param
#while getopts Cf:F:h:H:S param
do
	case $param in
#	C)
#		switches=1;;
	f)
		host=1
		HOSTS_FILE="$OPTARG";;
#	F)
#		switches=1
#		SWITCHES_FILE="$OPTARG";;
	h)
		host=1
		HOSTS="$OPTARG";;
#	H)
#		switches=1
#		SWITCHES="$OPTARG";;
#	S)
#		Sopt=y;;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [[ $# -gt 0 ]]
then
	Usage
fi
if [[ $(($switches+$host)) -gt 1 ]]
then
	echo "$BASENAME: conflicting arguments, both hosts and switches specified" >&2
	Usage
fi
if [[ $(($switches+$host)) -eq 0 ]]
then
	host=1
fi

if [ "$switches" -eq 0 ]
then

	check_host_args $BASENAME
	driver="ice"
	for hostname in $HOSTS
	do
		echo "--------------------------------------------------------------------"
		echo "$hostname:"
		cmd="
		    devs=\"$(ls -l /sys/class/net/*/device/driver | grep "$driver$" | awk "{print \$9}" | cut -d '/' -f5)\"
		    for dev in \$devs; do
		    	ethtool \$dev
		    	echo \"Statistics for \$dev:\"
		    	ethtool -S \$dev | grep --color=never '^\s\+[tr]x_\(bytes\|errors\|dropped\):'
		    done
		    "
		$TL_DIR/tcl_proc hosts_run_cmd "$hostname" "root" "$cmd" 1
	done
else

	check_switches_args $BASENAME
	export CFG_SWITCH_LOGIN_METHOD=$FF_SWITCH_LOGIN_METHOD
	export CFG_SWITCH_ADMIN_PASSWORD=$FF_SWITCH_ADMIN_PASSWORD
	if [ "$Sopt" = y ]
	then
		echo -n "Password for admin on all switches: " > /dev/tty
		stty -echo < /dev/tty > /dev/tty
		password=
		read password < /dev/tty
		stty echo < /dev/tty > /dev/tty
		echo > /dev/tty
		export CFG_SWITCH_ADMIN_PASSWORD="$password"
	fi
	for switch in $SWITCHES
	do
		switch=`strip_chassis_slots "$switch"`
		echo "--------------------------------------------------------------------"
		echo "$switch:"
		$TL_DIR/tcl_proc chassises_run_cmd "$switch" "admin" 'ismPortStats -noprompt' 1 2>&1|egrep 'FAIL|Port State|Link Qual|Link Width|Link Speed|^[[:space:]]|^Name' | egrep -v 'Tx|Rx'
	done
fi
