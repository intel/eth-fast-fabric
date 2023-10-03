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

# Run ethreport -o links, then:
#   Extract optional cable values, and values for both ports of each link
#   Remove redundant information and combine cable and port information

tempfile="$(mktemp)"
trap "rm -f $tempfile; exit 1" SIGHUP SIGTERM SIGINT
trap "rm -f $tempfile" EXIT

Usage_full()
{
	cmd=`basename $0`
	echo >&2
	echo "Usage: ${cmd} [--help]|[ethreport options]" >&2
	echo "   --help - Produces full help text." >&2
	echo "   [ethreport options] - Options passed to ethreport." >&2
	echo >&2
	echo "Produces a CSV file listing all or some of the links in the fabric." >&2
	echo "${cmd} is a front end to the ethreport tool. The output from this tool" >&2
	echo "can be imported into a spreadsheet or parsed by other scripts." >&2
	echo >&2
	echo "Examples:" >&2
	echo "   List all the links in the fabric:" >&2
	echo "      ${cmd}" >&2
	echo >&2
	echo "   List all the links to a switch named \"coresw1\":" >&2
	echo "      ${cmd} -F \"node:coresw1\"" >&2
	echo >&2
	echo "   List all the links to end-nodes:" >&2
	echo "      ${cmd} -F \"nodetype:NIC\"" >&2
	echo >&2
	echo "See the man page for \"ethreport\" for the full set of options." >&2
	echo "By design, the tool ignores \"-o/--output\" report option." >&2
	echo >&2
}

if [[ "$1" == "--help" ]]
then
	Usage_full
	exit 0
fi

  # ethreport -o links generates XML output of this general form:
  #  <Link>
  #     <Rate>....
  #     <LinkDetails>....
  #     <Cable>
  #       ... cable information from topology.xml
  #     </Cable>
  #     <Port>
  #       .. information about 1st port excluding its CableInfo
  #     </Port>
  #     <Port>
  #       .. information about 2nd port excluding its CableInfo
  #     </Port>
  #     <CableInfo>
  #       .. information about the CableInfo for the cable between the two ports
  #     </CableInfo>
  #  </Link>
  # ethxmlextract produces the following CSV format on each line:
  #    1 Link ID (CSV 1) LinkID
  #    2 Link values (CSV 2-3) (Rate, LinkDetails)
  #    3 Cable values (CSV 4-6) (CableLength, CableLabel, CableDetails)
  #    5 CableInfo values (CSV 7-11) (DeviceTechShort, ... VendorRev)
  #    3 Port values (CSV 12-14) (NodeDesc, PortNum, PortId)
  # due to the nesting of tags, ethxmlextract will output the following
  #    All lines have LinkID and Rate and one set of Cable values, Cableinfo values, or Port Values

## Main function:
function genReport()
{
	link1=0
	link2=0
	printHeader=1
	curLinkID=""
	prevLinkID=""
	tempresults=""

	/usr/sbin/ethreport -x -Q -o links "$@" -d 3 > $tempfile
	if [ ! -s $tempfile ]
	then
		echo "ethextractlink: Unable to get links report" >&2
		Usage_full
		exit 1
	fi

	while read line
	do
		curLinkID=`echo $line | cut -d \; -f 1`

		# When the LinkL:ID changes, we're now reading a line of data from a new link record.
		# We can print the previous link record now that we know it is complete.
		if [ "$curLinkID" != "$prevLinkID" ]
		then
			# Display the header the first time through
			if [ $printHeader -eq 1 ]
			then
				echo "Rate;LinkDetails;CableLength;CableLabel;CableDetails;DeviceTechShort;CableInfo.Length;CableInfo.VendorName;CableInfo.VendorPN;CableInfo.VendorRev;Port.NodeDesc;Port.PortNum;Port.PortId;Port.NodeDesc;Port.PortNum;Port.PortId"
				printHeader=0
			fi

			# Display the link line.
			echo $tempresults

			# Reset for the next set of data
			link1=0
			link2=0
			RateStr=""
			LinkDetailsStr=""
			CableValuesStr=";;"
			CableInfoValuesStr=";;;;"
			Port1ValuesStr=";"
			Port2ValuesStr=";"

			prevLinkID=$curLinkID
		fi

		if [ "$RateStr" == "" ]
		then
			RateStr=`echo $line | cut -d \; -f 2`
		fi

		if [ "$LinkDetailsStr" == "" ]
		then
			LinkDetailsStr=`echo $line | cut -d \; -f 3`
		fi

		if [ "$CableValuesStr" == ";;" ]
		then
			CableValuesStr=`echo $line | cut -d \; -f 4-6`
		fi

		if [ "$CableInfoValuesStr" == ";;;;" ]
		then
			CableInfoValuesStr=`echo $line | cut -d \; -f 7-11`
			tempresults=$RateStr";"$LinkDetailsStr";"$CableValuesStr";"$CableInfoValuesStr";"$Port1ValuesStr";"$Port2ValuesStr
		fi

		if [ $link1 -eq 0 ]
		then
			if [ "$Port1ValuesStr" == ";" ]
			then
				Port1ValuesStr=`echo $line | cut -d \; -f 12-14`
			fi

			if [ "$Port1ValuesStr" != ";" ]
			then
				link1=1
			fi
		elif [ $link2 -eq 0 ]
		then
			if [ "$Port2ValuesStr" == ";" ]
			then
				Port2ValuesStr=`echo $line | cut -d \; -f 12-14`
			fi

			if [ "$Port2ValuesStr" != ";" ]
			then
				link2=1
				tempresults=$RateStr";"$LinkDetailsStr";"$CableValuesStr";"$CableInfoValuesStr";"$Port1ValuesStr";"$Port2ValuesStr
			fi
		fi
	done < <(cat $tempfile | \
		/usr/sbin/ethxmlextract -H -d \; -e Link:id -e Rate -e LinkDetails -e CableLength \
		-e CableLabel -e CableDetails -e DeviceTechShort -e CableInfo.Length \
		-e CableInfo.VendorName -e CableInfo.VendorPN -e CableInfo.VendorRev \
		-e Port.NodeDesc -e Port.PortNum -e Port.PortId)


	# Now print the final link record
	echo $tempresults
}

genReport "$@"
exit 0
