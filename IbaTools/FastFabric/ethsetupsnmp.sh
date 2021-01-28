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
# run a command on all hosts or chassis
set -eo pipefail
if [ -f /etc/eth-tools/ethfastfabric.conf ]
then
	. /etc/eth-tools/ethfastfabric.conf
fi

. /usr/lib/eth-tools/ethfastfabric.conf.def

. /usr/lib/eth-tools/ff_funcs

readonly BASENAME="$(basename $0)"
readonly SNMP_CONF="/etc/snmp/snmpd.conf"
readonly SNMP_CONF_BAK="/etc/snmp/snmpd.conf.ifsbak"
readonly -a FF_MIBS=(
	"1.3.6.1.2.1.1" #system
	"1.3.6.1.2.1.2" #interfaces
	"1.3.6.1.2.1.4" #ip
	"1.3.6.1.2.1.10.7" #dot3
	"1.3.6.1.2.1.31.1" #ifMIBObjects
)
readonly -a FF_MIBS_DESC=(
	"1.3.6.1.2.1.1 (SNMPv2-MIB:system)"
	"1.3.6.1.2.1.2 (IF-MIB:interfaces)"
	"1.3.6.1.2.1.4 (IP-MIB:ip)"
	"1.3.6.1.2.1.10.7 (EtherLike-MIB:dot3)"
	"1.3.6.1.2.1.31.1 (IP-MIB:ifMIBObjects) "
)
readonly ETHFF_HEAD="### Created by Intel Eth FS. Please do not change! ###"
readonly ETHFF_TAIL="### End of Intel Eth FS conf ###"

p_opt=n
l_opt=n
f_opt=""
h_opt=""
a_opt=n
c_opt=n
m_opt=n
admins="$(hostname)"
community="public"
mibs=""

Usage_full() {
	echo "Usage: $BASENAME [-p] [-f hostfile] [-h 'hosts'] [-a 'admin']" >&2
	echo "                    [-c 'community'] [-m 'mibs']" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -p - run command in parallel on all hosts" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
	echo "   -h hosts - list of hosts in cluster" >&2
	echo "   -a admins - list of admin hosts that can issue SNMP query" >&2
	echo "               Default is the current host" >&2
	echo "   -c community - community string used for SNMP query, default is 'public'" >&2
	echo "   -m mibs - list of MIBs that are readable in SNMP queries" >&2
	echo "             Default is all MIBs required by FastFabric" >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
	echo "   FF_MAX_PARALLEL - when -p option is used, maximum concurrent operations" >&2
	echo "for example:" >&2
	echo "   $BASENAME -h 'elrond arwen' -a 'elrond'" >&2
	echo "   HOSTS='elrond arwen' $BASENAME -a 'elrond'" >&2
	echo "   $BASENAME -a 'elrond' -c 'public' -m '1.3.6.1.2.1.1 1.3.6.1.2.1.2'" >&2
	exit 0
}

Usage() {
	echo "Usage: $BASENAME [-p] [-f hostfile] [-h 'hosts'] [-a 'admin']" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -p - run command in parallel on all hosts" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
	echo "   -h hosts - list of hosts in cluster" >&2
	echo "   -a admins - list of admin hosts that can issue SNMP query" >&2
	echo "               Default is the current host" >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
	echo "   FF_MAX_PARALLEL - when -p option is used, maximum concurrent operations" >&2
	echo "for example:" >&2
	echo "   $BASENAME -h 'elrond arwen' -a 'elrond'" >&2
	echo "   HOSTS='elrond arwen' $BASENAME -a 'elrond'" >&2
	exit 2
}

function create_snmp_conf() {
	community=$1
	mibs=$2
	admins=$3
	conf=$4

	sec_name="ethFFUser"
	group_name="ethFFGroup"
	view_name="ethFFView"

	echo "${ETHFF_HEAD}" > ${conf}
	echo "#" >> ${conf}

	echo "#       sec.name	source		community" >> ${conf}
	for admin in ${admins}; do
		echo "com2sec ${sec_name}	${admin}	${community}" >> ${conf}
	done

	echo "#       groupName	securityModel	securityName" >> ${conf}
	echo "group   ${group_name}	v2c		${sec_name}" >> ${conf}

	mibs="$(echo ${mibs} | xargs -n1 | sort -u | xargs)"
	echo "#       name		incl/excl	subtree	mask(optional)" >> ${conf}
	for mib in ${mibs}; do
		echo "view    ${view_name}	included	.${mib}" >> ${conf}
	done

	echo "#       group		context	sec.model	sec.level	prefix	read		write	notif" >> ${conf}
	echo "access  ${group_name}	\"\"	any		noauth		exact	${view_name}	none	none" >> ${conf}

	echo "#" >> ${conf}
	echo "${ETHFF_TAIL}" >> ${conf}
}

function merge_snmp_conf() {
	new_conf=$1
	mv -f ${SNMP_CONF} ${SNMP_CONF_BAK}
	mv -f ${new_conf} ${SNMP_CONF}
	skip=0
	while IFS= read -r line; do
		if [[ "${line}" = "${ETHFF_HEAD}" ]]; then
			skip=1
		elif [[ "${line}" = "${ETHFF_TAIL}" ]]; then
			skip=0
		elif [[ ${skip} -ne 1 ]]; then
			echo ${line} >> ${SNMP_CONF}
		fi
	done < ${SNMP_CONF_BAK}
	if [[ ${skip} -eq 1 ]]; then
		# shouldn't happen. But just in case.
		mv -f ${SNMP_CONF_BAK} ${SNMP_CONF}
		echo "Corrupted ${SNMP_CONF}"
		exit 1
	fi
}

function process_arguments() {
	while getopts "plf:h:a:c:m:" opt; do
		case ${opt} in
			p)
				p_opt=y;;
			l)
				l_opt=y;;
			f)
				f_opt="$OPTARG";;
			h)
				h_opt="$OPTARG";;
			a)
				a_opt=y
				admins="$OPTARG";;
			c)
				c_opt=y
				community="$OPTARG";;
			m)
				m_opt=y
				mibs="$OPTARG";;
			?)
				Usage;;
		esac
	done

	shift $((OPTIND - 1))
	if [[ $# -gt 0 ]]; then
		Usage
	fi
}

function get_arguments() {
	read -e -p "Enter space separated list of admin hosts ($admins): " opt
	if [[ -n ${opt} ]]; then
		admins="${opt}"
	fi

	read -e -p "Enter SNMP community string ($community): " opt
	if [[ -n ${opt} ]]; then
		community="${opt}"
	fi

	include_ffmibs=1
	echo "Fast Fabric requires the following MIBs:"
	for mib in "${FF_MIBS_DESC[@]}"; do
		echo "	${mib}"
	done
	while true; do
		read -e -p "Do you accept these MIBs [y/n] (y): " opt
		case ${opt} in
			"" | [Yy]* )
				break;;
			[Nn]* )
				include_ffmibs=0
				break;;
		esac
	done
	if [[ -z "${mibs}" ]]; then
		read -e -p "Enter space separated list of extra MIBs to support (NONE): " opt
	else
		read -e -p "Enter space separated list of extra MIBs to support (${mibs}): " opt
	fi
	if [[ -n ${opt} && ! "${opt}" = "NONE" ]]; then
		mibs="${opt}"
	fi

	echo ""
	echo "Will config SNMP with the following settings:"
	echo "  admin hosts: ${admins}"
	echo "  community: ${community}"
	if [[ ${include_ffmibs} -eq 1 ]]; then
		echo "  MIBs: ${FF_MIBS[*]} ${mibs}"
	else
		echo "  MIBs: ${mibs}"
	fi
	while true; do
		read -e -p "Do you accept these settings [y/n] (y): " opt
		case ${opt} in
			"" | [Yy]* )
				if [[ ${include_ffmibs} -eq 1 ]]; then
					mibs="${FF_MIBS[*]} ${mibs}"
				fi
				break;;
			[Nn]* )
				get_arguments
				break;;
		esac
	done
}

function config_local_host() {
	tmp_file="$(mktemp /tmp/ethff_snmp.XXXXXX)"
	create_snmp_conf "${community}" "${mibs}" "${admins}" "${tmp_file}"
	merge_snmp_conf ${tmp_file}
	rm -f ${tmp_file}
	if systemctl is-active snmpd > /dev/null; then
		systemctl restart snmpd
	fi
}

function main() {
	process_arguments "$@"
	echo "Configuring SNMP..."

	if [[ "${a_opt}" = "n" || "${c_opt}" = "n" || "${m_opt}" = "n" ]]; then
		get_arguments
	fi
	if [[ "${l_opt}" = "y" ]]; then
		config_local_host
	else
		p_arg=""
		fh_arg=""
		fh_param=""
		if [[ "${p_opt}" = "y" ]]; then
			p_arg="-p"
		fi
		if [[ -n ${f_opt} ]]; then
			fh_arg="-f"
			fh_param="${f_opt}"
		elif [[ -n ${h_opt} ]]; then
			fh_arg="-h"
			fh_param="${h_opt}"
		else
			config_local_host
		fi
		local_name="/tmp/${BASENAME}"
		if [[ -z ${fh_arg} ]]; then
			ethscpall ${p_arg} /usr/sbin/${BASENAME} ${local_name}
			ethcmdall ${p_arg} "${local_name} -l -a '${admins}' -c '${community}' -m '${mibs}';rm -f ${local_name}"
		else
			ethscpall ${p_arg} ${fh_arg} "${fh_param}" /usr/sbin/${BASENAME} ${local_name}
			ethcmdall ${p_arg} ${fh_arg} "${fh_param}" "${local_name} -l -a '${admins}' -c '${community}' -m '${mibs}';rm -f ${local_name}"
		fi
	fi
	echo "SNMP configuration completed"
}

if [[ x"$1" = "x--help" ]]; then
	Usage_full
fi
trap "exit 1" SIGHUP SIGTERM SIGINT
main "$@"
