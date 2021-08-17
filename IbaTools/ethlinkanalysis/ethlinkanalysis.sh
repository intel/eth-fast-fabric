#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015, Intel Corporation
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

# analyzes all the links in the fabric

# optional override of defaults
if [ -f /etc/eth-tools/ethfastfabric.conf ]
then
	. /etc/eth-tools/ethfastfabric.conf
fi

. /usr/lib/eth-tools/ethfastfabric.conf.def

. /usr/lib/eth-tools/ff_funcs

tempfile="$(mktemp)"
trap "rm -f $tempfile; exit 1" SIGHUP SIGTERM SIGINT
trap "rm -f $tempfile" EXIT

punchlist=$FF_RESULT_DIR/punchlist.csv
del=';'
timestamp=$(date +"%Y/%m/%d %T")
readonly BASENAME="$(basename $0)"
ETHREPORT="/usr/sbin/ethreport"
ETHXMLEXTRACT="/usr/sbin/ethxmlextract"

Usage_full()
{
	echo "Usage: $BASENAME [-U] [-T topology_inputs] [-X snapshot_input]" >&2
	echo "                  [-x snapshot_suffix] [-c file] [-E file] [-p planes] reports ..." >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -U - omit unexpected devices and links in punchlist from verify reports" >&2
	echo "   -T topology_inputs - name of topology input filenames separated by space." >&2
	echo "             See ethreport for more information on topology_input files" >&2
	echo "   -X snapshot_input - perform analysis using data in snapshot_input" >&2
	echo "             snapshot_input must have been generated via a previous" >&2
	echo "             ethreport -o snapshot run." >&2
	echo "   -x snapshot_suffix - create a snapshot file per selected plane" >&2
	echo "             The files will be created in FF_RESULT_DIR with names of the form:">&2
	echo "             snapshotSUFFIX.<plane_name>.xml.">&2
	echo "   -c file - error thresholds config file" >&2
	echo "             default is $CONFIG_DIR/$FF_PRD_NAME/ethmon.si.conf" >&2
	echo "   -E file - Ethernet Mgt config file" >&2
	echo "             default is $CONFIG_DIR/$FF_PRD_NAME/mgt_config.xml" >&2
	echo "   -p planes - Fabric planes separated by space. Default is" >&2
	echo "             the first enabled plane defined in config file." >&2
	echo "             Value 'ALL' will use all enabled planes." >&2
	echo "    reports - The following reports are supported" >&2
	echo "         errors - link error analysis" >&2
	echo "         slowlinks - links running slower than expected" >&2
	echo "         misconfiglinks - links configured to run slower than supported" >&2
	echo "         misconnlinks - links connected with mismatched speed potential" >&2
	echo "         all - includes all reports above" >&2
	echo "         verifylinks - verify links against topology input" >&2
	echo "         verifyextlinks - verify links against topology input" >&2
	echo "                     limit analysis to links external to systems" >&2
	echo "         verifyniclinks - verify links against topology input" >&2
	echo "                     limit analysis to NIC links" >&2
	echo "         verifyislinks - verify links against topology input" >&2
	echo "                     limit analysis to inter-switch links" >&2
	echo "         verifyextislinks - verify links against topology input" >&2
	echo "                     limit analysis to inter-switch links external to systems" >&2
	echo "         verifynics - verify NICs against topology input" >&2
	echo "         verifysws - verify Switches against topology input" >&2
	echo "         verifynodes - verify NICs, Switches against topology input" >&2
	echo "         verifyall - verify links, NICs, Switches against topology input" >&2
  	echo >&2
	echo "A punchlist of bad links is also appended to FF_RESULT_DIR/punchlist.csv" >&2
  	echo >&2
	echo "example:">&2
	echo "   $BASENAME errors" >&2
	echo "   $BASENAME slowlinks" >&2
	exit 0
}

Usage()
{
	echo "Usage: $BASENAME [-U] reports ..." >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
  	echo >&2
	echo "   -U - omit unexpected devices and links in punchlist from verify reports" >&2
	echo "    reports - The following reports are supported" >&2
	echo "         errors - link error analysis" >&2
	echo "         slowlinks - links running slower than expected" >&2
	echo "         misconfiglinks - links configured to run slower than supported" >&2
	echo "         misconnlinks - links connected with mismatched speed potential" >&2
	echo "         all - includes all reports above" >&2
	echo "         verifylinks - verify links against topology input" >&2
	echo "         verifyextlinks - verify links against topology input" >&2
	echo "                     limit analysis to links external to systems" >&2
	echo "         verifynics - verify NICs against topology input" >&2
	echo "         verifysws - verify Switches against topology input" >&2
	echo "         verifynodes - verify NICs, Switches against topology input" >&2
	echo "         verifyall - verify links, NICs, Switches against topology input" >&2
  	echo >&2
	echo "A punchlist of bad links is also appended to FF_RESULT_DIR/punchlist.csv" >&2
  	echo >&2
	echo "example:">&2
	echo "   $BASENAME errors" >&2
	echo "   $BASENAME slowlinks" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

append_punchlist()
# $1 = device
# $2 = issue
{
	echo "$timestamp$del$1$del$2" >> $punchlist
}

gen_errors_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	(
	# TBD - is cable information available?
	export IFS=';'
	port1=
	#$ETHREPORT "$@" -o errors -x | /usr/sbin/ethxmlextract -H -d \; -e LinkErrors.Link.Port.NodeGUID -e LinkErrors.Link.Port.PortNum -e LinkErrors.Link.Port.NodeType -e LinkErrors.Link.Port.NodeDesc|while read line
	eval $ETHREPORT "$@" -o errors -x | /usr/sbin/ethxmlextract -H -d \; -e LinkErrors.Link.Port.NodeDesc -e LinkErrors.Link.Port.PortNum -e LinkErrors.Link.Port.PortId|while read desc port portid
	do
		if [ x"$port1" = x ]
		then
			port1="$desc p$port $portid"
		else
			append_punchlist "$port1 $desc p$port $portid" "Link errors"
			port1=
		fi
	done
	)
}

gen_slowlinks_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	(
	# TBD - is cable information available?
	export IFS=';'
	port1=
	#$ETHREPORT "$@" -o slowlinks -x | /usr/sbin/ethxmlextract -H -d \; -e LinksExpected.Link.Port.NodeGUID -e LinksExpected.Link.Port.PortNum -e LinksExpected.Link.Port.NodeType -e LinksExpected.Link.Port.NodeDesc|while read line
	eval $ETHREPORT "$@" -o slowlinks -x | /usr/sbin/ethxmlextract -H -d \; -e LinksExpected.Link.Port.NodeDesc -e LinksExpected.Link.Port.PortNum -e LinksExpected.Link.Port.PortId|while read desc port portid
	do
		if [ x"$port1" = x ]
		then
			port1="$desc p$port $portid"
		else
			append_punchlist "$port1 $desc p$port $portid" "Link speed/width lower than expected"
			port1=
		fi
	done
	)
}

gen_misconfiglinks_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	(
	# TBD - is cable information available?
	export IFS=';'
	port1=
	#$ETHREPORT "$@" -o misconfiglinks -x | /usr/sbin/ethxmlextract -H -d \; -e LinksConfig.Link.Port.NodeGUID -e LinksConfig.Link.Port.PortNum -e LinksConfig.Link.Port.NodeType -e LinksConfig.Link.Port.NodeDesc|while read line
	eval $ETHREPORT "$@" -o misconfiglinks -x | /usr/sbin/ethxmlextract -H -d \; -e LinksConfig.Link.Port.NodeDesc -e LinksConfig.Link.Port.PortNum -e LinksConfig.Link.Port.PortId|while read desc port portid
	do
		if [ x"$port1" = x ]
		then
			port1="$desc p$port $portid"
		else
			append_punchlist "$port1 $desc p$port $portid" "Link speed/width configured lower than supported"
			port1=
		fi
	done
	)
}

gen_misconnlinks_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	(
	# TBD - is cable information available?
	export IFS=';'
	line1=
	#$ETHREPORT "$@" -o misconnlinks -x | /usr/sbin/ethxmlextract -H -d \; -e LinksMismatched.Link.Port.NodeGUID -e LinksMismatched.Link.Port.PortNum -e LinksMismatched.Link.Port.NodeType -e LinksMismatched.Link.Port.NodeDesc|while read line
	eval $ETHREPORT "$@" -o misconnlinks -x | /usr/sbin/ethxmlextract -H -d \; -e LinksMismatched.Link.Port.NodeDesc -e LinksMismatched.Link.Port.PortNum -e LinksMismatched.Link.Port.PortId|while read desc port portid
	do
		if [ x"$line1" = x ]
		then
			line1="$desc p$port $portid"
		else
			append_punchlist "$line1 $desc p$port $portid" "Link speed/width mismatch"
			line1=
		fi
	done
	)
}

append_verify_punchlist()
# $1 = device
# $2 = issue
{
	if [ $skip_unexpected = y ]
	then
		case "$2" in
		Unexpected*)	> /dev/null;;
		*) echo "$timestamp$del$1$del$2" >> $punchlist;;
		esac
	else
		echo "$timestamp$del$1$del$2" >> $punchlist
	fi

}

process_links_csv()
# stdin is a csv with all the bad links from a verify*links report
{
	(
	export IFS=';'
	port1=
	port2=
	foundPort=
	prob=
	while read desc port portid portprob linkprob
	do
		if [ x"$port1" = x ]
		then
			port1="$desc p$port $portid"
			prob="$portprob"
			if [ x"$prob" = x ]
			then
				prob=$linkprob	# unlikely to occur here
			fi
		elif [ x"$port2" = x ]
		then
			port2="$desc p$port $portid"
			if [ x"$prob" = x ]
			then
				prob=$portprob
			fi
			if [ x"$prob" = x ]
			then
				prob=$linkprob	# unlikely to occur here
			fi
		fi

		if [ x"$linkprob" != x ]
		then
			if [ x"$prob" = x ]
			then
				prob=$linkprob
			fi

			# more port information available
			if [ x"$desc" != x ]
			then 
				foundPort="$desc p$port"
			fi

			append_verify_punchlist "$port1 $port2" "$prob $foundPort"
			port1=
			port2=
			foundPort=
			prob=
		fi
	done
	)
}

gen_verifylinks_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	# TBD - is cable information available?
	eval $ETHREPORT "$@" -o verifylinks -x | /usr/sbin/ethxmlextract -H -d \; -e VerifyLinks.Link.Port.NodeDesc -e VerifyLinks.Link.Port.PortNum -e VerifyLinks.Link.Port.PortId -e VerifyLinks.Link.Port.Problem -e VerifyLinks.Link.Problem|process_links_csv
}

gen_verifyextlinks_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	# TBD - is cable information available?
	eval $ETHREPORT "$@" -o verifyextlinks -x | /usr/sbin/ethxmlextract -H -d \; -e VerifyExtLinks.Link.Port.NodeDesc -e VerifyExtLinks.Link.Port.PortNum -e VerifyExtLinks.Link.Port.PortId -e VerifyExtLinks.Link.Port.Problem -e VerifyExtLinks.Link.Problem|process_links_csv
}

gen_verifyniclinks_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	# TBD - is cable information available?
	eval $ETHREPORT "$@" -o verifyniclinks -x | /usr/sbin/ethxmlextract -H -d \; -e VerifyNICLinks.Link.Port.NodeDesc -e VerifyNICLinks.Link.Port.PortNum -e VerifyNICLinks.Link.Port.PortId -e VerifyNICLinks.Link.Port.Problem -e VerifyNICLinks.Link.Problem|process_links_csv
}

gen_verifyislinks_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	# TBD - is cable information available?
	eval $ETHREPORT "$@" -o verifyislinks -x | /usr/sbin/ethxmlextract -H -d \; -e VerifyISLinks.Link.Port.NodeDesc -e VerifyISLinks.Link.Port.PortNum -e VerifyISLinks.Link.Port.PortId -e VerifyISLinks.Link.Port.Problem -e VerifyISLinks.Link.Problem|process_links_csv
}

gen_verifyextislinks_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	# TBD - is cable information available?
	eval $ETHREPORT "$@" -o verifyextislinks -x | /usr/sbin/ethxmlextract -H -d \; -e VerifyExtISLinks.Link.Port.NodeDesc -e VerifyExtISLinks.Link.Port.PortNum -e VerifyExtISLinks.Link.Port.PortId -e VerifyExtISLinks.Link.Port.Problem -e VerifyExtISLinks.Link.Problem|process_links_csv
}

gen_verifynics_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	(
	export IFS=';'
	#eval $ETHREPORT "$@" -o verifynics -x | /usr/sbin/ethxmlextract -H -d \; -e VerifyNICs.Node.NodeGUID -e VerifyNICs.Node.Desc -e VerifyNICs.Node.Problem|while read line
	eval $ETHREPORT "$@" -o verifynics -x | /usr/sbin/ethxmlextract -H -d \; -e VerifyNICs.Node.NodeDesc -e VerifyNICs.Node.Problem |while read desc prob
	do
		append_verify_punchlist "$desc" "$prob"
	done
	)
}

gen_verifysws_punchlist()
# $@ =  snapshot, port and/or topology selection options for ethreport
{
	(
	export IFS=';'
	#eval $ETHREPORT "$@" -o verifysws -x | /usr/sbin/ethxmlextract -H -d \; -e VerifySWs.Node.NodeGUID -e VerifySWs.Node.Desc -e VerifySWs.Node.Problem|while read line
	eval $ETHREPORT "$@" -o verifysws -x | /usr/sbin/ethxmlextract -H -d \; -e VerifySWs.Node.NodeDesc -e VerifySWs.Node.Problem |while read desc prob
	do
		append_verify_punchlist "$desc" "$prob"
	done
	)
}

report_opts=""
verify_opts=""
errors=n
clearerrors=n
clearhwerrors=n
slowlinks=n
misconfiglinks=n
misconnlinks=n
verifylinks=n
verifyextlinks=n
verifyniclinks=n
verifyislinks=n
verifyextislinks=n
verifynics=n
verifysws=n
reports=""
read_snapshot=n
snapshot_input=
save_snapshot=n
snapshot_suffix=
skip_unexpected=n
config_file="$CONFIG_DIR/$FF_PRD_NAME/ethmon.si.conf"
mgt_file="$CONFIG_DIR/$FF_PRD_NAME/mgt_config.xml"
topology_inputs=
planes=
status=0

while getopts UT:X:x:c:E:p: param
do
	case $param in
	U)	skip_unexpected=y;;
	T)	topology_inputs="$OPTARG";;
	X)	read_snapshot=y; export snapshot_input="$OPTARG";;
	x)	save_snapshot=y; export snapshot_suffix="$OPTARG";;
	c)	config_file="$OPTARG";;
	E)	mgt_file="$OPTARG";;
	p)	planes="$OPTARG";;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -le 0 ]
then
	echo "$BASENAME: Error: must specify at least 1 report" >&2
	Usage
fi
while [ $# -gt 0 ]
do
	case "$1" in
	errors) errors=y;;
	slowlinks) slowlinks=y;;
	misconfiglinks) misconfiglinks=y;;
	misconnlinks) misconnlinks=y;;
	all) errors=y; slowlinks=y; misconfiglinks=y; misconnlinks=y;;
	verifylinks) verifylinks=y;;
	verifyextlinks) verifyextlinks=y;;
	verifyniclinks) verifyniclinks=y;;
	verifyislinks) verifyislinks=y;;
	verifyextislinks) verifyextislinks=y;;
	verifynics) verifynics=y;;
	verifysws) verifysws=y;;
	verifynodes)  verifynics=y; verifysws=y;;
	verifyall) verifylinks=y; verifynics=y; verifysws=y;;
	*)
		echo "$BASENAME: Invalid report: $1" >&2
		Usage;;
	esac
	shift
done

for report in errors slowlinks misconfiglinks misconnlinks verifylinks verifyextlinks verifyniclinks verifyislinks verifyextislinks verifynics verifysws
do
	yes=$(eval echo \$$report)
	if [ $yes = y ]
	then
		case $report in
		verify*)
			verify_opts="$verify_opts -o $report"
			reports="$reports $report";;
		*)
			report_opts="$report_opts -o $report"
			reports="$reports $report";;
		esac
	fi
done

snapshot_plane=
snapshot_top=
snapshopt_opts=
if [ $errors = y ]
then
	#snapshopt_opts="-s"
	report_opts="$report_opts -c '$config_file'"
fi

if [ $read_snapshot = y ]
then
	if [ $save_snapshot = y ]
	then
		echo "$BASENAME: -X and -x options are mutually exclusive" >&2
	fi
	if [[ -n $planes ]]
	then
		echo "$BASENAME: -p ignored for -X" >&2
	fi
	snapshot_plane="$($ETHXMLEXTRACT -H -e Snapshot:plane* -X "$snapshot_input")"
	if [ -z "$snapshot_plane" ]
	then
		# shouldn't happen
		echo "$BASENAME: Error: No plane defined in snapshot file '$snapshot_input'" >&2
		exit 2
	fi
fi

top_count=0
first_top=
if [ -n "$topology_inputs" ]
then
	if [[ -n $planes && $read_snapshot = n ]]
	then
		echo "$BASENAME: -p ignored for -T" >&2
	fi
	planes=""
	for top_in in $topology_inputs
	do
		if [[ -z $first_top ]]
		then
			first_top="$top_in"
		fi
		plane_name="$($ETHXMLEXTRACT -H -e Report:plane* -X "$top_in")"
		if [ -z "$plane_name" ]
		then
			# shouldn't happen
			echo "$BASENAME: Error: No plane defined in topology file '$top_in'" >&2
			exit 2
		fi
		top_count=$((top_count + 1))
		if [[ "$snapshot_plane" = "$plane_name" ]]
		then
			snapshot_top="$top_in"
		fi
		planes="$planes$plane_name;$top_in "
	done
fi

available_planes="$($ETHXMLEXTRACT -H -e Plane.Name -e Plane.TopologyFile -X "$mgt_file")"
if [ -z "$available_planes" ]
then
	# shouldn't happen
	echo "$BASENAME: Error: No Fabric planes in config file '$mgt_file'" >&2
	exit 2
fi

if [[ $read_snapshot = y ]]
then
	planes="$snapshot_plane;$snapshot_top"
	if [[ -z $snapshot_top ]]
	then
		# try to use topology file in conf file
		available_plane="$(echo "$available_planes" | grep "^$snapshot_plane;" 2>/dev/null)"
		if [[ -n ${available_plane#*;} ]]
		then
			planes="$available_plane"
		elif [[ top_count -eq 1 ]]
		then
			# special case - no matched topology file on either user provided
			# topology files or conf file. if user only provides one topology file,
			# we will use it even it has mismatched plane name. This is the same
			# behavior as ethreport
			planes="$snapshot_plane;$first_top"
			echo "$BASENAME: Warning: use topology file $first_top that has mismatched plane name with snapshot $snapshot_input"
		else
			echo "$BASENAME: Warning: -T ignored because multiple topology files provided but none of them"
			echo "	matches the plane defined in snapshot $snapshot_input."
		fi
	fi
elif [[ -z $planes ]]
then
	# if no planes defined, use the first plane in conf file
	planes="$(echo "$available_planes" | head -n 1)"
elif [[ "$planes" = "ALL" ]]
then
	planes="$available_planes"
fi

do_analysis()
{
	plane_name="${1%;*}"
	top_file="${1#*;}"

	topt=""
	if [ "$top_file" != "" ]
	then
		topt="-T '$top_file'"
	fi

	if [ "$read_snapshot" = n ]
	then
		echo "Fabric Plane $plane_name Analysis:"
		if [ "$save_snapshot" = y ]
		then
			snapshot_input=$FF_RESULT_DIR/snapshot$snapshot_suffix.$plane_name.xml
		else
			snapshot_input=$tempfile
		fi
		# generate a snapshot per fabric then analyze
		ETHREPORT="/usr/sbin/ethreport -E $mgt_file"
		if [ x"$plane_name" != x ]
		then
			$ETHREPORT -p $plane_name $snapshopt_opts -o snapshot > $snapshot_input
		else
			$ETHREPORT $snapshopt_opts -o snapshot > $snapshot_input
		fi
		ETHREPORT="$ETHREPORT -q"
	fi

	# generate human readable reports
	if [ x"$report_opts" != x ]
	then
		eval $ETHREPORT -X $snapshot_input $topt $report_opts
	fi

	if [ x"$verify_opts" != x ]
	then
		if [ "$top_file" != "" ]
		then
			eval $ETHREPORT -X $snapshot_input $topt $verify_opts
		else
			echo "Unable to verify topology for plane $plane_name, no topology file found" >&2
			status=1
		fi
	fi

	# now generate punchlist
	for report in $reports
	do
		case "$report" in
		errors) gen_errors_punchlist -X $snapshot_input -c "$config_file";;
		slowlinks) gen_slowlinks_punchlist -X $snapshot_input;;
		misconfiglinks) gen_misconfiglinks_punchlist -X $snapshot_input;;
		misconnlinks) gen_misconnlinks_punchlist -X $snapshot_input;;
		verifylinks) [ "$top_file" != "" ] && gen_verifylinks_punchlist -X $snapshot_input $topt;;
		verifyextlinks) [ "$top_file" != "" ] && gen_verifyextlinks_punchlist -X $snapshot_input $topt;;
		verifyniclinks) [ "$top_file" != "" ] && gen_verifyniclinks_punchlist -X $snapshot_input $topt;;
		verifyislinks) [ "$top_file" != "" ] && gen_verifyislinks_punchlist -X $snapshot_input $topt;;
		verifyextislinks) [ "$top_file" != "" ] && gen_verifyextislinks_punchlist -X $snapshot_input $topt;;
		verifynics) [ "$top_file" != "" ] && gen_verifynics_punchlist -X $snapshot_input $topt;;
		verifysws) [ "$top_file" != "" ] && gen_verifysws_punchlist -X $snapshot_input $topt;;
		*) continue;;	# should not happen
		esac
	done
}

first_plane=1
for plane in $planes
do
	plane_count=$((plane_count + 1))
	if [[ $first_plane -eq 1 ]]
	then
		first_plane=0
	else
		echo ""
	fi
	id="${plane%;*}"
	available_plane="$(echo "$available_planes" | grep "^$id;" 2>/dev/null)"
	if [[ -z $available_plane && $read_snapshot = n ]]
	then
		echo "$BASENAME: Error: Couldn't find fabric plane '$id' in config file '$mgt_file'" >&2
		status=1
		continue
	fi
	if [[ "$id" = "$plane" ]]
	then
		# happens on -p used without -X/-T that the planes has no topology file info
		# so we use what is available in conf file
		do_analysis "$available_plane"
	else
		# happens on -X/-T that we already figured out topology file and put it in planes
		do_analysis "$plane"
	fi
done

rm -f $tempfile
exit $status
