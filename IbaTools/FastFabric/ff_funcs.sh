#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2020, Intel Corporation
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

# This is a bash sourced file for support functions

FF_PRD_NAME=eth-tools

# A map between lower case node name and its ports. Please do not directly
# use this map because you need to ensure your node name is in lower case.
# Instead, suggest use function get_node_ports to get the ports for a node,
# or function get_nodes_ports to get nodes associated with ports info.
declare -A LC_NODE_PORTS

if [ "$CONFIG_DIR" = "" ]
then
	CONFIG_DIR=/etc
	export CONFIG_DIR
fi

resolve_file()
{
	# $1 is command name
	# $2 is file name
	# outputs full path to file, or exits with Usage if not found
	if [ -f "$2" ]
	then
		echo "$2"
	elif [ -f "$CONFIG_DIR/$FF_PRD_NAME/$2" ]
	then
		echo "$CONFIG_DIR/$FF_PRD_NAME/$2"
	else
		echo "$1: $2 not found" >&2
	fi
}

# filter out blank and comment lines
ff_filter_comments()
{
	egrep -v '^[[:space:]]*#'|egrep -v '^[[:space:]]*$'
}

expand_file()
{
	# $1 is command name
	# $2 is file name
	# outputs list of all non-comment lines in file
	# expands any included files
	local file

	# tabs and spaces are permitted as field splitters for include lines
	# however spaces are permitted in other lines (to handle node descriptions)
	# and tabs must be used for any comments on non-include lines
	# any line whose 1st non-whitespace character is # is a comment
	cat "$2"|ff_filter_comments|while read line
	do
		f1=$(expr "$line" : '\([^ 	]*\).*')
		if [ x"$f1" = x"include" ]
		then
			f2=$(expr "$line" : "[^ 	]*[ 	][ 	]*\([^ 	]*\).*")
			file=`resolve_file "$1" "$f2"`
			if [ "$file" != "" ]
			then
				expand_file "$1" "$file"
			fi
		else
			echo "$line"|cut -f1
		fi
	done
}

extract_device_name()
{
	# $1 is command name
	# $2 is device list content
	# outputs list of device names. Take any string before the delimiter
	#         ':', ',', '[', '(', '{'
	echo "$2" | while read line
	do
		echo "$line" | awk -F '[:,[({]' '{print $1}'
	done
}

trim_string()
{
	str=$1
	echo "$str" | sed -e "s/^[[:space:]]*//" -e "s/[[:space:]]*$//"
}

# Given a list of nodes, return nodes associated with ports in format
#   <node>:<port1,port2...>
# If no ports, the returned line will be node name only, i.e. <node>
get_nodes_ports()
{
	nodes=$1
	for node in $nodes
	do
		ports="${LC_NODE_PORTS[${node,,}]}"
		if [[ -z $ports ]]; then
			echo "$node"
		else
			echo "$node:${ports// /,}"
		fi
	done
}

# Find the ports for a given node. Return empty string if not found or
# no ports info
get_node_ports()
{
	node=$1
	echo "${LC_NODE_PORTS[${node,,}]}"
}

extract_node_ports()
{
	content=$1
	for line in $content
	do
		raw_node="${line%%:*}"
		node="$(trim_string $raw_node)"
		if [[ "$raw_node" = "$line" ]]; then
			LC_NODE_PORTS[${node,,}]=""
		else
			ports="$(echo "${line#*:}" | sed -e 's/,/ /g' -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
			ports="$(trim_string "$ports")"
			LC_NODE_PORTS[${node,,}]="$ports"
		fi
	done
}

check_host_args()
{
	# $1 is command name
	# uses $HOSTS and $HOSTS_FILE
	# sets $HOSTS or calls Usage which should exit
	local l_hosts_file

	if [ "$HOSTS_FILE" = "" ]
	then
		HOSTS_FILE=$CONFIG_DIR/$FF_PRD_NAME/hosts
	fi
	if [ "$HOSTS" = "" ]
	then
		l_hosts_file=$HOSTS_FILE
		HOSTS_FILE=`resolve_file "$1" "$HOSTS_FILE"`
		if [ "$HOSTS_FILE" = "" ]
		then
			echo "$1: HOSTS env variable is empty and the file $l_hosts_file does not exist" >&2 
			echo "$1: Must export HOSTS or HOSTS_FILE or use -f or -h option" >&2
			Usage
		fi
		CONTENTS=`expand_file "$1" "$HOSTS_FILE"`
		HOSTS=`extract_device_name "$1" "$CONTENTS"`
		if [ "$HOSTS" = "" ]
		then
			echo "$1: HOSTS env variable and the file $HOSTS_FILE are both empty" >&2
			echo "$1: Must export HOSTS or HOSTS_FILE or use -f or -h option" >&2
			Usage
		fi
	else
		CONTENTS="$HOSTS"
	fi
	extract_node_ports "$CONTENTS"
}

check_switches_args()
{
	# $1 is command name
	# uses $SWITCHES and $SWITCHES_FILE
	# sets $SWITCHES or calls Usage which should exit
	local l_switches_file

	if [ "$SWITCHES_FILE" = "" ]
	then
		SWITCHES_FILE=$CONFIG_DIR/$FF_PRD_NAME/switches
	fi
	if [ "$SWITCHES" = "" ]
	then
		l_switches_file=$SWITCHES_FILE
		SWITCHES_FILE=`resolve_file "$1" "$SWITCHES_FILE"`
		if [ "$SWITCHES_FILE" = "" ]
		then
			echo "$1: SWITCHES env variable is empty and the file $l_switches_file does not exist" >&2
			echo "$1: Must export SWITCHES or SWITCHES_FILE or use -F or -H option" >&2
			Usage
		fi
		CONTENTS=`expand_file "$1" "$SWITCHES_FILE"`
		SWITCHES=`extract_device_name "$1" "$CONTENTS"`
		if [ "$SWITCHES" = "" ]
		then
			echo "$1: SWITCHES env variable and the file $SWITCHES_FILE are both empty" >&2
			echo "$1: Must export SWITCHES or SWITCHES_FILE or use -F or -H option" >&2
			Usage
		fi
	fi
	
	export CFG_SWITCHES_LOGIN_METHOD=$FF_SWITCH_LOGIN_METHOD
	export CFG_SWITCHES_ADMIN_PASSWORD=$FF_SWITCH_ADMIN_PASSWORD
}

check_ports_args()
{
	# $1 is command name
	# uses $PORTS and $PORTS_FILE
	# sets $PORTS or calls Usage which should exit
	local have_file_name

	if [ "$PORTS_FILE" = "" ]
	then
		PORTS_FILE=$CONFIG_DIR/$FF_PRD_NAME/ports
	fi
	if [ "$PORTS" = "" ]
	then
		# allow case where PORTS_FILE is not found (ignore stderr)
		if [ "$PORTS_FILE" != "$CONFIG_DIR/$FF_PRD_NAME/ports" ]
		then
			PORTS_FILE=`resolve_file "$1" "$PORTS_FILE"`
			have_file_name=1
		else
			# quietly hide a missing ports file
			PORTS_FILE=`resolve_file "$1" "$PORTS_FILE" 2>/dev/null`
			have_file_name=0
		fi
		if [ "$PORTS_FILE" = "" ]
		then
			if [ "$have_file_name" = 1 ]
			then
				Usage
			fi
		else
			PORTS=`expand_file "$1" "$PORTS_FILE"`
		fi
	fi
	if [ "$PORTS" = "" ]
	then
		PORTS="0:0"	# default to 1st active port
		#echo "$1: Must export PORTS or PORTS_FILE or use -l or -p option" >&2
		#Usage
	fi
	
}

strip_chassis_slots()
{
	# removes any slot numbers and returns chassis network name
	case "$1" in
	*\[*\]*:*) # [chassis]:slot format
			#echo "$1"|awk -F \[ '{print $2}'|awk -F \] '{print $1}'
			echo "$1"|sed -e 's/.*\[//' -e 's/\].*//'
			;;
	*\[*\]) # [chassis] format
			#echo "$1"|awk -F \[ '{print $2}'|awk -F \] '{print $1}'
			echo "$1"|sed -e 's/.*\[//' -e 's/\].*//'
			;;
	*:*:*) # ipv6 without [] nor slot
			echo "$1"
			;;
	*:*) # chassis:slot format
			echo "$1"|cut -f1 -d:
			;;
	*) # chassis without [] nor slot
			echo "$1"
			;;
	esac
}

strip_switch_name()
{
	# $1 is a switches entry
	# removes any node name and returns node GUID
	echo "$1"|cut -f1 -d,
}

ping_host()
{
	#$1 is the destination to ping
	#return 1 if dest doesn't respond: unknown host or unreachable

	if type /usr/lib/$FF_PRD_NAME/ethgetipaddrtype >/dev/null 2>&1
	then
		iptype=`/usr/lib/$FF_PRD_NAME/ethgetipaddrtype $1 2>/dev/null`
		if [ x"$iptype" = x ]
		then
			iptype='ipv4'
		fi
	else
		iptype="ipv4"
	fi
	if [ "$iptype" == "ipv4" ]
	then
		ping -c 2 -w 4 $1 >/dev/null 2>&1
	else
		ping6 -c 2 -w 4 $1 >/dev/null 2>&1
	fi
	return $?
}


# convert the supplied $1 list into a one line per entry style output
# this is useful to take a parsed input like "$HOSTS" and convert it
# to a pipeline for use in some of the functions below or other
# basic shell commands which use stdin
ff_var_to_stdout()
{
	# translate spaces to newlines and get rid of any blank lines caused by
	# extra spaces
	echo "$1"|tr -s ' ' '\n'|sed -e '/^$/d'
}

# take the list on stdin and convert to lower case
ff_to_lc()
{
	tr A-Z a-z
}

# take the list on stdin and convert to lowercase,
# sort alphabetically filtering any dups
# assumed the list is a set of TCP/IP names which are hence case insensitive
ff_filter_dups()
{
	ff_to_lc|sort -u
}

# convert the supplied $1 list into a one line per entry style output
# and convert to lowercase, filter dups and alphabetically sort
ff_var_filter_dups_to_stdout()
{
	ff_var_to_stdout "$1"|ff_filter_dups
}

# find all interfaces that have the given driver
get_ifs_by_driver()
{
	driver=$1
	ls -l /sys/class/net/*/device/driver | grep "${driver}$" | awk '{print $9}' | cut -d '/' -f5
}

# find the corresponding irdma device based on given eth interface
# return empty if not found
eth_to_irdma()
{
	node=$1
	slot=$(ls -l /sys/class/net | grep $dev | awk '{print $11}' | cut -d '/' -f 6)
	if [[ -n $slot ]]; then
		ls $(find /sys/devices/ -name $slot)/infiniband 2> /dev/null
	fi
}

get_ip_by_if()
{
	dec=$1
	ip a show dev $dev | grep 'inet ' | awk '{print $2}' | cut -d '/' -f1
}

# get inet4 IP address of the interfaces that have given driver on all fabric hosts
# return IP addresses in format <hostname>:<ifname>:<ip>
get_all_ips()
{
	driver=$1
	cmd="
		host=\$(hostname -s)
		for dev in \$(ls -l /sys/class/net/*/device/driver | grep "${driver}\$" | awk '{print \$9}' | cut -d '/' -f5)
		do
			echo \$host:\$dev:\$(ip a show dev \$dev | grep 'inet ' | awk '{print \$2}' | cut -d'/' -f1)
		done
	"
	ethcmdall -h "$HOSTS" -q -p "$cmd"
}