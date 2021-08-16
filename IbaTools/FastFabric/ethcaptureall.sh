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
# perform an ethcapture on all hosts/switches and upload to this host

# optional override of defaults
if [ -f /etc/eth-tools/ethfastfabric.conf ]
then
	. /etc/eth-tools/ethfastfabric.conf
fi

. /usr/lib/eth-tools/ethfastfabric.conf.def

. /usr/lib/eth-tools/ff_funcs

trap "exit 1" SIGHUP SIGTERM SIGINT

readonly BASENAME="$(basename $0)"

Usage_full()
{
#	echo "Usage: $BASENAME [-Cp] [-f hostfile] [-F switchesfile]" >&2
#	echo "                  [ -h 'hosts'] [-H 'switches'] [-t portsfile]" >&2
#	echo "                  [-S] [-D detail_level] [file]" >&2
	echo "Usage: $BASENAME [-p] [-f hostfile] [ -h 'hosts'] [-d upload_dir] " >&2
	echo "                  [-D detail_level] [file]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
#	echo "   -C - perform capture against switches, default is hosts" >&2
#	echo "   -p - perform capture upload in parallel on all hosts/switches" >&2
	echo "   -p - perform capture upload in parallel on all hosts" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
#	echo "   -F switchesfile - file with switches in cluster" >&2
#	echo "           default is $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
	echo "   -h hosts - list of hosts to perform capture of" >&2
#	echo "   -H switches - list of switches to perform capture of" >&2
#	echo "   -t portsfile - file with list of local HFI ports used to access" >&2
#	echo "           fabric(s) for switch access, default is /etc/$FF_PRD_NAME/ports" >&2
	echo "   -d upload_dir - directory to upload to, default is uploads" >&2
#	echo "   -S - securely prompt for password for admin on switches" >&2
	echo "   -D detail_level - level of detail passed to host ethcapture" >&2
	echo "           1-Local 2-Fabric 3-Analysis" >&2
	echo "   file - name for capture file [.tgz will be appended]" >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
#	echo "   SWITCHES - list of switches, used if -C used and -h option not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
#	echo "   SWITCHES_FILE - file containing list of switches, used in absence of -F and -H" >&2
	echo "   UPLOADS_DIR - directory to upload to, used in absence of -d" >&2
	echo "   FF_MAX_PARALLEL - when -p option is used, maximum concurrent operations" >&2
#	echo "   FF_SWITCH_LOGIN_METHOD - how to login to switches: telnet or ssh" >&2
#	echo "   FF_SWITCH_ADMIN_PASSWORD - admin password for switches, used in absence of -S" >&2
	echo "example:">&2
	echo "  Operations on hosts" >&2
	echo "   $BASENAME" >&2
	echo "   $BASENAME mycapture" >&2
	echo "   $BASENAME -h 'arwen elrond' 030127capture" >&2
#	echo "  Operations on switches" >&2
#	echo "   $BASENAME -C" >&2
#	echo "   $BASENAME -C mycapture" >&2
#	echo "   $BASENAME -C -H 'switch1 switch2' 030127capture" >&2
	echo "For hosts:" >&2
	echo "   ethcapture will be run to create the specified capture file within ~root" >&2
	echo "   on each host (with the .tgz suffix added). The files will be" >&2
	echo "   uploaded and unpacked into a matching directory name within" >&2
	echo "   upload_dir/hostname/ on the local system" >&2
	echo "   default file name is hostcapture" >&2
#	echo "For Switches:" >&2
#	echo "   The capture CLI command will be run on each switch and its output will be" >&2
#	echo "   saved to upload_dir/switchname/file on the local system" >&2
#	echo "   default file name is switchcapture" >&2
	echo "The uploaded captures will be combined into a tgz file with the file name" >&2
	echo "specified and the suffix .all.tgz added" >&2
	exit 0
}

Usage()
{
#	echo "Usage: $BASENAME [-Cp] [-f hostfile] [-F switchesfile] [-S]" >&2
#	echo "                  [-D detail_level] [file]" >&2
	echo "Usage: $BASENAME [-p] [-f hostfile] [-D detail_level] [file]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
#	echo "   -C - perform capture against switches, default is hosts" >&2
#	echo "   -p - perform capture upload in parallel on all hosts/switches" >&2
	echo "   -p - perform capture upload in parallel on all hosts" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
#	echo "   -F switchesfile - file with switches in cluster" >&2
#	echo "           default is $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
#	echo "   -S - securely prompt for password for admin on switch" >&2
	echo "   -D detail_level - level of detail passed to host ethcapture" >&2
	echo "   file - name for capture file [.tgz will be appended]" >&2
	echo "example:">&2
	echo "  Operations on hosts" >&2
	echo "   $BASENAME" >&2
	echo "   $BASENAME mycapture" >&2
#	echo "  Operations on switches" >&2
#	echo "   $BASENAME -C" >&2
#	echo "   $BASENAME -C mycapture" >&2
	echo "For hosts:" >&2
	echo "   ethcapture will be run to create the specified capture file within ~root" >&2
	echo "   on each host (with the .tgz suffix added). The files will be" >&2
	echo "   uploaded and unpacked into a matching directory name within" >&2
	echo "   uploads/hostname/ on the local system" >&2
	echo "   default file name is hostcapture" >&2
#	echo "For Switches:" >&2
#	echo "   The capture CLI command will be run on each switch and its output will be" >&2
#	echo "   saved to uploads/switchname/file on the local system" >&2
#	echo "   default file name is switchcapture" >&2
	echo "The uploaded captures will be combined into a tgz file with the file name" >&2
	echo "specified and the suffix .all.tgz added" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

popt=n
switches=0
host=0
Sopt=n
Dopt=
#while getopts Cf:F:h:H:t:d:pD:S param
while getopts f:h:d:pD: param
do
	case $param in
#	C)
#		switches=1;;
	f)
		host=1
		export HOSTS_FILE="$OPTARG";;
#	F)
#		switches=1
#		export SWITCHES_FILE="$OPTARG";;
	h)
		host=1
		export HOSTS="$OPTARG";;
#	H)
#		switches=1
#		SWITCHES="$OPTARG";;
	t)
		export PORTS_FILE="$OPTARG";;
	d)
		export UPLOADS_DIR="$OPTARG";;
	p)
		popt=y;;
	D)
		Dopt="-d $OPTARG";;
	S)
		Sopt=y;;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [[ $# -gt 1 ]]
then
	Usage
fi
if [[ $(($switches+$host)) -gt 1 ]]
then
	echo "$BASENAME: conflicting arguments, more than one of hosts, and switches specified" >&2
	Usage
fi
if [[ $(($switches+$host)) -eq 0 ]]
then
	host=1
fi

if [[ $# -gt 0 ]]
then
	file="$1"
else
	if [ $switches -eq 0 ]
	then
		file=hostcapture
	else
		file=switchcapture
	fi
fi
if [ `dirname $file` != . ]
then
	echo "filename for capture must not specify a directory" >&2
	Usage
fi
file=`basename $file .tgz`	# remove .tgz suffix if given
host_local=""

expand_host_capture()
{
# $1 = hostname
	if [ ! -f $UPLOADS_DIR/$1/${file}.tgz ]
	then
		echo "ERROR: capture and/or upload from $1 failed"
	else
		# chmod so we can remove
		chmod -R u+w $UPLOADS_DIR/$1/$file >/dev/null 2>/dev/null
		rm -rf $UPLOADS_DIR/$1/$file
		mkdir -p $UPLOADS_DIR/$1/$file
		( cd $UPLOADS_DIR/$1/$file; tar xfz ../${file}.tgz; chmod -R u+w . )
	fi
}

if [ $switches -eq 0 ]
then
	check_host_args $BASENAME

	# remove local host from $HOSTS so that ethcmdall will not run ethcapture locally
	hosts_no_local=""
	lc_myhostname=$(hostname -s|tr A-Z a-z)
	for hostname in $HOSTS
	do
		lc_hostname=$(echo $hostname|tr A-Z a-z)
		if [ "$lc_hostname" != "$lc_myhostname" -a "$lc_hostname" != "localhost" ]
		then
			hosts_no_local="$hosts_no_local $hostname"
		else
			host_local="$hostname"
		fi
	done

	# run capture in parallel since its time consuming
	echo "Running capture on all non-local hosts ..."
	/usr/sbin/ethcmdall -p -h "$hosts_no_local" -u "root" "rm -f ~root/${file}.tgz; ethcapture ~root/${file}.tgz"

	echo "Uploading capture from each host ..."
	running=0
	captures=
	for hostname in $hosts_no_local
	do
		captures="$captures $hostname/$file"
		if [ "$popt" = "y" ]
		then
			if [ $running -ge $FF_MAX_PARALLEL ]
			then
				wait
				running=0
			fi
			(
			/usr/sbin/ethuploadall -d $UPLOADS_DIR -u root -h "$hostname" ${file}.tgz .
			expand_host_capture "$hostname"
			) &
			running=$(( $running + 1))
		else
			/usr/sbin/ethuploadall -d $UPLOADS_DIR -u root -h "$hostname" ${file}.tgz .
			expand_host_capture "$hostname"
		fi
	done
else
	check_switches_args $BASENAME
	if [ "$Sopt" = y ]
	then
		echo -n "Password for admin on all switches: " > /dev/tty
		stty -echo < /dev/tty > /dev/tty
		password=
		read password < /dev/tty
		stty echo < /dev/tty > /dev/tty
		echo > /dev/tty
		export FF_SWITCH_ADMIN_PASSWORD="$password"
	fi

	echo "Running capture on all switches ..."
	running=0
	captures=
	for switch in $SWITCHES
	do
		switch=`strip_chassis_slots "$switch"`
		captures="$captures $switch/$file"
		if [ "$popt" = "y" ]
		then
			if [ $running -ge $FF_MAX_PARALLEL ]
			then
				wait
				running=0
			fi
			(
			mkdir -p $UPLOADS_DIR/$switch
			# filter out carriage returns
			/usr/sbin/ethcmdall -C -H "$switch" -m 'End of Capture' "capture" |tr -d '\015' > $UPLOADS_DIR/$switch/$file
			if [ $? != 0 ]
			then
				echo "ERROR: capture from $switch failed"
			fi
			) &
			running=$(( $running + 1))
		else
			mkdir -p $UPLOADS_DIR/$switch
			# filter out carriage returns
			/usr/sbin/ethcmdall -C -H "$switch" -m 'End of Capture' "capture" |tr -d '\015' > $UPLOADS_DIR/$switch/$file
			if [ $? != 0 ]
			then
				echo "ERROR: capture from $switch failed"
			fi
		fi
	done
fi

if [ "$host_local" == "" -a "x$Dopt" != "x" ]
then
	host_local=$(hostname -s)
fi
if [ "$host_local" != "" ]
then
	echo "Running capture on local host ..."
	rm -f $UPLOADS_DIR/$host_local/$file.tgz
	mkdir -p $UPLOADS_DIR/$host_local
	/usr/sbin/ethcapture $Dopt $UPLOADS_DIR/$host_local/$file.tgz
	expand_host_capture "$host_local"
	captures="$captures $host_local/$file"
fi

wait
echo "Combining captured files into $UPLOADS_DIR/$file.all.tgz ..."
(cd $UPLOADS_DIR; rm -rf $file.all.tgz; tar --format=gnu -czf $file.all.tgz $captures --warning=no-file-changed)
echo "Done."
