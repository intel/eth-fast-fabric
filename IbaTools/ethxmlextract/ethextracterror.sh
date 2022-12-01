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

# Run ethreport with standard options and pipe output to ethxmlextract to extract
#  error counts

cmd=`basename $0`
Usage_full()
{
	echo >&2
	echo "Usage: ${cmd} [--help]|[ethreport options]" >&2
	echo "   --help - produce full help text" >&2
	echo "   [ethreport options] - options will be passed to ethreport." >&2
	echo >&2
	echo "${cmd} is a front end to the ethreport tool that generates" >&2
	echo "a report listing all or some of the errors in the current fabric." >&2
	echo "The output is in a CSV format suitable for importing into a spreadsheet or" >&2
	echo "parsed by other scripts." >&2
	echo >&2
	echo "for example:" >&2
	echo "   List all the link errors in the fabric:" >&2
	echo "      ${cmd}" >&2
	echo >&2
	echo "   List all the link errors related to a switch named \"coresw1\":" >&2
	echo "      ${cmd} -F \"node:coresw1\"" >&2
	echo >&2
	echo "   List all the link errors for end-nodes:" >&2
	echo "      ${cmd} -F \"nodetype:NIC\"" >&2
	echo >&2
	echo "See the man page for \"ethreport\" for the full set of options." >&2
	echo "By design, the tool ignores \"-o/--output\" report option." >&2
	echo >&2
	exit 0
}

Usage()
{
	echo >&2
	echo "Usage: ${cmd} [--help]|[ethreport options]" >&2
	echo "   --help - produce full help text" >&2
	echo "   [ethreport options] - options will be passed to ethreport." >&2
	echo >&2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

BINDIR=${BINDIR:-/usr/sbin}


$BINDIR/ethreport -o comps -s -x -d 10 "$@" | $BINDIR/ethxmlextract -d \; -e NodeDesc -e ChassisID -e PortNum -e PortId -e LinkSpeedActive -e Dot3HCStatsInternalMacTransmitErrors -e Dot3HCStatsInternalMacReceiveErrors -e Dot3HCStatsSymbolErrors -e IfOutErrors -e IfInErrors -e IfInUnknownProtos -e Dot3HCStatsAlignmentErrors -e Dot3HCStatsFCSErrors -e Dot3HCStatsFrameTooLongs -e IfOutDiscards -e IfInDiscards -e Dot3StatsCarrierSenseErrors -e Dot3StatsSingleCollisionFrames -e Dot3StatsMultipleCollisionFrames -e Dot3StatsSQETestErrors -e Dot3StatsDeferredTransmissions -e Dot3StatsLateCollisions -e Dot3StatsExcessiveCollisions -s Neighbor
if [ $? -ne 0 ]; then
	echo "${cmd}: Unable to get error report" >&2
	Usage
	exit 1
fi
