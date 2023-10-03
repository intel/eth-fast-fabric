#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2022-2023, Intel Corporation
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

# If a PSM3 job terminates abnormally, such as with a segmentation fault, there could
# be POSIX shared memory files left over in the /dev/shm directory. This script is
# intended to remove unused file

readonly BASENAME="$(basename $0)"

Usage_full()
{
	echo "Usage: $BASENAME" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
	echo >&2
	echo "Clean up unused PSM3 POSIX shared memory files in /dev/shm:"  >&2
	echo "    /dev/shm/psm3_shm.*" >&2
	echo "    /dev/shm/sem.psm3_nic_affinity*" >&2
	echo "    /dev/shm/psm3_nic_affinity*" >&2
	echo >&2
	echo "Example:">&2
	echo "   ${BASENAME}" >&2	
	exit 0
}

Usage()
{
	echo "Usage: $BASENAME" >&2
	echo "              or" >&2
	echo "       $BASENAME --help" >&2
	echo "   --help - Produces full help text." >&2
	echo >&2
    echo "Example:">&2
	echo "   ${BASENAME}" >&2	
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

if [ $# -gt 0 ]
then
	Usage
fi

files=`ls /dev/shm/psm3_shm.* \
		  /dev/shm/sem.psm3_nic_affinity* \
		  /dev/shm/psm3_nic_affinity* 2> /dev/null`;

for file in $files;
do
	# fuser util returns PIDs of processes using the file.
	# So, if output is empty, no one uses shm file and it
	# could be removed
	# If error is detected during fuser command execution,
	# no actions are performed.
	#
	# Note, that return code of "fuser" cannot be used here,
	# because fuser returns a non-zero return code if no one uses file
	# or upon a fatal error.  So we use empty stdout and stderr to indicate
	# file is not in use (which implies a successful return since no error
	# messages).

	rc=`fuser $file 2>&1`;

	if [ -z "$rc" ];
	then
		echo "rm -f $file"
		rm -f $file
	fi;
done;

exit 0
