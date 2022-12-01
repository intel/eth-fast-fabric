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
# copy a file to all hosts

# optional override of defaults
if [ -f /etc/eth-tools/ethfastfabric.conf ]
then
	. /etc/eth-tools/ethfastfabric.conf
fi

. /usr/lib/eth-tools/ethfastfabric.conf.def

. /usr/lib/eth-tools/ff_funcs

readonly BASENAME="$(basename $0)"

temp="$(mktemp --tmpdir "$BASENAME.XXXXXX")"
trap "rm -f $temp; exit 1" SIGHUP SIGTERM SIGINT
trap "rm -f $temp" EXIT

# Copy behaviors:
# if copying 1 source directory
# if dest doesn't exist
#	-r creates dir and copies into it, source dir not a dir under
#	-R makes trash/ and copies to it, source directory becomes directory under
#	-t creates dir and copies into it, source dir not a dir under
# if dest does exist (and empty or no prior copy of files)
#	-r copies to it, source directory becomes directory under
#	-R copies to it, source directory becomes directory under
#	-t copies into it, source dir not a dir under
# if dest does exist (and may have prior copy of files)
#	-r copies to it, source directory becomes directory under
#	-R copies only what has changed since prior copy
#	-t copies into it, source dir not a dir under

# if copying > 1 source directory or file
# if dest doesn't exist
#	-r fails
#	-R makes trash/ and copies to it, source directory becomes directory under
#	-t not allowed
# if dest does exist (and empty or no prior copy of files)
#	-r copies to it, source directory becomes directory under
#	-R copies to it, source directory becomes directory under
#	-t not allowed
# if dest does exist (and may have prior copy of files)
#	-r copies to it, source directory becomes directory under
#	-R copies only what has changed since prior copy
#	-t not allowed
# differences above for -r are an unfortunate consequence of scp

Usage_full()
{
	echo "Usage: $BASENAME [-pq] [-r|-R] [-f hostfile] [-h 'hosts'] [-u user]" >&2
	echo "                    [-B interface] source_file ... dest_file" >&2
	echo "       $BASENAME -t [-pq] [-Z tarcomp] [-f hostfile] [-h 'hosts'] [-u user]" >&2
	echo "                    [-B interface] [source_dir [dest_dir]]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -p - perform copy in parallel on all hosts" >&2
	echo "   -q - don't list files being transferred" >&2
	echo "   -r - recursive copy of directories using scp" >&2
	echo "   -R - recursive copy of directories using rsync (only copy changed files)" >&2
	echo "   -t - optimized recursive copy of directories using tar" >&2
	echo "        if dest_dir omitted, defaults to current directory name" >&2
	echo "        if source_dir and dest_dir omitted, both default to current directory" >&2
	echo "   -h hosts - list of hosts to copy to" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
	echo "   -u user - user to perform copy to, default is current user code" >&2
	echo "   -B interface - local network interface to use for scp or rsync" >&2
	echo "        Note the destination hosts specified must be accessible via the given" >&2
	echo "        interface's IP subnet.  This may imply the use of alternate" >&2
	echo "        hostnames or IP addresses for the destination hosts" >&2 
	echo "   -Z tarcomp - a simple tar compression option to use.  Such as --xz or --lzip" >&2
	echo "        When host list is large better compression may be preferred." >&2
	echo "        When host list is small faster compression may be preferred." >&2
 	echo "        -Z '' will not use compression. Default is -z" >&2
	echo "   source_file - list of source files to copy" >&2
	echo "   source_dir - source directory to copy, if omitted . is used" >&2
	echo "   dest_file - destination for copy." >&2
	echo "        If more than 1 source file, this must be a directory" >&2
	echo "   dest_dir - destination for copy.  If omitted current directory name is used" >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
	echo "   FF_MAX_PARALLEL - when -p option is used, maximum concurrent operations" >&2
	echo "example:">&2
	echo "   $BASENAME MPI-PMB /root/MPI-PMB" >&2
	echo "   $BASENAME -t -p /usr/src/eth/mpi_apps /usr/src/eth/mpi_apps" >&2
	echo "   $BASENAME a b c /root/tools/" >&2
	echo "   $BASENAME -h 'arwen elrond' a b c /root/tools" >&2
	echo "   $BASENAME -h 'arwen elrond' -B eth2 a b c /root/tools" >&2
	echo "   HOSTS='arwen elrond' $BASENAME a b c /root/tools" >&2
	echo "user@ syntax cannot be used in filenames specified" >&2
	echo "To copy from hosts in the cluster to this host, use ethuploadall" >&2
	echo "Beware: For the -r option, when copying a single source directory, if the" >&2
	echo "destination directory does not exist it will be created and the source files" >&2
	echo "placed directly in it." >&2
	echo "In other situations, the -r option will copy the source directory as a" >&2
	echo "directory under the destination directory." >&2
	echo "The -R option will always copy the source directory as a directory under the" >&2
	echo "destination directory." >&2
	echo "The -t option will always place the files found in source_dir directly in the" >&2
	echo "destination directory." >&2
	exit 0
}

Usage()
{
	echo "Usage: $BASENAME [-pq] [-r|-R] [-f hostfile] source_file ... dest_file" >&2
	echo "       $BASENAME -t [-pq] [-f hostfile] [source_dir [dest_dir]]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -p - perform copy in parallel on all hosts" >&2
	echo "   -q - don't list files being transferred" >&2
	echo "   -r - recursive copy of directories using scp" >&2
	echo "   -R - recursive copy of directories using rsync (only copy changed files)" >&2
	echo "   -t - optimized recursive copy of directories using tar" >&2
	echo "        if dest_dir omitted, defaults to current directory name" >&2
	echo "        if source_dir and dest_dir omitted, both default to current directory" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/$FF_PRD_NAME/hosts" >&2
	echo "   source_file - list of source files to copy" >&2
	echo "   source_dir - source directory to copy, if omitted . is used" >&2
	echo "   dest_file - destination for copy." >&2
	echo "        If more than 1 source file, this must be a directory" >&2
	echo "   dest_dir - destination for copy.  If omitted current directory name is used" >&2
	echo "example:">&2
	echo "   $BASENAME MPI-PMB /root/MPI-PMB" >&2
	echo "   $BASENAME -t -p /usr/src/eth/mpi_apps /usr/src/eth/mpi_apps" >&2
	echo "   $BASENAME a b c /root/tools/" >&2
	echo "user@ syntax cannot be used in filenames specified" >&2
	echo "To copy from hosts in the cluster to this host, use ethuploadall" >&2
	echo "Beware: For the -r option, when copying a single source directory, if the" >&2
	echo "destination directory does not exist it will be created and the source files" >&2
	echo "placed directly in it." >&2
	echo "In other situations, the -r option will copy the source directory as a" >&2
	echo "directory under the destination directory." >&2
	echo "The -R option will always copy the source directory as a directory under the" >&2
	echo "destination directory." >&2
	echo "The -t option will always place the files found in source_dir directly in the" >&2
	echo "destination directory." >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

# gzip compression by default
tarcomp='-z'
user=`id -u -n`
scpopts=
sshopts=
rsyncopts=
topt=n
popt=n
ropt=
Bopt=
status=0
pids=''
rsyncverbose='-P'
tarverbose='-v'

while getopts f:h:u:B:pqrRtZ: param
do
	case $param in
	h)
		HOSTS="$OPTARG";;
	f)
		HOSTS_FILE="$OPTARG";;
	u)
		user="$OPTARG";;
	B)
		if [ -n "$Bopt" ]
		then
		    echo "$BASENAME: -B can only be specified once" >&2
		    Usage
		fi
		scpopts="$scpopts -o BindInterface=$OPTARG"
		sshopts="$sshopts -o BindInterface=$OPTARG"
		Bopt="$OPTARG";;
	p)
		scpopts="$scpopts -q"
		rsyncopts="$rsyncopts -q"
		popt=y;;
	q)
		scpverbose='-q'
		rsyncverbose='-q'
		tarverbose=;;
	r)
		if [ -n "$ropt" -o "$topt" = "y" ]
		then
			echo "$BASENAME: only one of -r, -R or -t permitted" >&2
			Usage
		fi
		scpopts="$scpopts -r"
		ropt=r;;
	R)
		if [ -n "$ropt" -o "$topt" = "y" ]
		then
			echo "$BASENAME: only one of -r, -R or -t permitted" >&2
			Usage;
		fi
		ropt=R;;
	t)
		if [ -n "$ropt" ]
		then
			echo "$BASENAME: only one of -r, -R or -t permitted" >&2
			Usage;
		fi
		topt=y;;
	Z)
		tarcomp="$OPTARG";;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [ "$topt" = "n" -a $# -lt 2 ]
then
	Usage
fi
if [ "$topt" = "y" -a $# -gt 2 ]
then
	Usage
fi
if [ "$topt" = "n" -a "x$tarcomp" != "x-z" ]
then
	Usage
fi
check_host_args $BASENAME

if [ "$topt" = "n" ]
then
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

	for hostname in $HOSTS
	do
	  if [ "$popt" = "y" ]
	        then
		   if [ $running -ge $FF_MAX_PARALLEL ]
		     then
			   wait
			   running=0
		   fi
		   if [ "$ropt" = "R" ]
		   then
			if [ -n "$Bopt" ]
			then
			    echo "rsync -a -e 'ssh -B $Bopt' $rsyncopts $files $user@[$hostname]:$dest"
			    rsync -a -e "ssh -B $Bopt" $rsyncopts $files $user@\[$hostname\]:$dest & 
			else
			    echo "rsync -a $rsyncopts $files $user@[$hostname]:$dest"
			    rsync -a $rsyncopts $files $user@\[$hostname\]:$dest & 
			fi
		   else
			echo "scp $scpopts $files $user@[$hostname]:$dest"
			scp $scpopts $files $user@\[$hostname\]:$dest & 
		   fi
		   pid=$!
           pids="$pids $pid"
		   running=$(( $running + 1))
	    else
		   if [ "$ropt" = "R" ]
		   then
			if [ -n "$Bopt" ]
			then
			    echo "rsync -a $rsyncverbose -e 'ssh -B $Bopt' $rsyncopts $files $user@[$hostname]:$dest"
			    rsync -a $rsyncverbose -e "ssh -B $Bopt" $rsyncopts $files $user@\[$hostname\]:$dest
			else
			    echo "rsync -a $rsyncverbose $rsyncopts $files $user@[$hostname]:$dest"
			    rsync -a $rsyncverbose $rsyncopts $files $user@\[$hostname\]:$dest
			fi
		   else
			echo "scp $scpverbose $scpopts $files $user@[$hostname]:$dest"
			scp $scpverbose $scpopts $files $user@\[$hostname\]:$dest
		   fi
		if [ "$?" -ne 0 ]
		  then
		    status=1
	        fi		   
	  fi
	done

#checking exit status for background jobs	
	for pid in $pids
	do
	   wait $pid
   	   if [ "$?" -ne 0 ]
 	      then
	         status=1
	   fi	
	done

else
	if [ $# -lt 2 ]
	then
		destdir=$PWD
	else
		destdir=$2
	fi
	if [ $# -lt 1 ]
	then
		srcdir=$PWD
	else
		srcdir=$1
	fi
	if [ ! -d $srcdir ]
	then
		echo "$BASENAME: $srcdir: No such directory" >&2
		Usage
	fi
	echo "cd $srcdir; tar c $tarcomp $tarverbose -f $temp ."
	cd $srcdir; tar c $tarcomp $tarverbose -f $temp .

	running=0
	for hostname in $HOSTS
	do
		if [ "$popt" = "y" ]
		then
			if [ $running -ge $FF_MAX_PARALLEL ]
			then
				wait
				running=0
			fi
			(
                echo "$user@$hostname: mkdir -p $destdir; cd $destdir; tar x $tarcomp"
                ssh $sshopts $user@$hostname "mkdir -p $destdir; cd $destdir; tar x $tarcomp" < $temp
			) &
			pid=$!
		   	pids="$pids $pid"
			running=$(( $running + 1))
		else
			echo "$user@$hostname: mkdir -p $destdir; cd $destdir; tar x $tarcomp"
			ssh $sshopts $user@$hostname "mkdir -p $destdir; cd $destdir; tar x $tarcomp" < $temp
			if [ "$?" -ne 0 ]
		  	  then
		    	     status=1
	        	fi		   
		fi
	done

#checking exit status for background jobs	
	for pid in $pids
	do
	   wait $pid
   	   if [ "$?" -ne 0 ]
 	      then
	         status=1
	   fi	
	done
	rm -f $temp
fi

if [ $status -ne 0 ]
  then
     exit 1
fi
