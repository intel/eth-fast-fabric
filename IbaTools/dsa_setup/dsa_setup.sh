#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2022, Intel Corporation
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

# Setup the Xeon Data Streaming Accelerator (DSA) WorkQueues.
# The Data Streaming Accelerator may be used by PSM3 to assist
# in intra-node data communications.  The number of DSA devices and
# DSA engines per device varies with the Xeon CPU SKU selected.  In some
# cases CPU soft-keys may allow upgrades to DSA resources in a given CPU model.

# This sample script can be copied and edited as appropriate for the system.
# The resulting script must be run as root to configure DSA work queues each
# time the system reboots or immediately prior to and after each job which will
# use PSM3 with DSA enabled. When configuring DSA work queues, this script will
# remove all existing DSA queues, so if run per job, it should only be used
# when no other applications are using DSA.

# For information on having dsa_setup be run at boot time by systemd, see
# /usr/share/eth-tools/samples/dsa.service

# If the DSA configuration is to be selected per job, this script
# may be used in post job processing with the -w none or -w restart
# options to remove DSA resources after the job finishes.  Then at the
# start of the next job, the appropriate -w workload option can be provided.
# The use of restart is only required on some older distros, such
# as RHEL 8.6, to fully clear out DSA resources.
# Beware, use of restart may affect other applications which are using any
# of the CPU accelerators managed by the idxd kernel driver.

# As given this sample will permit use of DSA by PSM3 in environments
# with a small number of processes per node, such as AI training or some
# HPC use cases.
# This script may need to be modified if DSA will also be used by other
# applications or services on the given server.

# PSM3 requires dedicated DSA work queues.
# A dedicated work queue may only be used by a single process, but can be
# shared across multiple threads within the process.
# Workqueues are created in /dev/dsa and named wq#.#.  For example,
# /dev/dsa/wq2.0 represents the first work queue on DSA device 2 (dsa2).

# Samples below use the setup_dsa() function found later in this script.
# The arguments to setup_dsa are shown in the setup_dsa_usage() function below.
# Note that setup_dsa() must be run in a sub-shell via ( setup_dsa ... ).

# A unique setup_all_$workload function appears below per workload.
# Workloads may be selected using the -w option to this script (see Usage()).
# DSA configurations for additional workloads may be easily added by simply
# adding additional functions here and then selecting them with the -w option.
# As needed these sample workload configuration functions could also be edited.

# This variable is only used for Usage() help text.  When adding a new
# setup_all_WORKLOAD function, also add WORKLOAD to this list so it appears
# in help text.
workloads="ai hpc shared none restart"

# This sample config is well suited to low process per node workloads such as
# AI Training where each process may have multiple communication threads
# (such as multiple oneCCL worker threads).  It can also be useful in
# HPC jobs where there are very few processes per CPU socket
# (processes_per_node <= DSA_devices_per_node).
#
# This sample removes all previous DSA workqueues and creates
# 1 dedicated work queue per DSA device. The work queues created will
# make use of all DSA engines within the corresponding DSA device.
setup_all_ai()
{
	# for each DSA device found in the system
	for dsa in $(list_dsa_devices)
	do
		echo "Removing all work queues on DSA device $dsa"
		( setup_dsa -d$dsa )	# clear previous settings
		echo "Creating 1 dedicated work queue on DSA device $dsa"
		( setup_dsa -d$dsa -w1 -md )	# 1 dedicated work queue w/ all engines
	done
}

# This sample config is well suited to medium process per node workloads such as
# multi-threaded HPC apps where there are only a few processes per node.
# (processes_per_node <= DSA_devices_per_node * DSA_engines_per_device).
#
# This sample removes all previous DSA workqueues and creates
# 1 dedicated work queue per DSA engine. Each work queue created will
# each use only a single DSA engine within the corresponding DSA device.
# The result is more DSA workqueues, and hence the ability to run more processes
# using PSM3.
setup_all_hpc()
{
	# for each DSA device found in the system
	for dsa in $(list_dsa_devices)
	do
		echo "Removing all work queues on DSA device $dsa"
		( setup_dsa -d$dsa )	# clear previous settings
		# create a Dedicated Work Queue (DWQ) per Engine.
		engines=`cat $DSA_CONFIG_PATH/$dsa/max_engines`
		wq_size=`cat $DSA_CONFIG_PATH/$dsa/max_work_queues_size`
		size=$(( $wq_size / $engines ))
		echo "Creating $engines dedicated work queues on DSA device $dsa"
		for wq in $(seq 0 $(($engines -1)))
		do
			( setup_dsa -d$dsa -w1 -md -g$wq -q$wq -s $size -e1 -b0 )
		done
		( setup_dsa -d$dsa -b1 )	# bind the WQs to engines and start them
	done
}

# This sample config is well suited to Multiple clients or when the number of 
# processes per server exceeds the total number of DSA engines available. 
# Also useful for some middlewares which only implement use of shared DSA WQs. 
#
# This sample removes all previous DSA workqueues and creates
# 1 shared work queue per DSA device. The work queues created will
# make use of all DSA engines within the corresponding DSA device.
setup_all_shared()
{
	# for each DSA device found in the system
	for dsa in $(list_dsa_devices)
	do
		echo "Removing all work queues on DSA device $dsa"
		( setup_dsa -d$dsa )	# clear previous settings

		engines=`cat $DSA_CONFIG_PATH/$dsa/max_engines`
		wq_size=`cat $DSA_CONFIG_PATH/$dsa/max_work_queues_size`
		echo "Creating 1 shared work queue on DSA device $dsa"
		( setup_dsa -d$dsa -w1 -ms -s$wq_size -e$engines )
	done
}

# -----------------------------------------------------------------------------
# Do not edit code below.

# This removes all previous DSA workqueues
# Do not edit this function
setup_all_none()
{
	# for each DSA device found in the system
	for dsa in $(list_dsa_devices)
	do
		echo "Removing all work queues on DSA device $dsa"
		( setup_dsa -d$dsa )	# clear previous settings
	done
}

# This removes all previous DSA WQs by restarting the idxd kernel driver.
# On some older distros, such as RHEL 8.6, restart of the idxd driver is
# sometimes needed when reconfiguring DSA workqueues.
# Beware, this restart may affect other applications which are using any
# of the CPU accelerators managed by the idxd kernel driver.
# Do not edit this function
setup_all_restart()
{
	# must remove WQs so can restart driver
	setup_all_none

	echo "Resetting all DSA resources by restarting idxd kernel driver"
	rmmod idxd
	modprobe idxd
}




# -----------------------------------------------------------------------------
# Do not edit code below.

DSA_CONFIG_PATH=/sys/bus/dsa/devices
DSA_DEV_PATH=/dev/dsa

list_dsa_devices()
{
	ls $DSA_CONFIG_PATH 2>/dev/null| grep  "dsa[0-9]\+"
}

# if there are configurable DSA devices return success (0)
# This depends on idxd driver having loaded and enumerated the devices
have_dsa_devices()
{
	if [ ! -e $DSA_CONFIG_PATH ] || [ $(list_dsa_devices | wc -l) -eq 0 ]
	then
		return 1
	else
		return 0
	fi
}

init_common() {
	NUM_ENGINES=`cat /sys/bus/dsa/devices/$dname/max_engines`
	NUM_WQS=`cat /sys/bus/dsa/devices/$dname/max_work_queues`
	DEV_DRV_PATH=/sys/bus/dsa/drivers/dsa
	WQ_DRV_PATH=$DEV_DRV_PATH
	[ -d /sys/bus/dsa/drivers/idxd ] && DEV_DRV_PATH=/sys/bus/dsa/drivers/idxd
	[ -d /sys/bus/dsa/drivers/user ] && WQ_DRV_PATH=/sys/bus/dsa/drivers/user
}

reset_config() {
	local did=$1

	for ((i = 0; i < $NUM_ENGINES ; i++ ))
	do
		echo -1 > $DSA_CONFIG_PATH/$dname/engine$did.$i/group_id
	done
	for ((i = 0; i < $NUM_WQS ; i++ ))
	do
		echo 0 > $DSA_CONFIG_PATH/$dname/wq$did.$i/size
	done
}

assign_free_engine() {

	local did=$1
	local gid=$2

	for ((i = 0; i < $NUM_ENGINES ; i++ ))
	do
		if (( `cat $DSA_CONFIG_PATH/$dname/engine$did.$i/group_id` == -1 ))
		then
			echo $gid > $DSA_CONFIG_PATH/$dname/engine$did.$i/group_id
			return 0
		fi
	done

	echo "Unable to find free engine"
	exit 1
}

# Usage for the setup_dsa function below
setup_dsa_usage() {
    cat <<HELP_USAGE

    usage: setup_dsa [-d device (dsa0/iax1/..) ] [-w num wqs] [-q wq id] [-m wq mode (d or s)] [-s wq sz] [-e num eng] [-g grpID] [-b bind or not? (0 or 1)]
		configures wqs
	   E.g. Setup dsa0 with 1 group/1DWQ/1eng :
			 setup_dsa -d dsa0 -w1 -md -e1
		Setup dsa0 with 2 groups: grp1/1DWQ-64qd/1eng, grp2/1SWQ-64qd/1eng
			 setup_dsa -d dsa0 -g0 -w1 -q0 -s64 -md -e1 -b0
			 setup_dsa -d dsa0 -g1 -w1 -q1 -s64 -ms -e1 -b0
			 setup_dsa -d dsa0 -b1

	   setup_dsa [-d device]
		disables device and resets config

	   setup_dsa <config file path>
HELP_USAGE
	exit 0
}

unbind() {

	case $1 in

	0)
		for ((i = 0; i < $NUM_WQS ; i++ ))
		do
			echo wq$did.$i > $WQ_DRV_PATH/unbind 2>/dev/null && echo disabled wq$did.$i
		done

		echo $dname  > $DEV_DRV_PATH/unbind 2>/dev/null && echo disabled $dname
		reset_config $did

		;;

	1)

		readarray -d a  -t tmp <<< "$dname"
		d=`echo ${tmp[1]}`

		for i in {0..7}
		do
			[[ `cat /sys/bus/dsa/devices/$dname/wq$d\.$i/state` == "enabled" ]] && sudo accel-config disable-wq $dname/wq$d\.$i
		done

		[[ `cat /sys/bus/dsa/devices/$dname/state` == "enabled" ]] && sudo accel-config disable-device $dname
		reset_config $d

		;;

	*)
		echo "unknown"
		;;
	esac
}

configure() {

	case $1 in

	0)
		for ((i = 0; i < $num_eng ; i++ ))
		do
			assign_free_engine $did $grp_id
		done

		q=$qid
		[[ "$q" == "-1" ]] && q=0

		for ((i = 0; i < $num_wq ; i++, q++ ))
		do
			[ -d $DSA_CONFIG_PATH/$dname/wq$did.$q/ ] && wq_dir=$DSA_CONFIG_PATH/$dname/wq$did.$q/
			[ -d $DSA_CONFIG_PATH/wq$did.$q/ ] && wq_dir=$DSA_CONFIG_PATH/wq$did.$q/

			echo 0 > $wq_dir/block_on_fault
			echo $grp_id > $wq_dir/group_id
			echo $mode > $wq_dir/mode
			echo 10 > $wq_dir/priority
			echo $size > $wq_dir/size
			[[ $mode == shared ]] && echo $size > $wq_dir/threshold
			echo "user" > $wq_dir/type
			[ -f $wq_dir/driver_name ] && echo "user" > $wq_dir/driver_name
			echo "app$i"  > $wq_dir/name
		done
		;;

	1)
		sudo accel-config load-config -c $config
		;;

	*)
		echo "Unknown"
		;;
	esac
}

bind() {
	# start devices
	case $1 in
	0)
		echo $dname  > $DEV_DRV_PATH/bind && echo enabled $dname

		for i in {0..7}
		do
			[[ `cat /sys/bus/dsa/devices/$dname/wq$did\.$i/size` -ne "0" ]] && echo wq$did.$i > $WQ_DRV_PATH/bind && echo enabled wq$did.$i
		done
		;;
	1)
		sudo accel-config enable-device  $dname

		for i in {0..7}
		do
			[[ `cat /sys/bus/dsa/devices/$dname/wq$d\.$i/size` -ne "0" ]] && sudo accel-config enable-wq $dname/wq$d\.$i
		done
		;;
	*)
		echo "Unknown"
		;;
	esac
}

do_config_file() {

	config=$1
	dname=`cat $config  | grep \"dev\":\"dsa  | cut -f2 -d: | cut -f1 -d, | sed -e s/\"//g`
	init_common

	unbind 1
	configure 1
	bind 1
}

do_options() {
	num_wq=0
	num_eng=4
	grp_id=0
	wsz=0
	qid=-1
	do_bind=-1
	do_cfg=-1
	mode=d

	if [[ ! $@ =~ ^\-.+ ]]
	then
	setup_dsa_usage
	fi

	while getopts d:w:m:e:g:s:b:c:q: flag
	do
	    case "${flag}" in
		d)
			dname=${OPTARG}
			did=`echo $dname | awk '{print substr($0,4)}'`
			;;
		w)
			num_wq=${OPTARG}
			;;
		e)
			num_eng=${OPTARG}
			;;
		g)
			grp_id=${OPTARG}
			;;
		m)
			mode=${OPTARG}
			;;
		s)
			wsz=${OPTARG}
			;;
		b)
			do_bind=${OPTARG}
			;;
		c)
			do_cfg=${OPTARG}
			;;
		q)
			qid=${OPTARG}
			;;
		:)
			echo 1
			setup_dsa_usage >&2
			;;
		*)
			echo 2
			setup_dsa_usage >&2
			;;
	    esac
	done

	init_common

	if (( $qid >= $NUM_WQS ))
	then
		echo "Queue num should be less than $NUM_WQS" && exit 1
	fi

	if (( $qid != -1 && wsz == 0 ))
	then
		echo "Missing WQ size"
		setup_dsa_usage && exit
	fi

	if (( $qid != -1 ))
	then
		num_wq=1
	fi

	if (( $num_wq != 0 ))
	then
		do_cfg=1
	fi

	if (( $do_cfg == 1 && $do_bind == -1 ))
	then
		unbind 0
		do_bind=1
	fi

	[ -d /sys/bus/dsa/devices/$dname ] || { echo "Invalid dev name $dname" && exit 1; }

	if (( $do_bind != -1 || $do_cfg != -1  ))
	then
		[[ $mode == "d" ]] && mode=dedicated
		[[ $mode == "s" ]] && mode=shared

		if (( $wsz == 0 && num_wq != 0 ))
		then
			wq_size=`cat /sys/bus/dsa/devices/$dname/max_work_queues_size`
			size=$(( wq_size / num_wq ))
		else
			size=$wsz
		fi

		[[ "$do_cfg" == "1" ]] && configure 0
		[[ "$do_bind" == "1" ]] && bind 0
	else
		unbind 0
	fi
}

setup_dsa()
{
	if [ $# -eq "0" ]
	then
		setup_dsa_usage
	elif [ -f "$1" ]
	then
		do_config_file $1
	else
		do_options $@
	fi
}


show_dsa_resources()
{
	echo "-----------------------------------------------------------------------------"
	echo "Summary of DSA Configuration:"
	devdir=/sys/bus/dsa/devices
	num_dsa=$(list_dsa_devices | wc -l)
	num_wq=0 # counted below
	for dsa in $(list_dsa_devices)
	do
		dsadir=$DSA_CONFIG_PATH/$dsa
		dsanum=$(echo "$dsa"|sed -e 's/dsa//')
		numa=`cat $dsadir/numa_node`
		engines=`cat $dsadir/max_engines`
		wq_size=`cat $dsadir/max_work_queues_size`
		echo "$dsa: NUMA: $numa Engines: $engines Max Total WQ Entries: $wq_size"
		for wq in $(ls $dsadir/ | grep "wq[0-9.]\+")
		do
			wqdir=$dsadir/$wq
			mode=`cat $wqdir/mode`
			group_id=`cat $wqdir/group_id`
			size=`cat $wqdir/size`
			if [ "$size" != 0 ] && [ "$group_id" != -1 ]
			then
				num_wq=$(($num_wq + 1))
				groupdir=$dsadir/group$dsanum.$group_id
				if [ -e $groupdir/engines ]
				then
					wq_engines=$(cat $groupdir/engines|wc -w)
				else
					wq_engines="unknown"
				fi
				echo "    $wq: Mode: $mode Size: $size Group: $group_id Engines: $wq_engines"
			fi
		done
	done
	echo "$num_dsa DSA Devices, $num_wq Total Work Queues"
	echo "-----------------------------------------------------------------------------"
	if [ ! -e $DSA_DEV_PATH ] || [ $(ls $DSA_DEV_PATH/wq* 2>/dev/null | wc -l) -eq 0 ]
	then
		echo "No DSA Work Queues"
	else
		echo "DSA Work Queues:"
		ls -l $DSA_DEV_PATH/wq*
	fi
}


wait_dsa_devices()
{
	# when dsa_setup is used during startup the idxd driver may not yet be
	# done enumerating devices, in which case we may find no devices
	# and need to wait
	wait=$1
	while [ $wait -gt 0 ] && ! have_dsa_devices
	do
		sleep 1
		wait=$(($wait - 1))
	done
	if ! have_dsa_devices
	then
		echo "$BASENAME: No DSA devices found" >&2
		exit 1
	fi
}

# Main Program

readonly BASENAME="$(basename $0)"

Usage()
{
	echo "Usage: $BASENAME [-u user] [-w workload] [-T timelimit]" >&2
	echo "              or" >&2
	echo "       $BASENAME --list" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help        - produce full help text" >&2
	echo "   --list        - show DSA resources and configuration" >&2
	echo "   -w workload   - configure DSA work queues for workload (default 'ai')" >&2
	echo "                   When run to configure DSA work queues, must be run as root."  >&2
	echo "                   Workloads may be added by adding setup_all_WORKLOAD functions" >&2
	echo "                   Valid workload values: $workloads" >&2
	echo "   -u user       - owner for DSA work queue devices (default root)" >&2
	echo "                   Specified as [owner][:[group]] similar to chown command." >&2
	echo "                   If a : is specified, the queues will have group and owner rw" >&2
	echo "                   access, otherwise just owner rw access. If a : is specified" >&2
	echo "                   without an explicit group, the user's group is used." >&2
	echo "                   If specified as 'all' then DSA queues are created with" >&2
	echo "                   rw access for anyone to use." >&2
	echo "   -T timelimit  - seconds to wait for DSA device discovery.  Default 0" >&2
	echo "                   Sometimes during boot a non-zero timeout is needed to allow" >&2
	echo "                   time for the idxd kernel driver to discover and enumerate" >&2
	echo "                   the devices" >&2
	echo >&2
	echo "Create Data Streaming Accelerator Work Queues in $DSA_DEV_PATH for use by" >&2
	echo "PSM3 jobs."  >&2
	echo >&2
	echo "examples:">&2
	echo "   ${BASENAME} --help" >&2
	echo "   ${BASENAME} --list" >&2
	echo "   ${BASENAME}" >&2
	echo "   ${BASENAME} -u myname -w ai" >&2
	echo "   ${BASENAME} -u myname: -w ai" >&2
	echo "   ${BASENAME} -u myname:mygroup -w hpc" >&2
	echo "   ${BASENAME} -u :mygroup -w hpc" >&2
	echo "   ${BASENAME} -w none" >&2
	echo "   ${BASENAME} -w restart" >&2
}

if [ x"$1" = "x--help" ]
then
	Usage
	exit 0
fi

if [ x"$1" = "x--list" ]
then
	wait_dsa_devices 0
	show_dsa_resources
	exit 0
fi

if [ `/usr/bin/id -u` != 0 ]
then
	echo "$BASENAME: You must be 'root' to run this tool"
	exit 1
fi

user=root
workload=ai
timelimit=0
while getopts u:w:T: param
do
    case "$param" in
	u) user="$OPTARG";;
	w) workload="$OPTARG";;
	T) timelimit="$OPTARG";;
	?) Usage; exit 2;;
	esac
done
shift $((OPTIND -1))
if [ $# -ge 1 ]
then
    Usage; exit 2
fi
if ! type "setup_all_$workload" 2>/dev/null|grep function >/dev/null
then
	echo "Unsupported workload: '$workload'" >&2
    Usage; exit 2
fi
OPTIND=1 # reset so setup_dsa can use getopt

wait_dsa_devices $timelimit

echo "Configuring DSA work queues for $workload:"
setup_all_$workload

# adjust ownership and permissions based on -u option
if [ -e $DSA_DEV_PATH ] && [ $(ls $DSA_DEV_PATH/wq* 2>/dev/null| wc -l) -gt 0 ]
then
	if [ x"$user" = x"all" ]
	then
		# wide open access
		chmod go+rw $DSA_DEV_PATH/wq*
	else
		chown $user $DSA_DEV_PATH/wq*
		if echo "$user"|grep ':' >/dev/null
		then
			# enable access for all in group
			chmod g+rw $DSA_DEV_PATH/wq*
		fi
	fi
fi

echo "Created DSA work queues for $workload:"
show_dsa_resources
exit 0
