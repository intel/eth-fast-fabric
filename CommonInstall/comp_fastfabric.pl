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
# Fast Fabric installation

my $FF_CONF_FILE = "/usr/lib/eth-tools/ethfastfabric.conf";

sub get_rpms_dir_fastfabric
{
	my $srcdir=$ComponentInfo{'fastfabric'}{'SrcDir'};
	my $pkg_dir = get_binary_pkg_dir($srcdir);
	return "$pkg_dir/*";
}

sub pkg_fastfabric
{
	my $pkg_dir = get_rpms_dir_fastfabric();
	return (rpm_resolve("$pkg_dir/eth-tools-fastfabric", "any"));
}

sub pkg_mpiapps
{
	my $pkg_dir = get_rpms_dir_fastfabric();
	return (rpm_resolve("$pkg_dir/eth-mpi-apps", "any"));
}


sub available_fastfabric
{
	return ((pkg_fastfabric() ne "") && (pkg_mpiapps() ne ""));
}

sub installed_fastfabric
{
	return rpm_is_installed("eth-tools-fastfabric", "any");
}

# only called if installed_fastfabric is true
sub installed_version_fastfabric
{
	my $version = rpm_query_version_release_pkg("eth-tools-fastfabric");
	return dot_version("$version");
}

# only called if available_fastfabric is true
sub media_version_fastfabric
{
	my $rpmfile = pkg_fastfabric();
	my $version= rpm_query_version_release("$rpmfile");
	# assume media properly built with matching versions for all rpms
	return dot_version("$version");
}

sub build_fastfabric
{
	my $osver = $_[0];
	my $debug = $_[1];	# enable extra debug of build itself
	my $build_temp = $_[2];	# temp area for use by build
	my $force = $_[3];	# force a rebuild
	return 0;	# success
}

sub need_reinstall_fastfabric($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	return "no";
}

sub check_os_prereqs_fastfabric
{	
	return rpm_check_os_prereqs("fastfabric", "user");
}

sub preinstall_fastfabric
{
	my $install_list = $_[0];	# total that will be installed when done
	my $installing_list = $_[1];	# what items are being installed/reinstalled

	return 0;	# success
}

sub install_fastfabric
{
	my $install_list = $_[0];	# total that will be installed when done
	my $installing_list = $_[1];	# what items are being installed/reinstalled

	my $deprecated_dir = "/etc/sysconfig/eth-tools";

	my $version=media_version_fastfabric();
	chomp $version;
	printf("Installing $ComponentInfo{'fastfabric'}{'Name'} $version $DBG_FREE...\n");
	LogPrint "Installing $ComponentInfo{'fastfabric'}{'Name'} $version $DBG_FREE for $CUR_DISTRO_VENDOR $CUR_VENDOR_VER\n";

	install_comp_rpms('fastfabric', " -U --nodeps ", $install_list);

	# TBD - spec file should do this
	check_dir("/usr/share/eth-tools/samples");
	system "chmod ug+x /usr/share/eth-tools/samples/hostverify.sh";
	system "rm -f /usr/share/eth-tools/samples/nodeverify.sh";

	check_rpm_config_file("$CONFIG_DIR/eth-tools/ethmon.conf", $deprecated_dir);
	check_rpm_config_file("$CONFIG_DIR/eth-tools/ethfastfabric.conf", $deprecated_dir);
	check_rpm_config_file("$CONFIG_DIR/eth-tools/allhosts", $deprecated_dir);
	check_rpm_config_file("$CONFIG_DIR/eth-tools/hosts", $deprecated_dir);
	check_rpm_config_file("$CONFIG_DIR/eth-tools/switches", $deprecated_dir);
	check_rpm_config_file("$CONFIG_DIR/eth-tools/mgt_config.xml", $deprecated_dir);
# TBD - this should not be a config file
	check_rpm_config_file("/usr/lib/eth-tools/osid_wrapper");

	$ComponentWasInstalled{'fastfabric'}=1;
}

sub postinstall_fastfabric
{
	my $install_list = $_[0];	# total that will be installed when done
	my $installing_list = $_[1];	# what items are being installed/reinstalled
}

sub uninstall_fastfabric
{
	my $install_list = $_[0];	# total that will be left installed when done
	my $uninstalling_list = $_[1];	# what items are being uninstalled

	uninstall_comp_rpms('fastfabric', '', $install_list, $uninstalling_list, 'verbose');

	NormalPrint("Uninstalling $ComponentInfo{'fastfabric'}{'Name'}...\n");
	remove_conf_file("$ComponentInfo{'fastfabric'}{'Name'}", "$FF_CONF_FILE");
	remove_conf_file("$ComponentInfo{'fastfabric'}{'Name'}", "$ETH_CONFIG_DIR/iba_stat.conf");

	# remove samples we installed (or user compiled), however do not remove
	# any logs or other files the user may have created
	remove_installed_files "/usr/share/eth-tools/samples";
	system "rmdir /usr/share/eth-tools/samples 2>/dev/null";	# remove only if empty
	# just in case, newer rpms should clean these up

	system("rm -rf /usr/lib/eth-tools/.comp_fastfabric.pl");
	system "rmdir /usr/lib/eth-tools 2>/dev/null";	# remove only if empty
	system "rmdir $BASE_DIR 2>/dev/null";	# remove only if empty
	system "rmdir $ETH_CONFIG_DIR 2>/dev/null";	# remove only if empty
	$ComponentWasInstalled{'fastfabric'}=0;
}

