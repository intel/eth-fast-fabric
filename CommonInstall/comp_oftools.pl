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

# ==========================================================================
# Fast Fabric Support tools for OFA (eth_tools) installation

sub get_rpms_dir_eth_tools
{
	my $srcdir=$ComponentInfo{'eth_tools'}{'SrcDir'};
	my $pkg_dir = get_binary_pkg_dir($srcdir);
	return "$pkg_dir/*";
}

sub available_eth_tools
{
# TBD - could we move the algorithms for many of these functions into
# util_component.pl and simply put a list of rpms in the ComponentInfo
# as well as perhaps config files
	my $srcdir=$ComponentInfo{'eth_tools'}{'SrcDir'};
	my $pkg_dir = get_binary_pkg_dir($srcdir);
	return (rpm_resolve("$pkg_dir/*/eth-tools-basic", "any") ne "");
}

sub installed_eth_tools
{
	return rpm_is_installed("eth-tools-basic", "any");
}

# only called if installed_eth_tools is true
sub installed_version_eth_tools
{
	my $version = rpm_query_version_release_pkg("eth-tools-basic");
	return dot_version("$version");
}

# only called if available_eth_tools is true
sub media_version_eth_tools
{
	my $srcdir=$ComponentInfo{'eth_tools'}{'SrcDir'};
	my $pkg_dir = get_binary_pkg_dir($srcdir);
	my $rpmfile = rpm_resolve("$pkg_dir/*/eth-tools-basic", "any");
	my $version= rpm_query_version_release("$rpmfile");
	# assume media properly built with matching versions for all rpms
	return dot_version("$version");
}

sub build_eth_tools
{
	my $osver = $_[0];
	my $debug = $_[1];	# enable extra debug of build itself
	my $build_temp = $_[2];	# temp area for use by build
	my $force = $_[3];	# force a rebuild
	return 0;	# success
}

sub need_reinstall_eth_tools($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	return "no";
}

sub preinstall_eth_tools
{
	my $install_list = $_[0];	# total that will be installed when done
	my $installing_list = $_[1];	# what items are being installed/reinstalled

	return 0;	# success
}

sub install_eth_tools
{
	my $install_list = $_[0];	# total that will be installed when done
	my $installing_list = $_[1];	# what items are being installed/reinstalled

	my $version=media_version_eth_tools();
	chomp $version;
	printf("Installing $ComponentInfo{'eth_tools'}{'Name'} $version $DBG_FREE...\n");
	# TBD - review all components and make installing messages the same
	#LogPrint "Installing $ComponentInfo{'eth_tools'}{'Name'} $version $DBG_FREE for $CUR_OS_VER\n";
	LogPrint "Installing $ComponentInfo{'eth_tools'}{'Name'} $version $DBG_FREE for $CUR_DISTRO_VENDOR $CUR_VENDOR_VER\n";

	# RHEL7.4 and older in-distro IFS defines opa-address-resolution depends on eth-tools-basic with exact version match
	# that will fail our installation because of dependency check. We need to use '-nodeps' to force the installation
	install_comp_rpms('eth_tools', " -U --nodeps ", $install_list);

# TBD - could we figure out the list of config files from a query of rpm
# and then simply iterate on each config file?
	check_rpm_config_file("/etc/rdma/dsap.conf");

	$ComponentWasInstalled{'eth_tools'}=1;
}

sub postinstall_eth_tools
{
	my $install_list = $_[0];	# total that will be installed when done
	my $installing_list = $_[1];	# what items are being installed/reinstalled
}

sub uninstall_eth_tools
{
	my $install_list = $_[0];	# total that will be left installed when done
	my $uninstalling_list = $_[1];	# what items are being uninstalled

	NormalPrint("Uninstalling $ComponentInfo{'eth_tools'}{'Name'}...\n");

	uninstall_comp_rpms('eth_tools', '', $install_list, $uninstalling_list, 'verbose');

	# may be created by ethverifyhosts
	system("rm -rf /usr/lib/eth-tools/nodescript.sh");
	system("rm -rf /usr/lib/eth-tools/nodeverify.sh");

	system "rmdir /usr/lib/eth-tools 2>/dev/null";	# remove only if empty

	# eth_tools is a prereq of fastfabric can cleanup shared files here
	system("rm -rf $BASE_DIR/version_ff");
	system "rmdir $BASE_DIR 2>/dev/null";	# remove only if empty
	system "rmdir $ETH_CONFIG_DIR 2>/dev/null";	# remove only if empty
	system("rm -rf /usr/lib/eth-tools/.comp_eth_tools.pl");
	system "rmdir /usr/lib/eth-tools 2>/dev/null";	# remove only if empty
	$ComponentWasInstalled{'eth_tools'}=0;
}

sub check_os_prereqs_eth_tools
{
	return rpm_check_os_prereqs("eth_tools", "user");
}
