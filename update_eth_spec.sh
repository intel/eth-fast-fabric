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

#[ICS VERSION STRING: unknown]

id=$(./get_id_and_versionid.sh | cut -f1 -d' ')
versionid=$(./get_id_and_versionid.sh | cut -f2 -d' ')

if [ "$(rpm --eval "%build_cflags")" = "%build_cflags" ]
then
	sed -i "s/__RPM_FS/OPA_FEATURE_SET=$OPA_FEATURE_SET/g" eth-tools.spec
else
	sed -i "s/__RPM_FS/OPA_FEATURE_SET=$OPA_FEATURE_SET CLOCAL='%build_cflags' CCLOCAL='%build_cxxflags' LDLOCAL='%build_ldflags'/g" eth-tools.spec
fi

if [ "$(rpm --eval "%{version_no_tilde}")" = "%{version_no_tilde}" ]
then
	version=$(grep '^Version:' eth-tools.spec |cut -d' ' -f2 |sed 's/~/-/g')
	sed -i "s/%{version_no_tilde}/$version/g" eth-tools.spec
fi

source ./OpenIb_Host/ff_filegroups.sh

if [ "$id" = "rhel" -o "$id" = "centos" -o "$id" = "rocky" -o "$id" = "almalinux" -o "$id" = "circle" ]
then
	# __RPM_REQ_BASIC -
	sed -i "s/__RPM_REQ_BASIC1/expect%{?_isa}, tcl%{?_isa}, libibverbs-utils%{?_isa}, librdmacm-utils%{?_isa}, net-snmp%{?_isa}, net-snmp-utils%{?_isa}/g" eth-tools.spec
	sed -i "/__RPM_REQ_BASIC2/d" eth-tools.spec

	sed -i "s/__RPM_BLDREQ1/make, expat-devel, gcc-c++, tcl-devel, rdma-core-devel, net-snmp-devel/g" eth-tools.spec
	sed -i "/__RPM_BLDREQ2/d" eth-tools.spec

	# __RPM_DEBUG same for all RHEL versions
	sed -i "/__RPM_DEBUG_PKG/,+1d" eth-tools.spec

	#Setup Epoch tags for RHEL rpms
	sed -i "s/__RPM_EPOCH/Epoch: 1/g" eth-tools.spec

elif [ "$id" = "sles" ]
then
	sed -i "s/__RPM_DEBUG_PKG/%debug_package/g" eth-tools.spec

	sed -i "s/__RPM_REQ_BASIC1/libibverbs-utils, librdmacm-utils, expect, tcl, net-snmp/g" eth-tools.spec
	sed -i "s/__RPM_BLDREQ1/make, libexpat-devel, gcc-c++, tcl-devel, rdma-core-devel, net-snmp-devel/g" eth-tools.spec

	sed -i "/__RPM_REQ_BASIC2/d" eth-tools.spec
	sed -i "/__RPM_BLDREQ2/d" eth-tools.spec

	#Cleanup EPOCH macros from sles spec.
	#Note that SUSE discourages and does not use epochs
	sed -i "/__RPM_EPOCH/d" eth-tools.spec
elif [ "$id" = "fedora" ]
then
	# __RPM_REQ_BASIC -
	sed -i "s/__RPM_REQ_BASIC1/expect%{?_isa}, tcl%{?_isa}, libibverbs-utils%{?_isa}, librdmacm-utils%{?_isa}, net-snmp%{?_isa}, net-snmp-utils%{?_isa}/g" eth-tools.spec
	sed -i "/__RPM_REQ_BASIC2/d" eth-tools.spec

	buildreqs="BuildRequires: make\n\
BuildRequires: expat-devel\n\
BuildRequires: gcc-c++\n\
BuildRequires: tcl-devel\n\
BuildRequires: rdma-core-devel\n\
BuildRequires: net-snmp-devel\n"
	sed -i "s/^BuildRequires: __RPM_BLDREQ1/${buildreqs}/g" eth-tools.spec
	sed -i "/__RPM_BLDREQ2/d" eth-tools.spec

	# __RPM_DEBUG same for all RHEL versions
	sed -i "/__RPM_DEBUG_PKG/,+1d" eth-tools.spec

	#Setup Epoch tags for RHEL rpms
	sed -i "s/__RPM_EPOCH/Epoch: 1/g" eth-tools.spec

else
	echo ERROR: Unsupported distribution: $id $versionid
	exit 1
fi

> .tmpspec
while read line
do
	if [ "$line" = "__RPM_BASIC_FILES" ]
	then
		for i in $basic_tools_sbin $basic_tools_sbin_sym
		do
			echo "%{_sbindir}/$i" >> .tmpspec
		done
		for i in $basic_tools_opt
		do
			echo "%{_prefix}/lib/eth-tools/$i" >> .tmpspec
		done
		for i in $basic_mans
		do
			echo "%{_mandir}/man1/${i}*" >> .tmpspec
		done
		for i in $basic_samples
		do
			echo "%{_datadir}/eth-tools/samples/$i" >> .tmpspec
		done
	else
		echo "$line" >> .tmpspec
	fi
done < eth-tools.spec
mv .tmpspec eth-tools.spec

> .tmpspec
while read line
do
	if [ "$line" = "__RPM_FF_FILES" ]
	then
		echo "%{_sbindir}/*" >> .tmpspec
		for i in $basic_tools_sbin $basic_tools_sbin_sym
		do
			echo "%exclude %{_sbindir}/$i" >> .tmpspec
		done
		echo "%{_prefix}/lib/eth-tools/*" >> .tmpspec
		for i in $basic_tools_opt
		do
			echo "%exclude %{_prefix}/lib/eth-tools/$i" >> .tmpspec
		done
		echo "%{_datadir}/eth-tools/*" >> .tmpspec
		for i in $basic_samples
		do
			echo "%exclude %{_datadir}/eth-tools/samples/$i" >> .tmpspec
		done
		echo "%{_mandir}/man8/eth*.8*" >> .tmpspec
		echo "%{_usrsrc}/eth/*" >> .tmpspec
	else
		echo "$line" >> .tmpspec
	fi
done < eth-tools.spec
mv .tmpspec eth-tools.spec

exit 0
