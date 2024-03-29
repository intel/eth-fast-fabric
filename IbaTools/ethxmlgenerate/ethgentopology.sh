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

readonly TOOL_DIR="/usr/share/eth-tools/samples"
readonly BASENAME="$(basename $0)"

Usage()
{
	echo "Usage: ${TOOL_DIR}/${BASENAME} [--help] [plane]" >&2
	echo "    --help - produce full help text" >&2
	echo "    plane - plane name. Default is 'plane'" >&2
	echo "    ${BASENAME} should be invoked using the full path - ${TOOL_DIR}/${BASENAME} " >&2

	exit 2
}

Usage_full()
{
	echo "Usage: ${TOOL_DIR}/${BASENAME} [--help] [plane]" >&2
	echo "    --help - produce full help text" >&2
	echo "    plane - plane name. Default is 'plane'" >&2
	echo >&2
	echo "    should be invoked using the full path - ${TOOL_DIR}/${BASENAME}." >&2
	echo "    generates (to stdout) sample topology XML with subsections:" >&2
	echo "      <LinkSummary>" >&2
	echo "      <NICs>" >&2
	echo "      <Switches>" >&2

	exit 0
}

# Run ethxmlgenerate with fabric topology link information

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

plane="plane"
argnunm=$#
if [ $argnunm -gt 1 ]
then
	Usage
elif [ $argnunm -eq 1 ]
then
	plane="$1"
fi

echo '<?xml version="1.0" encoding="utf-8" ?>'
echo "<Report plane=\"$plane\">"
echo '<LinkSummary>'
/usr/sbin/ethxmlgenerate -X ${TOOL_DIR}/ethtopology_links.txt -d \; -h Link -g Rate -g MTU -g Internal -g LinkDetails -h Cable -g CableLength -g CableLabel -g CableDetails -e Cable -h Port -g IfAddr -g PortNum -g PortId -g NodeDesc -g MgmtIfAddr -g NodeType -g PortDetails -e Port -h Port -g IfAddr -g PortNum -g PortId -g NodeDesc -g MgmtIfAddr -g NodeType -g PortDetails -e Port -e Link
echo '</LinkSummary>'
echo '<Nodes>'
echo '<NICs>'
/usr/sbin/ethxmlgenerate -X ${TOOL_DIR}/ethtopology_NICs.txt -d \; -h Node -g IfAddr -g NodeDesc -g NodeDetails -e Node
echo '</NICs>'
echo '<Switches>'
/usr/sbin/ethxmlgenerate -X ${TOOL_DIR}/ethtopology_SWs.txt -d \; -h Node -g IfAddr -g NodeDesc -g NodeDetails -e Node
echo '</Switches>'
echo '</Nodes>'
echo '</Report>'

exit 0
