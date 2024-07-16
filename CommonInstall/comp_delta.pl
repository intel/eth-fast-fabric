#!/usr/bin/perl
# BEGIN_ICS_COPYRIGHT8
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
# END_ICS_COPYRIGHT8

# This file incorporates work covered by the following copyright and permission notice

#
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

use strict;
use Cwd 'getcwd';
#use Term::ANSIColor;
#use Term::ANSIColor qw(:constants);
#use File::Basename;
#use Math::BigInt;

# ==========================================================================
# OFA_Delta installation, includes Intel value-adding packages only

my %sles_srpm = (
	"Available" => "",
	"ReadableName" => "Eth Module",
	"SourcePackageGlob" => "iefs-kernel-updates*.src.rpm",
	"GpuAware" => "yes",
	"BuildProducts" => [
		"iefs-kernel-updates-kmp-default",
		"iefs-kernel-updates-devel",
	],
	"PartOf" => ["eth_module"],
	"DKMSpackage" => "iefs-kernel-updates-dkms",
	"MainPackage" => "iefs-kernel-updates-kmp-default",
);

my %redhat_srpm = (
	"Available" => "",
	"ReadableName" => "Eth Module",
	"SourcePackageGlob" => "iefs-kernel-updates*.src.rpm",
	"GpuAware" => "yes",
	"BuildProducts" => [
		"kmod-iefs-kernel-updates",
		"iefs-kernel-updates-devel",
	],
	"PartOf" => ["eth_module"],
	"DKMSpackage" => "iefs-kernel-updates-dkms",
	"MainPackage" => "kmod-iefs-kernel-updates",
);

my %debian_tarball = (
	"Available" => "",
	"ReadableName" => "Eth Module",
	"SourcePackageGlob" => "iefs-kernel-updates_*.dsc",
	"GpuAware" => "yes",
	"BuildProducts" => [
		"kmod-iefs-kernel-updates",
		"iefs-kernel-updates-devel",
	],
	"PartOf" => ["eth_module"],
	"DKMSpackage" => "iefs-kernel-updates-dkms",
	"MainPackage" => "kmod-iefs-kernel-updates",
);

# all kernel srpms
# these are in the order we must build/process them to meet basic dependencies
my %source_pkgs_by_distro = (
	'SuSE*ES124'    => [ \%sles_srpm ],
	'SuSE*ES125'    => [ \%sles_srpm ],
	'SuSE*ES15'     => [ \%sles_srpm ],
	'SuSE*ES151'    => [ \%sles_srpm ],
	'SuSE*ES152'    => [ \%sles_srpm ],
	'SuSE*ES153'    => [ \%sles_srpm ],
	'SuSE*ES154'    => [ \%sles_srpm ],
	'SuSE*ES155'    => [ \%sles_srpm ],
	'SuSE*ES156'    => [ \%sles_srpm ],
	'redhat*ES79'   => [ \%redhat_srpm ],
	'redhat*ES87'   => [ \%redhat_srpm ],
	'redhat*ES88'   => [ \%redhat_srpm ],
	'redhat*ES89'   => [ \%redhat_srpm ],
	'redhat*ES810'  => [ \%redhat_srpm ],
	'redhat*ES91'   => [ \%redhat_srpm ],
	'redhat*ES92'   => [ \%redhat_srpm ],
	'redhat*ES93'   => [ \%redhat_srpm ],
	'redhat*ES94'   => [ \%redhat_srpm ],
	'ubuntu*UB2204' => [ \%debian_tarball ],
);

my @delta_kernel_srpms = ( );
my %delta_autostart_save = ();

# ==========================================================================
# Delta eth_module build in prep for installation

# based on %delta_srpm_info{}{'Available'} determine if the given SRPM is
# buildable and hence available on this CPU for $osver combination
# "user" and kernel rev values for mode are treated same
sub available_srpm($$$)
{
	my $srpm = shift();
	# $mode can be any other value,
	# only used to select Available
	my $mode = shift();	# "user" or kernel rev
	my $osver = shift();
	my $avail = $srpm->{"Available"};

	DebugPrint("checking [$srpm->{'ReadableName'}] $mode $osver against '$avail'\n");
	return arch_kernel_is_allowed($osver, $avail);
}

# initialize delta srpm list based on specified osver
# for present system
sub init_delta_info($)
{
	my $osver = shift();
	my $distro = "$CUR_DISTRO_VENDOR*$CUR_VENDOR_VER";
	if (exists $source_pkgs_by_distro{$distro}) {
		@delta_kernel_srpms = @{$source_pkgs_by_distro{$distro};}
	} else {
		# unknown distro, leave empty
		@delta_kernel_srpms = ( );
	}

	if (DebugPrintEnabled() ) {
		# dump all SRPM info
		DebugPrint "\nSPKGs:\n";
		foreach my $srpm ( @delta_kernel_srpms ) {
			DebugPrint("$srpm->{'ReadableName'} => Builds: '$srpm->{'Builds'}'\n");
			DebugPrint("           Available: '$srpm->{'Available'}'\n");
			DebugPrint("           Available: ".available_srpm($srpm, "user", $osver)." PartOf: [@{$srpm->{'PartOf'}}]\n");
		}
		DebugPrint "\n";
	}
}

sub get_rpms_dir_delta($$);

# verify the rpmfiles exist for all the RPMs listed
sub delta_rpm_exists_list($$$)
{
	my $mode = shift();	#  "user" or kernel rev or "firmware"
	my $packages_ref = shift();
	my $gpu_aware = shift();

	foreach my $package ( @$packages_ref )
	{
		my $rpmdir = get_rpms_dir_delta($package, $gpu_aware);
		if (! rpm_exists("$rpmdir/$package", $mode) ) {
			return 0;
		}
	}
	return 1;
}

# resolve filename within source packages subdir
# and return filename based on $srcdir
sub delta_srpm_file($$$)
{
	my ($srcdir, $globname, $gpu_aware) = @_;
	my $spkg_dir = get_source_pkg_dir($srcdir);
	my $result = file_glob("$spkg_dir/$globname");
	if ( $gpu_aware eq "yes" ) {
		if ( $GPU_Install eq "NV_GPU" ) {
			if ( -d "$spkg_dir/CUDA" ) {
				$result = file_glob("$spkg_dir/CUDA/$globname");
			} else {
				NormalPrint("CUDA specific SPKGs do not exist\n");
				exit 1;
			}
		} elsif ( $GPU_Install eq "INTEL_GPU" ) {
			if ( -d "$spkg_dir/ONEAPI-ZE" ) {
				$result = file_glob("$spkg_dir/ONEAPI-ZE/$globname");
			} else {
				NormalPrint("ONEAPI ZE specific SPKGs do not exist\n");
				exit 1;
			}
		}
	}

	return $result;
}

# indicate where DELTA built RPMs can be found
sub delta_rpms_dir()
{
	my $srcdir=$ComponentInfo{'eth_module'}{'SrcDir'};
	my $pkg_dir = get_binary_pkg_dir($srcdir);

	if (-d "$pkg_dir/$CUR_DISTRO_VENDOR-$CUR_VENDOR_VER" ) {
		return "$pkg_dir/$CUR_DISTRO_VENDOR-$CUR_VENDOR_VER";
	} else {
		return "$pkg_dir/$CUR_DISTRO_VENDOR-$CUR_VENDOR_MAJOR_VER";
	}
}

# package is supplied since for a few packages (such as GPU Direct specific
# packages) we may pick a different directory based on package name
sub get_rpms_dir_delta($$)
{
	my $package = shift();
	my $gpu_aware = shift();

	my $rpmsdir = delta_rpms_dir();

	# Note no need to check kernel levels since GPU Direct now supported
	# for all distros.  If we add a distro without this, such as Ubuntu,
	# perhaps argument parser should turn off GPU_Install on that distro.
	if ( $gpu_aware eq "yes" ) {
		if ( $GPU_Install eq "NV_GPU" ) {
			if ( -d $rpmsdir."/CUDA" ) {
				$rpmsdir=$rpmsdir."/CUDA";
			} else {
				NormalPrint("CUDA specific packages do not exist\n");
				exit 1;
			}
		} elsif ( $GPU_Install eq "INTEL_GPU" ) {
			if ( -d $rpmsdir."/ONEAPI-ZE" ) {
				$rpmsdir=$rpmsdir."/ONEAPI-ZE";
			} else {
				NormalPrint("ONEAPI ZE specific packages do not exist\n");
				exit 1;
			}
		}
	}
	DebugPrint("get_rpms_dir_delta($package, GPU-aware=$gpu_aware)->$rpmsdir\n");
	return $rpmsdir;
}

# verify if all rpms have been built from the given srpm
sub is_built_srpm($$)
{
	my $srpm = shift();	# srpm descriptor
	my $mode = shift();	# "user" or kernel rev

	return ( delta_rpm_exists_list($mode, $srpm->{'BuildProducts'}, $srpm->{'GpuAware'}) );
}

# see if srpm is part of any of the components being installed/reinstalled
sub need_srpm_for_install($$$$)
{
	my $srpm = shift();	# srpm descriptor
	my $mode = shift();	# "user" or kernel rev
	my $osver = shift();
	# add space at start and end so can search
	# list with spaces around searched comp
	my $installing_list = " ".shift()." "; # items being installed/reinstalled

	if (! available_srpm($srpm, $mode, $osver)) {
		DebugPrint("$srpm->{'ReadableName'} $mode $osver not available\n");
		return 0;
	}

	my @complist = @{$srpm->{'PartOf'}};
	foreach my $comp (@complist) {
		DebugPrint("Check for $comp in ( $installing_list )\n");
		if ($installing_list =~ / $comp /) {
			return 1;
		}
	}
	return 0;
}

sub need_build_srpm($$$$$$)
{
	my $srpm = shift();	# srpm descriptor
	my $mode = shift();	# "user" or kernel rev
	my $osver = shift();	# kernel rev
	my $installing_list = shift(); # what items are being installed/reinstalled
	my $force = shift();	# force a rebuild
	my $prompt = shift();	# prompt (only used if ! $force)

	return ( need_srpm_for_install($srpm, $mode, $osver, $installing_list)
			&& ($force
				|| ! is_built_srpm($srpm, $mode)
				|| ($prompt && GetYesNo("Rebuild $srpm->{'ReadableName'} source package for $mode?", "n"))));
}

# build all OFA components specified in installing_list
# if already built, prompt user for option to build
sub build_delta($$$$$$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift(); # what items are being installed/reinstalled
	my $K_VER = shift();	# osver
	my $debug = shift();	# enable extra debug of build itself
	my $build_temp = shift();	# temp area for use by build
	my $force = shift();	# force a rebuild

	my $prompt_srpm = 0;	# prompt per SRPM
	my $force_srpm = $force;	# force SRPM rebuild
	my $rpmsdir = delta_rpms_dir();

	# we only support building kernel code, so if user wants to skip install
	# of kernel, we have nothing to do here
	# TBD, if its selected to install user_space_only we still
	# don't rebuild the kernel srpm, even though it creates the -devel package
	# However, that package is not kernel specific, so we should be ok
	if($user_space_only) {
		return 0;	# success
	}

	if (! $force && ! $Default_Prompt) {
		my $choice = GetChoice("Rebuild OFA kernel Source Package (a=all, p=prompt per package, n=only as needed)?", "n", ("a", "p", "n"));
		if ("$choice" eq "a") {
			$force_srpm=1;
		} elsif ("$choice" eq "p") {
			$prompt_srpm=1;
		} elsif ("$choice" eq "n") {
			$prompt_srpm=0;
		}
	}

	# -------------------------------------------------------------------------
	# do all rebuild prompting first so user doesn't have to wait 5 minutes
	# between prompts
	my @build_kernel_srpms = (0)x($#delta_kernel_srpms);
	my $need_build = 0;

	for my $i ( 0 .. $#delta_kernel_srpms ) {
		my $build_this = need_build_srpm(
			$delta_kernel_srpms[$i],
			"$K_VER", "$K_VER",
			$installing_list,
			$force_srpm,
			$prompt_srpm
		);
		$build_kernel_srpms[$i] = $build_this;
		$need_build |= $build_this;
	}

	if (! $need_build) {
		DebugPrint("No source packages selected for build\n");
		return 0;	# success
	}

	# -------------------------------------------------------------------------
	# check OS dependencies for all srpms which we will build
	my $dep_error = 0;

	NormalPrint "Checking OS Dependencies needed for builds...\n";

	for my $i ( 0 .. $#delta_kernel_srpms ) {
		next if ( ! $build_kernel_srpms[$i] );
		my $srpm_id = $delta_kernel_srpms[$i]{'ReadableName'};
		VerbosePrint("check dependencies for $srpm_id\n");
		if (check_kbuild_dependencies($K_VER, $srpm_id )) {
			DebugPrint "$srpm_id kbuild dependency failure\n";
			$dep_error = 1;
		}
		if (check_rpmbuild_dependencies($srpm_id)) {
			DebugPrint "$srpm_id rpmbuild dependency failure\n";
			$dep_error = 1;
		}
	}

	if ($dep_error) {
		NormalPrint "ERROR - unable to perform builds due to need for additional OS rpms\n";
		return 1;	# failure
	}

	DebugPrint("Building started\n");
	# -------------------------------------------------------------------------
	# perform the builds
	my $srcdir=$ComponentInfo{'eth_module'}{'SrcDir'};

	# use a different directory for BUILD_ROOT to limit conflict with OFED
	if ("$build_temp" eq "" ) {
		$build_temp = "/var/tmp/IntelEth-DELTA";
	}
	my $BUILD_ROOT="$build_temp/build";
	my $BUILD_OUTPUT_DIR="$build_temp/DELTA_PACKAGES";
	my $resfileop = "replace";	# replace for 1st build, append for rest

	system("rm -rf ${build_temp}");

	if (0 != make_build_dirs($BUILD_ROOT, $BUILD_OUTPUT_DIR)) {
		NormalPrint("Unable to create output directory layout\n");
		return 1;
	}

	DebugPrint("Output directory structure prepared\n");

	for my $i ( 0 .. $#delta_kernel_srpms ) {
		my $srpm = $delta_kernel_srpms[$i];
		VerbosePrint("processing $srpm->{'ReadableName'}\n");
		next if ( ! $build_kernel_srpms[$i] );

		my $SRC_PKG = delta_srpm_file($srcdir, $srpm->{'SourcePackageGlob'}, $srpm->{'GpuAware'});
		$SRC_PKG = getcwd()."/".$SRC_PKG;
		DebugPrint("Source package to be built: $SRC_PKG\n");

		my $cmd = create_build_command_line($SRC_PKG, $BUILD_OUTPUT_DIR, $BUILD_ROOT, $K_VER);
		DebugPrint("Build command: $cmd\n");

		if (0 != run_build("$srcdir $SRC_PKG $RPM_KERNEL_ARCH $K_VER", "$srcdir", $cmd, "$resfileop")) {
			NormalPrint("Build failed\n");
			return 1;	# failure
		}
		$resfileop = "append";

		if (0 != move_packages($BUILD_OUTPUT_DIR, $rpmsdir, $GPU_Install)) {
			NormalPrint("Copy to destination dir failed\n");
			return 1;
		}
	}

	if (! $debug) {
		system("rm -rf ${build_temp}");
	} else {
		LogPrint "Build remnants left in $BUILD_ROOT and $BUILD_OUTPUT_DIR\n";
	}

	return 0;	# success
}

# TBD - might not need any more
# return 0 on success, != 0 otherwise
sub uninstall_old_delta_rpms($$$)
{
	my $mode = shift();	# "user" or kernel rev or "firmware"
						# "any"- checks if any variation of package is installed
	my $verbosity = shift();
	my $message = shift();

	my $ret = 0;	# assume success
	my @packages = ();

	if ("$message" eq "" ) {
		$message = "previous OFA Delta";
	}
	NormalPrint "\nUninstalling $message RPMs\n";

	# uninstall all present version OFA rpms, just to be safe
	foreach my $i ( "mpiRest", reverse(@Components) ) {
  		next if (! $ComponentInfo{$i}{'IsOFA'});
		@packages = (@packages, @{ $ComponentInfo{$i}{'DebugRpms'}});
		@packages = (@packages, @{ $ComponentInfo{$i}{'UserRpms'}});
		@packages = (@packages, @{ $ComponentInfo{$i}{'FirmwareRpms'}});
		@packages = (@packages, get_kernel_rpms_to_uninstall($i));
	}

	my @filtered_packages = ();
	foreach my $i ( @packages ) {
		if (scalar(grep /^$i$/, (@filtered_packages)) > 0) {
			# skip, already in list
		} else {
			@filtered_packages = (@filtered_packages, "$i");
		}
	}

	# get rpms we are going to remove
	my $installed_mpi_rpms = "";
	foreach my $rpm (@{ $ComponentInfo{'mpiRest'}{'UserRpms'}}) {
		if (rpm_is_installed($rpm, "user")) {
			$installed_mpi_rpms .= " $rpm";
		}
	}

	$ret ||= rpm_uninstall_all_list_with_options($mode, " --nodeps ", $verbosity, @filtered_packages);

	if ($ret == 0 && $installed_mpi_rpms ne "") {
		NormalPrint "The following MPIs were removed because they are built from a different version of IEFS.\n";
		NormalPrint "Please rebuild these MPIs if you still need them.\n  $installed_mpi_rpms\n";
	}

	if ( $ret ) {
		NormalPrint "Unable to uninstall $message RPMs\n";
	}
	return $ret;
}

# forward declarations
sub installed_eth_module();

# TBD - might not need anymore
# remove any old stacks or old versions of the stack
# this is necessary before doing builds to ensure we don't use old dependent
# rpms
sub uninstall_prev_versions()
{
	if (! installed_eth_module()) {
		return 0;
	} elsif (! comp_is_uptodate('eth_module')) { # all delta_comp same version
		if (0 != uninstall_old_delta_rpms("any", "silent", "previous OFA DELTA")) {
			return 1;
		}
	}
	return 0;
}

sub media_version_delta()
{
	# all OFA components at same version as eth_module
	my $srcdir=$ComponentInfo{'eth_module'}{'SrcDir'};
	return `cat "$srcdir/Version"`;
}

sub delta_save_autostart()
{
	foreach my $comp ( @Components ) {
  		next if (! $ComponentInfo{$comp}{'IsOFA'});
  		if ($ComponentInfo{$comp}{'HasStart'}
			&& $ComponentInfo{$comp}{'StartupScript'} ne "") {
			$delta_autostart_save{$comp} = comp_IsAutostart2($comp);
		} else {
			$delta_autostart_save{$comp} = 0;
		}
	}
}

sub delta_restore_autostart($)
{
	my $comp = shift();

	if ( $delta_autostart_save{$comp} ) {
		comp_enable_autostart2($comp, 1);
	} else {
  		if ($ComponentInfo{$comp}{'HasStart'}
			&& $ComponentInfo{$comp}{'StartupScript'} ne "") {
			comp_disable_autostart2($comp, 1);
		}
	}
}

# makes sure needed OFA components are already built, builts/rebuilds as needed
# called for every delta component's preinstall, noop for all but
# first OFA component in installing_list
sub preinstall_delta($$$)
{
	my $comp = shift();			# calling component
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	# ignore non-delta components at start of installing_list
	my @installing = split /[[:space:]]+/, $installing_list;
	while (scalar(@installing) != 0
			&& ("$installing[0]" eq ""
  				|| ! $ComponentInfo{$installing[0]}{'IsOFA'} )) {
		shift @installing;
	}
	# now, only do work if $comp is the 1st delta component in installing_list
	if ("$comp" eq "$installing[0]") {
		delta_save_autostart();
		init_delta_info($CUR_OS_VER);

# TBD - do we really need this any more?
		# Before we do any builds make sure old stacks removed so we don't
		# build against the wrong version of dependent rpms
		if (0 != uninstall_prev_versions()) {
			return 1;
		}
		print_separator;
		my $version=media_version_delta();
		chomp $version;
		printf("Preparing OFA $version $DBG_FREE for Install...\n");
		LogPrint "Preparing OFA $version $DBG_FREE for Install for $CUR_OS_VER\n";
		return build_delta("$install_list", "$installing_list", "$CUR_OS_VER",0,"",$OFED_force_rebuild);
	} else {
		return 0;
	}
}

# ==========================================================================
# OFA DELTA generic routines

# OFA has a single start script but controls which components are loaded via
# entries in $OFA_CONFIG (rdma.conf)
# change all StartupParams for given delta component to $newvalue
sub delta_comp_change_ofa_conf_param($$)
{
	my $comp=shift();
	my $newvalue=shift();

	VerbosePrint("edit /$OFA_CONFIG $comp StartUp set to '$newvalue'\n");
	foreach my $p ( @{ $ComponentInfo{$comp}{'StartupParams'} } ) {
		change_ofa_conf_param($p, $newvalue);
	}
}

# generic functions to handle autostart needs for delta components with
# more complex rdma.conf based startup needs.  These assume iefsconfig handles
# the actual startup script.  Hence these focus on the rdma.conf parameters
# determine if the given capability is configured for Autostart at boot
sub IsAutostart_delta_comp2($)
{
	my $comp = shift();	# component to check
	my $WhichStartup = $ComponentInfo{$comp}{'StartupScript'};
	my $ret = $WhichStartup eq "" ? 1 : IsAutostart($WhichStartup);	# just to be safe, test this too

	if ($ComponentInfo{$comp}{'StartupParams'} eq "") {
		return 0;
	}
	# to be true, all parameters must be yes
	foreach my $p ( @{ $ComponentInfo{$comp}{'StartupParams'} } ) {
			$ret &= ( read_conf_param($p, "/$OFA_CONFIG") eq "yes");
	}
	return $ret;
}
sub autostart_desc_comp($)
{
	my $comp = shift();	# component to describe
	my $WhichStartup = $ComponentInfo{$comp}{'StartupScript'};
	if ( "$WhichStartup" eq "" ) {
		return "$ComponentInfo{$comp}{'Name'}"
	} else {
		return "$ComponentInfo{$comp}{'Name'} ($WhichStartup)";
	}
}
# enable autostart for the given capability
sub enable_autostart_delta_comp2($)
{
	my $comp = shift();	# component to enable
	#my $WhichStartup = $ComponentInfo{$comp}{'StartupScript'};

	#iefsconfig handles this: enable_autostart($WhichStartup);
	delta_comp_change_ofa_conf_param($comp, "yes");
}
# disable autostart for the given capability
sub disable_autostart_delta_comp2($)
{
	my $comp = shift();	# component to disable
	#my $WhichStartup = $ComponentInfo{$comp}{'StartupScript'};

	delta_comp_change_ofa_conf_param($comp, "no");
}

# helper to determine if we need to reinstall due to parameter change
sub need_reinstall_delta_comp($$$)
{
	my $comp = shift();
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	if (! comp_is_uptodate('eth_module')) { # all delta_comp same version
		# on upgrade force reinstall to recover from uninstall of old rpms
		return "all";
	} else {
		return "no";
	}
}

# return 0 on success, 1 on failure
sub run_uninstall($$$)
{
	my $stack = shift();
	my $cmd = shift();
	my $cmdargs = shift();

	if ( "$cmd" ne "" && -e "$cmd" ) {
		NormalPrint "\nUninstalling $stack: $cmd $cmdargs\n";
		if (0 != system("yes | $cmd $cmdargs")) {
			NormalPrint "Unable to uninstall $stack\n";
			return 1;
		}
	}
	return 0;
}

# ==========================================================================
# eth_module installation

# determine if the given capability is configured for Autostart at boot
sub IsAutostart2_eth_module()
{
	return status_autostartconfig("RENDEZVOUS");
}
sub autostart_desc_eth_module()
{
	return autostart_desc_comp('eth_module');
}
# enable autostart for the given capability
sub enable_autostart2_eth_module()
{
	enable_autostartconfig("RENDEZVOUS")
}
# disable autostart for the given capability
sub disable_autostart2_eth_module()
{
	disable_autostartconfig("RENDEZVOUS")
}

sub get_rpms_dir_eth_module($)
{
	my $package = shift();
	return get_rpms_dir_delta($package, "yes")
}

sub available_eth_module()
{
	my $srcdir=$ComponentInfo{'eth_module'}{'SrcDir'};
        return (-d get_binary_pkg_dir($srcdir) || -d get_source_pkg_dir($srcdir));
}

sub installed_eth_module()
{
	my $dkms_name = $delta_kernel_srpms[0]{'DKMSpackage'};
	if (rpm_is_installed($dkms_name, "any")) {
		DebugPrint("installed_eth_module: $dkms_name installed\n");
		return 1;
	}
	my $name = $delta_kernel_srpms[0]{'MainPackage'};
	my $result = rpm_is_installed($name, $CUR_OS_VER);
	DebugPrint("installed_eth_module: $name for $CUR_OS_VER installed=$result\n");
	return $result;
}

# only called if installed_eth_module is true
sub installed_version_eth_module()
{
	my $version;
	my $dkms_name = $delta_kernel_srpms[0]{'DKMSpackage'};
	if (rpm_is_installed($dkms_name, "any")) {
		$version = rpm_query_release_only_pkg($dkms_name);
		DebugPrint("installed_version_eth_module: $dkms_name -> $version\n");
	} else {
		my $name = $delta_kernel_srpms[0]{'MainPackage'};
		$version = rpm_query_release_only_pkg($name);
		DebugPrint("installed_version_eth_module: $name -> $version\n");
	}
	return $version;
}

# only called if available_eth_module is true
sub media_version_eth_module()
{
	my $name = $delta_kernel_srpms[0]{'MainPackage'};
	my $pkg_dir = get_rpms_dir_delta($name, "yes");
	my $rpmfile = rpm_resolve("$pkg_dir/$name", $CUR_OS_VER);
	my $version= rpm_query_release_only_file("$rpmfile");
	return $version;
}

sub build_eth_module($$$$)
{
	my $osver = shift();
	my $debug = shift();    # enable extra debug of build itself
	my $build_temp = shift();       # temp area for use by build
	my $force = shift();    # force a rebuild

	init_delta_info($osver);

	return build_delta("@Components", "@Components", $osver, $debug,$build_temp,$force);
}

sub need_reinstall_eth_module($$)
{
	my $install_list = shift();     # total that will be installed when done
	my $installing_list = shift();  # what items are being installed/reinstalled

	return (need_reinstall_delta_comp('eth_module', $install_list, $installing_list));
}

sub preinstall_eth_module($$)
{
	my $install_list = shift();     # total that will be installed when done
	my $installing_list = shift();  # what items are being installed/reinstalled

	return preinstall_delta("eth_module", $install_list, $installing_list);
}

sub install_eth_module($$)
{
	my $install_list = shift();     # total that will be installed when done
	my $installing_list = shift();  # what items are being installed/reinstalled

	print_comp_install_banner('eth_module');
	setup_env("ETH_INSTALL_CALLER", 1);

	install_comp_rpms('eth_module', " -U --nodeps ", $install_list);

	need_reboot();

	$ComponentWasInstalled{'eth_module'}=1;
}

sub postinstall_eth_module($$)
{
	my $install_list = shift();     # total that will be installed when done
	my $installing_list = shift();  # what items are being installed/reinstalled
	delta_restore_autostart('eth_module');
}

sub uninstall_eth_module($$)
{
	my $install_list = shift();     # total that will be left installed when done
	my $uninstalling_list = shift();        # what items are being uninstalled

	print_comp_uninstall_banner('eth_module');
	setup_env("ETH_INSTALL_CALLER", 1);
	uninstall_comp_rpms('eth_module', ' --nodeps ', $install_list, $uninstalling_list, 'verbose');
	need_reboot();
	$ComponentWasInstalled{'eth_module'}=0;
}

sub check_os_prereqs_eth_module
{
	return rpm_check_os_prereqs("eth_module", "any");
}

# ==========================================================================
# psm3 installation

# determine if the given capability is configured for Autostart at boot
sub IsAutostart2_psm3()
{
	return IsAutostart_delta_comp2('psm3');
}
sub autostart_desc_psm3()
{
	return autostart_desc_comp('psm3');
}
# enable autostart for the given capability
sub enable_autostart2_psm3()
{
	enable_autostart($ComponentInfo{'psm3'}{'StartupScript'});
}
# disable autostart for the given capability
sub disable_autostart2_psm3()
{
	disable_autostart($ComponentInfo{'psm3'}{'StartupScript'});
}

sub get_rpms_dir_psm3($)
{
	my $package = shift();
	return get_rpms_dir_delta($package, "yes")
}

sub available_psm3()
{
	my $srcdir=$ComponentInfo{'psm3'}{'SrcDir'};
        return (-d get_binary_pkg_dir($srcdir) || -d get_source_pkg_dir($srcdir));
}

sub installed_delta_psm3()
{
	return rpm_is_installed("libpsm3-fi", "any");
}

sub installed_psm3()
{
	return (installed_delta_psm3);
}

# only called if installed_psm3 is true
sub installed_version_psm3()
{
	my $version = rpm_query_version_release_pkg("libpsm3-fi");
	return dot_version("$version");
}

# only called if available_psm3 is true
sub media_version_psm3()
{
	my $pkg_dir = get_rpms_dir_delta("libpsm3-fi", "yes");
	my $rpmfile = rpm_resolve("$pkg_dir/libpsm3-fi", "any");
	my $version= rpm_query_version_release("$rpmfile");
	return dot_version("$version");
}

sub build_psm3($$$$)
{
    my $osver = shift();
    my $debug = shift();    # enable extra debug of build itself
    my $build_temp = shift();       # temp area for use by build
    my $force = shift();    # force a rebuild

    return 0;
}

sub need_reinstall_psm3($$)
{
    my $install_list = shift();     # total that will be installed when done
    my $installing_list = shift();  # what items are being installed/reinstalled

    return (need_reinstall_delta_comp('psm3', $install_list, $installing_list));
}

sub preinstall_psm3($$)
{
    my $install_list = shift();     # total that will be installed when done
    my $installing_list = shift();  # what items are being installed/reinstalled

    return 0;
}

sub install_psm3($$)
{
    my $install_list = shift();     # total that will be installed when done
    my $installing_list = shift();  # what items are being installed/reinstalled

    print_comp_install_banner('psm3');
    setup_env("ETH_INSTALL_CALLER", 1);

    install_comp_rpms('psm3', " -U --nodeps ", $install_list);

    my $version = media_version_delta();
	chomp $version;
    system "echo '$version' > $BASE_DIR/version_delta 2>/dev/null";
    $ComponentWasInstalled{'psm3'}=1;
}

sub postinstall_psm3($$)
{
    my $install_list = shift();     # total that will be installed when done
    my $installing_list = shift();  # what items are being installed/reinstalled
}

sub uninstall_psm3($$)
{
    my $install_list = shift();     # total that will be left installed when done
    my $uninstalling_list = shift();        # what items are being uninstalled

    print_comp_uninstall_banner('psm3');
    setup_env("ETH_INSTALL_CALLER", 1);
    uninstall_comp_rpms('psm3', ' --nodeps ', $install_list, $uninstalling_list, 'verbose');
	system("rm -rf $BASE_DIR/version_delta");
    $ComponentWasInstalled{'psm3'}=0;
}

sub check_os_prereqs_psm3
{
	return rpm_check_os_prereqs("psm3", "any");
}

# ==========================================================================
# Eth RoCE installation

sub available_eth_roce()
{
	return 1;
}

sub installed_eth_roce()
{
	return 1;
}

sub installed_version_eth_roce()
{
	if (rpm_is_installed("meta_eth_module", "any")) {
		my $version = rpm_query_version_release_pkg("meta_eth_module");
		return dot_version("$version");
	}
	if ( -e "$BASE_DIR/version_delta" ) {
		return `cat $BASE_DIR/version_delta`;
	} else {
		return 'Unknown';
	}
}

sub media_version_eth_roce()
{
	return media_version_delta();
}

sub build_eth_roce($$$$)
{
	my $osver = shift();
	my $debug = shift();	# enable extra debug of build itself
	my $build_temp = shift();	# temp area for use by build
	my $force = shift();	# force a rebuild
	return 0;	# success
}

sub need_reinstall_eth_roce($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	return (need_reinstall_delta_comp('eth_roce', $install_list, $installing_list));
}

sub preinstall_eth_roce($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	return preinstall_delta("eth_roce", $install_list, $installing_list);
}

sub install_eth_roce($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	print_comp_install_banner('eth_roce');
	install_comp_rpms('eth_roce', " -U --nodeps ", $install_list);

	# bonding is more involved, require user to edit to enable that
	config_roce("y");
	config_lmtsel("$DEFAULT_LIMITS_SEL");
	Config_ifcfg();
	#Config_IPoIB_cfg;
	need_reboot();
	$ComponentWasInstalled{'eth_roce'}=1;
}

sub postinstall_eth_roce($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled
	return 0;
}

sub uninstall_eth_roce($$)
{
	my $install_list = shift();	# total that will be left installed when done
	my $uninstalling_list = shift();	# what items are being uninstalled

	print_comp_uninstall_banner('eth_roce');
	uninstall_comp_rpms('eth_roce', ' --nodeps ', $install_list, $uninstalling_list, 'verbose');
	#TODO: we are setting back to default. Need a better solution to restore rather than reset configs
	config_roce("n");
	restore_lmtsel();
	Reset_ifcfg();
	need_reboot();
	$ComponentWasInstalled{'eth_roce'}=0;
}
sub IsAutostart2_eth_roce()
{
	return status_autostartconfig("LLDPAD");
}

sub autostart_desc_eth_roce()
{
	return autostart_desc_comp('eth_roce');
}
sub enable_autostart2_eth_roce()
{
	enable_autostartconfig("LLDPAD");
}

sub disable_autostart2_eth_roce()
{
	disable_autostartconfig("LLDPAD");
}

# ==========================================================================
# OFA delta_debug installation

# this is an odd component.  It consists of the debuginfo files which
# are built and identified in DebugRpms in other components.  Installing this
# component installs the debuginfo files for the installed components.
# uninstalling this component gets rid of all debuginfo files.
# uninstalling other components will get rid of individual debuginfo files
# for those components

sub get_rpms_dir_delta_debug($)
{
	my $package = shift();
	return get_rpms_dir_delta($package, "yes")
}

sub available_delta_debug()
{
	my $srcdir=$ComponentInfo{'delta_debug'}{'SrcDir'};
	return ((-d get_binary_pkg_dir($srcdir) || -d get_source_pkg_dir($srcdir))
			&& ( "$CUR_DISTRO_VENDOR" ne "SuSE" && rpm_will_build_debuginfo()));
}

sub installed_delta_debug()
{
	return rpm_is_installed("libpsm3-fi-debuginfo", "user");
}

# only called if installed_delta_debug is true
sub installed_version_delta_debug()
{
	if (rpm_is_installed("meta_eth_module", "any")) {
		my $version = rpm_query_version_release_pkg("meta_eth_module");
		return dot_version("$version");
	}
	if ( -e "$BASE_DIR/version_delta" ) {
		return `cat $BASE_DIR/version_delta`;
	} else {
		return "";
	}
}

# only called if available_delta_debug is true
sub media_version_delta_debug()
{
	return media_version_delta();
}

sub build_delta_debug($$$$)
{
	my $osver = shift();
	my $debug = shift();	# enable extra debug of build itself
	my $build_temp = shift();	# temp area for use by build
	my $force = shift();	# force a rebuild
	return 0;	# success
}

sub need_reinstall_delta_debug($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	my $reins = need_reinstall_delta_comp('delta_debug', $install_list, $installing_list);
	if ("$reins" eq "no" ) {
		# if delta components with DebugRpms have been added we need to reinstall
		# this component.  Note uninstall for individual components will
		# get rid of associated debuginfo files
		foreach my $comp ( @Components ) {
			# TBD can remove IsOFA test, the only
			# components with DebugRpms are for OFA delta debug
  			next if (! $ComponentInfo{$comp}{'IsOFA'});
			if ( " $installing_list " =~ / $comp /
				 && 0 != scalar(@{ $ComponentInfo{$comp}{'DebugRpms'}})) {
				return "this";
			}
		}

	}
	return $reins;
}

sub preinstall_delta_debug($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	return preinstall_delta("delta_debug", $install_list, $installing_list);
}

sub install_delta_debug($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	my @list;

	print_comp_install_banner('delta_debug');
	install_comp_rpms('delta_debug', " -U --nodeps ", $install_list);

	# install DebugRpms for each installed component
	foreach my $comp ( @Components ) {
		# TBD can remove IsOFA test, the only
		# components with DebugRpms are for OFA delta debug
  		next if (! $ComponentInfo{$comp}{'IsOFA'});
		if ( " $install_list " =~ / $comp / ) {
			install_comp_rpm_list("$comp", "user", " -U --nodeps ",
							@{ $ComponentInfo{$comp}{'DebugRpms'}});
		}
	}

	$ComponentWasInstalled{'delta_debug'}=1;
}

sub postinstall_delta_debug($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled
	#delta_restore_autostart('delta_debug');
}

sub uninstall_delta_debug($$)
{
	my $install_list = shift();	# total that will be left installed when done
	my $uninstalling_list = shift();	# what items are being uninstalled

	print_comp_uninstall_banner('delta_debug');

	uninstall_comp_rpms('delta_debug', ' --nodeps ', $install_list, $uninstalling_list, 'verbose');
	# uninstall debug rpms for all components
	foreach my $comp ( reverse(@Components) ) {
  		next if (! $ComponentInfo{$comp}{'IsOFA'});
		rpm_uninstall_list2("any", " --nodeps ", 'verbose',
					 @{ $ComponentInfo{$comp}{'DebugRpms'}});
	}
	$ComponentWasInstalled{'delta_debug'}=0;
}

# ==========================================================================
# OFA DELTA ibacm installation

# determine if the given capability is configured for Autostart at boot
sub IsAutostart2_ibacm()
{
	return IsAutostart_delta_comp2('ibacm');
}
sub autostart_desc_ibacm()
{
	return autostart_desc_comp('ibacm');
}
# enable autostart for the given capability
sub enable_autostart2_ibacm()
{
	enable_autostart($ComponentInfo{'ibacm'}{'StartupScript'});
}
# disable autostart for the given capability
sub disable_autostart2_ibacm()
{
	disable_autostart($ComponentInfo{'ibacm'}{'StartupScript'});
}

sub get_rpms_dir_ibacm($)
{
	my $package = shift();
	return get_rpms_dir_delta($package, "no")
}

sub available_ibacm()
{
	my $srcdir=$ComponentInfo{'ibacm'}{'SrcDir'};
	return (-d get_binary_pkg_dir($srcdir) || -d get_source_pkg_dir($srcdir));
}

sub installed_ibacm()
{
	return rpm_is_installed("ibacm", "user");
}

# only used on RHEL72, for other distros ibacm is only a SubComponent
# only called if installed_ibacm is true
sub installed_version_ibacm()
{
	my $version = rpm_query_version_release_pkg("ibacm");
	return dot_version("$version");
}

# only called if available_ibacm is true
sub media_version_ibacm()
{
	return media_version_delta();
}

sub build_ibacm($$$$)
{
	my $osver = shift();
	my $debug = shift();	# enable extra debug of build itself
	my $build_temp = shift();	# temp area for use by build
	my $force = shift();	# force a rebuild
	return 0;	# success
}

sub need_reinstall_ibacm($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	return (need_reinstall_delta_comp('ibacm', $install_list, $installing_list));
}

sub preinstall_ibacm($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	return preinstall_delta("ibacm", $install_list, $installing_list);
}

sub install_ibacm($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	print_comp_install_banner('ibacm');
	install_comp_rpms('ibacm', " -U --nodeps ", $install_list);

	$ComponentWasInstalled{'ibacm'}=1;
}

sub postinstall_ibacm($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled
	delta_restore_autostart('ibacm');
}

sub uninstall_ibacm($$)
{
	my $install_list = shift();	# total that will be left installed when done
	my $uninstalling_list = shift();	# what items are being uninstalled

	print_comp_uninstall_banner('ibacm');

	uninstall_comp_rpms('ibacm', ' --nodeps ', $install_list, $uninstalling_list, 'verbose');
	$ComponentWasInstalled{'ibacm'}=0;
}

sub check_os_prereqs_ibacm
{
	return rpm_check_os_prereqs("ibacm", "user");
}

# ------------------------------------------------------------------
# # subroutines for snmp component
# # -----------------------------------------------------------------
sub installed_snmp()
{
        return (rpm_is_installed("net-snmp", "user"));
}

sub enable_autostart2_snmp()
{
        enable_autostartconfig("SNMP");
}

sub disable_autostart2_snmp()
{
        disable_autostartconfig("SNMP");
}

sub IsAutostart2_snmp()
{
        return status_autostartconfig("SNMP");
}

