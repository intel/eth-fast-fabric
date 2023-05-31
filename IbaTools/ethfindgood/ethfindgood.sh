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

# analyzes hosts in fabric and outputs some lists:
#	alive - list of pingable hosts
#	running - subset of alive which can be sshed to via ethcmdall
#	active - list of hosts with 1 or more active ports
#	good - list of hosts which are running and active
#	bad - list of hosts which fail any of the above tests
# The intent is that the good list can be a candidate list of hosts for
# use in running MPI jobs to further use or test the cluster

# optional override of defaults
if [ -f /etc/eth-tools/ethfastfabric.conf ]
then
	. /etc/eth-tools/ethfastfabric.conf
fi

. /usr/lib/eth-tools/ethfastfabric.conf.def

. /usr/lib/eth-tools/ff_funcs

trap "exit 1" SIGHUP SIGTERM SIGINT

punchlist=${FF_RESULT_DIR}/punchlist.csv
del=';'
timestamp=$(date +"%Y/%m/%d %T")
readonly BASENAME="$(basename $0)"

Usage_full()
{
	echo "Usage: ${BASENAME} [-RA] [-d dir] [-p plane] [-f hostfile] [-h 'hosts'] [-T timelimit]" >&2
	echo "              or" >&2
	echo "       ${BASENAME} --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -R - skip the running test (ssh), recommended if password-less ssh not setup" >&2
	echo "   -A - skip the active test, recommended if Intel Ethernet software or fabric is not up" >&2
	echo "   -d - directory in which to create alive, active, running, good and bad files" >&2
	echo "        default is ${CONFIG_DIR}/${FF_PRD_NAME}" >&2
	echo "   -p plane - plane name" >&2
	echo "   -f hostfile - file with hosts in cluster, default is ${CONFIG_DIR}/${FF_PRD_NAME}/hosts" >&2
	echo "   -h hosts - list of hosts to ping" >&2
	echo "   -T timelimit - timelimit in seconds for host to respond to ssh" >&2
	echo "               default of 20 seconds" >&2
  	echo >&2
	echo "The files alive, running, active, good and bad are created in the selected" >&2
	echo "directory listing hosts passing each criteria. If a plane name is provided," >&2
	echo "filename will be xxx_<plane>, e.g good_plane1" >&2
	echo "A punchlist of bad hosts is also appended to FF_RESULT_DIR/punchlist.csv" >&2
	echo "The good file can be used as input for an mpi_hosts." >&2
  	echo "It will list each good host exactly once" >&2
  	echo >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
	echo "   FF_MAX_PARALLEL - maximum concurrent operations" >&2
	echo "example:">&2
	echo "   ${BASENAME}" >&2
	echo "   ${BASENAME} -f allhosts" >&2
	echo "   ${BASENAME} -h 'arwen elrond'" >&2
	echo "   HOSTS='arwen elrond' ${BASENAME}" >&2
	echo "   HOSTS_FILE=allhosts ${BASENAME}" >&2
	exit 0
}

Usage()
{
	echo "Usage: ${BASENAME} [-RA] [-d dir] [-p plane] [-f hostfile] [-h 'hosts'] [-T timelimit]" >&2
	echo "       ${BASENAME} --help" >&2
	echo "              or" >&2
	echo "   --help - produce full help text" >&2
	echo "   -R - skip the running test (ssh), recommended if password-less ssh not setup" >&2
	echo "   -A - skip the active test, recommended if Intel Ethernet Fabric Suite software" >&2
	echo "        or fabric is not up" >&2
	echo "   -d - directory in which to create alive, active, running, good and bad files" >&2
	echo "        default is ${CONFIG_DIR}/${FF_PRD_NAME}" >&2
	echo "   -f hostfile - file with hosts in cluster, default is ${CONFIG_DIR}/${FF_PRD_NAME}/hosts" >&2
	echo "" >&2
	echo "   See full help text for explanation of all options." >&2
  	echo >&2
	echo "The files alive, running, active, good and bad are created in the selected" >&2
	echo "directory listing hosts passing each criteria" >&2
	echo "A punchlist of bad hosts is also appended to FF_RESULT_DIR/punchlist.csv" >&2
	echo "The good file can be used as input for an mpi_hosts." >&2
  	echo "It will list each good host exactly once" >&2
  	echo >&2
	echo "example:">&2
	echo "   ${BASENAME}" >&2
	echo "   ${BASENAME} -f allhosts" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

skip_ssh=n
skip_active=n
dir=${CONFIG_DIR}/${FF_PRD_NAME}
plane=
timelimit=20
while getopts d:p:f:h:RAT: param
do
	case ${param} in
	d)	dir="${OPTARG}";;
	p)	plane="${OPTARG}";;
	h)	HOSTS="${OPTARG}";;
	f)	HOSTS_FILE="${OPTARG}";;
	R)	skip_ssh=y;;
	A)	skip_active=y;;
	T)	export timelimit="${OPTARG}";;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -gt 0 ]
then
	Usage
fi

check_host_args ${BASENAME}

# just pass host list via environment for ethpingall and ethcmdall below
export HOSTS
unset HOSTS_FILE

append_punchlist()
# file with $1 set of tested hosts (in lowercase with no dups, unsorted)
# file with $2 set of passing hosts (in lowercase with no dups, unsorted)
# text for punchlist entry for failing hosts
{
	comm -23 <(sort "$1") <(sort "$2")| ethsorthosts | while read host
	do
		echo "${timestamp}${del}${host}${del}$3"
	done >> ${punchlist}
}

# read stdin and convert to canonical lower case
# in field 1 of output, field2 is unmodified input
# fields are separated by a space
function to_canon()
{
	while read line
	do
		canon=$(echo ${line}|ff_to_lc)
		echo "${canon} ${line}"
	done|sort --ignore-case -t ' ' -k1,1
}

function mycomm12()
{
	/usr/lib/${FF_PRD_NAME}/comm12 $1 $2
}

function to_nodes_ports()
{
	src="$1"
	dst="$2"
	get_nodes_ports "$(cat "${src}")" > "${dst}"
}

good_meaning=	# indicates which tests a good host has passed
good_file=		# file (other than good) holding most recent good list
# cleanup generated host files. If we run this tool multiple times with
# different arguments, such as skipping different tests, hosts files generated
# in previous run may mislead a user.
alive_hostonly=$(mktemp)
running_hostonly=$(mktemp)
bak_files=""
for file in alive running active good bad
do
	if [ -n "${plane}" ]
	then
		file="${file}_${plane}"
	fi
	if [ -f "${dir}/${file}" ]
	then
		mv -f "${dir}/${file}" "${dir}/${file}.bak"
		if [[ -z "${bak_files}" ]]
		then
			bak_files="${file}"
		else
			bak_files="${bak_files},${file}"
		fi
	fi
done
if [[ -n "${bak_files}" ]]
then
	echo "Warning: backed up existing ${dir}/{${bak_files}} as *.bak files." >&2
fi

echo "$(ff_var_filter_dups_to_stdout "${HOSTS}"|wc -l) hosts will be checked"

good_file="${dir}/good"
if [ -n "${plane}" ]
then
	good_file="${good_file}_$plane"
fi
# ------------------------------------------------------------------------------
# ping test
# This test applies at host level. It uses alive_hostonly file that contains hostname only.
alive_file="${dir}/alive"
if [ -n "${plane}" ]
then
	alive_file="${alive_file}_${plane}"
fi
ethpingall -p|grep 'is alive'|sed -e 's/:.*//'|ff_filter_dups|ethsorthosts > ${alive_hostonly}
append_punchlist <(ff_var_filter_dups_to_stdout "${HOSTS}") ${alive_hostonly} "Doesn't ping"
good_meaning="alive"
candidate_file_hostonly=${alive_hostonly}
to_nodes_ports ${alive_hostonly} ${alive_file}
candidate_file=${alive_file}
echo "$(cat ${alive_file}|wc -l) hosts are pingable (alive)"

# ------------------------------------------------------------------------------
# ssh test
# This test applies at host level. It uses running_hostonly file that contains hostname only.
if [ "${skip_ssh}" = n ]
then
	# -h '' to override HOSTS env var so -f is used (HOSTS would override -f)
	# use comm command to filter out hosts with unexpected hostnames
	# put in alphabetic order for "comm" command
	running_file="${dir}/running"
	if [ -n "${plane}" ]
	then
		running_file="${running_file}_${plane}"
	fi
	mycomm12 <(to_canon < ${candidate_file_hostonly}) <(ethcmdall -h '' -f ${candidate_file_hostonly} -P -p -T ${timelimit} 'echo 123' |grep ': 123'|sed 's/:.*//'|ff_filter_dups|to_canon) | ethsorthosts > ${running_hostonly}
	append_punchlist ${candidate_file_hostonly} ${running_hostonly} "Can't ssh"
	to_nodes_ports ${running_hostonly} ${running_file}
	good_meaning="${good_meaning}, running"
	candidate_file=${running_file}
	echo "$(cat ${running_file}|wc -l) hosts are ssh'able (running)"
	rm -f ${running_hostonly}
fi

rm -f ${alive_hostonly}

# ------------------------------------------------------------------------------
# rdma port active test
if [ "${skip_active}" = n ]
then
	active_file="${dir}/active"
	if [ -n "${plane}" ]
	then
		active_file="${active_file}_${plane}"
	fi
	# don't waste time reporting hosts which don't ping or can't ssh
	# they are probably down so no use double reporting them
	for line in $(cat ${candidate_file}); do
		host=${line%%:*}
		ports="$(get_node_ports "${host}")"
		if [[ -z ${ports} ]]; then
			# check the node has at least one active RDMA port
			cmds="
				ports=\"\$(ls -l /sys/class/net/*/device/driver | grep 'ice$' | awk '{print \$9}' | cut -d '/' -f5)\"
				[ -z \"\$ports\" ] && exit 1
				for port in \$ports; do
					slot=\$(ls -l /sys/class/net | grep \"\$port \" | awk '{print \$11}' | cut -d '/' -f 6)
					[ -z \$slot ] && continue
					irdma_dev=\$(ls \"\$(find /sys/devices/ -path */\$slot/infiniband)\" 2> /dev/null)
					[ -z \$irdma_dev ] && continue
					ibv_devinfo -d \$irdma_dev | grep '^\s*transport:\s*InfiniBand' > /dev/null 2>&1 || exit 1
					ibv_devinfo -d \$irdma_dev | grep '^\s*state:\s*PORT_ACTIVE' > /dev/null 2>&1 && exit 0
				done
				exit 1
			"
		else
			# check all ports are active RDMA ports
			cmds="
				for port in $ports; do
					slot=\$(ls -l /sys/class/net | grep \"\$port \" | awk '{print \$11}' | cut -d '/' -f 6)
					[ -z \$slot ] && exit 1
					irdma_dev=\$(ls \"\$(find /sys/devices/ -path */\$slot/infiniband)\" 2> /dev/null)
					[ -z \$irdma_dev ] && exit 1
					ibv_devinfo -d \$irdma_dev | grep '^\s*transport:\s*InfiniBand' > /dev/null 2>&1 || exit 1
					ibv_devinfo -d \$irdma_dev | grep '^\s*state:\s*PORT_ACTIVE' > /dev/null 2>&1 || exit 1
				done
			"
		fi
		cmds="type ibv_devinfo > /dev/null 2>&1 || exit 1
			${cmds}
		"
		ssh ${host} "${cmds}" && echo ${line}
	done | ff_filter_dups|ethsorthosts > ${active_file}
	if [[ -z $ports ]]; then
		append_punchlist ${candidate_file} ${active_file} "Has no active RDMA port(s)"
	else
		append_punchlist ${candidate_file} ${active_file} "Has inactive RDMA port(s)"
	fi
	# put in alphabetic order for "comm" command
	mycomm12 <(to_canon < ${candidate_file})  <(to_canon < ${active_file}) | ethsorthosts > ${good_file}
	good_meaning="${good_meaning}, active"
	echo "$(cat ${active_file}|wc -l) total hosts have RDMA active ports on one or more fabrics (active)"
else
	cat ${candidate_file} > ${good_file}
fi

# ------------------------------------------------------------------------------
# final output
bad_file="${dir}/bad"
if [ -n "${plane}" ]
then
	bad_file="${bad_file}_${plane}"
fi

echo "$(cat ${good_file}|wc -l) hosts are ${good_meaning} (good)"
comm -23 <(ff_var_filter_dups_to_stdout "$(get_nodes_ports "${HOSTS}")") <(sort ${good_file})| ethsorthosts > ${bad_file}
echo "$(cat ${bad_file}|wc -l) hosts are bad (bad)"
echo "Bad hosts have been added to ${punchlist}"

exit 0
