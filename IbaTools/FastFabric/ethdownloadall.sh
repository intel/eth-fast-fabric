#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015-2017, Intel Corporation
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
	echo "Usage: $BASENAME [-pr] [-f hostfile] [-h 'hosts'] [-u user] [-d download_dir]" >&2
	echo "                       source_file ... dest_file" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   -p - Performs copy in parallel on all hosts." >&2
	echo "   -r - Performs recursive download of directories." >&2
	echo "   -f hostfile - Specifies the file with hosts in cluster. Default is" >&2
	echo "        $CONFIG_DIR/$FF_PRD_NAME/hosts file." >&2
	echo "   -h hosts - Specifies the list of hosts to download files to." >&2
	echo "   -u user - Specifies the user to perform the copy. Default is the current user." >&2
	echo "   -d download_dir - Specifies the directory to download files from. Default is" >&2
	echo "        downloads. If not specified, the environment variable DOWNLOADS_DIR is" >&2
	echo "        used. If that is not exported, the default is used." >&2
	echo "   source_file - Specifies the list of source files to copy from the system." >&2
	echo "        NOTE: The option source_file is relative to download_dir/hostname. A" >&2
	echo "              local directory within download_dir/ must exist for each host" >&2
	echo "              being downloaded to. Each downloaded file is copied from" >&2
	echo "              download_dir/hostname/source_file." >&2
	echo "   dest_file - Specifies the name of the file or directory on the destination" >&2
	echo "        hosts to copy to." >&2
	echo "        NOTE: If more than one source file is specified, dest_file is treated as" >&2
	echo "              a directory name. The given directory must already exist on the" >&2
	echo "              destination host. The copy fails for hosts where the directory" >&2
	echo "              does not exist." >&2
	echo " Environment:" >&2
	echo "   HOSTS - List of hosts; used if -h option not supplied." >&2
	echo "   HOSTS_FILE - File containing list of hosts; used in absence of -f and -h." >&2
	echo "   DOWNLOADS_DIR - Directory to download from, used in absence of -d." >&2
	echo "   FF_MAX_PARALLEL - When the -p option is used, the maximum concurrent" >&2
	echo "        operations are performed." >&2
	echo "Examples:">&2
	echo "   $BASENAME -h 'arwen elrond' irqbalance vncservers $CONFIG_DIR" >&2
	echo "   $BASENAME -p irqbalance vncservers $CONFIG_DIR" >&2
	echo  >&2
	echo "The tool ethdownloadall can only copy from this system to a group of hosts" >&2
	echo "in the cluster. To copy files from hosts in the cluster to this host, use" >&2
	echo "ethuploadall." >&2
	echo "NOTE: user@ syntax cannot be used in filenames specified." >&2
	exit 0
}

Usage()
{
	echo "Usage: $BASENAME [-pr] [-f hostfile] [-d download_dir]" >&2
	echo "                       source_file ... dest_file" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   -p - Performs copy in parallel on all hosts." >&2
	echo "   -r - Performs recursive download of directories." >&2
	echo "   -f hostfile - Specifies the file with hosts in cluster. Default is" >&2
	echo "        $CONFIG_DIR/$FF_PRD_NAME/hosts file." >&2
	echo "   -d download_dir - Specifies the directory to download files from. Default is" >&2
	echo "        downloads. If not specified, the environment variable DOWNLOADS_DIR is" >&2
	echo "        used. If that is not exported, the default is used." >&2
	echo "   source_file - Specifies the list of source files to copy from the system." >&2
	echo "   dest_file - Specifies the name of the file or directory on the destination" >&2
	echo "        hosts to copy to." >&2
	echo "Examples:">&2
	echo "   $BASENAME -p irqbalance vncservers $CONFIG_DIR" >&2
	echo "The tool ethdownloadall can only copy from this system to a group of hosts" >&2
	echo "in the cluster. To copy files from hosts in the cluster to this host, use" >&2
	echo "ethuploadall." >&2
	echo "NOTE: user@ syntax cannot be used in filenames specified." >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

user=`id -u -n`
opts=
popt=n
while getopts f:d:h:u:rp param
do
	case $param in
	f)
		HOSTS_FILE="$OPTARG";;
	d)
		DOWNLOADS_DIR="$OPTARG";;
	h)
		HOSTS="$OPTARG";;
	u)
		user="$OPTARG";;
	r)
		opts="$opts -r";;
	p)
		opts="$opts -q"
		popt=y;;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -lt 2 ]
then
	Usage
fi

check_host_args $BASENAME

# remove last name from the list
files=
dest=
for file in "$@"
do
	if [ ! -z "$dest" ]
	then
		files="$files $dest"
	fi
	dest="$file"
done

running=0
pids=""
stat=0
for hostname in $HOSTS
do
	src_files=
	for file in $files
	do
		src_files="$src_files $DOWNLOADS_DIR/$hostname/$file"
	done
		
	if [ "$popt" = "y" ]
	then
		if [ $running -ge $FF_MAX_PARALLEL ]
		then
			for pid in $pids; do
				wait $pid
				if [ "$?" -ne 0 ]; then
					stat=1
				fi
			done
			pids=""
			running=0
		fi
		echo "scp $opts $src_files $user@[$hostname]:$dest"
		scp $opts $src_files $user@\[$hostname\]:$dest &
		pid=$!
		pids="$pids $pid"
		running=$(( $running + 1))
	else
		echo "scp $opts $src_files $user@[$hostname]:$dest"
		scp $opts $src_files $user@\[$hostname\]:$dest
		if [ "$?" -ne 0 ]; then
			stat=1
		fi
	fi
done

for pid in $pids; do
	wait $pid
	if [ "$?" -ne 0 ]; then
		stat=1
	fi
done
exit $stat
