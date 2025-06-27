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
##

pkgversion=$1
pkgrelease=$2
export BUILD_ARGS="$3"


read id versionid <<< $(./get_id_and_versionid.sh)

echo "Building packages for OS [$id] version [$versionid]"
echo "Packages version [$pkgversion] release [$pkgrelease], args [$BUILD_ARGS]"

build_deb="no"
spec_mpi="UNDEFINED"
spec_ff="UNDEFINED"
if [ "$id" = "rhel" -o "$id" = "centos" -o "$id" = "rocky" -o "$id" = "almalinux" -o "$id" = "circle" -o "$id" = "ol" ]; then
	GE_8_0=$(echo "$versionid >= 8.0" | bc)
	if [ $GE_8_0 = 1 ]; then
		spec_mpi="mpi-apps.spec.rh8+"
	else
		spec_mpi="mpi-apps.spec.rh"
	fi
	spec_ff="eth-tools.spec.rh"
elif [ "$id" = "fedora" ]; then
	spec_mpi="mpi-apps.spec.fedora"
	spec_ff="eth-tools.spec.fedora"
elif [ "$id" = "ocs" ]; then
	spec_mpi="mpi-apps.spec.ocs"
	spec_ff="eth-tools.spec.ocs"
elif [ "$id" = "sles" ]; then
	GE_15_4=$(echo "$versionid >= 15.4" | bc)
	if [ $GE_15_4 = 1 ]; then
		spec_mpi="mpi-apps.spec.sles154+"
	else
		spec_mpi="mpi-apps.spec.sles"
	fi
	spec_ff="eth-tools.spec.sles"
elif [ "$id" = "ubuntu" ]; then
	build_deb="yes"
else
	echo ERROR: Unsupported distribution: $id $versionid
	exit 1
fi

TAR_DIR="${TL_DIR}/host_tools_sources"
ETH_TOOLS_TAR="${TAR_DIR}/eth-fast-fabric-${pkgversion}.tar.gz"
MPI_APPS_TAR="${TAR_DIR}/eth-mpi-apps.tgz"

if [ "$build_deb" = "yes" ]; then
	BUILDDIR="${TL_DIR}/debbuild"
	ETH_TOOLS_DIR="${BUILDDIR}/eth-fast-fabric-${pkgversion}"
	MPI_APPS_DIR="${BUILDDIR}/eth-mpi-apps-${pkgversion}"
	mkdir -p "${ETH_TOOLS_DIR}"
	mkdir -p "${MPI_APPS_DIR}"
	tar -zxf ${ETH_TOOLS_TAR} --directory ${ETH_TOOLS_DIR}
	tar -zxf ${MPI_APPS_TAR} --directory ${MPI_APPS_DIR}
	(
		set -e
		cd ${ETH_TOOLS_DIR}
		dpkg-buildpackage -uc -us
		cd ${MPI_APPS_DIR}
		dpkg-buildpackage -uc -us

	) || exit 1
else
	RPMDIR="${TL_DIR}/rpmbuild"
	mkdir -p ${RPMDIR}/{BUILD,SPECS,BUILDROOT,SOURCES,RPMS,SRPMS}

	cp ${spec_mpi} ${RPMDIR}/SPECS/mpi-apps.spec
	cp ${spec_ff}  ${RPMDIR}/SPECS/eth-tools.spec
	cp ${ETH_TOOLS_TAR} ${RPMDIR}/SOURCES
	cp ${MPI_APPS_TAR}  ${RPMDIR}/SOURCES
	(
		set -e
		cd $RPMDIR
		rpmbuild -ba --noclean --define "_topdir $RPMDIR" SPECS/eth-tools.spec
		rpmbuild -ba --noclean --define "_topdir $RPMDIR" SPECS/mpi-apps.spec
	) || exit 1
fi

exit 0
