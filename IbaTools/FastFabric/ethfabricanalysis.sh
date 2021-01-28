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
# Analyze fabric for errors and/or changes relative to baseline

# optional override of defaults
if [ -f /etc/eth-tools/ethfastfabric.conf ]
then
	. /etc/eth-tools/ethfastfabric.conf
fi

. /usr/lib/eth-tools/ethfastfabric.conf.def

. /usr/lib/eth-tools/ff_funcs

trap "exit 1" SIGHUP SIGTERM SIGINT

readonly BASENAME="$(basename $0)"
ETHREPORT="/usr/sbin/ethreport"
ETHXMLEXTRACT="/usr/sbin/ethxmlextract"

Usage_full()
{
	echo "Usage: $BASENAME [-b|-e] [-s] [-d dir] [-c file]" >&2
	echo "                   [-E file] [-p planes] [-T topology_inputs]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -b - baseline mode, default is compare/check mode" >&2
	echo "   -e - evaluate health only, default is compare/check mode" >&2
	echo "   -s - save history of failures (errors/differences)" >&2
	echo "   -d dir - top level directory for saving baseline and history of failed checks" >&2
	echo "            default is /var/usr/lib/eth-tools/analysis" >&2
	echo "   -c file - error thresholds config file" >&2
	echo "            default is $CONFIG_DIR/$FF_PRD_NAME/ethmon.conf" >&2
	echo "   -E file - Ethernet Mgt config file" >&2
	echo "            default is $CONFIG_DIR/$FF_PRD_NAME/mgt_config.xml" >&2
	echo "   -p planes - Fabric planes separated by space. Default is" >&2
	echo "            the first enabled plane defined in config file." >&2
	echo "            Value 'ALL' will use all enabled planes." >&2
	echo "   -T topology_inputs - name of topology input filenames separated by space." >&2
	echo "            See ethreport for more information on topology_input files" >&2
	echo " Environment:" >&2
	echo "   FF_ANALYSIS_DIR - top level directory for baselines and failed health checks" >&2
	echo "for example:" >&2
	echo "   $BASENAME" >&2
	exit 0
}

Usage()
{
	echo "Usage: $BASENAME [-b|-e] [-s]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -b - baseline mode, default is compare/check mode" >&2
	echo "   -e - evaluate health only, default is compare/check mode" >&2
	echo "   -s - save history of failures (errors/differences)" >&2
	echo "for example:" >&2
	echo "   $BASENAME" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

getbaseline=n
healthonly=n
savehistory=n
configfile=$CONFIG_DIR/$FF_PRD_NAME/ethmon.conf
mgt_file=$CONFIG_DIR/$FF_PRD_NAME/mgt_config.xml
planes=
topology_inputs=
status=ok
while getopts besd:c:E:p:T: param
do
	case $param in
	b)	getbaseline=y;;
	e)	healthonly=y;;
	s)	savehistory=y;;
	d)	export FF_ANALYSIS_DIR="$OPTARG";;
	c)	configfile="$OPTARG";;
	E)	mgt_file="$OPTARG";;
	p)	planes="$OPTARG";;
	T)	topology_inputs="$OPTARG";;
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

if [ -n "$topology_inputs" ]
then
	if [[ -n $planes ]]
	then
		echo "$BASENAME: -p ignored for -T" >&2
		planes=""
	fi
	for top_in in $topology_inputs
	do
		plane_name="$($ETHXMLEXTRACT -H -e Report:plane* -X "$top_in")"
		if [ -z "$plane_name" ]
		then
			# shouldn't happen
			echo "$BASENAME: Error: No plane defined in topology file '$top_in'" >&2
			exit 2
		fi
		planes="$planes$plane_name;$top_in "
	done
fi

# available_fids will be in format <plane_name>;<topology_file>, e.g
# plane1;topology.p1.xml
# plane2;topology.p2.xml
available_fids="$($ETHXMLEXTRACT -H -e Plane.Name -e Plane.TopologyFile -X "$mgt_file")"
if [ -z "$available_fids" ]
then
	# shouldn't happen
	echo "$BASENAME: Error: No Fabric planes in config file '$mgt_file'" >&2
	exit 2
fi

if [ -z "$planes" ]
then
	planes="$(echo "$available_fids" | head -n 1)"
elif [[ "$planes" = "ALL" ]]
then
	planes="$available_fids"
fi

#-----------------------------------------------------------------
# Set up file paths
#-----------------------------------------------------------------
baseline_dir="$FF_ANALYSIS_DIR/baseline"
latest_dir="$FF_ANALYSIS_DIR/latest"
export FF_CURTIME="${FF_CURTIME:-`date +%Y-%m-%d-%H:%M:%S`}"
failures_dir="$FF_ANALYSIS_DIR/$FF_CURTIME"

#-----------------------------------------------------------------
save_failures()
{
	if [ "$savehistory" = y ]
	then
		mkdir -p $failures_dir
		cp $* $failures_dir
		echo "$BASENAME: Failure information saved to: $failures_dir/" >&2
	fi
}

do_analysis()
{
	plane_name="${1%;*}"
	top_file="${1#*;}"
	baseline=$baseline_dir/fabric.$plane_name
	latest=$latest_dir/fabric.$plane_name

	topt=""
	if [ "$top_file" != "" ]
	then
		topt="-T $top_file"
	fi
	ETHREPORT="/usr/sbin/ethreport -E $mgt_file -p $plane_name"

	if [[ $getbaseline == n  && $healthonly == n ]]
	then
		if [ ! -f $baseline.links -o ! -f $baseline.comps ]
		then
			echo "$BASENAME: ${plane_name} Error: Previous baseline run required" >&2
			status=bad
			return
		fi
	fi

	if [[ $healthonly == n ]]
	then
		# get a new snapshot
		mkdir -p $latest_dir
		rm -rf $latest.*

		$ETHREPORT -o snapshot -q > $latest.snapshot.xml 2> $latest.snapshot.stderr
		if [ $? != 0 ]
		then
			echo "$BASENAME: ${plane_name} Error: Unable to access fabric. See $latest.snapshot.stderr" >&2
			status=bad
			save_failures $latest.snapshot.xml $latest.snapshot.stderr
			return
		fi

		$ETHREPORT -X $latest.snapshot.xml -q -o links > $latest.links 2> $latest.links.stderr
		if [ $? != 0 ]
		then
			echo "$BASENAME: ${plane_name} Error: Unable to analyze fabric snapshot. See $latest.links.stderr" >&2
			status=bad
			save_failures $latest.snapshot.xml $latest.links $latest.links.stderr
			return
		fi

		$ETHREPORT -X $latest.snapshot.xml -q -o comps -d 4 > $latest.comps 2> $latest.comps.stderr
		if [ $? != 0 ]
		then
			echo "$BASENAME: ${plane_name} Error: Unable to analyze fabric snapshot. See $latest.comps.stderr" >&2
			status=bad
			save_failures $latest.snapshot.xml $latest.comps $latest.comps.stderr
			return
		fi

		if [[ $getbaseline == y ]]
		then
			mkdir -p $baseline_dir
			rm -rf $baseline.*
			cp $latest.snapshot.xml $latest.links $latest.comps $baseline_dir
		fi
	fi

	if [[ $getbaseline == n ]]
	then
		# check fabric health
		mkdir -p $latest_dir

		$ETHREPORT $topt -c "$configfile" -q $FF_FABRIC_HEALTH > $latest.errors 2>$latest.errors.stderr
		if [ $? != 0 ]
		then
			echo "$BASENAME: ${plane_name} Error: Unable to access fabric. See $latest.errors.stderr" >&2
			status=bad
			save_failures $latest.errors $latest.errors.stderr
		elif grep 'Errors found' < $latest.errors | grep -v ' 0 Errors found' > /dev/null
		then
			echo "$BASENAME: ${plane_name} Fabric possible errors found.  See $latest.errors" >&2
			status=bad
			save_failures $latest.errors $latest.errors.stderr
		fi
	fi

	if [[ $getbaseline == n  && $healthonly == n ]]
	then
		# compare to baseline

		# cleanup old files
		rm -f $latest.links.changes $latest.links.changes.stderr
		rm -f $latest.links.rebase $latest.links.rebase.stderr
		rm -f $latest.comps.changes $latest.comps.changes.stderr
		rm -f $latest.comps.rebase $latest.comps.rebase.stderr

		base_comps=$baseline.comps
		$FF_DIFF_CMD $baseline.links $latest.links > $latest.links.diff 2>&1
		if [ -s $latest.links.diff ]
		then
			if [ ! -e $baseline.snapshot.xml ]
			then
				echo "$BASENAME: ${plane_name} New baseline required" >&2
			else
				# see if change is only due to new FF version
				$ETHREPORT -X $baseline.snapshot.xml -q -o links -P > $latest.links.rebase 2> $latest.links.rebase.stderr
				if [ $? != 0 ]
				then
					# there are other issues
					rm -f $latest.links.rebase $latest.links.rebase.stderr
				else
					cmp -s $latest.links $latest.links.rebase >/dev/null 2>&1
					if [ $? == 0 ]
					then
						# change is due to FF version
						# use new output format
						echo "$BASENAME: ${plane_name} New baseline recommended" >&2
						$FF_DIFF_CMD $latest.links.rebase $latest.links > $latest.links.diff 2>&1
						$ETHREPORT -X $baseline.snapshot.xml -q -o comps -P -d 4 > $latest.comps.rebase 2> $latest.comps.rebase.stderr
						if [ $? != 0 ]
						then
							# there are other issues
							rm -f $latest.comps.rebase $latest.comps.rebase.stderr
						else
							base_comps=$latest.comps.rebase
						fi
					fi
				fi
			fi
		fi
		if [ -s $latest.links.diff -a -e $baseline.snapshot.xml ]
		then
			# use the baseline snapshot to generate an XML links report
			# we then use that as the expect topology and compare the
			# latest snapshot to the expected topology to generate a more
			# precise and understandable summary of the differences
			( $ETHREPORT -X $baseline.snapshot.xml $topt -q -o links -P -x|
				$ETHREPORT -T - -X $latest.snapshot.xml -q -o verifylinks -P > $latest.links.changes ) 2> $latest.links.changes.stderr
		fi

		$FF_DIFF_CMD $base_comps $latest.comps > $latest.comps.diff 2>&1
		if [ -s $latest.comps.diff -a -e $baseline.snapshot.xml ]
		then
			# use the baseline snapshot to generate an XML nodes and SMs report
			# we then use that as the expect topology and compare the
			# latest snapshot to the expected topology to generate a more
			# precise and understandable summary of the differences
			( $ETHREPORT -X $baseline.snapshot.xml $topt -q -o brnodes -d 1 -P -x|
				$ETHREPORT -T - -X $latest.snapshot.xml -q -o verifynodes -o verifysms -P > $latest.comps.changes ) 2> $latest.comps.changes.stderr
		fi

		if [ -s $latest.links.changes -o -s $latest.comps.changes ]
		then
			echo "$BASENAME: ${plane_name} Fabric configuration changed.  See $latest.links.changes, $latest.comps.changes and/or $latest.comps.diff" >&2
			status=bad
			files="$latest.links $latest.links.diff $latest.comps $latest.comps.diff"
			[ -e $latest.links.changes ] && files="$files $latest.links.changes $latest.links.changes.stderr"
			[ -e $latest.comps.changes ] && files="$files $latest.comps.changes $latest.comps.changes.stderr"
			save_failures $files
		elif [ -s $latest.links.diff -o -s $latest.comps.diff ]
		then
			echo "$BASENAME: ${plane_name} Fabric configuration changed.  See $latest.links.diff and/or $latest.comps.diff" >&2
			status=bad
			save_failures $latest.links $latest.links.diff $latest.comps $latest.comps.diff
		else
			rm -f $latest.links.diff $latest.links.changes $latest.links.changes.stderr $latest.comps.diff $latest.comps.changes $latest.comps.changes.stderr
		fi
	fi
}

for plane in $planes
do
	id="${plane%;*}"
	available_fid="$(echo "$available_fids" | grep "^$id;" 2>/dev/null)"
	if [[ -z $available_fid ]]
	then
		echo "$BASENAME: Error: Couldn't find fabric plane '$id' in config file '$mgt_file'" >&2
		status=bad
		continue
	fi
	if [[ "$id" = "$plane" ]]
	then
		# no user specified topology file. use what we find in conf file
		do_analysis "$available_fid"
	else
		# use user specified topology file
		do_analysis "$plane"
	fi
done

if [ "$status" != "ok" ]
then
	if [[ $healthonly == n ]]
	then
		echo "$BASENAME: Possible fabric errors or changes found" >&2
	else
		echo "$BASENAME: Possible fabric errors found" >&2
	fi
	exit 1
else
	if [[ $getbaseline == n  ]]
	then
		echo "$BASENAME: Fabric Plane(s) OK"
	else
		echo "$BASENAME: Baselined"
	fi
	exit 0
fi
