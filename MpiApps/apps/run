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

# SLURM directives.  Must precede all non-comment lines and can't use variables
# slurm command line overrides any SBATCH settings here
# to disable a given setting below, use ## to comment out
##SBATCH --job-name=multi
#SBATCH --output=logs/multi.%j.out
# timelimit in days-hours:minutes:seconds
##SBATCH --time=0-01:00:00

# BASE PATH TO MPI EXECUTABLES: To use an alternate location,
# either edit this line or set MPICH_PREFIX in your environment.
# default to MPI used for build
MPICH_PREFIX=${MPICH_PREFIX:-`cat .prefix 2>/dev/null`}

trap "exit 1" SIGHUP SIGTERM SIGINT

if [ -z "$1" -o x"$1" == x"-h" -o x"$1" == x"--help" ]
then 
	echo "Usage: $0 number_of_processes [full]."
	echo "             or"
	echo "       $0 --help"
	echo "    number_of_processes may be 'all' in which case PROCS_PER_NODE ranks will"
	echo "                     be started for every entry in the MPI_HOSTS file"
	if [ -d NPB3.2.1/NPB3.2-MPI/bin ]
	then
	echo "    full - include NASA and HPL benchmarks"
	else
	echo "    full - include HPL benchmark"
	fi
	echo "For example: $0 2"
	exit 1
fi
full=n
if [ x"$2" = x"full" ]
then
	full=y
fi

NUM_PROCESSES=$1
PARAM_FILE=""
APP=multi
. ./prepare_run

LATENCY_CMD="latency/latency 100000 0"
LATENCY_CMD2="latency/latency 100000 4"
BANDWIDTH_CMD="bandwidth/bw 100 100000"
IMB_CMD="imb/IMB-MPI1"
#NPB_CMD="NPB2.3/bin/$test.$class.$NUM_PROCESSES"
HPL_CMD="./xhpl"
HPL_DIR="$PWD/hpl-2.3/bin/ICS.`uname -s`.`./get_mpi_blas.sh`"

{
if [ $USING_MPD = y ]
then
    MPI_RUN_CMD_HPL="$MPI_RUN_CMD -wdir $HPL_DIR"
else
    MPI_RUN_CMD_HPL="$MPI_RUN_CMD"
fi

show_mpi_hosts
show_mpi_env
if [ $NUM_PROCESSES -eq 2 ]
then
	echo " Running Latency ..."
	date
	set -x
	$MPI_RUN_CMD $LATENCY_CMD
	set +x
	date
	set -x
	$MPI_RUN_CMD $LATENCY_CMD2
	set +x
	date
	echo "########################################### "

	echo " Running Bandwidth ..."
	date
	set -x
	$MPI_RUN_CMD $BANDWIDTH_CMD
	set +x
	date
fi
echo "########################################### "
	
echo " Running IMB Benchmarks ..."
date
set -x
$MPI_RUN_CMD $IMB_CMD
set +x
date
echo "########################################### "

if [ "$full" = y -a -d NPB3.2.1/NPB3.2-MPI/bin ]
then
	echo " Running NASA Parallel Benchmarks ..."
	class="B"
	for test in cg ep lu is mg
	do
		echo "    Running $test.$class.$NUM_PROCESSES benchmark ..."
		date
		set -x
		$MPI_RUN_CMD NPB3.2.1/NPB3.2-MPI/bin/$test.$class.$NUM_PROCESSES
		set +x
		date
	done
	echo "########################################### "
fi


if [ "$full" = y ]
then
	echo " Running High Performance Computing Linpack Benchmark (HPL) ..."
	date
	set -x
	# need to cd so HPL.dat in current directory
	(cd $HPL_DIR; $MPI_RUN_CMD_HPL $HPL_CMD )
	set +x
	date
	echo "########################################### "
fi
} 2>&1 | tee -a -i $LOGFILE
