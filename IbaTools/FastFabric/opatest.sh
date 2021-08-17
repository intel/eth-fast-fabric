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
# perform installation verification on hosts in a cluster

# optional override of defaults
if [ -f /etc/eth-tools/ethfastfabric.conf ]
then
	. /etc/eth-tools/ethfastfabric.conf
fi

. /usr/lib/eth-tools/ethfastfabric.conf.def

. /usr/lib/eth-tools/ff_funcs

if [ x"$FF_IPOIB_SUFFIX" = xNONE ]
then
	export FF_IPOIB_SUFFIX=""
fi

# identify how we are being run, affects valid options and usage
mode=invalid
cmd=`basename $0`
case $cmd in
ethhostadmin) mode=$cmd;;
esac

if [ "$mode" = "invalid" ]
then
	echo "Invalid executable name for this file: $cmd; expected ethhostadmin" >&2
	exit 1
fi

Usage_ethhostadmin_full()
{
	echo "Usage: ethhostadmin [-c] [-f hostfile] [-h 'hosts'] [-r release] " >&2
#	echo "              [-i ipoib_suffix] [-m netmask]" >&2
	echo "              [-I install_options] [-U upgrade_options] [-d dir]" >&2
	echo "              [-T product] [-P packages] [-S] operation ..." >&2
	echo "              or" >&2
	echo "       ethhostadmin --help" >&2
	echo "  --help - produce full help text" >&2
	echo "  -c - clobber result files from any previous run before starting this run" >&2
#	echo "  -i ipoib_suffix - suffix to apply to host names to create ipoib host names" >&2
#	echo "                    default is '$FF_IPOIB_SUFFIX'" >&2
	echo "  -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
	echo "  -h hosts - list of hosts to execute operation against" >&2
	echo "  -r release - IntelEth release to load/upgrade to, default is $FF_PRODUCT_VERSION" >&2
	echo "  -d dir - directory to get product.release.tgz from for load/upgrade" >&2
	echo "  -I install_options - IntelEth install options" >&2
	echo "  -U upgrade_options - IntelEth upgrade options" >&2
	echo "  -T product - IntelEth product type to install" >&2
	echo "               default is $FF_PRODUCT" >&2
	echo "               Other options include: IntelEth-Basic.<distro>," >&2
	echo "                                      IntelEth-FS.<distro>" >&2
	echo "               Where <distro> is the distro and CPU, such as RHEL7-x86_64" >&2
	echo "  -P packages - IntelEth packages to install, default is '$FF_PACKAGES'" >&2
	echo "                See IntelEth INSTALL -C for a complete list of packages" >&2
	echo "  -S - securely prompt for password for user on remote system" >&2
	echo "  operation - operation to perform. operation can be one or more of:" >&2
	echo "              load - initial install of all hosts" >&2
	echo "              upgrade - upgrade install of all hosts" >&2
#	echo "              configipoib - create ifcfg-ib1 using host IP addr from /etc/hosts" >&2
	echo "              reboot - reboot hosts, ensure they go down and come back" >&2
	echo "              rping - verify this host can ping each host via RDMA" >&2
	echo "              pfctest - verify PFC works on all hosts" >&2
#	echo "              ipoibping - verify this host can ping each host via IPoIB" >&2
	echo "              mpiperf - verify latency and bandwidth for each host" >&2
	echo "              mpiperfdeviation - check for latency and bandwidth tolerance" >&2
	echo "                                 deviation between hosts" >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
	echo "   FF_MAX_PARALLEL - maximum concurrent operations" >&2
	echo "   FF_SERIALIZE_OUTPUT - serialize output of parallel operations (yes or no)" >&2
	echo "   FF_TIMEOUT_MULT - Multiplier for all timeouts associated with this command." >&2
	echo "                     Used if the systems are slow for some reason." >&2
	echo "for example:" >&2
	echo "   ethhostadmin -c reboot" >&2
	echo "   ethhostadmin upgrade" >&2
	echo "   ethhostadmin -h 'elrond arwen' reboot" >&2
	echo "   HOSTS='elrond arwen' ethhostadmin reboot" >&2
	echo "During run the following files are produced:" >&2
	echo "  test.res - appended with summary results of run" >&2
	echo "  test.log - appended with detailed results of run" >&2
	echo "  save_tmp/ - contains a directory per failed operation with detailed logs" >&2
	echo "  test_tmp*/ - intermediate result files while operation is running" >&2
	echo "-c option will remove all of the above" >&2
	exit 0
}
#Usage_opachassisadmin_full()
#{
#	echo "Usage: opachassisadmin [-c] [-F switchesfile] [-H 'switches'] " >&2
#	echo "              [-P packages] [-a action] [-I fm_bootstate]" >&2
#	echo "              [-S] [-d upload_dir] [-s securityfiles] operation ..." >&2
#	echo "              or" >&2
#	echo "       opachassisadmin --help" >&2
#	echo "  --help - produce full help text" >&2
#	echo "  -c - clobber result files from any previous run before starting this run" >&2
#	echo "  -F switchesfile - file with switches in cluster" >&2
#	echo "           default is $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
#	echo "  -H switches - list of switches to execute operation against" >&2
#	echo "  -P packages - filenames/directories of firmware" >&2
#	echo "                   images to install.  For directories specified, all" >&2
#	echo "                   .pkg, .dpkg and .spkg files in directory tree will be used." >&2
#	echo "                   shell wildcards may also be used within quotes." >&2
#	echo "                or for fmconfig, filename of FM config file to use" >&2
#	echo "                or for fmgetconfig, filename to upload to (default" >&2
#	echo "                   opafm.xml)" >&2
#	echo "  -a action - action for supplied file" >&2
#	echo "              For Switch upgrade:" >&2
#	echo "                 push   - ensure firmware is in primary or alternate" >&2
#	echo "                 select - ensure firmware is in primary" >&2
#	echo "                 run    - ensure firmware is in primary and running" >&2
#	echo "                 default is push" >&2
#	echo "              For Switch fmconfig:" >&2
#	echo "                 push   - ensure config file is in switch" >&2
#	echo "                 run    - after push, restart FM on master, stop on slave" >&2
#	echo "                 runall - after push, restart FM on all MM" >&2
#	echo "              For Switch fmcontrol:" >&2
#	echo "                 stop   - stop FM on all MM" >&2
#	echo "                 run    - make sure FM running on master, stopped on slave" >&2
#	echo "                 runall - make sure FM running on all MM" >&2
#	echo "                 restart- restart FM on master, stop on slave" >&2
#	echo "                 restartall- restart FM on all MM" >&2
#	echo "              For Switch fmsecurityfiles:" >&2
#	echo "                 push   - ensure FM security files are in switch" >&2
#	echo "                 restart- after push, restart FM on master, stop on slave" >&2
#	echo "                 restartall - after push, restart FM on all MM" >&2
#	echo "  -I fm_bootstate fmconfig and fmcontrol install options" >&2
#	echo "                 disable - disable FM start at switch boot" >&2
#	echo "                 enable - enable FM start on master at switch boot" >&2
#	echo "                 enableall - enable FM start on all MM at switch boot" >&2
#	echo "  -d upload_dir - directory to upload FM config files to, default is uploads" >&2
#	echo "  -S - securely prompt for password for admin on switches" >&2
#	echo "  -s securityFiles - security files to install, default is '*.pem'" >&2
#	echo "                For Switch fmsecurityfiles, filenames/directories of" >&2
#	echo "                   security files to install.  For directories specified," >&2
#	echo "                   all security files in directory tree will be used." >&2
#	echo "                   shell wildcards may also be used within quotes." >&2
#	echo "                or for Switch fmgetsecurityfiles, filename to upload to" >&2
#	echo "                   (default *.pem)" >&2
#	echo "  operation - operation to perform. operation can be one or more of:" >&2
#	echo "     reboot - reboot switch, ensure they go down and come back" >&2
#	echo "     configure - run wizard to set up switch configuration" >&2
#	echo "     upgrade - upgrade install of all switches" >&2
#	echo "     getconfig - get basic configuration of switch" >&2
#	echo "     fmconfig - FM config operation on all switches" >&2
#	echo "     fmgetconfig - Fetch FM config from all switches" >&2
#	echo "     fmcontrol - Control FM on all switches" >&2
#	echo "     fmsecurityfiles - FM security files operation on all switches" >&2
#	echo "     fmgetsecurityfiles - Fetch FM security files from all switches" >&2
#	echo " Environment:" >&2
#	echo "   SWITCHES - list of switches, used if -H and -F option not supplied" >&2
#	echo "   SWITCHES_FILE - file containing list of switches, used in absence of -F and -H" >&2
#	echo "   FF_MAX_PARALLEL - maximum concurrent operations" >&2
#	echo "   FF_SERIALIZE_OUTPUT - serialize output of parallel operations (yes or no)" >&2
#	echo "   FF_TIMEOUT_MULT - Multiplier for all timeouts associated with this command." >&2
#	echo "                     Used if the systems are slow for some reason." >&2
#	echo "   UPLOADS_DIR - directory to upload to, used in absence of -d" >&2
#	echo "for example:" >&2
#	echo "   opachassisadmin -c reboot" >&2
#	echo "   opachassisadmin -P /root/ChassisFw4.2.0.0.1 upgrade" >&2
#	echo "   opachassisadmin -H 'switch1 switch2' reboot" >&2
#	echo "   SWITCHES='switch1 switch2' opachassisadmin reboot" >&2
#	echo "   opachassisadmin -a run -P '*.pkg' upgrade" >&2
#	echo "During run the following files are produced:" >&2
#	echo "  test.res - appended with summary results of run" >&2
#	echo "  test.log - appended with detailed results of run" >&2
#	echo "  save_tmp/ - contains a directory per failed operation with detailed logs" >&2
#	echo "  test_tmp*/ - intermediate result files while operation is running" >&2
#	echo "-c option will remove all of the above" >&2
#	exit 0
#}
Usage_full()
{
	case $mode in
	ethhostadmin) Usage_ethhostadmin_full;;
	#opachassisadmin) Usage_opachassisadmin_full;;
	esac
}
Usage_ethhostadmin()
{
	echo "Usage: ethhostadmin [-c] [-f hostfile] [-r release] [-d dir]" >&2
	echo "              [-T product] [-P packages] [-S] operation ..." >&2
	echo "              or" >&2
	echo "       ethhostadmin --help" >&2
	echo "  --help - produce full help text" >&2
	echo "  -c - clobber result files from any previous run before starting this run" >&2
	echo "  -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
	echo "  -r release - IntelEth release to load/upgrade to, default is $FF_PRODUCT_VERSION" >&2
	echo "  -d dir - directory to get product.release.tgz from for load/upgrade" >&2
	echo "  -T product - IntelEth product type to install" >&2
	echo "               default is $FF_PRODUCT" >&2
	echo "               Other options include: IntelEth-Basic.<distro>, IntelEth-FS.<distro>" >&2
	echo "               Where <distro> is the distro and CPU, such as RHEL7-x86_64" >&2
	echo "  -P packages - IntelEth packages to install, default is '$FF_PACKAGES'" >&2
	echo "                See IntelEth INSTALL -C for a complete list of packages" >&2
	echo "  -S - securely prompt for password for user on remote system" >&2
	echo "  operation - operation to perform. operation can be one or more of:" >&2
	echo "              load - initial install of all hosts" >&2
	echo "              upgrade - upgrade install of all hosts" >&2
#	echo "              configipoib - create ifcfg-ib1 using host IP addr from /etc/hosts" >&2
	echo "              reboot - reboot hosts, ensure they go down and come back" >&2
	echo "              rping - verify this host can ping each host via RDMA" >&2
	echo "              pfctest - verify PFC works on all hosts" >&2
#	echo "              ipoibping - verify this host can ping each host via IPoIB" >&2
	echo "              mpiperf - verify latency and bandwidth for each host" >&2
	echo "              mpiperfdeviation - check for latency and bandwidth tolerance" >&2
	echo "                        	       deviation between hosts" >&2
	echo "for example:" >&2
	echo "   ethhostadmin  -c reboot" >&2
	echo "   ethhostadmin  upgrade" >&2
	echo "During run the following files are produced:" >&2
	echo "  test.res - appended with summary results of run" >&2
	echo "  test.log - appended with detailed results of run" >&2
	echo "  save_tmp/ - contains a directory per failed test with detailed logs" >&2
	echo "  test_tmp*/ - intermediate result files while test is running" >&2
	echo "-c option will remove all of the above" >&2
	exit 2
}
#Usage_opachassisadmin()
#{
#	echo "Usage: opachassisadmin [-c] [-F switchesfile] " >&2
#	echo "              [-P packages] [-I fm_bootstate] [-a action]" >&2
#    echo "              [-S] [-d upload_dir] [-s securityfiles] operation ..." >&2
#	echo "              or" >&2
#	echo "       opachassisadmin --help" >&2
#	echo "  --help - produce full help text" >&2
#	echo "  -c - clobber result files from any previous run before starting this run" >&2
#	echo "  -F switchesfile - file with switches in cluster" >&2
#	echo "           default is $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
#	echo "  -P packages - filenames/directories of firmware" >&2
#	echo "                   images to install.  For directories specified, all" >&2
#	echo "                   .pkg, .dpkg and .spkg files in directory tree will be used." >&2
#	echo "                   shell wildcards may also be used within quotes." >&2
#	echo "                or for fmconfig, filename of FM config file to use" >&2
#	echo "                or for fmgetconfig, filename to upload to (default" >&2
#	echo "                   opafm.xml)" >&2
#	echo "  -a action - action for supplied file" >&2
#	echo "              For Switch upgrade:" >&2
#	echo "                 push   - ensure firmware is in primary or alternate" >&2
#	echo "                 select - ensure firmware is in primary" >&2
#	echo "                 run    - ensure firmware is in primary and running" >&2
#	echo "                 default is push" >&2
#	echo "              For Switch fmconfig:" >&2
#	echo "                 push   - ensure config file is in switch" >&2
#	echo "                 run    - after push, restart FM on master, stop on slave" >&2
#	echo "                 runall - after push, restart FM on all MM" >&2
#	echo "              For Switch fmcontrol:" >&2
#	echo "                 stop   - stop FM on all MM" >&2
#	echo "                 run    - make sure FM running on master, stopped on slave" >&2
#	echo "                 runall - make sure FM running on all MM" >&2
#	echo "                 restart- restart FM on master, stop on slave" >&2
#	echo "                 restartall- restart FM on all MM" >&2
#	echo "              For Switch fmsecurityfiles:" >&2
#	echo "                 push   - ensure FM security files are in switch" >&2
#	echo "                 restart- after push, restart FM on master, stop on slave" >&2
#	echo "                 restartall - after push, restart FM on all MM" >&2
#	echo "  -I fm_bootstate fmconfig and fmcontrol install options" >&2
#	echo "                 disable - disable FM start at switch boot" >&2
#	echo "                 enable - enable FM start on master at switch boot" >&2
#	echo "                 enableall - enable FM start on all MM at switch boot" >&2
#	echo "  -d upload_dir - directory to upload FM config files to, default is uploads" >&2
#	echo "  -S - securely prompt for password for admin on switches" >&2
#	echo "  -s securityFiles - security files to install, default is '*.pem'" >&2
#	echo "                For Switch fmsecurityfiles, filenames/directories of" >&2
#	echo "                   security files to install.  For directories specified," >&2
#	echo "                   all security files in directory tree will be used." >&2
#	echo "                   shell wildcards may also be used within quotes." >&2
#	echo "                or for Switch fmgetsecurityfiles, filename to upload to" >&2
#	echo "                   (default *.pem)" >&2
#	echo "  operation - operation to perform. operation can be one or more of:" >&2
#	echo "     reboot - reboot switch, ensure they go down and come back" >&2
#	echo "     configure - run wizard to set up switch configuration" >&2
#	echo "     upgrade - upgrade install of all switches" >&2
#	echo "     getconfig - get basic configuration of switch" >&2
#	echo "     fmconfig - FM config operation on all switches" >&2
#	echo "     fmgetconfig - Fetch FM config from all switches" >&2
#	echo "     fmcontrol - Control FM on all switches" >&2
#	echo "     fmsecurityfiles - FM security files operation on all switches" >&2
#	echo "     fmgetsecurityfiles - Fetch FM security files from all switches" >&2
#	echo "for example:" >&2
#	echo "   opachassisadmin -c reboot" >&2
#	echo "   opachassisadmin -P /root/ChassisFw4.2.0.0.1 upgrade" >&2
#	echo "   opachassisadmin -a run -P '*.pkg' upgrade" >&2
#	echo "During run the following files are produced:" >&2
#	echo "  test.res - appended with summary results of run" >&2
#	echo "  test.log - appended with detailed results of run" >&2
#	echo "  save_tmp/ - contains a directory per failed operation with detailed logs" >&2
#	echo "  test_tmp*/ - intermediate result files while operation is running" >&2
#	echo "-c option will remove all of the above" >&2
#	exit 2
#}
Usage()
{
	case $mode in
	ethhostadmin) Usage_ethhostadmin;;
	#opachassisadmin) Usage_opachassisadmin;;
	esac
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

# default to install wrapper version
if [ -e /etc/eth-tools/version_wrapper ]
then
	CFG_RELEASE=`cat /etc/eth-tools/version_wrapper 2>/dev/null`;
fi
if [ x"$CFG_RELEASE" = x ]
then
# if no wrapper, use version of FF itself as filled in at build time
# version string is filled in by prep, special marker format for it to use
CFG_RELEASE="THIS_IS_THE_ICS_VERSION_NUMBER:@(#)000.000.000.000B000"
fi
export CFG_RELEASE=`echo $CFG_RELEASE|sed -e 's/THIS_IS_THE_ICS_VERSION_NUMBER:@(#.//' -e 's/%.*//'`
# THIS_IS_THE_ICS_INTERNAL_VERSION_NUMBER:@(#)000.000.000.000B000
# test automation configuration defaults
export CFG_INIC_SUFFIX=
#export CFG_IPOIB_SUFFIX="$FF_IPOIB_SUFFIX"
export CFG_USERNAME="$FF_USERNAME"
export CFG_PASSWORD="$FF_PASSWORD"
export CFG_ROOTPASS="$FF_ROOTPASS"
export CFG_LOGIN_METHOD="$FF_LOGIN_METHOD"
export CFG_SWITCH_LOGIN_METHOD="$FF_SWITCH_LOGIN_METHOD"
export CFG_SWITCH_ADMIN_PASSWORD="$FF_SWITCH_ADMIN_PASSWORD"
export CFG_FAILOVER="n"
export CFG_FTP_SERVER=""
#export CFG_IPOIB="y"
#export CFG_IPOIB_MTU="2030"
#export CFG_IPOIB_COMBOS=TBD
export CFG_INIC=n
export CFG_SDP=n
export CFG_SRP=n
export CFG_MPI=y
export CFG_UDAPL=n
export TEST_TIMEOUT_MULT="$FF_TIMEOUT_MULT"
export TEST_RESULT_DIR="$FF_RESULT_DIR"
export TEST_MAX_PARALLEL="$FF_MAX_PARALLEL"
export TEST_CONFIG_FILE="/dev/null"
export TL_DIR=/usr/lib/eth-tools
export TEST_IDENTIFY=no
export TEST_SHOW_CONFIG=no
export TEST_SHOW_START=yes
export CFG_PRODUCT="${FF_PRODUCT:-IntelEth-Basic}"
export CFG_INSTALL_OPTIONS="$FF_INSTALL_OPTIONS"
export CFG_UPGRADE_OPTIONS="$FF_UPGRADE_OPTIONS"
#export CFG_IPOIB_NETMASK="$FF_IPOIB_NETMASK"
#export CFG_IPOIB_CONNECTED="$FF_IPOIB_CONNECTED"
export CFG_MPI_ENV="$FF_MPI_ENV"
export TEST_SERIALIZE_OUTPUT="$FF_SERIALIZE_OUTPUT"

clobber=n
host=0
dir=.
packages="notsupplied"
action=default
Sopt=n
sopt=n
securityFiles="notsupplied"
case $mode in
ethhostadmin) host=1; options='cd:h:f:r:I:U:P:T:S';;
#ethhostadmin) host=1; options='cd:h:f:i:r:I:U:P:T:m:S';;
#opachassisadmin) switches=1; options='a:I:cH:F:P:d:Ss:';;
esac
while getopts "$options"  param
do
	case $param in
	a)
		action="$OPTARG";;
	c)
		clobber=y;;
	d)
		dir="$OPTARG"
		export UPLOADS_DIR="$dir";;
	h)
		host=1
		HOSTS="$OPTARG";;
#	H)
#		switches=1
#		SWITCHES="$OPTARG";;
	f)
		host=1
		HOSTS_FILE="$OPTARG";;
#	F)
#		switches=1
#		SWITCHES_FILE="$OPTARG";;
#	i)
#		export CFG_IPOIB_SUFFIX="$OPTARG"
#		export FF_IPOIB_SUFFIX="$OPTARG";;
	r)
		export FF_PRODUCT_VERSION="$OPTARG";;
	I)
		export CFG_INSTALL_OPTIONS="$OPTARG";;
	U)
		export CFG_UPGRADE_OPTIONS="$OPTARG";;
	P)
		packages="$OPTARG";;
	T)
		export CFG_PRODUCT="$OPTARG";;
#	m)
#		export CFG_IPOIB_NETMASK="$OPTARG";;
	p)
		export PORTS="$OPTARG";;
	t)
		export PORTS_FILE="$OPTARG";;
	s)
		securityFiles="$OPTARG";;
	S)
		Sopt=y;;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))

if [ $# -lt 1 ] 
then
	Usage
fi

for tkn in $*
do
	if [[ $tkn == -* ]]
	then
		Usage
	fi
done
# given mode checks, this error should not be able to happen
if [[ $(($host)) -eq 0 ]]
then
	host=1
fi
if [ ! -d "$FF_RESULT_DIR" ]
then
	echo "$cmd: Invalid FF_RESULT_DIR: $FF_RESULT_DIR: No such directory" >&2
	exit 1
fi

check_host_args $cmd

if [ "$packages" = "notsupplied" ]
then
	packages="$FF_PACKAGES"
fi
if [ "x$packages" != "x" ]
then
	for p in $packages
	do
		CFG_INSTALL_OPTIONS="$CFG_INSTALL_OPTIONS -i $p"
	done
fi
if [ "x$CFG_INSTALL_OPTIONS" = "x" ]
then
	CFG_INSTALL_OPTIONS='-i eth -i eth_rdma'
fi

export CFG_HOSTS="$HOSTS"
cfg_host_ports=""
for host in $HOSTS; do
	if [ -z "$cfg_host_ports" ]; then
		cfg_host_ports="${host}:$(get_node_ports "$host")"
	else
		cfg_host_ports="${cfg_host_ports};${host}:$(get_node_ports "$host")"
	fi
done
export CFG_HOST_PORTS="${cfg_host_ports}"

export CFG_MPI_PROCESSES="$HOSTS"
#export CFG_PERF_PAIRS=TBD
export CFG_SCPFROMDIR="$dir"
if [ x"$FF_PRODUCT_VERSION" != x ]
then
	CFG_RELEASE="$FF_PRODUCT_VERSION"
fi

## use NONE so ff_function's inclusion of defaults works properly
#if [ x"$FF_IPOIB_SUFFIX" = x ]
#then
#	export FF_IPOIB_SUFFIX="NONE"
#	export CFG_IPOIB_SUFFIX="NONE"
#fi

if [ "$clobber" = "y" ]
then
	( cd $TEST_RESULT_DIR; rm -rf test.res save_tmp test.log test_tmp* *.dmp )
fi

# create an empty test.log file
( cd $TEST_RESULT_DIR; >> test.log )

run_test()
{
	# $1 = test suite name
	TCLLIBPATH="$TL_DIR /usr/lib/tcl" expect -f $TL_DIR/$1.exp | tee -a $TEST_RESULT_DIR/test.res
}

if [ "$Sopt" = y ]
then
	read -sp "Password for $CFG_USERNAME on all hosts: " password
	echo
	export CFG_PASSWORD="$password"
	if [ "$CFG_USERNAME" != "root" ]
	then
		read -sp "Password for root on all hosts: " password
		echo
		export CFG_ROOTPASS="$password"
	else
		export CFG_ROOTPASS="$CFG_PASSWORD"
	fi
fi
for test_suite in $*
do
	case $test_suite in
	load|upgrade)
		if [ ! -f "$dir/$CFG_PRODUCT.$CFG_RELEASE.tgz" ]
		then
			echo "$cmd: $dir/$CFG_PRODUCT.$CFG_RELEASE.tgz not found" >&2
			exit 1
		fi
		run_test $test_suite;;
#	reboot|sacache|configipoib|ipoibping|mpiperf|mpiperfdeviation)
	reboot|rping|pfctest|mpiperf|mpiperfdeviation)
		run_test $test_suite;;
	*)
		echo "Invalid Operation name: $test_suite" >&2
		Usage;
		;;
	esac
done
