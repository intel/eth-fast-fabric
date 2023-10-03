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

# Run ethreport with standard options and pipe output to ethxmlextract to
#  extract all performance counters into a csv file format
tempfile="$(mktemp)"
trap "rm -f $tempfile; exit 1" SIGHUP SIGTERM SIGINT
trap "rm -f $tempfile" EXIT


Usage_full()
{
	echo "Usage: ${cmd} [ethreport options]" >&2
	echo "              or" >&2
	echo "       ${cmd} --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   ethreport options - Options passed to ethreport." >&2
	echo >&2
	echo "Provides a report of all performance counters in a CSV format suitable for" >&2
	echo "importing into a spreadsheet or parsed by other scripts for further analysis. It" >&2
	echo "generates a detailed ethreport component summary report and pipes the result to" >&2
	echo "ethxmlextract, extracting element values for NodeDesc, IfAddr, PortNum, neighbor" >&2
	echo "NodeDesc, neighbor IfAddr, neighbor PortNum and all the performance counters." >&2
	echo >&2
	echo "Examples:" >&2
	echo "   ${cmd}" >&2
	echo >&2
	echo "See the man page for \"ethreport\" for the full set of options.">&2
	echo "Do no use \"-o/--output\" report option." >&2
	echo >&2
	exit 0
}

Usage()
{
	echo "Usage: ${cmd} [ethreport options]" >&2
	echo "              or" >&2
	echo "       ${cmd} --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   ethreport options - Options passed to ethreport." >&2
	echo "Examples:" >&2
	echo "   ${cmd}" >&2
	echo >&2
	exit 2
}

## Main function

cmd=`basename $0`
if [ x"$1" = "x--help" ]
then
	Usage_full
fi

IFS=';'
/usr/sbin/ethreport -o comps -s -x -Q -d 10 "$@" > $tempfile
if [ -s $tempfile ]
then
	# minor reformatting of header line to condense column names
	echo "NodeDesc;NodeType;IfAddr;PortNum;PortId;nNodeDesc;nNodeType;nIfAddr;nPortNum;nPortId;LinkSpeedActive;IfHCOutOctetsMB;IfHCOutOctets;IfHCOutUcastPkts;IfHCOutMulticastPkts;IfHCInOctetsMB;IfHCInOctets;IfHCInUcastPkts;IfHCInMulticastPkts;Dot3HCStatsInternalMacTransmitErrors;Dot3HCStatsInternalMacReceiveErrors;Dot3HCStatsSymbolErrors;IfOutErrors;IfInErrors;IfInUnknownProtos;Dot3HCStatsAlignmentErrors;Dot3HCStatsFCSErrors;Dot3HCStatsFrameTooLongs;IfOutDiscards;IfInDiscards;Dot3StatsCarrierSenseErrors;Dot3StatsSingleCollisionFrames;Dot3StatsMultipleCollisionFrames;Dot3StatsSQETestErrors;Dot3StatsDeferredTransmissions;Dot3StatsLateCollisions;Dot3StatsExcessiveCollisions"
	cat $tempfile | /usr/sbin/ethxmlextract -H -d \; -e Node.NodeDesc -e Node.NodeType -e Node.IfAddr -e PortInfo.PortNum -e PortInfo.PortId -e Neighbor.Port.NodeDesc -e Neighbor.Port.NodeType -e Neighbor.Port.IfAddr -e Neighbor.Port.PortNum -e Neighbor.Port.PortId -e LinkSpeedActive -e IfHCOutOctetsMB -e IfHCOutOctets -e IfHCOutUcastPkts -e IfHCOutMulticastPkts -e IfHCInOctetsMB -e IfHCInOctets -e IfHCInUcastPkts -e IfHCInMulticastPkts -e Dot3HCStatsInternalMacTransmitErrors -e Dot3HCStatsInternalMacReceiveErrors -e Dot3HCStatsSymbolErrors -e IfOutErrors -e IfInErrors -e IfInUnknownProtos -e Dot3HCStatsAlignmentErrors -e Dot3HCStatsFCSErrors -e Dot3HCStatsFrameTooLongs -e IfOutDiscards -e IfInDiscards -e Dot3StatsCarrierSenseErrors -e Dot3StatsSingleCollisionFrames -e Dot3StatsMultipleCollisionFrames -e Dot3StatsSQETestErrors -e Dot3StatsDeferredTransmissions -e Dot3StatsLateCollisions -e Dot3StatsExcessiveCollisions | \
	while read NodeDesc NodeType IfAddr PortNum PortId nNodeDesc nNodeType nIfAddr nPortNum nPortId LinkSpeedActive IfHCOutOctetsMB IfHCOutOctets IfHCOutUcastPkts IfHCOutMulticastPkts IfHCInOctetsMB IfHCInOctets IfHCInUcastPkts IfHCInMulticastPkts Dot3HCStatsInternalMacTransmitErrors Dot3HCStatsInternalMacReceiveErrors Dot3HCStatsSymbolErrors IfOutErrors IfInErrors IfInUnknownProtos Dot3HCStatsAlignmentErrors Dot3HCStatsFCSErrors Dot3HCStatsFrameTooLongs IfOutDiscards IfInDiscards Dot3StatsCarrierSenseErrors Dot3StatsSingleCollisionFrames Dot3StatsMultipleCollisionFrames Dot3StatsSQETestErrors Dot3StatsDeferredTransmissions Dot3StatsLateCollisions Dot3StatsExcessiveCollisions
	do
		# output will be:
		# for switch port 0
		#   just 1 line, no neighbor
		# for other links
		#	1st line with neighbor
		#	2nd line with most fields except neighbor fields
		#	both lines will have NodeDesc, NodeType, IfAddr, PortNum
		lineno=$(($lineno + 1))
		if [ x"$nIfAddr" = x ]
		then
			# must be a port without a neighbor (switch port 0)
			echo "$NodeDesc;$NodeType;$IfAddr;$PortNum;$PortId;$nNodeDesc;$nNodeType;$nIfAddr;$nPortNum;$nPortId;$LinkSpeedActive;$IfHCOutOctetsMB;$IfHCOutOctets;$IfHCOutUcastPkts;$IfHCOutMulticastPkts;$IfHCInOctetsMB;$IfHCInOctets;$IfHCInUcastPkts;$IfHCInMulticastPkts;$Dot3HCStatsInternalMacTransmitErrors;$Dot3HCStatsInternalMacReceiveErrors;$Dot3HCStatsSymbolErrors;$IfOutErrors;$IfInErrors;$IfInUnknownProtos;$Dot3HCStatsAlignmentErrors;$Dot3HCStatsFCSErrors;$Dot3HCStatsFrameTooLongs;$IfOutDiscards;$IfInDiscards;$Dot3StatsCarrierSenseErrors;$Dot3StatsSingleCollisionFrames;$Dot3StatsMultipleCollisionFrames;$Dot3StatsSQETestErrors;$Dot3StatsDeferredTransmissions;$Dot3StatsLateCollisions;$Dot3StatsExcessiveCollisions"
		else
			# port with a neighbor will have a second line
			read NodeDesc_2 NodeType_2 IfAddr_2 PortNum_2 PortId_2 nNodeDesc_2 nNodeType_2 nIfAddr_2 nPortNum_2 nPortId_2 LinkSpeedActive_2 IfHCOutOctetsMB_2 IfHCOutOctets_2 IfHCOutUcastPkts_2 IfHCOutMulticastPkts_2 IfHCInOctetsMB_2 IfHCInOctets_2 IfHCInUcastPkts_2 IfHCInMulticastPkts_2 Dot3HCStatsInternalMacTransmitErrors_2 Dot3HCStatsInternalMacReceiveErrors_2 Dot3HCStatsSymbolErrors_2 IfOutErrors_2 IfInErrors_2 IfInUnknownProtos_2 Dot3HCStatsAlignmentErrors_2 Dot3HCStatsFCSErrors_2 Dot3HCStatsFrameTooLongs_2 IfOutDiscards_2 IfInDiscards_2 Dot3StatsCarrierSenseErrors_2 Dot3StatsSingleCollisionFrames_2 Dot3StatsMultipleCollisionFrames_2 Dot3StatsSQETestErrors_2 Dot3StatsDeferredTransmissions_2 Dot3StatsLateCollisions_2 Dot3StatsExcessiveCollisions_2
			if [ x"$IfAddr" != x"$IfAddr_2" -o x"$PortNum" != x"$PortNum_2" ]
			then
				echo "line: $lineno: Out of synchronization" >&2
			fi
			echo "$NodeDesc;$NodeType;$IfAddr;$PortNum;$PortId;$nNodeDesc;$nNodeType;$nIfAddr;$nPortNum;$nPortId;$LinkSpeedActive_2;$IfHCOutOctetsMB_2;$IfHCOutOctets_2;$IfHCOutUcastPkts_2;$IfHCOutMulticastPkts_2;$IfHCInOctetsMB_2;$IfHCInOctets_2;$IfHCInUcastPkts_2;$IfHCInMulticastPkts_2;$Dot3HCStatsInternalMacTransmitErrors_2;$Dot3HCStatsInternalMacReceiveErrors_2;$Dot3HCStatsSymbolErrors_2;$IfOutErrors_2;$IfInErrors_2;$IfInUnknownProtos_2;$Dot3HCStatsAlignmentErrors_2;$Dot3HCStatsFCSErrors_2;$Dot3HCStatsFrameTooLongs_2;$IfOutDiscards_2;$IfInDiscards_2;$Dot3StatsCarrierSenseErrors_2;$Dot3StatsSingleCollisionFrames_2;$Dot3StatsMultipleCollisionFrames_2;$Dot3StatsSQETestErrors_2;$Dot3StatsDeferredTransmissions_2;$Dot3StatsLateCollisions_2;$Dot3StatsExcessiveCollisions_2"
		fi
	done
	res=0
else
	echo "${cmd}: Unable to get performance report" >&2
	Usage
	res=1
fi
rm -rf $tempfile
exit $res
