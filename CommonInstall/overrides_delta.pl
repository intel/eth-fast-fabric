#!/usr/bin/perl
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

# [ICS VERSION STRING: unknown]
use strict;
#use Term::ANSIColor;
#use Term::ANSIColor qw(:constants);
#use File::Basename;
#use Math::BigInt;

	# Names of supported install components
	# must be listed in dependency order such that prereqs appear 1st
	# delta_debug must be last
my @delta_Components_sles12_sp4 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);
my @delta_Components_sles12_sp5 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);
my @delta_Components_rhel78 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);
my @delta_Components_rhel79 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);
my @delta_Components_rhel8 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);
my @delta_Components_rhel81 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);
my @delta_Components_rhel82 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);
my @delta_Components_rhel83 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);
my @delta_Components_sles15 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);
my @delta_Components_sles15_sp1 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);
my @delta_Components_sles15_sp2 = (
	"psm3",
	"eth_rdma",
	"delta_debug",
	);

@Components = ( );
# RHEL7.3 and newer AND SLES12.2 and newer
my @delta_SubComponents_newer = ( "snmp" );
@SubComponents = ( );

# override some of settings in main_omnipathwrap_delta.pl
sub overrides()
{
	# The component list has slight variations per distro
	if ( "$CUR_VENDOR_VER" eq "ES124" ) {
		@Components = ( @delta_Components_sles12_sp4 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES125" ) {
		@Components = ( @delta_Components_sles12_sp5 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES78" ) {
		@Components = ( @delta_Components_rhel78 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES79" ) {
		@Components = ( @delta_Components_rhel79 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES8" ) {
		@Components = ( @delta_Components_rhel8 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES81" ) {
		@Components = ( @delta_Components_rhel81 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES82" ) {
		@Components = ( @delta_Components_rhel82 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES83" ) {
		@Components = ( @delta_Components_rhel83 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES15" ) {
		@Components = ( @delta_Components_sles15 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES151" ) {
		@Components = ( @delta_Components_sles15_sp1 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES152" ) {
		@Components = ( @delta_Components_sles15_sp2 );
	} else {
		# unsupported OS
		@Components = ( );
	}

	# Sub components for autostart processing
	@SubComponents = ( @delta_SubComponents_newer );

	# TBD remove this concept
	# no WrapperComponent (eg. iefsconfig)
	$WrapperComponent = "";

	# set SrcDir for all components to .
	foreach my $comp ( @Components )
	{
        $ComponentInfo{$comp}{'SrcDir'} = ".";
	}
}
