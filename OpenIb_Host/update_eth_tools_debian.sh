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

#[ICS VERSION STRING: unknown]
debversion=$1
debrelease=$2

rm -rf debian
cp -r debian.template debian

find debian/ -type f -exec sed -i \
	-e "s/@DEBNAME@/eth-tools/g" \
	-e "s/@DEBRELEASE@/$debrelease/g" \
	-e "s/@DEBVERSION@/$debversion/g" \
	-e "s/@FEATURE_SET@/OPA_FEATURE_SET=$OPA_FEATURE_SET/g" \
{} \;

source ./ff_filegroups.sh

_where_="debian/eth-tools-basic"

for i in $basic_tools_sbin $basic_tools_sbin_sym; do
	echo "/usr/sbin/$i" >> ${_where_}.install
done
for i in $basic_tools_opt; do
	echo "/usr/lib/eth-tools/$i" >> ${_where_}.install
done
for i in $basic_samples
do
	echo "/usr/share/eth-tools/samples/$i" >> ${_where_}.install
done
for i in $basic_configs; do
	echo "/etc/eth-tools/$i" >> ${_where_}.install
done

for i in $basic_mans; do
	echo "usr/share/man/man1/$i" >> ${_where_}.manpages
done


_where_="debian/eth-tools-fastfabric"	
for i in $ff_tools_sbin; do
	echo "/usr/sbin/$i" >> ${_where_}.install
done
for i in $ff_tools_opt $ff_tools_exp $ff_tools_misc $ff_libs_misc; do
	echo "/usr/lib/eth-tools/$i" >> ${_where_}.install
done
for i in $ff_iba_samples; do
	echo "/usr/share/eth-tools/samples/$i" >> ${_where_}.install
done
for i in $mpi_apps_files; do
	echo "/usr/src/eth/mpi_apps/$i" >> ${_where_}.install
done
for i in $ff_configs; do
	echo "/etc/eth-tools/$i" >> ${_where_}.install
done
# debian will treat this as conffiles anyways
for i in $ff_configs_as_files; do
	echo "/etc/eth-tools/$i" >> ${_where_}.install
done
# debian will NOT treat this as conffiles
for i in $ff_configs_non_etc; do
	echo "/usr/lib/eth-tools/$i" >> ${_where_}.install
done

for i in $ff_mans; do
	echo "usr/share/man/man8/$i" >> ${_where_}.manpages
done


exit 0
