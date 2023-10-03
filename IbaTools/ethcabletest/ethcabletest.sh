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

# start and stop NIC-SW cable Bit Error Rate tests

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
readonly BASENAME="$(basename $0)"

# Default Values
numprocs=3

Usage_full()
{
	echo "Usage: $BASENAME [-p plane] [-f hostfile] [-h 'hosts'] [-n numprocs] start|stop" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   -p plane - Specifies the fabric plane the test will run on. The specified" >&2
	echo "              plane needs to be defined and enabled in the Mgt config file." >&2
	echo "              Default is the first enabled plane." >&2
	echo "   -f hostfile - Specifies the file with hosts to include in NIC-SW test." >&2
	echo "              It overrides the HostsFiles defined in Mgt config file for the" >&2
	echo "              corresponding plane." >&2
	echo "   -h hosts - Specifies the list of hosts to include in NIC-SW test." >&2
	echo "   -n numprocs - Number of processes per host for NIC-SW test. Default is ${numprocs}." >&2
	echo >&2
	echo "   start - Starts the NIC-SW tests." >&2
	echo "   stop - Stops the NIC-SW tests." >&2
	echo >&2
	echo "The NIC-SW cable test requires that the FF_MPI_APPS_DIR is set, and it contains a" >&2
	echo "pre-built copy of the Intel mpi_apps for an appropriate message passing interface (MPI)." >&2 
	echo >&2
	echo " Environment:" >&2
	echo "   HOSTS - List of hosts, used if -h option not supplied." >&2
	echo "   HOSTS_FILE - File containing list of hosts, used in absence of -f and -h." >&2
	echo "   FABRIC_PLANE - Name of fabric plane used in absence of -p, -f, and -h." >&2
	echo "   FF_MAX_PARALLEL - Maximum concurrent operations." >&2
	echo "example:">&2
	echo "   $BASENAME -p plane1 start" >&2
	echo "   $BASENAME -f good stop" >&2
	echo "   $BASENAME -h 'arwen elrond' start" >&2
	echo "   HOSTS='arwen elrond' $BASENAME stop" >&2
	rm -f $tempfile
	exit 0
}

Usage()
{
	echo "Usage: $BASENAME [-n numprocs] [-p plane] [-f hostfile] start|stop" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   -p plane - Specifies the fabric plane the test will run on. The specified" >&2
	echo "              plane needs to be defined and enabled in the Mgt config file." >&2
	echo "              Default is the first enabled plane." >&2
	echo "   -f hostfile - Specifies the file with hosts to include in NIC-SW test." >&2
	echo "   -n numprocs - Number of processes per host for NIC-SW test." >&2
	echo >&2
	echo "   start - Starts the NIC-SW tests." >&2
	echo "   stop - Stops the NIC-SW tests." >&2
	echo >&2
	echo "The NIC-SW cable test requires that the FF_MPI_APPS_DIR is set, and it contains a" >&2
	echo "pre-built copy of the Intel mpi_apps for an appropriate message passing interface (MPI)." >&2 
	echo >&2
	echo " Environment:" >&2
	echo "   FF_MAX_PARALLEL - maximum concurrent operations" >&2
	echo "example:">&2
	echo "   $BASENAME -p plane1 start" >&2
	echo "   $BASENAME -f good stop" >&2
	echo "   $BASENAME stop" >&2
	rm -f $tempfile
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

while getopts p:f:h:n: param
do
	case $param in
	h)
		HOSTS="$OPTARG";;
	f)
		HOSTS_FILE="$OPTARG";;
	p)
		FABRIC_PLANE="$OPTARG";;
	n)
		numprocs="$OPTARG";;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))

check_host_args $BASENAME 1

first_node=$(echo "$HOSTS" | head -n 1 | cut -d ' ' -f 1)
first_ports=$(get_node_ports "$first_node")
if [ -z "$first_ports" ]; then
	first_ports=$(get_ifs_by_driver ice)
fi
first_irdmas=$(get_irdmas "$first_ports")
if [ -n "$first_irdmas" ]; then
	export CFG_MPI_DEV="+(${first_irdmas// /|})"
else
	export CFG_MPI_DEV=
fi
export CFG_MPI_MULTIRAIL=1

# HOSTS now lists all the hosts, pass it along to the commands below via env
export HOSTS
unset HOSTS_FILE

start()
{
	if [ ! -e $FF_MPI_APPS_DIR/run_batch_cabletest ]
	then
		echo "$BASENAME: Invalid FF_MPI_APPS_DIR: $FF_MPI_APPS_DIR" >&2
		exit 1
	fi
	if [ ! -x $FF_MPI_APPS_DIR/groupstress/mpi_groupstress ]
	then
		echo "$BASENAME: FF_MPI_APPS_DIR ($FF_MPI_APPS_DIR) not compiled" >&2
		rm -f $tempfile
		exit 1
	fi
	ff_var_to_stdout "$HOSTS" > $tempfile
	cd $FF_MPI_APPS_DIR
	MPI_HOSTS=$tempfile ./run_batch_cabletest -n $numprocs infinite
}

stop()
{
	# we use patterns so the pkill doesn't kill this script or ethcmdall itself
	# use an echo at end so exit status is good
	/usr/sbin/ethcmdall -p -T 60 "pkill -9 -f '[m]pi_groupstress'; echo -n"
}

if [ $# -eq 0 ]
then
	Usage
fi

while [ $# -ne 0 ]
do
	case "$1" in
	start) start;;
	stop) stop;;
	*)	Usage;;
	esac
	shift
done

rm -f $tempfile
