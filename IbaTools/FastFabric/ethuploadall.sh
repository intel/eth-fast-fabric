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
	echo "Usage: $BASENAME [-rp] [-f hostfile] [-d upload_dir] [-h 'hosts'] [-u user]" >&2
	echo "                       source_file ... dest_file" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   -p - Performs copy in parallel on all hosts." >&2
	echo "   -r - Performs recursive upload of directories." >&2
	echo "   -f hostfile - Specifies the file with hosts in cluster. Default is" >&2
	echo "        $CONFIG_DIR/$FF_PRD_NAME/hosts file." >&2
	echo "   -h hosts - Specifies the list of hosts to upload from." >&2
	echo "   -u user - Specifies the user to perform copy to. Default is current user." >&2
	echo "   -d upload_dir - Specifies the directory to upload to. Default is uploads. If" >&2
	echo "        not specified, the environment variable UPLOADS_DIR is used. If that is" >&2
	echo "        not exported, the default, uploads, is used." >&2
	echo "   source_file - Specifies the name of files to copy to this system, relative to" >&2
	echo "        the current directory. Multiple files may be listed." >&2
	echo "   dest_file - Specifies the name of the file or directory on this system to" >&2
	echo "        copy to. It is relative to upload_dir/hostname." >&2
	echo " Environment:" >&2
	echo "   HOSTS - List of hosts; used if -h option not supplied." >&2
	echo "   HOSTS_FILE - File containing list of hosts; used in absence of -f and -h." >&2
	echo "   UPLOADS_DIR - Directory to upload to, used in absence of -d." >&2
	echo "   FF_MAX_PARALLEL - When the -p option is used, maximum concurrent operations" >&2
	echo "        are performed." >&2
	echo "Examples:">&2
	echo "   $BASENAME -h 'arwen elrond' capture.tgz /etc/init.d/ipoib.cfg ." >&2
	echo "   $BASENAME -p capture.tgz /etc/init.d/ipoib.cfg ." >&2
	echo "   $BASENAME capture.tgz /etc/init.d/ipoib.cfg pre-install" >&2
	echo "NOTES:" >&2
	echo "- To copy files from this host to hosts in the cluster use ethscpall or" >&2
	echo "  ethdownloadall." >&2
	echo "- user@ syntax cannot be used in filenames specified." >&2
	echo "- A local directory within upload_dir/ will be created for each hostname." >&2
	echo "- Each uploaded file is copied to upload_dir/hostname/dest_file within the" >&2
	echo "  local system." >&2
	echo "- If more than one source file is specified or dest_file has a trailing '/', a" >&2
	echo "  dest_file directory will be created." >&2
	exit 0
}

Usage()
{
	echo "Usage: $BASENAME [-rp] [-f hostfile] source_file ... dest_file" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
	echo "   -p - Performs copy in parallel on all hosts." >&2
	echo "   -r - Performs recursive upload of directories." >&2
	echo "   -f hostfile - Specifies the file with hosts in cluster. Default is" >&2
	echo "        $CONFIG_DIR/$FF_PRD_NAME/hosts file." >&2
	echo "   source_file - Specifies the name of files to copy to this system, relative to" >&2
	echo "        the current directory. Multiple files may be listed." >&2
	echo "   dest_file - Specifies the name of the file or directory on this system to" >&2
	echo "        copy to. It is relative to upload_dir/hostname." >&2
	echo "Examples:">&2
	echo "   $BASENAME -p capture.tgz /etc/sysconfig/lldpd ." >&2
	echo "   $BASENAME capture.tgz /etc/sysconfig/lldpd pre-install" >&2
	echo "NOTES:" >&2
	echo "- To copy files from this host to hosts in the cluster use ethscpall or" >&2
	echo "  ethdownloadall." >&2
	echo "- user@ syntax cannot be used in filenames specified." >&2
	echo "- A local directory within upload_dir/ will be created for each hostname." >&2
	echo "- Each uploaded file is copied to upload_dir/hostname/dest_file within the" >&2
	echo "  local system." >&2
	echo "- If more than one source file is specified or dest_file has a trailing '/', a" >&2
	echo "  dest_file directory will be created." >&2
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
		UPLOADS_DIR="$OPTARG";;
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
file_count=0
for file in "$@"
do
	if [ ! -z "$dest" ]
	then
		if [ ! -z "$files" ]
		then
			files="$files $dest"
		else
			files="$dest"
		fi
		file_count=`expr $file_count + 1`
	fi
	dest="$file"
done

# Determine if we need to create a directory
create_dir=y
if [ $file_count -le 1 ] && [ "${dest: -1}" != "/" ]
then
	create_dir=n
	echo "$files" | grep -e "*" -e "\[" -e "?" >/dev/null
	if [ "$?" -eq 0 ]; then
		echo "$BASENAME: Warning: possible wildcards in source_file. If multiple" >&2
		echo "   files may match, dest_file should be a directory with trailing /" >&2
	fi
fi

# Convert files to an array to iterate through while
# avoiding wildcard expansion
read -a files <<< $files

running=0
pids=""
stat=0
for hostname in $HOSTS
do
	src_files=
	for file in "${files[@]}"
	do
		src_files="$src_files $user@[$hostname]:$file"
	done
	mkdir -p $UPLOADS_DIR/$hostname
	if [ "$create_dir" == "y" ]
	then
		mkdir -p $UPLOADS_DIR/$hostname/$dest
	fi

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
		echo "scp $opts $src_files $UPLOADS_DIR/$hostname/$dest"
		scp $opts $src_files $UPLOADS_DIR/$hostname/$dest &
		pid=$!
		pids="$pids $pid"
		running=$(( $running + 1))
	else
		echo "scp $opts $src_files $UPLOADS_DIR/$hostname/$dest"
		scp $opts $src_files $UPLOADS_DIR/$hostname/$dest
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
