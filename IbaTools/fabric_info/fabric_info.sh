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
# This script provides a quick summary of fabric configuration
# it uses saquery to gather the information from the SM

if [ -f /usr/lib/eth-tools/ff_funcs ]
then
	# optional override of defaults
	if [ -f /etc/eth-tools/ethfastfabric.conf ]
	then
		. /etc/eth-tools/ethfastfabric.conf
	fi

	. /usr/lib/eth-tools/ethfastfabric.conf.def

	. /usr/lib/eth-tools/ff_funcs
fi

trap "exit 1" SIGHUP SIGTERM SIGINT

readonly BASENAME="$(basename $0)"

Usage_basic()
{
	echo "Usage: $BASENAME [-v] [-q] [-E file] [-p planes] [-f host_files] [-X snapshot_input]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -v - verbose output" >&2
	echo "   -q - disable progress reports" >&2
	echo "   -E file - Ethernet Mgt config file" >&2
	echo "            default is $CONFIG_DIR/$FF_PRD_NAME/mgt_config.xml" >&2
	echo "   -p planes - Fabric planes separated by space. Default is" >&2
	echo "            the first enabled plane defined in config file." >&2
	echo "            Value 'ALL' will use all enabled planes" >&2
	echo "   -f host_files - Hosts files separated by space. It overrides" >&2
	echo "            the HostsFiles defined in Mgt config file for the" >&2
	echo "            corresponding planes. Value 'DEFAULT' will use the" >&2
	echo "            HostFile defined in Mgt config file for the corresponding plane" >&2
	echo "   -X snapshot_input - generate report using data in snapshot_input" >&2
	echo "for example:" >&2
	echo "   $BASENAME" >&2
	echo "   $BASENAME -X snapshot.xml" >&2
	echo "   $BASENAME -p 'p1 p2' -f 'hosts1 DEFAULT'" >&2
}

Usage_ff()
{
	Usage_basic
	exit 2
}

print_split()
{
	echo "-------------------------------------------------------------------------------"
}

if [ x"$1" = "x--help" ]
then
	Usage_basic
	exit 0
fi

opts=""
mgt_file=$CONFIG_DIR/$FF_PRD_NAME/mgt_config.xml
planes=
snapshot=
hfiles=
while getopts vqE:p:f:X: param
do
	case $param in
	v) opts="$opts -v";;
	q) opts="$opts -q";;
	E) mgt_file="$OPTARG";;
	p) planes="$OPTARG";;
	f) hfiles="$OPTARG";;
	X) snapshot="$OPTARG";;
	?) Usage_ff;;
	esac
done

if [[ ! -e $mgt_file ]]; then
	echo "Couldn't find config file '$mgt_file'!"
	Usage_ff
fi

if [[ -n $snapshot && ! -e $snapshot ]]; then
	echo "Couldn't find snapshot file '$snapshot'!"
	Usage_ff
fi

if [[ -n $snapshot ]]; then
	if [[ -n $planes ]]; then
		echo "$BASENAME: -p ignored for -X"
	fi
	if [[ -n $hfiles ]]; then
		echo "$BASENAME: -f ignored for -X"
	fi
fi

status=ok
if [[ -n $snapshot ]]; then
	if ! ethreport $opts -E "$mgt_file" -X "$snapshot" -o fabricinfo; then
		status=bad
	fi
	print_split
elif [[ -z $planes ]]; then
	if ! ethreport $@ -o fabricinfo; then
		status=bad
	fi
	print_split
else
	if [[ "$planes" = "ALL" ]]; then
		planes="$(/usr/sbin/ethxmlextract -H -e Plane.Name -X "$mgt_file")"
	fi
	if [[ -z $hfiles ]]; then
		for plane in $planes; do
			echo "Fabric Plane $plane Information:"
			if ! ethreport $opts -E "$mgt_file" -p "$plane" -o fabricinfo; then
				status=bad
			fi
			print_split
		done
	else
		a_planes=($planes)
		a_hfiles=($hfiles)
		if [[ ${#a_planes[@]} -ne ${#a_hfiles[@]} ]]; then
			echo "Error: Number of hosts files (${#a_hfiles[@]}) doesn't match number of planes (${#a_planes[@]})!" >&2
			exit 1
		fi
		for i in "${!a_planes[@]}"; do
			echo "Fabric Plane ${a_planes[i]} Information:"
			if ! ethreport $opts -E "$mgt_file" -p "${a_planes[i]}" -f "${a_hfiles[i]}" -o fabricinfo; then
				status=bad
			fi
			print_split
		done
	fi
fi

if [ "$status" != "ok" ]; then
	exit 1
else
	exit 0
fi