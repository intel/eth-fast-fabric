#!/usr/bin/perl
## BEGIN_ICS_COPYRIGHT8 ****************************************
#
# Copyright (c) 2015-2024, Intel Corporation
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
## END_ICS_COPYRIGHT8   ****************************************
#
## [ICS VERSION STRING: unknown]
#use strict;
##use Term::ANSIColor;
##use Term::ANSIColor qw(:constants);
##use File::Basename;
##use Math::BigInt;
#
## ==========================================================================
#
#Installation Prequisites array for delta components
my @iefsconfig_prereq = (
    "bash",
    "iproute",
    "lldpad",
);
$comp_prereq_hash{'iefsconfig_prereq'} = \@iefsconfig_prereq;

my @eth_module_prereq = (
    "bash",
    "kernel-core",
    "kmod",
);
$comp_prereq_hash{'eth_module_prereq'} = \@eth_module_prereq;

my @psm3_prereq = (
    "bash",
    "glibc",
    "hwloc-libs",
    "libatomic",
    "libgcc",
    "libibverbs",
    "numactl-libs",
    "libuuid",
    "rdma-core",
);
$comp_prereq_hash{'psm3_prereq'} = \@psm3_prereq;

my @openmpi_prereq = (
    "bash",
    "glibc",
    "libgcc",
    "libgfortran",
    "gcc-gfortran",
    "libgomp",
    "libibverbs",
    "libquadmath",
    "librdmacm",
    "libstdc++",
    "libstdc++-devel",
    "opensm-libs",
    "pkgconf",
    "zlib-ng-compat",
);
$comp_prereq_hash{'openmpi_prereq'} = \@openmpi_prereq;

my @openmpi_gcc_ofi_prereq = (
    "bash",
    "glibc",
    "libfabric",
    "libgcc",
    "libgfortran",
    "libnl3",
    "libquadmath",
    "perl",
    "pkgconf-pkg-config",
    "zlib-ng-compat",
);
$comp_prereq_hash{'openmpi_gcc_ofi_prereq'} = \@openmpi_gcc_ofi_prereq;

my @openmpi_intel_ofi_prereq = (
    "bash",
);
$comp_prereq_hash{'openmpi_intel_ofi_prereq'} = \@openmpi_intel_ofi_prereq;
