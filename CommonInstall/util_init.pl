#!/usr/bin/perl
## BEGIN_ICS_COPYRIGHT8 ****************************************
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

#

#============================================================================
# initialization
use strict;
use Term::ANSIColor;
use Term::ANSIColor qw(:constants);
use File::Basename;
use Math::BigInt;
use Cwd;

#Setup some defaults
my $exit_code=0;

my $Force_Install = 0;# force option used to force install on unsupported distro
my $GPU_Install = "NONE";
my $GPU_Dir = "";
# When --user-space is selected we are targeting a user space container for
# installation and will skip kernel modules and firmware
my $user_space_only = 0; # can be set to 1 by --user-space argument

# some options specific to OFA builds
my $OFED_force_rebuild=0;

my $CUR_OS_VER = `uname -r`;
chomp $CUR_OS_VER;

my $RPMS_SUBDIR = "RPMS";
my $SRPMS_SUBDIR = "SRPMS";

# firmware and data files
my $OLD_BASE_DIR = "/etc/sysconfig/eth-tools";
my $BASE_DIR = "/etc/eth-tools";
# iba editable config scripts
my $OFA_CONFIG_DIR = "/etc/rdma";
my $ETH_CONFIG_DIR = "/etc/eth-tools";

my $UVP_CONF_FILE = "$BASE_DIR/uvp.conf";
my $UVP_CONF_FILE_SOURCE = "uvp.conf";
my $DAT_CONF_FILE_SOURCE = "dat.conf";
my $NETWORK_CONF_DIR = "/etc/sysconfig/network-scripts";
my $BIN_DIR = "/usr/sbin";
# Path to ethsystemconfig
my $ETH_SYSTEMCFG_FILE = "/sbin/ethsystemconfig";
# Path to ethautostartconfig
my $ETH_ASCFG_FILE = "/sbin/ethautostartconfig";
my $DEFAULT_LIMITS_SEL="5";


#This string is compared in verify_os_rev for correct revision of
#kernel release.
my $CUR_DISTRO_VENDOR = "";
my $CUR_VENDOR_VER = "";	# full version (such as ES5.1)
my $CUR_VENDOR_MAJOR_VER = "";    # just major number part (such as ES5)
my $ARCH = `uname -m | sed -e s/ppc/PPC/ -e s/powerpc/PPC/ -e s/i.86/IA32/ -e s/ia64/IA64/ -e s/x86_64/X86_64/`;
chomp $ARCH;
my $DRIVER_SUFFIX=".o";
if (substr($CUR_OS_VER,0,3) eq "2.6" || substr($CUR_OS_VER,0,2) eq "3.")
{
	$DRIVER_SUFFIX=".ko";
}
my $DBG_FREE="release";


# Command paths
my $RPM = "/bin/rpm";

# a few key commands to verify exist
my @verify_cmds = ( "uname", "mv", "cp", "rm", "ln", "cmp", "yes", "echo", "sed", "chmod", "chown", "chgrp", "mkdir", "rmdir", "grep", "diff", "awk", "find", "xargs", "sort");

# eth-scripts expects the following env vars to be 0 or 1. We set them to the default value here
setup_env("ETH_INSTALL_CALLER", 0);
default_opascripts_env_vars();

sub Abort(@);
sub NormalPrint(@);
sub LogPrint(@);
sub HitKeyCont();
# ============================================================================
# General utility functions

# verify the given command can be found in the PATH
sub check_cmd_exists($)
{
	my $cmd=shift();
	return (0 == system("which $cmd >/dev/null 2>/dev/null"));
}

sub check_root_user()
{
	my $user;
	$user=`/usr/bin/id -u`;
	if ($user != 0)
	{
		die "\n\nYou must be \"root\" to run this install program\n\n";
	}

	@verify_cmds = (@verify_cmds, rpm_get_cmds_for_verification());
	# verify basic commands are in path
	foreach my $cmd ( @verify_cmds ) {
		if (! check_cmd_exists($cmd)) {
			die "\n\n$cmd not found in PATH.\nIt is required to login as root or su - root.\nMake sure the path includes /sbin and /usr/sbin\n\n";
		}
	}
}

sub my_tolower($)
{
	my($str) = shift();

	$str =~ tr/[A-Z]/[a-z]/;
	return "$str";
}

# ============================================================================
# Version and branding

# version string is filled in by prep, special marker format for it to use
my $VERSION = "THIS_IS_THE_ICS_VERSION_NUMBER:@(#)000.000.000.000B000";
$VERSION =~ s/THIS_IS_THE_ICS_VERSION_NUMBER:@\(#\)//;
$VERSION =~ s/%.*//;
my $INT_VERSION = "THIS_IS_THE_ICS_INTERNAL_VERSION_NUMBER:@(#)000.000.000.000B000";
$INT_VERSION =~ s/THIS_IS_THE_ICS_INTERNAL_VERSION_NUMBER:@\(#\)//;
$INT_VERSION =~ s/%.*//;
my $BRAND = "THIS_IS_THE_ICS_BRAND:Intel%                    ";
# backslash before : is so patch_brand doesn't replace string
$BRAND =~ s/THIS_IS_THE_ICS_BRAND\://;
$BRAND =~ s/%.*//;

# convert _ and - in version to dots
sub dot_version($)
{
	my $version = shift();

	$version =~ tr/_-/./;
	return $version;
}

# ============================================================================
# installation paths

# where to install libraries
my $LIB_DIR = "/lib";
my $UVP_LIB_DIR = "/lib";
my $USRLOCALLIB_DIR = "/usr/local/lib";
my $USRLIB_DIR = "/usr/lib";
# if different from $LIB_DIR, where to remove libraries from past release
my $OLD_LIB_DIR = "/lib";
my $OLD_USRLOCALLIB_DIR = "/usr/local/lib";
my $OLD_UVP_LIB_DIR = "/lib";

sub set_libdir()
{
	if ( -d "/lib64" )
	{
		$LIB_DIR = "/lib64";
		$UVP_LIB_DIR = "/lib64";
		$UVP_CONF_FILE_SOURCE = "uvp.conf.64";
		$DAT_CONF_FILE_SOURCE = "dat.conf.64";
		$USRLOCALLIB_DIR = "/usr/local/lib64";
		$USRLIB_DIR = "/usr/lib64";
	}
}

# determine the os vendor release level based on build system
# this script is stolen from funcs-ext.sh and should
# be maintained in parallel
sub os_vendor_version($$)
{
	my $vendor = shift();
	my $ver_num = shift();

	my $rval = "";
	$rval = $ver_num;
	$rval =~ m/(\d+).(\d+)/;
	if ($2 eq "0") {
		$rval = $1;
	} else {
		$rval = $1.$2;
	}
	chomp($rval);

	if ($vendor eq "ubuntu") {
		$rval = "UB".$rval;
	} elsif ($vendor =~ /^(SuSE|redhat|opencloudos)$/) {
		$rval = "ES".$rval;
	} else {
		# Should not Happen
		Abort "Please contact your support representative...\n";
	}
	return $rval;
}

# Get OS distribution, vendor and vendor version
# set CUR_DISTRO_VENDOR, CUR_VENDOR_VER, CUR_VENDOR_MAJOR_VER
sub determine_os_version()
{
	# we use the current system to select the distribution
	# All modern OSes (we support) use os-release file in one of 2 locations:
	#  /etc and /usr/lib (/etc is often symlink to /usr/lib)
	# Check if '^ID=' is in file
	my $os_release_file = "";
	if ( -e "/etc/os-release" && !system("cat /etc/os-release | grep -q '^ID='")) {
		$os_release_file = "/etc/os-release";
	} elsif ( -e "/usr/lib/os-release" && !system("cat /usr/lib/os-release | grep -q '^ID='")) {
		$os_release_file = "/usr/lib/os-release";
	} else {
		NormalPrint "INSTALL could not read 'ID=...' in os-release file(s)\n";
		Abort "Please contact your support representative...\n";
	}

	my %distroVendor = (
		"rhel" => "redhat",
		"centos" => "redhat",
		"rocky" => "redhat",
		"almalinux" => "redhat",
		"navix" => "redhat",
		"circle" => "redhat",
		"oracle" => "redhat",
		"opencloudos" => "opencloudos",
		"sles" => "SuSE",
		"sle_hpc" => "SuSE",
		"ubuntu" => "ubuntu"
	);
	my %network_conf_dir  = (
		"rhel" => $NETWORK_CONF_DIR,
		"centos" => $NETWORK_CONF_DIR,
		"rocky" => $NETWORK_CONF_DIR,
		"almalinux" => $NETWORK_CONF_DIR,
		"navix" => $NETWORK_CONF_DIR,
		"circle" => $NETWORK_CONF_DIR,
		"oracle" => $NETWORK_CONF_DIR,
		"opencloudos" => $NETWORK_CONF_DIR,
		"sles" => "/etc/sysconfig/network",
		"sle_hpc" => "/etc/sysconfig/network",
		"ubuntu" => "/etc/sysconfig/network-scripts",
	);
	my $os_id = `cat $os_release_file | grep '^ID=' | cut -d'=' -f2 | tr -d [\\"\\.0] | tr -d ["\n"]`;
	if (! exists $distroVendor{$os_id}) {
		NormalPrint "INSTALL does not support this vendor ($os_id)\n";
		Abort "Please contact your support representative...\n";
	}
	$CUR_DISTRO_VENDOR = $distroVendor{$os_id};
	$NETWORK_CONF_DIR = $network_conf_dir{$os_id};
	if ( $CUR_DISTRO_VENDOR eq "SuSE" ) {
		$OFA_CONFIG_DIR = "/etc/rdma/modules";
	}

	my $os_ver = `cat $os_release_file | grep '^VERSION_ID=' | cut -d'=' -f2 | tr -d [\\"] | tr -d ["\n"]`;
	if ($os_ver eq "") {
		NormalPrint "INSTALL could not read '^VERSION_ID=...' from os-release file(s)\n";
		Abort "Please contact your support representative...\n";
	}
	$CUR_VENDOR_VER = os_vendor_version($CUR_DISTRO_VENDOR, $os_ver);
	$CUR_VENDOR_MAJOR_VER = $CUR_VENDOR_VER;
	$CUR_VENDOR_MAJOR_VER =~ s/\..*//;	# remove any . version suffix
}

# verify distrib of this system matches files indicating supported
#  arch, distro, distro_version
sub verify_distrib_files
{
	my $supported_arch=`cat ./arch 2>/dev/null`;
	chomp($supported_arch);
	my $supported_distro_vendor=`cat ./distro 2>/dev/null`;
	chomp($supported_distro_vendor);
	my $supported_distro_vendor_ver=`cat ./distro_version 2>/dev/null`;
	chomp($supported_distro_vendor_ver);

	if ( "$supported_arch" eq "" || $supported_distro_vendor eq ""
		|| $supported_distro_vendor_ver eq "") {
		NormalPrint "Unable to proceed: installation image corrupted or install not run as ./INSTALL\n";
		NormalPrint "INSTALL must be run from within untar'ed install image directory\n";
		Abort "Please contact your support representative...\n";
	}

	my $archname;
	my $supported_archname;
	if ( "$supported_arch" ne "$ARCH"
		|| "$supported_distro_vendor" ne "$CUR_DISTRO_VENDOR"
		|| ("$supported_distro_vendor_ver" ne "$CUR_VENDOR_VER"
			&& "$supported_distro_vendor_ver" ne "$CUR_VENDOR_MAJOR_VER"))
	{
		#LogPrint "Unable to proceed, $CUR_DISTRO_VENDOR $CUR_VENDOR_VER not supported by $INT_VERSION media\n";

		$archname=$ARCH;
		if ( $ARCH eq "IA32") {
			$archname="the Pentium Family";
		}
		if ( $ARCH eq "IA64" ) {
			$archname="the Itanium family";
		}
		if ( $ARCH eq "X86_64" ) {
			$archname="the EM64T or Opteron";
		}
		if ( $ARCH eq "PPC64" ) {
			$archname="the PowerPC 64 bit";
		}
		if ( $ARCH eq "PPC" ) {
			$archname="the PowerPC";
		}

		NormalPrint "$CUR_DISTRO_VENDOR $CUR_VENDOR_VER for $archname is not supported by this installation\n";
		NormalPrint "This installation supports the following Linux Distributions:\n";
		$supported_archname=$ARCH;
		if ( $supported_arch eq "IA32") {
			$supported_archname="the Pentium Family";
		}
		if ( $supported_arch eq "IA64" ) {
			$supported_archname="the Itanium family";
		}
		if ( $supported_arch eq "X86_64" ) {
			$supported_archname="the EM64T or Opteron";
		}
		if ( $supported_arch eq "PPC64" ) {
			$supported_archname="the PowerPC 64 bit";
		}
		if ( $supported_arch eq "PPC" ) {
			$supported_archname="the PowerPC";
		}
		NormalPrint "For $supported_archname: $supported_distro_vendor.$supported_distro_vendor_ver\n";
		if ( $Force_Install ) {
			NormalPrint "Installation Forced, will proceed with risk of undefined results\n";
			HitKeyCont;
		} else {
			Abort "Please contact your support representative...\n";
		}
	}
}

# set the env vars to their default value, when the first we install eth-scripts, we will have proper configs
sub default_opascripts_env_vars()
{
	setup_env("ETH_UDEV_RULES", 1);
	setup_env("ETH_LIMITS_CONF", 1);
	setup_env("ETH_ARPTABLE_TUNING", 1);
#	setup_env("ETH_IRQBALANCE", 1);
	setup_env("ETH_ROCE_ON", 1);
	setup_env("ETH_UFFD_ACCESS", 1);
}

sub has_ascfg_file()
{
	if (! -e "$ETH_ASCFG_FILE")
	{
		if (rpm_is_installed("iefsconfig", "any")) {
			NormalPrint("Couldn't find file $ETH_ASCFG_FILE\n");
		}
		return 0;
	}
	return 1;
}

sub enable_autostartconfig($)
{
	my $name = shift();
	if (has_ascfg_file())
	{
		system("$ETH_ASCFG_FILE --enable $name");
	}
}

sub disable_autostartconfig($)
{
	my $name = shift();
	if (has_ascfg_file())
	{
		system("$ETH_ASCFG_FILE --disable $name");
	}
}

sub status_autostartconfig($)
{
	my $name = shift();
	if ( has_ascfg_file() && ( 0 == system("$ETH_ASCFG_FILE --status $name | grep ' \\[ENABLED\\]' > /dev/null 2>&1") ) ) {
		return 1;
	}
	return 0;
}

# this will be replaced in component specific INSTALL with any special
# overrides of things in main*pl
sub overrides()
{
}
