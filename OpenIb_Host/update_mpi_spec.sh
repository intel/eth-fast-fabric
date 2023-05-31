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

substitute_file_contents() {
    # usage: substitute_file_contents <file_where> <string> <file_from>
    perl -pe "s/$2/\`cat $3\`/ge" -i $1
}

rpmversion=$1
rpmrelease=$2

rm -f mpi-apps.spec
cp mpi-apps.spec.in mpi-apps.spec

sed -i "s/__RPM_VERSION/${rpmversion}/g" mpi-apps.spec
sed -i "s/__RPM_RELEASE/${rpmrelease}%{?dist}/g" mpi-apps.spec
substitute_file_contents mpi-apps.spec __RPM_CHANGELOG changelog_mpi-apps.in

cp mpi-apps.spec mpi-apps.spec.rh
cp mpi-apps.spec mpi-apps.spec.rh8+
cp mpi-apps.spec mpi-apps.spec.fedora
cp mpi-apps.spec mpi-apps.spec.sles
cp mpi-apps.spec mpi-apps.spec.sles154+
cp mpi-apps.spec mpi-apps.spec.ocs

rm -f mpi-apps.spec

sed -i "/__RPM_REQ/,+1d"                              mpi-apps.spec.rh
sed -i "/__RPM_REQ/,+1d"                              mpi-apps.spec.rh8+
sed -i "/__RPM_REQ/,+1d"                              mpi-apps.spec.fedora
sed -i "/__RPM_REQ/,+1d"                              mpi-apps.spec.sles
sed -i "/__RPM_REQ/,+1d"                              mpi-apps.spec.sles154+
sed -i "/__RPM_REQ/,+1d"                              mpi-apps.spec.ocs

sed -i "/__RPM_DBG/,+1d"                              mpi-apps.spec.rh
sed -i "s/__RPM_DBG/%global debug_package %{nil}/"    mpi-apps.spec.rh8+
sed -i "s/__RPM_DBG/%global debug_package %{nil}/"    mpi-apps.spec.fedora
sed -i "/__RPM_DBG/,+1d"                              mpi-apps.spec.sles
sed -i "/__RPM_DBG/,+1d"                              mpi-apps.spec.sles154+
sed -i "/__RPM_DBG/,+1d"                              mpi-apps.spec.ocs

exit 0
