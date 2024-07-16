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
# setup password-less ssh on a group of hosts

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
#	echo "Usage: $BASENAME [-CpU] [-f hostfile] [-F switchesfile] [-h 'hosts']" >&2
#	echo "                      [-H 'switches'] [-u user] [-S]" >&2
#	echo "                      [-RP]" >&2
	echo "Usage: $BASENAME [-pU] [-f hostfile] [-h 'hosts'] [-u user] [-S] [-RP]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo >&2
	echo "   --help - Produces full help text." >&2
#	echo "   -C - perform operation against switches, default is hosts" >&2
	echo "   -p - Performs operation against all hosts in parallel." >&2
	echo "   -U - Performs connect only (to enter in local hosts, known hosts). When" >&2
	echo "        run in this mode, the -S option is ignored." >&2
	echo >&2
	echo "   -f hostfile - Specifies the file with hosts in cluster. Default is" >&2
	echo "        $CONFIG_DIR/$FF_PRD_NAME/hosts file." >&2
#	echo "   -F switchesfile  - file with switches in cluster, default is" >&2
#	echo "                     $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
	echo "   -h hosts - Specifies the list of hosts to set up." >&2
#	echo "   -H switches      - list of switches to setup" >&2
	echo >&2
	echo "   -u user - Specifies the user on remote system to allow this user to SSH to." >&2
	echo "        Default is current user code for host(s)." >&2
	echo "   -S - Securely prompts for password for user on remote system." >&2
	echo "   -R - Skips setup of SSH to local host." >&2
	echo "   -P - Skips ping of host (for SSH to devices on Internet with ping firewalled)." >&2
	echo >&2
	echo " Environment:" >&2
	echo "   HOSTS_FILE - File containing list of hosts, used in absence of -f and -h." >&2
#	echo "   SWITCHES_FILE - file containing list of switches, used in absence of -F and -H" >&2
	echo "   HOSTS - List of hosts, used if -h option not supplied." >&2
#	echo "   SWITCHES - list of switches, used if -C used and -H and -F options not supplied" >&2
	echo "   FF_MAX_PARALLEL - When -p option is used, maximum concurrent operations." >&2
#	echo "   FF_SWITCH_LOGIN_METHOD - how to login to switch: telnet or ssh" >&2
#	echo "   FF_SWITCH_ADMIN_PASSWORD - password for switch, used in absence of -S" >&2
	echo >&2
	echo "Examples:">&2
	echo "  Operations on hosts" >&2
	echo "   $BASENAME -S" >&2
	echo "   $BASENAME -U" >&2
	echo "   $BASENAME -h 'arwen elrond' -U" >&2
	echo "   HOSTS='arwen elrond' $BASENAME -U" >&2
#	echo "  Operations on switches" >&2
#	echo "   $BASENAME -C" >&2
#	echo "   $BASENAME -C -H 'switch1 switch2'" >&2
#	echo "   SWITCHES='switch1 switch2' $BASENAME -C" >&2
	exit 0
}

Usage()
{
#	echo "Usage: $BASENAME [-CpU] [-f hostfile] [-F switchesfile]" >&2
#	echo "                      [-S]" >&2
	echo "Usage: $BASENAME [-pU] [-f hostfile] [-S]" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
#	echo "   -C - perform operation against switches, default is hosts" >&2
	echo "   -p - Performs operation against all hosts in parallel." >&2
	echo "   -U - Performs connect only (to enter in local hosts, known hosts). When" >&2
	echo "        run in this mode, the -S option is ignored." >&2
	echo >&2
	echo "   -f hostfile - Specifies the file with hosts in cluster. Default is" >&2
	echo "        $CONFIG_DIR/$FF_PRD_NAME/hosts file." >&2
#	echo "   -F switchesfile  - file with switches in cluster, default is" >&2
#	echo "                     $CONFIG_DIR/$FF_PRD_NAME/switches" >&2
	echo >&2
	echo "   -S - Securely prompts for password for user on remote system." >&2
	echo >&2
	echo "Examples:">&2
	echo "  Operations on hosts" >&2
	echo "   $BASENAME -S" >&2
	echo "   $BASENAME -U" >&2
#	echo "  Operations on switches" >&2
#	echo "   $BASENAME -C" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

myuser=`id -u -n`
user=$myuser
Uopt=n
popt=n
Ropt=n
Sopt=n
uopt=n
password=
switches=0
host=0
skip_ping=n
bracket=''
close_bracket=''

shellcmd="ssh -o StrictHostKeyChecking=no"
copycmd=scp
bracket='['
close_bracket=']'

#while getopts CpUf:h:u:H:F:SRP param
while getopts pUf:h:u:SRP param
do
	case $param in
#	C)
#		switches=1;;
	p)
		popt=y;;
	U)
		Uopt=y;;
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
	u)
		uopt=y
		user="$OPTARG";;
	S)
		Sopt=y;;
	R)
		Ropt=y;;
	P)
		skip_ping=y;;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -gt 0 ]
then
	Usage
fi
if [[ $(($switches+$host)) -gt 1 ]]
then
	echo "$BASENAME: conflicting arguments, hosts and switches both specified" >&2
	Usage
fi
if [[ $(($switches+$host)) -eq 0 ]]
then
	host=1
fi
if [ "$Uopt" = y -a "$Sopt" = y ]
then
	echo "$BASENAME: Warning: -S option ignored in conjunction with -U" >&2
	Sopt=n
fi
if [ "$Uopt" = y ]
then
	# need commands setup so can run setup_self_ssh on remote host
	shellcmd="ssh -o StrictHostKeyChecking=no"
	copycmd=scp
	bracket='['
	close_bracket=']'
fi
if [ "$Sopt" = y ]
then
        bracket='\['
        close_bracket='\]'
fi
if [ "$switches" = 1 -a "$Ropt" = y ]
then
	echo "$BASENAME: Warning: -R option ignored for switches" >&2
	Ropt=n
fi
# for switches, make the default user to be admin (unless specified)
if [ $switches = 1 -a  "$uopt" = n ]
then
	user="admin"
fi

if [ $switches -eq 1 ]
then
	shellcmd=ssh
	copycmd=scp
	export CFG_SWITCH_LOGIN_METHOD="$FF_SWITCH_LOGIN_METHOD"
	export CFG_SWITCH_ADMIN_PASSWORD="$FF_SWITCH_ADMIN_PASSWORD"
	if [ "$Sopt" = y ]
	then
		given_pwd='entered'
	else		
		given_pwd='default'
	fi
fi
#  if user requested to enter a password securely, prompt user and save password
if [ $host -eq 1 ]
then
	check_host_args $BASENAME
	if [ "$Sopt" = y ]
	then
		echo -n "Password for $user on all hosts: " > /dev/tty
		stty -echo < /dev/tty > /dev/tty
		password=
		read password < /dev/tty
		stty echo < /dev/tty > /dev/tty
		echo > /dev/tty
		export password
	 fi
else
	check_switches_args $BASENAME
	if [ "$Sopt" = y ]
	then
		echo -n "Password for $user on all switches: " > /dev/tty
		stty -echo < /dev/tty > /dev/tty
		password=
		read password < /dev/tty
		stty echo < /dev/tty > /dev/tty
		echo > /dev/tty
		export CFG_SWITCH_ADMIN_PASSWORD="$password"
	fi
fi

# connect to host via ssh
connect_to_host()
{
	# $1 = user
	# $2 = host

	# We use an alternate file to build up the new keys
	# this way parallel calls can let ssh itself handle file locking
	# then update_known_hosts can replace data in real known_hosts file
	ssh -o 'UserKnownHostsFile=~/.ssh/.known_hosts-ffnew' -o 'StrictHostKeyChecking=no' $1@$2 echo "$2: Connected"
}

# update known_hosts file with information from connect_to_host calls
update_known_hosts()
{
	if [ -e ~/.ssh/.known_hosts-ffnew ]
	then
		if [ -e ~/.ssh/known_hosts ]
		then
			(
			IFS=" ,	"
			while read name trash
			do
				# remove old entry from known hosts in case key changed
				if grep "^$name[, ]" < ~/.ssh/known_hosts > /dev/null 2>&1
				then
					grep -v "^$name[, ]" < ~/.ssh/known_hosts > ~/.ssh/.known_hosts-fftmp
					mv ~/.ssh/.known_hosts-fftmp ~/.ssh/known_hosts
				fi
			done < ~/.ssh/.known_hosts-ffnew
			)
		fi
		total_processed=$(( $total_processed + $(wc -l < ~/.ssh/.known_hosts-ffnew) ))
		cat ~/.ssh/.known_hosts-ffnew >> ~/.ssh/known_hosts
		chmod go-w ~/.ssh/known_hosts

		rm -rf ~/.ssh/.known_hosts-ffnew ~/.ssh/.known_hosts-fftmp
	fi
}

# Generate public and private SSH key pairs
cd ~
mkdir -m 0700 -p ~/.ssh ~/.ssh2
permission_ssh=$(stat -c %a ~/.ssh)
if [ "$permission_ssh" -ne 700 ]; then
		chmod 700 ~/.ssh
        echo "Warning: ~/.ssh dir permissions are $permission_ssh. Changed to 700.."
fi

permission_ssh2=$(stat -c %a ~/.ssh2)
if [ "$permission_ssh2" -ne 700 ]; then
		chmod 700 ~/.ssh2
        echo "Warning: ~/.ssh2 dir permissions are $permission_ssh2. Changed to 700."
fi

if [ ! -f ~/.ssh/id_rsa.pub -o ! -f ~/.ssh/id_rsa ]
then
	ssh-keygen -P "" -t rsa -f ~/.ssh/id_rsa
fi
if [ ! -f .ssh/id_dsa.pub -o ! -f .ssh/id_dsa ]
then
	ssh-keygen -P "" -t dsa -f ~/.ssh/id_dsa
fi
# recreate public key in Reflection format for ssh2
if [ ! -f .ssh2/ssh2_id_dsa.pub ]
then
	# older distros may not support this, ignore error
	ssh-keygen -P "" -O ~/.ssh/id_dsa.pub -o ~/.ssh2/ssh2_id_dsa.pub 2>/dev/null
fi

# send command to the host and handle any password prompt if user supplied secure password
run_host_cmd()
{
if [ "$Sopt" = "n" ]
then
	$*
else
	expect -c "
global env
spawn -noecho $*
expect {
{assword:} {
		puts -nonewline stdout \"<password supplied>\"
		flush stdout
		exp_send \"\$env(password)\\r\"
		interact
		wait
	}
{assphrase for key} {
		puts stdout \"\nError: PassPhrase not supported. Remove PassPhrase\"
		flush stdout
		exit
        }
{continue connecting} { exp_send \"yes\\r\"
		exp_continue
	}
}
"
fi
}

# send command to the switches and always expect to be prompted for password
run_switch_cmd()
{
expect -c "
global env
spawn -noecho $*
expect {
{assword:} {
		puts -nonewline stdout \"<password supplied>\"
		flush stdout
		exp_send \"$CFG_SWITCH_ADMIN_PASSWORD\r\"
		interact
		wait
	}
{assphrase for key} {
		puts stdout \"\nError: PassPhrase not supported. Remove PassPhrase\"
		flush stdout
		exit
        }
{continue connecting} { exp_send \"yes\\r\"
		exp_continue
	}
}
"
}

# connect to switch via ssh
connect_to_switch()
{
	# $1 = user
	# $2 = switch
	# $3 = 1 if display connected

	# We use an alternate file to build up the new keys
	# this way parallel calls can let ssh itself handle file locking
	# then update_known_hosts can replace data in real known_hosts file
	ssh -o 'UserKnownHostsFile=~/.ssh/.known_hosts-ffnew' -o 'StrictHostKeyChecking=no' $1@$2 "chassisQuery" 2>&1| grep -q 'slots:'
	if [ $? -eq 0 ]
	then
		if [ $3 = 1 ]
		then 
			echo "$2: Connected"
		fi
		return 0
	else
		if [ $3 = 1 ]
		then 
			echo "$2: Can't Connect"
		fi
		return 1
	fi
}

# do selected operation for a single host
process_host()
{
	local hostname=$1
	local setup_self_ssh

	#setup_self_ssh='[ -x /usr/lib/eth-tools/setup_self_ssh ] && /usr/lib/eth-tools/setup_self_ssh'
	setup_self_ssh='/tmp/setup_self_ssh'
	[ "$skip_ping" = "y" ] || ping_host $hostname
	if [ $? != 0 ]
	then
		echo "Couldn't ping $hostname"
		sleep 1
	else
		if [ "$Uopt" = n ]
		then
			echo "Configuring $hostname..."
			run_host_cmd $shellcmd -l $user $hostname mkdir -m 0700 -p '~/.ssh' '~/.ssh2'
			run_host_cmd $shellcmd -l $user $hostname "stat -c %a ~/.ssh > /tmp/perm_ssh &&  stat -c %a ~/.ssh2 > /tmp/perm_ssh2"
			run_host_cmd $copycmd $user@$bracket$hostname$close_bracket:/tmp/perm_ssh /tmp/perm_ssh.$user.$hostname
			run_host_cmd $copycmd $user@$bracket$hostname$close_bracket:/tmp/perm_ssh2 /tmp/perm_ssh2.$user.$hostname
			run_host_cmd $shellcmd -l $user $hostname "rm -f /tmp/perm_ssh /tmp/perm_ssh2"

			if [ -f /tmp/perm_ssh.$user.$hostname ]
			then
				permission_ssh=$(cat /tmp/perm_ssh.$user.$hostname)
			else
				echo "Warning: /tmp/perm_ssh.$user.$hostname: No such File."
			fi

			if [ -f /tmp/perm_ssh2.$user.$hostname ]
			then
				permission_ssh2=$(cat /tmp/perm_ssh2.$user.$hostname)
			else
				echo "Warning: /tmp/perm_ssh2.$user.$hostname: No such File."
			fi

			if [ "$permission_ssh" -ne 700 -o "$permission_ssh2" -ne 700 ]; then
				run_host_cmd $shellcmd -l $user $hostname chmod 700 '~/.ssh' '~/.ssh2'
				echo "Warning: $user@$hostname:~/.ssh, ~/.ssh2 dir permissions are $permission_ssh/$permission_ssh2. Changed to 700.."
			fi
			rm -f /tmp/perm_ssh.$user.$hostname /tmp/perm_ssh2.$user.$hostname
			run_host_cmd $copycmd ~/.ssh/id_rsa.pub $user@$bracket$hostname$close_bracket:.ssh/$ihost.$myuser.id_rsa.pub
			run_host_cmd $copycmd ~/.ssh/id_dsa.pub $user@$bracket$hostname$close_bracket:.ssh/$ihost.$myuser.id_dsa.pub

			run_host_cmd $shellcmd -l $user $hostname ">> ~/.ssh/authorized_keys"
			run_host_cmd $shellcmd -l $user $hostname "cat ~/.ssh/authorized_keys ~/.ssh/$ihost.$myuser.id_rsa.pub ~/.ssh/$ihost.$myuser.id_dsa.pub |sort -u > ~/.ssh/.tmp_keys"
			run_host_cmd $shellcmd -l $user $hostname "mv ~/.ssh/.tmp_keys ~/.ssh/authorized_keys"

			run_host_cmd $shellcmd -l $user $hostname ">> ~/.ssh/authorized_keys2"
			run_host_cmd $shellcmd -l $user $hostname "cat ~/.ssh/authorized_keys2 ~/.ssh/$ihost.$myuser.id_rsa.pub ~/.ssh/$ihost.$myuser.id_dsa.pub |sort -u > ~/.ssh/.tmp_keys2"
			run_host_cmd $shellcmd -l $user $hostname "mv ~/.ssh/.tmp_keys2 ~/.ssh/authorized_keys2"

			if [ -f ~/.ssh2/ssh2_id_dsa.pub ]
			then
				run_host_cmd $copycmd ~/.ssh2/ssh2_id_dsa.pub $user@$hostname:.ssh2/$ihost.$myuser.ssh2_id_dsa.pub
			else
				# older distros may not support this, ignore error
				run_host_cmd $shellcmd -l $user $hostname "ssh-keygen -P "" -O ~/.ssh/$ihost.$myuser.id_dsa.pub -o ~/.ssh2/$ihost.$myuser.ssh2_id_dsa.pub 2>/dev/null"
			fi
			run_host_cmd $shellcmd -l $user $hostname "test -f ~/.ssh2/$ihost.$myuser.ssh2_id_dsa.pub && ! grep '^Key $ihost.$myuser.ssh2_id_dsa.pub\$' ~/.ssh2/authorization >/dev/null 2>&1 && echo 'Key $ihost.$myuser.ssh2_id_dsa.pub' >> .ssh2/authorization"

			run_host_cmd $shellcmd -l $user $hostname "chmod go-w ~/.ssh/authorized_keys ~/.ssh/authorized_keys2"
			run_host_cmd $shellcmd -l $user $hostname "test -f ~/.ssh2/authorization && chmod go-w ~/.ssh2/authorization"
			connect_to_host $user $hostname
			if [ "$Ropt" = "n" ]
			then
				run_host_cmd $copycmd /usr/lib/$FF_PRD_NAME/setup_self_ssh $user@$bracket$hostname$close_bracket:/tmp/setup_self_ssh
				run_host_cmd $shellcmd -l $user $hostname "$setup_self_ssh"
				run_host_cmd $shellcmd -l $user $hostname "rm -f /tmp/setup_self_ssh"
			fi
			echo "Configured $hostname"
		else
			echo "Connecting to $hostname..."
			connect_to_host $user $hostname
			if [ "$Ropt" = "n" ]
			then
				run_host_cmd $copycmd /usr/lib/$FF_PRD_NAME/setup_self_ssh $user@$bracket$hostname$close_bracket:/tmp/setup_self_ssh
				run_host_cmd $shellcmd -l $user $hostname "$setup_self_ssh -U"
				run_host_cmd $shellcmd -l $user $hostname "rm -f /tmp/setup_self_ssh"
			fi
		fi
	fi
}

# do selected operation for a single switch
process_switch()
{
	local switchname=$1
	local tempfile=~/.ssh/.tmp_switch_keys.$running

	tswitch=`strip_chassis_slots  $switchname`
	[ "$skip_ping" = "y" ] || ping_host $tswitch
	if [ $? != 0 ]
	then
		echo "Couldn't ping $tswitch"
		sleep 1
	else
		if [ "$Uopt" = n ]
		then
			echo "Configuring $tswitch..."
			# ensure that we can login successfully by trying any simple command with expected response
			$switch_cmd $tswitch $user "chassisQuery" 2>&1| grep -q 'slots:'
			if [ $? -ne 0 ]
			then
				echo "Login to $tswitch failed for the $given_pwd password, skipping..."
				continue
			fi
			rm -f $tempfile $tempfile.2 2>/dev/null
			/usr/lib/$FF_PRD_NAME/tcl_proc chassis_sftp_cmd "sftp $user@\[${tswitch}\]:" "get /firmware/$user/authorized_keys $tempfile" 2>&1| grep -q 'FAILED'
			if [ $? -eq 0 ] || [ ! -f $tempfile ]
			then
				echo "Unable to configure $tswitch for password-less ssh, skipping..."
				continue
			fi
			cat ~/.ssh/id_rsa.pub $tempfile | sort -u > $tempfile.2
			/usr/lib/$FF_PRD_NAME/tcl_proc chassis_sftp_cmd "sftp $user@\[${tswitch}\]:" "put $tempfile.2 /firmware/$user/authorized_keys" 2>&1| grep -q 'FAILED'
			if [ $? -eq 0 ]
			then
				echo "$tswitch password-less ssh config failed, skipping..."
			else
				connect_to_switch $user $tswitch 1
				if [ $? -eq 0 ]
				then
					echo "Configured $tswitch"
				fi
			fi
			rm -f $tempfile $tempfile.2 2>/dev/null
		else
			echo "Connecting to $tswitch..."
			connect_to_switch $user $tswitch 1
		fi
	fi
} 

# configure ssh on the host(s) or switches
running=0
total_processed=0
rm -rf ~/.ssh/.known_hosts-ffnew ~/.ssh/.known_hosts-fftmp

stty_settings=`stty -g`

if [ $host -eq 1 ]
then
	ihost=`hostname | cut -f1 -d.`
	# setup ssh to ourselves
	# This can also help if the .ssh directory is in a shared filesystem
	rm -f ~/.ssh/.tmp_keys$$

	>> ~/.ssh/authorized_keys
	cat ~/.ssh/authorized_keys ~/.ssh/id_rsa.pub ~/.ssh/id_dsa.pub|sort -u > ~/.ssh/.tmp_keys$$
	mv ~/.ssh/.tmp_keys$$ ~/.ssh/authorized_keys

	>> ~/.ssh/authorized_keys2
	cat ~/.ssh/authorized_keys2 ~/.ssh/id_rsa.pub ~/.ssh/id_dsa.pub|sort -u > ~/.ssh/.tmp_keys$$
	mv ~/.ssh/.tmp_keys$$ ~/.ssh/authorized_keys2

	# set up ssh2 DSA authorization
	# older distros may not support this
	[ -f ~/.ssh2/ssh2_id_dsa.pub ] && !  grep '^Key ssh2_id_dsa.pub$' ~/.ssh2/authorization >/dev/null 2>&1 && echo "Key ssh2_id_dsa.pub" >> ~/.ssh2/authorization

	chmod go-w ~/.ssh/authorized_keys ~/.ssh/authorized_keys2
	test -f ~/.ssh2/authorization && chmod go-w ~/.ssh2/authorization

	if [ "$Ropt" = "n" ]
	then
		echo "Verifying localhost ssh..."
		if [ "$uopt" = n ]
		then
			connect_to_host $user localhost
			connect_to_host $user $ihost
		else
			run_host_cmd $shellcmd -l $user localhost echo localhost: Connected
			run_host_cmd $shellcmd -l $user $ihost echo $ihost: Connected
		fi

		stty $stty_settings
		update_known_hosts
	fi

	for hostname in $HOSTS
	do
		if [ "$popt" = y ]
		then
			if [ $running -ge $FF_MAX_PARALLEL ]
			then
				wait
				stty $stty_settings
				update_known_hosts
				running=0
			fi
			process_host $hostname < /dev/tty &
			running=$(($running +1))
		else
			process_host $hostname
			stty $stty_settings
			update_known_hosts
		fi
	done
else
	switch_cmd='/usr/lib/$FF_PRD_NAME/tcl_proc chassis_run_cmd'
	for switchname in $SWITCHES
	do
		if [ "$popt" = y ]
		then
			if [ $running -ge $FF_MAX_PARALLEL ]
			then
				wait
				stty $stty_settings
				update_known_hosts
				running=0
			fi
			process_switch $switchname < /dev/tty &
			running=$(($running +1))
		else
			process_switch $switchname
			stty $stty_settings
			update_known_hosts
		fi
	done
fi
wait
stty $stty_settings
update_known_hosts

echo "Successfully processed: $total_processed"
