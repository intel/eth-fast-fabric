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

# This file incorporates work covered by the following copyright and permission notice

# [ICS VERSION STRING: unknown]

# Copyright (c) 2006 Mellanox Technologies. All rights reserved.
#
# This Software is licensed under one of the following licenses:
#
# 1) under the terms of the "Common Public License 1.0" a copy of which is
#    available from the Open Source Initiative, see
#    http://www.opensource.org/licenses/cpl.php.
#
# 2) under the terms of the "The BSD License" a copy of which is
#    available from the Open Source Initiative, see
#    http://www.opensource.org/licenses/bsd-license.php.
#
# 3) under the terms of the "GNU General Public License (GPL) Version 2" a
#    copy of which is available from the Open Source Initiative, see
#    http://www.opensource.org/licenses/gpl-license.php.
#
# Licensee has the right to choose one of the above licenses.
#
# Redistributions of source code must retain the above copyright
# notice and one of the license notices.
#
# Redistributions in binary form must reproduce both the above copyright
# notice, one of the license notices in the documentation
# and/or other materials provided with the distribution.


# rebuild OpenMPI to target a specific compiler

ID=""
VERSION_ID=""

if [ -e /etc/os-release ]; then
	. /etc/os-release
	if [[ "$ID" == "sle_hpc" ]]; then
		ID="sles"
	fi
else
    echo /etc/os-release is not available !!!
fi

PREREQ=( )
MPI_DIR="/usr/src/eth/MPI"
PERL=${PERL:-$(which perl)}

CheckPreReqs()
{
	e=0;
	i=0;
	while [ $i -lt ${#PREREQ[@]} ]; do
		rpm -qa | grep ${PREREQ[$i]} >/dev/null
		if [ $? -ne 0 ]; then
			if [ $e -eq 0 ]; then
				 echo
			fi
			echo "ERROR: Before re-compiling OpenMPI you must first install the ${PREREQ[$i]} package." >&2
			e+=1;
		fi
		i=$((i+1))
	done

	if [ $e -ne 0 ]; then
		if [ $e -eq 1 ]; then
			echo "ERROR: Cannot build. Please install the missing package before re-trying." >&2
		else
			echo "ERROR: Cannot build. Please install the listed packages before re-trying." >&2
		fi
		echo
		exit 2
	fi
}

Usage()
{
	echo "Usage: do_openmpi_build [-d|-O|-C] [config_opt [install_dir]]" >&2
	echo "       -d - use default settings for openmpi options" >&2
	echo "            if omitted, will be prompted for each option" >&2
	echo "       -O - build the MPI targeted for the OFI API." >&2
	echo "       -C - build the MPI targeted for the OFI API with CUDA support." >&2
	echo "       config_opt - a compiler selection option (gcc, pathscale, pgi or intel)" >&2
	echo "             if config_opt is not specified, the user will be prompted" >&2
	echo "             based on compilers found on this system" >&2
	echo "       install_dir - where to install MPI, see MPICH_PREFIX below" >&2
	echo "" >&2
	echo "Environment:" >&2
	echo "    STACK_PREFIX - where to find IEFS stack. Default is /usr" >&2
	echo "    BUILD_DIR - temporary directory to use during build of MPI" >&2
	echo "            Default is /var/tmp/Intel-openmpi" >&2
	echo "    MPICH_PREFIX - selects location for installed MPI" >&2
	echo "            default is /usr/mpi/<COMPILER>/openmpi-<VERSION>" >&2
	echo "            where COMPILER is selected compiler (gcc, pathscale, etc above)" >&2
	echo "            VERSION is openmpi version (eg. 1.0.0)" >&2
	echo "    CONFIG_OPTIONS - additional OpenMPI configuration options to be" >&2
	echo "            specified via configure_options parameter to srpm" >&2
	echo "            Default is ''" >&2
	echo "    INSTALL_ROOT - location of system image in which to install." >&2
	echo "            Default is '/'" >&2
	echo "" >&2
	echo "The RPMs built during this process will be installed on this system" >&2
	echo "they can also be found in $MPI_DIR" >&2
	exit 2
}

unset MAKEFLAGS

# Convert the architecture to all-caps to simplify comparisons.
ARCH=$(uname -m | tr "[:lower:]" "[:upper:]")

target_cpu=$(rpm --eval '%{_target_cpu}')
dist_rpm_rel_int=0

nocomp()
{
	echo "ERROR: No Compiler Found, unable to Rebuild OpenMPI MPI" >&2
	exit 1
}

# determine if the given tool/compiler exists in the PATH
have_comp()
{
	type $1 > /dev/null 2>&1
	return $?
}

# global $ans set to 1 for yes or 0 for no
get_yes_no()
{
	local prompt default input
	prompt="$1"
	default="$2"
	while true
	do
		echo -n "$prompt [$default]:"
		read input
		if [ "x$input" = x ]
		then
			input="$default"
		fi
		case "$input" in
		[Yy]*)	ans=1; break;;
		[Nn]*)	ans=0; break;;
		esac
	done
}

check_arg()
{
	if [ -n "$opt" ]
	then
		echo "ERROR: Option '-$o': cannot use with '-$opt'" >&2
		Usage
	fi
	opt="$1"
}

opt=
skip_prompt=n
iflag=n	# undocumented option, build in context of install
Oflag=n
Cflag=n
bflag=n # undocumented option, include build-id (default disabled)
while getopts "idOCb" o
do
	case "$o" in
	i)
		iflag=y;;
	O)
		check_arg $o
		Oflag=y;;
	C)
		check_arg $o
		Cflag=y;;
	d)
		check_arg $o
		skip_prompt=y;;
	b)
		bflag=y;;
	*)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -gt 2 ]
then
	Usage
fi

if [ "$(/usr/bin/id -u)" != 0 ]
then
	echo "ERROR: You must be 'root' to run this program" >&2
	exit 1
fi
if [ "$iflag" = n ]
then
	cd $MPI_DIR
	if [ $? != 0 ]
	then
		echo "ERROR: Unable to cd to $MPI_DIR" >&2
		exit 1
	fi
fi

echo
echo "IEFS OpenMPI MPI Library/Tools rebuild"

if [ x"$1" != x"" ]
then
	compiler="$1"
else
	compiler=none
	choices=""
	# open MPI does not require fortran compiler, so just check for C
	if have_comp gcc
	then
		choices="$choices gcc"
	fi
	if have_comp pathcc
	then
		choices="$choices pathscale"
	fi
	if have_comp pgcc
	then
		choices="$choices pgi"
	fi
	if have_comp icx
	then
		choices="$choices intel"
	fi
	if have_comp icc
	then
		choices="$choices intellegacy"
	fi
	if [ x"$choices" = x ]
	then
		nocomp
	else
		PS3="Select Compiler: "
		select compiler in $choices
		do
			case "$compiler" in
			gcc|pathscale|pgi|intel) break;;
			esac
		done
	fi
fi

case "$compiler" in
gcc|pathscale|pgi|intel) >/dev/null;;
*)
	echo "ERROR: Invalid Compiler selection: $compiler" >&2
	exit 1;;
esac
shift
if [ ! -z "$1" ]
then
	export MPICH_PREFIX="$1"
fi

# find newest nvcc and add to path
NVCC=$(find /usr/local -name nvcc 2> /dev/null)
if [ "$NVCC" != "" ]
then
	NVCC1=$(echo $NVCC | xargs ls -t | head -n 1)
	NVCCDIR=$(dirname $NVCC1)
fi
export PATH=$PATH:$NVCCDIR

# now get openmpi options.
if [ -z "$opt" ]
then
	Oflag=y # by default build for OFI if no options
	if  rpm -qa|grep cuda-cudart-dev >/dev/null 2>&1
	then
		echo
		get_yes_no "Build for OFI with Cuda" "y"
		if [ "$ans" = 1 ]
		then
			Cflag=y
		fi
	fi
fi
# if -d (skip_prompt) is the only option provided, ./configure will run with
# no paramters and build what is auto-detected

openmpi_conf_psm=''

# we no longer supports verbs.
openmpi_verbs='--enable-mca-no-build=btl-openib --without-verbs'

if [ "$Oflag" = y ]
then
	PREREQ+=('libfabric-devel')
	openmpi_conf_psm=" $openmpi_conf_psm --with-libfabric=/usr "
	# OFI indicated by ofi suffix so user can ID from other MPI builds
	openmpi_path_suffix="-ofi"
	openmpi_rpm_suffix="_ofi"
	interface=ofi
fi

if [ "$Cflag" = y ]
then
	PREREQ+=('libfabric-devel' 'cuda-cudart-dev')
	openmpi_conf_psm=" $openmpi_conf_psm --with-libfabric=/usr --with-cuda=/usr/local/cuda "
	# CUDA indicated by -cuda-ofi suffix so user can ID from other MPI builds
	openmpi_path_suffix="-cuda-ofi"
	openmpi_rpm_suffix="_cuda_ofi"
	interface=cuda_ofi
fi

CheckPreReqs

# just to be safe
unset LDFLAGS
unset CFLAGS
unset CPPFLAGS
unset CXXFLAGS
unset FFLAGS
unset F90FLAGS
unset FCFLAGS
unset LDLIBS

if [ -z "$interface" ]
then
	logfile=make.openmpi.$compiler
else
	logfile=make.openmpi.$interface.$compiler
fi
(
	STACK_PREFIX=${STACK_PREFIX:-/usr}
	BUILD_DIR=${BUILD_DIR:-/var/tmp/Intel-openmpi}
	BUILD_ROOT="$BUILD_DIR/build";
	RPM_DIR="$BUILD_DIR/OFEDRPMS";
	DESTDIR="$MPI_DIR"
	if [ "$iflag" = n ]
	then
		openmpi_srpm=$(ls -v $MPI_DIR/openmpi-*.src.rpm | tail -1)
	else
		openmpi_srpm=$(ls -v ./SRPMS/openmpi-*.src.rpm | tail -1)
	fi
	openmpi_version=$(ls -v $openmpi_srpm 2>/dev/null|tail -1|cut -f2 -d-)

	# For RHEL7x: %{?dist} resolves to '.el7'. For SLES, an empty string
	# E.g. on rhel7.x: openmpi_gcc_ofi-2.1.2-11.el7.x86_64.rpm; on SLES openmpi_gcc_ofi-2.1.2-11.x86_64.rpm
	openmpi_fullversion=$(rpm -q --queryformat '%{VERSION}-%{RELEASE}' $openmpi_srpm)
	MPICH_PREFIX=${MPICH_PREFIX:-$STACK_PREFIX/mpi/$compiler/openmpi-$openmpi_version$openmpi_path_suffix}
	CONFIG_OPTIONS=${CONFIG_OPTIONS:-""}

	if [ x"$openmpi_version" = x"" ]
	then
		echo "Error $openmpi_srpm: Not Found"
		exit 1
	fi

	enable_cxx_bindings=1
	if [ "${openmpi_version//.*/}" -ge 5 ]; then
		echo "Disable CXX Bindings (removed in 5.0)"
		# The MPI C++ bindings were removed from Open MPI v5.0.0 in 2022.
		enable_cxx_bindings=0
	fi

	if [ ! -f $PERL ]; then
		echo "Error: perl is required to build Open MPI: set PERL env or add to PATH"
		exit 1
	fi

	echo "Environment:"
	env
	echo "=========================================================="
	echo
	echo "Build Settings:"
	echo "STACK_PREFIX='$STACK_PREFIX'"
	echo "BUILD_DIR='$BUILD_DIR'"
	echo "MPICH_PREFIX='$MPICH_PREFIX'"
	echo "CONFIG_OPTIONS='$CONFIG_OPTIONS'"
	echo "OpenMPI Version: $openmpi_version"
	echo "OpenMPI Full Version: $openmpi_fullversion"
	echo "=========================================================="
	if [ "$iflag" = n ]
	then
		echo "MPICH_PREFIX='$MPICH_PREFIX'"> $MPI_DIR/.mpiinfo
		#echo "MPI_RUNTIME='$MPICH_PREFIX/bin $MPICH_PREFIX/lib* $MPICH_PREFIX/etc $MPICH_PREFIX/share $MPICH_PREFIX/tests'">> $MPI_DIR/.mpiinfo
		echo "MPI_RPMS='openmpi_$compiler$openmpi_rpm_suffix-$openmpi_fullversion.$target_cpu.rpm'">> $MPI_DIR/.mpiinfo
		chmod +x $MPI_DIR/.mpiinfo
	fi

	echo
	echo "Cleaning build tree..."
	rm -rf $BUILD_DIR > /dev/null 2>&1

	echo "=========================================================="
	echo "Building OpenMPI MPI $openmpi_version Library/Tools..."
	mkdir -p $BUILD_ROOT $RPM_DIR/BUILD $RPM_DIR/RPMS $RPM_DIR/SOURCES $RPM_DIR/SPECS $RPM_DIR/SRPMS

	if [ "$ARCH" = "X86_64" ]
	then
		openmpi_lib="lib64"
	else
		openmpi_lib="lib"
	fi

	use_default_rpm_opt_flags=1
	disable_auto_requires=""
	openmpi_ldflags=""
	openmpi_wrapper_cxx_flags=""

	if [ "$bflag" = "n" ]; then
		openmpi_ldflags="LDFLAGS=-Wl,--build-id=none"
	fi

	# need to create proper openmpi_comp_env value for OpenMPI builds
	openmpi_comp_env=""

	case "$compiler" in
	gcc)
		openmpi_comp_env="$openmpi_comp_env CC=gcc CFLAGS=\"-O3 -fPIC\""
		if (( enable_cxx_bindings == 1 )); then
			if have_comp g++
			then
				openmpi_comp_env="$openmpi_comp_env --enable-mpi-cxx CXX=g++"
			else
				openmpi_comp_env="$openmpi_comp_env --disable-mpi-cxx"
			fi
		fi
		if have_comp gfortran
		then
			openmpi_comp_env="$openmpi_comp_env --enable-mpi-fortran FC=gfortran"
		else
			openmpi_comp_env="$openmpi_comp_env --disable-mpi-fortran"
		fi;;

	intel)
		if ! have_comp icx; then
			echo "ERROR - Requested intel compiler, but icx was not found in the path." >&2
			exit 1
		fi
		disable_auto_requires="--define 'disable_auto_requires 1'"
		openmpi_comp_env="$openmpi_comp_env CC=icx CXX=icpx CFLAGS=\"-O3 -fPIC\""
		varsfile=$(ls -v /opt/intel/oneapi/compiler/[0-9]*/env/vars.sh | tail -1)
		source $varsfile intel64
		if (( enable_cxx_bindings == 1 )); then
			if have_comp icpx
			then
				openmpi_comp_env="$openmpi_comp_env --enable-mpi-cxx CXX=icpx CXXFLAGS="
			else
				openmpi_comp_env="$openmpi_comp_env --disable-mpi-cxx"
			fi
		fi
		if have_comp ifx
		then
			openmpi_comp_env="$openmpi_comp_env --enable-mpi-fortran FC=ifx FCFLAGS="
		else
			openmpi_comp_env="$openmpi_comp_env --disable-mpi-fortran"
		fi;;

	intellegacy)
		disable_auto_requires="--define 'disable_auto_requires 1'"
		openmpi_comp_env="$openmpi_comp_env CC=icc"
		if (( enable_cxx_bindings == 1 )); then
			if have_comp icpc
			then
				openmpi_comp_env="$openmpi_comp_env --enable-mpi-cxx CXX=icpc"
			else
				openmpi_comp_env="$openmpi_comp_env --disable-mpi-cxx"
			fi
		fi
		if have_comp ifort
		then
			openmpi_comp_env="$openmpi_comp_env --enable-mpi-fortran FC=ifort"
		else
			openmpi_comp_env="$openmpi_comp_env --disable-mpi-fortran"
		fi;;

	*)
		echo "ERROR: Invalid compiler"
		exit 1;;
	esac

	openmpi_comp_env="$openmpi_comp_env --enable-mpirun-prefix-by-default"
	if [ "$openmpi_wrapper_cxx_flags" ]
	then
		openmpi_comp_env="$openmpi_comp_env --with-wrapper-cxxflags=\"$openmpi_wrapper_cxx_flags\""
	fi

	pref_env=
	if [ "$STACK_PREFIX" != "/usr" ]
	then
		pref_env="$pref_env LD_LIBRARY_PATH=$STACK_PREFIX/lib64:$STACK_PREFIX/lib:\$LD_LIBRARY_PATH"
	fi

	if [ "$Cflag" = y ]
	then
		pref_env="$pref_env"
	else
		# HWLOC component auto detects CUDA and will use it even if it is NOT
		# a CUDA OMPI build. So, tell HWLOC to ignore CUDA (if found on the system)
		# when not creating a CUDA build.
		pref_env="$pref_env enable_gl=no"
	fi

	cmd="$pref_env rpmbuild --rebuild \
				--define '_topdir $RPM_DIR' \
				--buildroot '$BUILD_ROOT' \
				--define 'build_root $BUILD_ROOT' \
				--target $target_cpu \
				--define '_name openmpi_$compiler$openmpi_rpm_suffix' \
				--define 'compiler $compiler' \
				--define 'install_shell_scripts 1' \
				--define 'shell_scripts_basename mpivars' \
				--define '_usr $STACK_PREFIX' \
				--define '__perl $PERL' \
				--define 'ofed 0' \
				--define '_prefix $MPICH_PREFIX' \
				--define '_defaultdocdir $MPICH_PREFIX/doc/..' \
				--define '_mandir %{_prefix}/share/man' \
				--define 'mflags -j 4' \
				--define 'all_external_3rd_party 0' \
				--define 'configure_options $CONFIG_OPTIONS $openmpi_ldflags $openmpi_comp_env $openmpi_conf_psm --with-devel-headers --disable-oshmem $openmpi_verbs --with-libevent=external' \
				--define 'use_default_rpm_opt_flags $use_default_rpm_opt_flags' \
				$disable_auto_requires"
	cmd="$cmd \
				$openmpi_srpm"
	echo "Executing: $cmd"
	eval $cmd
	if [ $? != 0 ]
	then
		echo "error: openmpi_$compiler$openmpi_rpm_suffix Build ERROR: bad exit code"
		exit 1
	fi
	if [ "$iflag" = n ]
	then
		cp $RPM_DIR/RPMS/$target_cpu/openmpi_$compiler$openmpi_rpm_suffix-$openmpi_fullversion.$target_cpu.rpm $DESTDIR
	fi

	echo "=========================================================="
	echo "Installing OpenMPI MPI $openmpi_version Library/Tools..."
	rpmfile=$RPM_DIR/RPMS/$target_cpu/openmpi_$compiler$openmpi_rpm_suffix-$openmpi_fullversion.$target_cpu.rpm

	# need force for reinstall case
	if [ x"$INSTALL_ROOT" != x"" -a x"$INSTALL_ROOT" != x"/" ]
	then
		tempfile=/var/tmp/rpminstall.tmp.rpm
		mkdir -p $INSTALL_ROOT/var/tmp
		cp $rpmfile $INSTALL_ROOT$tempfile
		#chroot /$INSTALL_ROOT rpm -ev --nodeps openmpi_$compiler$openmpi_rpm_suffix-$openmpi_version
		chroot /$INSTALL_ROOT rpm -Uv --force $tempfile
		rm -f $tempfile
	else
		#rpm -ev --nodeps openmpi_$compiler$openmpi_rpm_suffix-$openmpi_version
		rpm -Uv --force $rpmfile
	fi

) 2>&1|tee $logfile.res
set +x

# review log for errors and warnings
# disable output of build warnings, way too many
#echo
#echo "Build Warnings:"
# ignore the warning for old C++ header usage in sample programs
grep -E 'warning:' $logfile.res |sort -u |
	grep -E -v 'at least one deprecated or antiquated header.*C\+\+ includes' > $logfile.warn
#cat $logfile.warn
#echo

#grep -E 'error:|Error | Stop' $logfile.res| sort -u |
#	grep -E -v 'error: this file was generated for autoconf 2.61.' > $logfile.err
grep -E 'error:|Error | Stop' $logfile.res| sort -u |
	grep -E -v 'configure: error: no BPatch.h found; check path for Dyninst package|configure: error: no vtf3.h found; check path for VTF3 package|configure: error: MPI Correctness Checking support cannot be built inside Open MPI|configure: error: no bmi.h found; check path for BMI package first...|configure: error: no ctool/ctool.h found; check path for CTool package first...|configure: error: no cuda.h found; check path for CUDA Toolkit first...|configure: error: no cuda_runtime_api.h found; check path for CUDA Toolkit first...|configure: error: no cupti.h found; check path for CUPTI package first...|configure: error: no f2c.h found; check path for CLAPACK package first...|configure: error: no jvmti.h found; check path for JVMTI package first...|configure: error: no libcpc.h found; check path for CPC package first...|configure: error: no tau_instrumentor found; check path for PDToolkit first...|configure: error: no unimci-config found; check path for UniMCI package first...|"Error code:|"Unknown error:|strerror_r|configure: error: CUPTI API version could not be determined...|asprintf\(&msg, "Unexpected sendto\(\) error: errno=%d \(%s\)",' > $logfile.err

if [ -s $logfile.err ]
then
	echo "Build Errors:"
	sort -u $logfile.err
	echo
	echo "FAILED Build, errors detected"
	exit 1
elif [ -s $logfile.warn ]
then
	# at present lots of warnings are expected
	echo "SUCCESSFUL Build, no errors detected"
	exit 0

	# No warnings are expected
	echo "QUESTIONABLE Build, warnings detected"
	exit 1
else
	echo "SUCCESSFUL Build, no errors detected"
	exit 0
fi
