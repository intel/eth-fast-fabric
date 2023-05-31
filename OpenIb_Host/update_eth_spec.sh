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
rpmversion=$1
rpmrelease=$2

update_common () {
	# usage: <file_where> <use_epoch?> <use_debug?>
	sed -i "s/__RPM_VERSION/${rpmversion}/g" $1
	sed -i "s/__RPM_RELEASE/${rpmrelease}%{?dist}/g" $1
	if [ "$2" = "yes" ]; then
		sed -i "s/__RPM_EPOCH/Epoch: 1/g" $1
	else
		sed -i "/__RPM_EPOCH/,+1d" $1
	fi
	if [ "$3" = "yes" ]; then
		sed -i "s/__RPM_DEBUG_PKG/%debug_package/g" $1
	else
		sed -i "/__RPM_DEBUG_PKG/,+1d" $1
	fi

	if [ "$(rpm --eval "%build_cflags")" = "%build_cflags" ]; then
		sed -i "s/__RPM_FS/OPA_FEATURE_SET=$OPA_FEATURE_SET/g" $1
	elif [ "$(rpm --eval "%build_ldflags")" = "%build_ldflags" ]; then
		sed -i "s/__RPM_FS/OPA_FEATURE_SET=$OPA_FEATURE_SET CLOCAL='%build_cflags' CCLOCAL='%build_cxxflags'/g" $1
	else
		sed -i "s/__RPM_FS/OPA_FEATURE_SET=$OPA_FEATURE_SET CLOCAL='%build_cflags' CCLOCAL='%build_cxxflags' LDLOCAL='%build_ldflags'/g" $1
	fi

	if [ "$(rpm --eval "%{version_no_tilde}")" = "%{version_no_tilde}" ]; then
		version=$(grep '^Version:' $1 |cut -d' ' -f2 |sed 's/~/-/g')
		sed -i "s/%{version_no_tilde}/$version/g" $1
	fi
}

substitute_file_contents() {
    # usage: substitute_file_contents <file_where> <string> <file_from>
    perl -pe "s/$2/\`cat $3\`/ge" -i $1
}

update_basic_files() {
	#usage: update_basic_files <template_file> <output_file>
	> $2
	while read line; do
		if [ "$line" = "__RPM_BASIC_FILES" ]
		then
			for i in $basic_tools_sbin $basic_tools_sbin_sym; do
				echo "%{_sbindir}/$i" >> $2
			done
			for i in $basic_tools_opt; do
				echo "%{_prefix}/lib/eth-tools/$i" >> $2
			done
			for i in $basic_mans; do
				echo "%{_mandir}/man1/${i}*" >> $2
			done
			for i in $basic_samples; do
				echo "%{_datadir}/eth-tools/samples/$i" >> $2
			done
			for i in $basic_configs; do
				echo "%config(noreplace) %{_sysconfdir}/eth-tools/$i" >> $2
			done
		else
			echo "$line" >> $2
		fi
	done < $1
}

update_ff_files() {
	#usage: update_ff_files <template_file> <output_file>
	> $2
	while read line; do
		if [ "$line" = "__RPM_FF_FILES" ]; then
			echo "%{_sbindir}/*" >> $2
			for i in $basic_tools_sbin $basic_tools_sbin_sym; do
				echo "%exclude %{_sbindir}/$i" >> $2
			done
			echo "%{_prefix}/lib/eth-tools/*" >> $2
			for i in $basic_tools_opt; do
				echo "%exclude %{_prefix}/lib/eth-tools/$i" >> $2
			done
			echo "%{_datadir}/eth-tools/*" >> $2
			for i in $basic_samples; do
				echo "%exclude %{_datadir}/eth-tools/samples/$i" >> $2
			done
			echo "%{_mandir}/man8/eth*.8*" >> $2
			echo "%{_usrsrc}/eth/*" >> $2
			for i in $ff_configs; do
				echo "%config(noreplace) %{_sysconfdir}/eth-tools/$i" >> $2
			done
			for i in $ff_configs_as_files; do
				echo "%{_sysconfdir}/eth-tools/$i" >> $2
			done
			for i in $ff_configs_non_etc; do
				echo "%config(noreplace) /usr/lib/eth-tools/$i" >> $2
			done
		else
			echo "$line" >> $2
		fi
	done < $1
}

rm -f eth-tools.spec.rh eth-tools.spec.sles eth-tools.spec.fedora

source ./ff_filegroups.sh

update_basic_files eth-tools.spec.in .basicfiles
update_ff_files .basicfiles .allfiles

cp .allfiles eth-tools.spec.rh
cp .allfiles eth-tools.spec.sles
cp .allfiles eth-tools.spec.fedora
cp .allfiles eth-tools.spec.ocs

rm -f .basicfies .allfiles
#             Input                 Epoch Debug
update_common eth-tools.spec.rh     yes   no
update_common eth-tools.spec.sles   no    yes
update_common eth-tools.spec.fedora yes   no
update_common eth-tools.spec.ocs    no    no

substitute_file_contents eth-tools.spec.rh     __RPM_CHANGELOG    changelog_ff.in
substitute_file_contents eth-tools.spec.sles   __RPM_CHANGELOG    changelog_ff.in
substitute_file_contents eth-tools.spec.fedora __RPM_CHANGELOG    changelog_ff.in
substitute_file_contents eth-tools.spec.ocs    __RPM_CHANGELOG    changelog_ff.in

substitute_file_contents eth-tools.spec.rh     __RPM_DEPENDENCIES dependencies.rh.in
substitute_file_contents eth-tools.spec.sles   __RPM_DEPENDENCIES dependencies.sles.in
substitute_file_contents eth-tools.spec.fedora __RPM_DEPENDENCIES dependencies.fedora.in
substitute_file_contents eth-tools.spec.ocs    __RPM_DEPENDENCIES dependencies.ocs.in

substitute_file_contents eth-tools.spec.rh     __FF_RPM_DEPENDENCIES ff_dependencies.rh.in
substitute_file_contents eth-tools.spec.sles   __FF_RPM_DEPENDENCIES ff_dependencies.sles.in
substitute_file_contents eth-tools.spec.fedora __FF_RPM_DEPENDENCIES ff_dependencies.fedora.in
substitute_file_contents eth-tools.spec.ocs    __FF_RPM_DEPENDENCIES ff_dependencies.ocs.in

exit 0
