#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015-2023, Intel Corporation
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
# Analyze fabric and switches for errors and/or changes relative to baseline

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
	echo "Usage: ${BASENAME} [-b|-e] [-s] [-d dir] [-c file] [-T topology_inputs]" >&2
	echo "                    [-E file] [-p planes] [-f host_files]">&2
#	echo "                    [-F switchesfile] [-H 'switches']">&2
	echo "              or" >&2
	echo "       ${BASENAME} --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   -b - Sets the baseline mode. Default is compare/check mode." >&2
	echo "   -e - Evaluates health only. Default is compare/check mode." >&2
	echo "   -s - Saves the history of failures (errors/differences)." >&2
	echo "   -d dir - Identifies the top-level directory for saving baseline and" >&2
	echo "            history of failed checks. Default is /var/usr/lib/eth-tools/analysis" >&2
	echo "   -c file - Specifies the error thresholds configuration file. Default is" >&2
	echo "            $CONFIG_DIR/$FF_PRD_NAME/ethmon.conf." >&2
	echo "   -E file - Ethernet Mgt configuration file. The default is" >&2
	echo "            $CONFIG_DIR/$FF_PRD_NAME/mgt_config.xml" >&2
	echo "   -p planes - Fabric planes separated by space. The default is the first" >&2
	echo "            enabled plane defined in config file. Value 'ALL' will use all" >&2
	echo "            enabled planes." >&2
	echo "   -f host_files - Hosts files separated by space. It overrides the HostsFiles" >&2
	echo "            defined in Mgt config file for the corresponding planes. Value" >&2
	echo "            'DEFAULT' will use the HostFile defined in Mgt config file for" >&2
	echo "            the corresponding plane." >&2
	echo "   -T topology_inputs - Specifies the name of topology input filenames separated by" >&2
	echo "            space. See ethreport for more information on topology_input files." >&2
#	echo "   -F switchesfile - file with switches in cluster" >&2
#	echo "           default is $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
#	echo "   -H switches - list of switches to analyze" >&2
	echo " Environment:" >&2
#	echo "   SWITCHES - list of switches, used if -F and -H options not supplied" >&2
#	echo "   SWITCHES_FILE - file containing list of switches, used if -F and -H options not" >&2
#	echo "                  supplied" >&2
	echo "   FF_ANALYSIS_DIR - Top-level directory for baselines and failed health checks." >&2
#	echo "   FF_SWITCH_CMDS - list of commands to issue during analysis," >&2
#	echo "                     unused if -e option supplied" >&2
#	echo "   FF_SWITCH_HEALTH - single command to issue to check overall health during analysis," >&2
#	echo "                       unused if -b option supplied" >&2
	echo "for example:" >&2
	echo "   ${BASENAME}" >&2
	echo "   ${BASENAME} -p 'p1 p2' -f 'hosts1 DEFAULT'" >&2
	exit 0
}

Usage()
{
#	echo "Usage: ${BASENAME} [-b|-e] [-s] [-F switchesfile]" >&2
	echo "Usage: ${BASENAME} [-b|-e] [-s]" >&2
	echo "              or" >&2
	echo "       ${BASENAME} --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   -b - Sets the baseline mode. Default is compare/check mode." >&2
	echo "   -e - Evaluates health only. Default is compare/check mode." >&2
	echo "   -s - Saves the history of failures (errors/differences)." >&2
#	echo "   -F switchesfile - file with switches in cluster" >&2
#	echo "           default is $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
	echo "for example:" >&2
	echo "   ${BASENAME}" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

getbaseline=n
healthonly=n
savehistory=n
opts=""
configfile=$CONFIG_DIR/$FF_PRD_NAME/ethmon.conf
status=ok
#while getopts besd:c:p:t:T:H:F: param
while getopts besd:c:E:p:f:T: param
do
	case $param in
	b)	opts="$opts -b"; getbaseline=y;;
	e)	opts="$opts -e"; healthonly=y;;
	s)	opts="$opts -s"; savehistory=y;;
	d)	export FF_ANALYSIS_DIR="$OPTARG";;
	c)	configfile="$OPTARG";;
	E)	opts="$opts -E '$OPTARG'";;
	p)	opts="$opts -p '$OPTARG'";;
	f)	opts="$opts -f '$OPTARG'";;
	T)	opts="$opts -T '$OPTARG'";;
#	H)	export SWITCHES="$OPTARG";;
#	F)	export SWITCHES_FILE="$OPTARG";;
	?)	Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -ge 1 ]
then
	Usage
fi
if [ "$getbaseline" = y -a "$healthonly" = y ]
then
	Usage
fi


# first verify all arguments
for i in $FF_ALL_ANALYSIS
do
	case $i in
	fabric)
		;;
	switches)
		check_switches_args ${BASENAME};;
	*)
		echo "${BASENAME}: Invalid setting in FF_ALL_ANALYSIS: $i" >&2
		exit 1;;
	esac
done
	
export FF_CURTIME="${FF_CURTIME:-`date +%Y-%m-%d-%H:%M:%S`}"

#-----------------------------------------------------------------
for i in $FF_ALL_ANALYSIS
do
	case $i in
	fabric)
		eval /usr/sbin/ethfabricanalysis $opts -c "$configfile"
		if [ $? != 0 ]
		then
			status=bad
		fi
		;;
	switches)
		eval /usr/sbin/opachassisanalysis $opts
		if [ $? != 0 ]
		then
			status=bad
		fi
		;;
	esac
done

if [ "$status" != "ok" ]
then
	if [[ $healthonly == n ]]
	then
		echo "${BASENAME}: Possible errors or changes found" >&2
	else
		echo "${BASENAME}: Possible errors found" >&2
	fi
	exit 1
else
	if [[ $getbaseline == n  ]]
	then
		echo "${BASENAME}: All OK"
	else
		echo "${BASENAME}: Baselined"
	fi
	exit 0
fi
