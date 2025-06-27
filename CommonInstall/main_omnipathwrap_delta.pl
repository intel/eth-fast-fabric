#!/usr/bin/perl
# BEGIN_ICS_COPYRIGHT8 ****************************************
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
# END_ICS_COPYRIGHT8   ****************************************

# [ICS VERSION STRING: unknown]
use strict;
#use Term::ANSIColor;
#use Term::ANSIColor qw(:constants);
#use File::Basename;
#use Math::BigInt;

# ===========================================================================
# Main menus, option handling and version handling for FF for OFA install
#

my $Build_OsVer=$CUR_OS_VER;
my $Build_Debug=0;	# should we provide more info for debug
my $Build_Temp="";	# temp area to use for build
my $Default_Build = 0;	# -B option used to select build
my $Build_Force = 0;# rebuild option used to force full rebuild
my $To_Show_Comps = 0; # indicate whether we need to show components or not
my $allow_install;

$FirstIPoIBInterface=0; # first device is ib0

	# Names of supported install components
	# must be listed in dependency order such that prereqs appear 1st
	# delta_debug must be last

my @EthAllComponents = (
	"eth_tools", "psm3", "eth_module", "fastfabric",
	"eth_roce", "openmpi_gcc_ofi",
	"openmpi_gcc_cuda_ofi", #"openmpi_gcc", "openmpi_intel_ofi", "mpiRest",
	"mpisrc", "delta_debug"	);

my @EthUbuntuComponents = (
	"eth_tools",
	"psm3",
	"eth_module",
	"fastfabric",
	"eth_roce",
);

my %Components_by_distro = (
	'SuSE*ES153'    => \@EthAllComponents,
	'SuSE*ES154'    => \@EthAllComponents,
	'SuSE*ES155'    => \@EthAllComponents,
	'SuSE*ES156'    => \@EthAllComponents,
	'SuSE*ES157'    => \@EthAllComponents,
	'redhat*ES88'   => \@EthAllComponents,
	'redhat*ES89'   => \@EthAllComponents,
	'redhat*ES810'  => \@EthAllComponents,
	'redhat*ES92'   => \@EthAllComponents,
	'redhat*ES93'   => \@EthAllComponents,
	'redhat*ES94'   => \@EthAllComponents,
	'redhat*ES95'   => \@EthAllComponents,
	'redhat*ES96'   => \@EthAllComponents,
	'redhat*ES10'   => \@EthAllComponents,
	'ubuntu*UB2204' => \@EthUbuntuComponents,
);

@Components = ( );

# RHEL7.3 and newer AND SLES12.2 and newer
my @SubComponents_newer = ( "snmp" );
@SubComponents = ( );

	# an additional "special" component which is always installed when
	# we install/upgrade anything and which is only uninstalled when everything
	# else has been uninstalled.  Typically this will be the iefsconfig
	# file and related files absolutely required by it (such as wrapper_version)
$WrapperComponent = "iefsconfig";

# This provides more detailed information about each Component in @Components
# since hashes are not retained in the order specified, we still need
# @Components and @SubComponents to control install and menu order
# Only items listed in @Components and @SubComponents are considered for install
# As such, this may list some components which are N/A to the selected distro
# Fields are as follows:
#	Name => full name of component/subcomponent for prompts
# 	DefaultInstall => default installation (State_DoNotInstall or State_Install)
#					  used only when available and ! installed
#	SrcDir => directory name within install media for component
# 	PreReq => other components which are prereqs of a given
#				component/subcomponent
#				Need space before and after each component name to
#				facilitate compares so that compares do not mistake names
#				such as mpidev for mpi
#	CoReq =>  other components which are coreqs of a given component
#				Note that CoReqs can also be listed as prereqs to provide
#				install verification as each component is installed,
#				however prereqs should only refer to items earlier in the list
#				Need space before and after each component name to
#				facilitate compares so that compares do not mistake names
#				such as mpidev for mpi
#	Hidden => component should not be shown in menus/prompts
#			  used for hidden PreReq.  Hidden components can't HasStart
#			  nor HasFirmware
#	Disabled =>  components/subcomponents which are disabled from installation
#	IsOFA =>  is an in-distro OFA component we upgrade (excludes MPI)
#	KernelRpms => kernel rpms for given component, in dependency order
#				These are rpms which are kernel version specific and
#				will have kernel uname -r in rpm package name.
#				For a given distro a separate version of each of these rpms
#				may exist per kernel.  These are always architecture dependent
#				Note KernelRpms are always installed before FirmwareRpms and
#				UserRpms
#	KernelDkms => DKMS version KernelRpms. Will install KernelDkms rather than
#				KernelRpms when dkms is installed and KernelDkms is not empty.
#				If not all rpms have DKMS version rpm, such as -devel rpm, leave
#				the non-dkms version rpm here. They will get installed
#	FirmwareRpms => firmware rpms for given component, in dependency order
#				These are rpms which are not kernel specific.  For a given
#				distro a single version of each of these rpms will
#				exist per distro/arch combination.  In most cases they will
#				be architecture independent (noarch).
#				These are rpms which are installed in user space but
#				ultimately end up in hardware such as HFI firmware, TMM firmware
#				BIOS firmware, etc.
#	UserRpms => user rpms for given component, in dependency order
#				These are rpms which are not kernel specific.  For a given
#				distro a single version of each of these rpms will
#				exist per distro/arch combination.  Some of these may
#				be architecture independent (noarch).
#	DebugRpms => user rpms for component which should be installed as part
#				of delta_debug component.
#	HasStart =>  components/subcomponents which have autostart capability
#	DefaultStart =>  should autostart default to Enable (1) or Disable (0)
#				Not needed/ignored unless HasStart=1
# 	StartPreReq => other components/subcomponents which must be autostarted
#				before autostarting this component/subcomponent
#				Not needed/ignored unless HasStart=1
#				Need space before and after each component name to
#				facilitate compares so that compares do not mistake names
#				such as mpidev for mpi
#	StartComponents => components/subcomponents with start for this component
#				if a component has a start script
#				list the component as a subcomponent of itself
#	StartupScript => name of startup script which controls startup of this
#				component
#	StartupParams => list of parameter names in $ETH_CONFIG which control
#				startup of this component (set to yes/no values)
#	HasFirmware =>  components which need HCA firmware update after installed
#
# Note both Components and SubComponents are included in the list below.
# Components require all fields be supplied
# SubComponents only require the following fields:
#	Name, PreReq (should reference only components), HasStart, StartPreReq,
#	DefaultStart, and optionally StartupScript, StartupParams
#	Also SubComponents only require the IsAutostart2_X, autostart_desc_X,
#	enable_autostart2_X, disable_autostart2_X and installed_X functions.
#	Typically installed_X for a SubComponent will simply call installed_X for
#	the component which contains it.
%ComponentInfo = (
		# our special WrapperComponent, limited use
	"iefsconfig" =>	{ Name => "Intel",
					  DefaultInstall => $State_Install,
					  SrcDir => ".",
					  PreReq => "", CoReq => "",
					  Hidden => 0, Disabled => 0, IsOFA => 0,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ ],
					  DebugRpms => [ ],
					  HasStart => 1, HasFirmware => 0, DefaultStart => 1,
					  StartPreReq => "",
					  StartComponents => [ "iefsconfig" ],
					  StartupScript => "iefs",
					  StartupParams => [ "ARPTABLE_TUNING" ]

					},
	"eth_tools" =>	{ Name => "Basic Tools",
					  DefaultInstall => $State_Install,
					  SrcDir => file_glob("./IntelEth-Tools*.*"),
					  PreReq => "", CoReq => "",
					  Hidden => 0, Disabled => 0, IsOFA => 0,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ "eth-tools-basic" ],
					  DebugRpms => [ ],
					  HasStart => 1, HasFirmware => 0, DefaultStart => 1,
					  StartPreReq => " iefsconfig ", # TBD
					  StartComponents => [ "snmp" ],
					  StartupScript => "",
					  StartupParams => [ ]
					},
	"fastfabric" =>	{ Name => "FastFabric",
					  DefaultInstall => $State_Install,
					  SrcDir => file_glob("./IntelEth-Tools*.*"),
					  PreReq => " eth_tools ", CoReq => "",
					  Hidden => 0, Disabled => 0, IsOFA => 0,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ "eth-tools-fastfabric", "eth-mpi-apps" ],
					  DebugRpms => [ ],
					  HasStart => 0, HasFirmware => 0, DefaultStart => 0,
					  StartPreReq => " iefsconfig ",
					  StartComponents => [ ],
					  StartupScript => "",
					  StartupParams => [ ]
					},
# a virtual component with install and uninstall behaviors so we can config and restore RDMA confs
	"eth_roce" =>	{ Name => "Eth RoCE",
					  DefaultInstall => $State_Install,
					  SrcDir => "",
					  PreReq => "", CoReq => "",
					  Hidden => 0, Disabled => 0, IsOFA => 0,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ ],
					  DebugRpms => [ ],
					  HasStart => 1, HasFirmware => 0, DefaultStart => 1,
					  StartPreReq => " iefsconfig ",
					  StartComponents => [ "eth_roce" ],
					  StartupScript => "lldpad",
					  StartupParams => [ ]
					},
	"psm3" =>	{ Name => "PSM3",
					  DefaultInstall => $State_Install,
					  SrcDir => file_glob("./IntelEth-OFA_DELTA.*"),
					  PreReq => "", CoReq => "",
					  Hidden => 0, Disabled => 0, IsOFA => 0,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ "libpsm3-fi" ],
					  DebugRpms =>  [ "libpsm3-fi-debuginfo" ],
					  HasStart => 0, HasFirmware => 0, DefaultStart => 0,
					  StartPreReq => " ",
					  StartComponents => [ ],
					  StartupScript => "",
					  StartupParams => [ ]
	},
	"openmpi_gcc" =>	{ Name => "OpenMPI (verbs,gcc)",
					  DefaultInstall => $State_DoNotInstall,
					  SrcDir => file_glob ("./OFA_MPIS.*"),
					  PreReq => " psm3 ", CoReq => "",
					  Hidden => 1, Disabled => 0, IsOFA => 0,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ "openmpi_gcc" ],
					  DebugRpms => [ ],
					  HasStart => 0, HasFirmware => 0, DefaultStart => 0,
					  StartPreReq => "",
					  StartComponents => [ ],
					  StartupScript => "",
					  StartupParams => [ ]
					},
	"openmpi_gcc_cuda_ofi" =>{ Name => "OpenMPI (cuda,gcc)",
					  DefaultInstall => $State_Install,
					  SrcDir => file_glob ("./OFA_MPIS.*"),
					  PreReq => " psm3 ", CoReq => "",
					  Hidden => 1, Disabled => 1, IsOFA => 0,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ "openmpi_gcc_cuda_ofi" ],
					  DebugRpms => [ ],
					  HasStart => 0, HasFirmware => 0, DefaultStart => 0,
					  StartPreReq => "",
					  StartComponents => [ ],
					  StartupScript => "",
					  StartupParams => [ ]
					},
	"openmpi_gcc_ofi" =>	{ Name => "OpenMPI (ofi,gcc)",
					  DefaultInstall => $State_Install,
					  SrcDir => file_glob ("./OFA_MPIS.*"),
					  PreReq => " psm3 ", CoReq => "",
					  Hidden => 0, Disabled => 0, IsOFA => 0,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ "openmpi_gcc_ofi" ],
					  DebugRpms => [ ],
					  HasStart => 0, HasFirmware => 0, DefaultStart => 0,
					  StartPreReq => "",
					  StartComponents => [ ],
					  StartupScript => "",
					  StartupParams => [ ]
					},
	"openmpi_intel_ofi" =>	{ Name => "OpenMPI (ofi,Intel)",
					  DefaultInstall => $State_Install,
					  SrcDir => file_glob ("./OFA_MPIS.*"),
					  PreReq => " psm3 ", CoReq => "",
					  Hidden => 1, Disabled => 0, IsOFA => 0,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ "openmpi_intel_ofi" ],
					  DebugRpms => [ ],
					  HasStart => 0, HasFirmware => 0, DefaultStart => 0,
					  StartPreReq => "",
					  StartComponents => [ ],
					  StartupScript => "",
					  StartupParams => [ ]
					},
# rest of MPI stuff which customer can build via do_build
# this is included here so we can uninstall
# special case use in comp_delta.pl, omitted from Components list
# TBD - how to best refactor this so we remove these (do we still need to remove them?)
	"mpiRest" =>	{ Name => "MpiRest (pgi,Intel)",
					  DefaultInstall => $State_DoNotInstall,
					  SrcDir => file_glob ("./OFA_MPIS.*"),
					  PreReq => "", CoReq => "",
					  Hidden => 1, Disabled => 1, IsOFA => 1,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [
									"openmpi_pgi",
									"openmpi_intel", "openmpi_pathscale",
									"openmpi_pgi_qlc", "openmpi_gcc_qlc",
									"openmpi_intel_qlc", "openmpi_pathscale_qlc",
									"openmpi_pgi_ofi", "openmpi_pathscale_ofi",
									"openmpi_pgi_cuda_ofi", "openmpi_pathscale_cuda_ofi",
								],
					  DebugRpms => [ ],
					  HasStart => 0, HasFirmware => 0, DefaultStart => 0,
					  StartPreReq => "",
					  StartComponents => [ ],
					  StartupScript => "",
					  StartupParams => [ ]
					},
	"mpisrc" =>{ Name => "MPI Source",
					  DefaultInstall => $State_Install,
					  SrcDir => file_glob ("./OFA_MPIS.*"),
					  PreReq => "", CoReq => "",
					  Hidden => 0, Disabled => 0, IsOFA => 0,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ ],
					  DebugRpms => [ ],
					  HasStart => 0, HasFirmware => 0, DefaultStart => 0,
					  StartPreReq => "",
					  StartComponents => [ ],
					  StartupScript => "",
					  StartupParams => [ ]
					},
	"delta_debug" =>	{ Name => "OFA Debug Info",
					  DefaultInstall => $State_DoNotInstall,
					  SrcDir => file_glob("./IntelEth-OFA_DELTA.*"),
					  PreReq => "", CoReq => "",
					  Hidden => 0, Disabled => 0, IsOFA => 1,
					  KernelRpms => [ ],
					  KernelDkms => [ ],
					  FirmwareRpms => [ ],
					  UserRpms => [ ],
					  DebugRpms => [ ],	# listed per comp
					  HasStart => 0, HasFirmware => 0, DefaultStart => 0,
					  StartPreReq => "",
					  StartComponents => [ ],
					  StartupScript => "",
					  StartupParams => [ ]
					},
		# snmp is a subcomponent
		# it has a startup, but is considered part of eth_tools
	"snmp" =>		{ Name => "SNMP",
					  PreReq => "",
					  HasStart => 1, DefaultStart => 1,
					  StartPreReq => "",
					  StartComponents => [ "snmp" ],
					  StartupScript => "",
					  StartupParams => [ ]
					},
	);

# We can improve ComponentInfo to include the following. But since they are used
# for mpisrc only, extending ComponentInfo doesn't benefit other components.
# We directly define them here where
#     Dest => installation location
#     SrcRpms => src rpms to install
#     BuildScripts => build script to install
#     MiscFiles => misc files, such as version file. This field includes 'Dest'
#                  and 'Src' to define the installed file name and source file
#                  name. For SrcRpms and BuildScript fields, the installed file
#                  name will be the same name as source file.
#     DirtyFiles => dirty files will be cleared during install/uninstall, such
#                   as the build generated files.
%ExtraMpisrcInfo = (
	Dest => "/usr/src/eth/MPI",
	SrcRpms => ["openmpi"],
	BuildScripts => ["do_build", "do_openmpi_build"],
	MiscFiles => [{
		Dest => ".version",
		Src => "version"}],
	DirtyFiles => [ "openmpi_*.rpm",
	                "make.*.{res,err,warn}", ".mpiinfo"]
);

# For RHEL73, SLES12sp2 and other newer distros
		# ibacm is a subcomponent
		# it has a startup, but is considered part of iefsconfig
my %ibacm_comp_info = (
		# TBD - should be a StartComponent only for these distros
	"ibacm" =>		{ Name => "OFA IBACM",
					  PreReq => "",
					  HasStart => 1, DefaultStart => 0,
					  StartPreReq => " iefsconfig ",
					  StartComponents => [ "ibacm" ],
					  StartupScript => "ibacm",
					  StartupParams => [ ]
					},
);


my %eth_module_rhel_comp_info = (
	"eth_module" =>	{ Name => "Kernel Module",
					  DefaultInstall => $State_Install,
					  SrcDir => file_glob("./IntelEth-OFA_DELTA.*"),
					  PreReq => " psm3 ", CoReq => " ",
					  Hidden => 0, Disabled => 0, IsOFA => 1,
					  KernelRpms => [ "kmod-iefs-kernel-updates", "iefs-kernel-updates-devel" ],
					  KernelDkms => [ "iefs-kernel-updates-dkms", "iefs-kernel-updates-devel" ],
					  FirmwareRpms => [ ],
					  UserRpms => [ ],
					  DebugRpms =>  [ ],
					  HasStart => 1, HasFirmware => 0, DefaultStart => 1,
					  StartPreReq => " iefsconfig ",
					  StartComponents => [ "eth_module" ],
					  StartupScript => "Rendezvous",
					  StartupParams => [ ]
					},
);

my %eth_module_sles_comp_info = (
	"eth_module" =>	{ Name => "Kernel Module",
					  DefaultInstall => $State_Install,
					  SrcDir => file_glob("./IntelEth-OFA_DELTA.*"),
					  PreReq => " psm3 ", CoReq => " ",
					  Hidden => 0, Disabled => 0, IsOFA => 1,
					  KernelRpms => [ "iefs-kernel-updates-kmp-default", "iefs-kernel-updates-devel" ],
					  KernelDkms => [ "iefs-kernel-updates-dkms", "iefs-kernel-updates-devel" ],
					  FirmwareRpms => [ ],
					  UserRpms => [ ],
					  DebugRpms =>  [ ],
					  HasStart => 1, HasFirmware => 0, DefaultStart => 1,
					  StartPreReq => " iefsconfig ",
					  StartComponents => [ "eth_module" ],
					  StartupScript => "Rendezvous",
					  StartupParams => [ ]
					},
);

my %eth_module_debian_comp_info = (
	"eth_module" =>	{ Name => "Kernel Module",
					  DefaultInstall => $State_Install,
					  SrcDir => file_glob("./IntelEth-OFA_DELTA.*"),
					  PreReq => " psm3 ", CoReq => " ",
					  Hidden => 0, Disabled => 0, IsOFA => 1,
					  KernelRpms => [ "kmod-iefs-kernel-updates", "iefs-kernel-updates-devel" ],
					  KernelDkms => [ "iefs-kernel-updates-dkms", "iefs-kernel-updates-devel" ],
					  FirmwareRpms => [ ],
					  UserRpms => [ ],
					  DebugRpms =>  [ ],
					  HasStart => 1, HasFirmware => 0, DefaultStart => 1,
					  StartPreReq => " iefsconfig ",
					  StartComponents => [ "eth_module" ],
					  StartupScript => "Rendezvous",
					  StartupParams => [ ]
					},
);

my %Comp_info_by_distro = (
	'SuSE*ES153'    => { %ibacm_comp_info, %eth_module_sles_comp_info },
	'SuSE*ES154'    => { %ibacm_comp_info, %eth_module_sles_comp_info },
	'SuSE*ES155'    => { %ibacm_comp_info, %eth_module_sles_comp_info },
	'SuSE*ES156'    => { %ibacm_comp_info, %eth_module_sles_comp_info },
	'SuSE*ES157'    => { %ibacm_comp_info, %eth_module_sles_comp_info },

	'redhat*ES88'   => { %ibacm_comp_info, %eth_module_rhel_comp_info },
	'redhat*ES89'   => { %ibacm_comp_info, %eth_module_rhel_comp_info },
	'redhat*ES810'  => { %ibacm_comp_info, %eth_module_rhel_comp_info },
	'redhat*ES92'   => { %ibacm_comp_info, %eth_module_rhel_comp_info },
	'redhat*ES93'   => { %ibacm_comp_info, %eth_module_rhel_comp_info },
	'redhat*ES94'   => { %ibacm_comp_info, %eth_module_rhel_comp_info },
	'redhat*ES95'   => { %ibacm_comp_info, %eth_module_rhel_comp_info },
	'redhat*ES96'   => { %ibacm_comp_info, %eth_module_rhel_comp_info },
	'redhat*ES10'   => { %ibacm_comp_info, %eth_module_rhel_comp_info },

	'ubuntu*UB2204' => { %ibacm_comp_info, %eth_module_debian_comp_info },
);


	# translate from startup script name to component/subcomponent name
%StartupComponent = (
				"iefsconfig" => "iefsconfig",
			);
	# has component been loaded since last configured autostart
%ComponentWasInstalled = (
				"ibacm" => 0,
				"eth_tools" => 0,
				"fastfabric" => 0,
				"eth_roce" => 0,
				"openmpi_gcc" => 0,
				"openmpi_gcc_ofi" => 0,
				"openmpi_gcc_cuda_ofi" => 0,
				"openmpi_intel_ofi" => 0,
				"mpiRest" => 0,
				"mpisrc" => 0,
				"delta_debug" => 0,
				"snmp" => 0,
			);

sub init_components
{
	# The component list has slight variations per distro
	my $distro = "$CUR_DISTRO_VENDOR*$CUR_VENDOR_VER";
	if (exists $Components_by_distro{$distro}) {
		# Common for all packages
		@Components = @{$Components_by_distro{$distro}};
		@SubComponents = ( @SubComponents_newer );
		%ComponentInfo = ( %ComponentInfo,
			%{$Comp_info_by_distro{$distro}}
		);
	} else {
		# unknown or unsupported distro, leave lists empty
		# verify_distrib_files will catch unsupported distro
		@Components = ( );
		@SubComponents = ( );
	}
}

# ==========================================================================
# iefsconfig installation
# This is a special WrapperComponent which only needs:
#	available, install and uninstall
# it cannot have startup scripts, dependencies, prereqs, etc
sub IsAutostart2_iefsconfig()
{
	return IsAutostart($ComponentInfo{"iefsconfig"}{'StartupScript'});
}
sub autostart_desc_iefsconfig()
{
	return autostart_desc_comp('iefsconfig');
}
# enable autostart for the given capability
sub enable_autostart2_iefsconfig()
{
	enable_autostart("iefs");
}
# disable autostart for the given capability
sub disable_autostart2_iefsconfig()
{
	disable_autostart("iefs");
}

sub get_rpms_dir_iefsconfig()
{
	my $srcdir=$ComponentInfo{'iefsconfig'}{'SrcDir'};
	my $pkg_dir = get_binary_pkg_dir($srcdir);
	return "$pkg_dir/*";
}

sub pkg_iefsconfig
{
	my $pkg_dir = get_rpms_dir_iefsconfig();
	return (rpm_resolve("$pkg_dir/iefsconfig", "any"));
}

sub available_iefsconfig
{
	return (pkg_iefsconfig() ne "");
}

sub installed_iefsconfig
{
	return rpm_is_installed("iefsconfig", "any");
}

sub installed_version_iefsconfig
{
	my $version = rpm_query_version_release_pkg("iefsconfig");
	return dot_version("$version");
}

sub media_version_iefsconfig
{
	my $rpm = pkg_iefsconfig();
	my $version = rpm_query_version_release($rpm);
	return dot_version("$version");
}

sub build_iefsconfig
{
	my $osver = $_[0];
	my $debug = $_[1];      # enable extra debug of build itself
	my $build_temp = $_[2]; # temp area for use by build
	my $force = $_[3];      # force a rebuild
	return 0;       # success
}

sub need_reinstall_iefsconfig($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	return "no";
}

sub check_os_prereqs_iefsconfig
{
	return rpm_check_os_prereqs("iefsconfig", "user");
}

sub preinstall_iefsconfig
{
        my $install_list = $_[0];       # total that will be installed when done
        my $installing_list = $_[1];    # what items are being installed/reinstalled

        return 0;       # success
}

sub install_iefsconfig
{
	my $install_list = $_[0];	# total that will be installed when done
	my $installing_list = $_[1];	# what items are being installed/reinstalled

	my $srcdir=$ComponentInfo{'iefsconfig'}{'SrcDir'};
	NormalPrint("Installing $ComponentInfo{'iefsconfig'}{'Name'}...\n");

	# all meta pkgs depend on iefsconfig. We may directly upgrade iefsconfig, so we remove them first.
	rpm_uninstall_matches("meta_", "meta_", "", "");

	#override the udev permissions.
	#install_udev_permissions("$srcdir/config");

	# setup environment variable so that RPM can configure limits conf
	setup_env("ETH_LIMITS_CONF", 1);
	# so setting up envirnment to install driver for this component. actual install is done by rpm
	setup_env("ETH_INSTALL_CALLER", 1);

	# Check $BASE_DIR directory ...exist
	check_config_dirs();
	check_dir("/usr/lib/eth-tools");

	config_arptbl_tunning();

	config_uffd_access();

	# New Install Code
	my $pkg_dir = get_binary_pkg_dir($srcdir);
	my $rpmfile = rpm_resolve("$pkg_dir/*/iefsconfig", "any");
	rpm_run_install($rpmfile, "any", " -U --nodeps ");
	# New Install Code

	# remove the old style version file
	system("rm -rf /$BASE_DIR/version");
	# version_wrapper is only for support (fetched in ethcapture)
	system("echo '$VERSION' > $BASE_DIR/version_wrapper 2>/dev/null");

	$ComponentWasInstalled{'iefsconfig'}=1;
}

sub postinstall_iefsconfig
{
	my $old_eth_conf = 0;               # do we have an existing eth conf file
	my $old_ofa_conf = 0;               # do we have an existing ofa conf file
	my $install_list = $_[0];       # total that will be installed when done
	my $installing_list = $_[1];    # what items are being installed/reinstalled
	my $conf_file = "";
	my $old_conf = 0;

	if ( -e "/$ETH_CONFIG" ) {
		if (0 == system("cp /$ETH_CONFIG /$ETH_CONFIG-save")) {
			$old_eth_conf=1;
		}
	}

	if ( -e "/$OFA_CONFIG" ) {
		if (0 == system("cp /$OFA_CONFIG /$OFA_CONFIG-save")) {
			$old_ofa_conf=1;
		}
	}

	# adjust autostart settings
	foreach my $c ( @Components )
	{
		if ($ComponentInfo{$c}{'IsOFA'}) {
			$conf_file = "$OFA_CONFIG";
			$old_conf = $old_ofa_conf;
		} else {
			$conf_file = "$ETH_CONFIG";
			$old_conf = $old_eth_conf;
		}

		if ($install_list !~ / $c /) {
			# disable autostart of uninstalled components
			# iefsconfig is at least installed
			foreach my $p ( @{ $ComponentInfo{$c}{'StartupParams'} } ) {
				change_conf_param($p, "no", $conf_file);
			}
		} else {
			# retain previous setting for components being installed
			# set to no if initial install
			# TBD - should move this to rpm .spec file
			# the rpm might do this for us, repeat just to be safe
			foreach my $p ( @{ $ComponentInfo{$c}{'StartupParams'} } ) {
				my $old_value = "";
				if ( $old_conf ) {
					$old_value = read_conf_param($p, "/$conf_file-save");
				}
				if ( "$old_value" eq "" ) {
					$old_value = "no";
				}
				change_conf_param($p, $old_value, $conf_file);
			}
		}
	}

}

sub uninstall_iefsconfig
{
	my $install_list = $_[0];	# total that will be left installed when done
	my $uninstalling_list = $_[1];	# what items are being uninstalled

	NormalPrint("Uninstalling $ComponentInfo{'iefsconfig'}{'Name'}...\n");

	# all meta pkgs depend on iefsconfig. Remove them first.
	rpm_uninstall_matches("meta_", "meta_", "", "");

	# New Uninstall Code
	setup_env("ETH_INSTALL_CALLER", 0);
	rpm_uninstall_list("any", "verbose", ("iefsconfig") );
	remove_limits_conf;
	# New Uninstall Code

	system("rm -rf $BASE_DIR/version_wrapper");
	system("rm -rf $BASE_DIR/osid_wrapper");
	system("rm -rf $ETH_CONFIG");
	# remove the old style version file
	system("rm -rf /$BASE_DIR/version");
	system("rm -rf /sbin/iefsconfig");
	# there is no ideal answer here, if we install updates separately
	# then uninstall all with wrapper, make sure we cleanup
	system "rmdir $BASE_DIR 2>/dev/null";	# remove only if empty
	system "rmdir $ETH_CONFIG_DIR 2>/dev/null";	# remove only if empty
	$ComponentWasInstalled{'iefsconfig'}=0;
}

if ( my_basename($0) ne "INSTALL" )
{
	$allow_install=0;
} else {
	$allow_install=1;
	$FabricSetupScpFromDir="..";
}

sub Usage
{
	if ( $allow_install ) {
		#printf STDERR "Usage: $0 [-r root] [-v|-vv] -R osver -B osver [-d][-t tempdir] [--prefix dir] [--without-depcheck] [--rebuild] [--force] [--answer keyword=value]\n";
		#printf STDERR "               or\n";
		#printf STDERR "Usage: $0 [-r root] [-v|-vv] [-a|-n|-U|-F|-u|-s|-i comp|-e comp] [-E comp] [-D comp] [-f] [--fwupdate asneeded|always] [-l] [--prefix dir] [--without-depcheck] [--rebuild] [--force] [--answer keyword=value]\n";
		#printf STDERR "Usage: $0 [-r root] [-v|-vv] [-a|-n|-U|-F|-u|-s|-i comp|-e comp] [-E comp] [-D comp] [-f] [--fwupdate asneeded|always] [--prefix dir] [--without-depcheck] [--rebuild] [--force] [--answer keyword=value]\n";
		printf STDERR "Usage: $0 [-v|-vv] -R osver [-a|-n|-U|-u|-s|-O|-N|-i comp|-e comp] [-G]\n";
		printf STDERR "       %*s [-E comp] [-D comp] [--user-space] [--without-depcheck]\n", length($0), " ";
		printf STDERR "       %*s [--rebuild] [--force] [--answer keyword=value]\n", length($0), " ";
	} else {
#		printf STDERR "Usage: $0 [-r root] [-v|-vv] [-F|-u|-s|-e comp] [-E comp] [-D comp]\n";
#		printf STDERR "          [--fwupdate asneeded|always] [--user_queries|--no_user_queries] [--answer keyword=value]\n";
		printf STDERR "Usage: $0 [-v|-vv] [-u|-s|-e comp] [-E comp] [-D comp] [--answer keyword=value]\n";
#		printf STDERR "          [--user_queries|--no_user_queries]\n";
	}
	printf STDERR "               or\n";
	printf STDERR "Usage: $0 -C\n";
	printf STDERR "               or\n";
	printf STDERR "Usage: $0 -V\n";
	if ( $allow_install ) {
		printf STDERR "       -a - Installs all components and drivers with\n";
		printf STDERR "            the default options.\n";
		printf STDERR "       -n - Installs all components and drivers with the default options,\n";
		printf STDERR "            but does not change the autostart options.\n";
		printf STDERR "       -U - Upgrades/reinstalls all presently installed components and drivers\n";
		printf STDERR "            with the default options, and does not change the autostart options.\n";
		printf STDERR "       -i comp - Installs the given component with the default options.\n";
		printf STDERR "            This option can appear multiple times on a command line.\n";
		printf STDERR "       --user-space - Skips kernel space components during installation.\n";
		printf STDERR "       --without-depcheck - Disables the check of OS dependencies.\n";
		printf STDERR "       --rebuild - Forces a rebuild of kernel module srpms.\n";
		printf STDERR "       --force - Forces the installation, even if the distributions do not match.\n";
		printf STDERR "            Use of this option can result in undefined behaviors.\n";
		printf STDERR "       -O - Keeps the current modified rpm configuration file.\n";
		printf STDERR "       -N - Uses a new default rpm configuration file.\n";

		# -B, -t and -d options are purposely not documented
		#printf STDERR "       -B osver - run build for all components targetting kernel osver\n";
		#printf STDERR "       -t - temp area for use by builds, only valid with -B\n";
		#printf STDERR "       -d - enable build debugging assists, only valid with -B\n";
		printf STDERR "       -R osver - Forces install for specific OS kernel version, rather\n";
		printf STDERR "            than running kernel.\n";
	}
#	printf STDERR "       -F - upgrade HCA Firmware with default options\n";
#	printf STDERR "       --fwupdate asneeded|always - select fw update auto update mode\n";
#	printf STDERR "            asneeded - update or downgrade to match version in this release\n";
#	printf STDERR "            always - rewrite with this releases version even if matches\n";
#	printf STDERR "            default is to upgrade as needed but not downgrade\n";
#	printf STDERR "            this option is ignored for interactive install\n";
	printf STDERR "       -u - Uninstalls all components and drivers with the default options.\n";
	printf STDERR "       -s - Enables autostart for all installed software.\n";
	printf STDERR "       -e comp - Uninstalls the given component with the default options.\n";
	printf STDERR "            This option can appear multiple times on a command line.\n";
	printf STDERR "       -E comp - Enables autostart of given component. This option can\n";
	printf STDERR "            appear with -D or multiple times on a command line.\n";
	printf STDERR "       -D comp - Disables autostart of the given component. This option can\n";
	printf STDERR "            appear with -E or multiple times on a command line.\n";
	printf STDERR "       -G - Installs GPU support components (must have GPU drivers).\n";
	printf STDERR "            Also, either NVIDIA_GPU_DIRECT=<DIR> or INTEL_GPU_DIRECT=<DIR>\n";
	printf STDERR "            may be in env. If neither is set, INSTALL will automatically detect GPU\n";
	printf STDERR "            drivers and software, and install the appropriate IEFS components for that\n";
	printf STDERR "            type of GPU.\n";
	printf STDERR "       -v - Provides verbose logging. Logs to the $LogFileDefault file.\n";
	printf STDERR "       -vv - Provides very verbose debug logging. Logs to the $LogFileDefault file.\n";
	printf STDERR "       -C - Shows the list of supported component names.\n";
#       Hidden feature for internal use
#       printf STDERR "       -c comp - output component information in JSON format\n";
	printf STDERR "       -V - Outputs the version number of the software.\n";

#	printf STDERR "       --user_queries - permit non-root users to query the fabric. (default)\n";
#	printf STDERR "       --no_user_queries - non root users cannot query the fabric.\n";
	showAnswerHelp();

	printf STDERR "       Default options retain existing configuration files.\n";
	printf STDERR "       Supported component names:\n";
	printf STDERR "            ";
	ShowComponents(\*STDERR);
	printf STDERR "       Supported component name aliases:\n";
	printf STDERR "            eth mpi psm_mpi tools\n";
	if (scalar(@SubComponents) > 0) {
		printf STDERR "       Additional component names allowed for -E and -D options:\n";
		printf STDERR "            ";
		foreach my $comp ( @SubComponents )
		{
			printf STDERR " $comp";
		}
		printf STDERR "\n";
	}
	exit (2);
}

my $Default_FirmwareUpgrade=0;	# -F option used to select default firmware upgrade

# translate an alias component name into the corresponding list of OFA comps
# if the given name is invalid or has no corresponding OFA component
# returns an empty list
sub translate_comp
{
	my($arg)=$_[0];
	if ("$arg" eq "eth"){
		my @res = ("eth_tools", "psm3", "eth_module", "openmpi_gcc_ofi");
		if ($GPU_Install eq "NV_GPU") {
			push(@res, "openmpi_gcc_cuda_ofi");
		}
		return @res;
	} elsif ("$arg" eq "mpi"){
		my @res = ("openmpi_gcc_ofi");
		if ($GPU_Install eq "NV_GPU") {
			push(@res, "openmpi_gcc_cuda_ofi");
		}
		return @res;
	} elsif ("$arg" eq "psm_mpi"){
		my @res = ("psm3", "eth_module", "openmpi_gcc_ofi");
		if ($GPU_Install eq "NV_GPU") {
			push(@res, "openmpi_gcc_cuda_ofi");
		}
		return @res;
	} elsif ("$arg" eq "delta_mpisrc"){
		return ( "mpisrc" ); # legacy
		# no ibaccess argument equivalent for:
		#	delta_debug
		#
	} elsif ("$arg" eq "tools"){
		my @res = ("eth_tools");
		return @res;
	} else {
		return ();	# invalid name
	}
}

sub process_args
{
	my $arg;
	my $last_arg;
	my $install_opt = 0;
	my $setcomp = 0;
	my $setanswer = 0;
	my $setenabled = 0;
	my $setdisabled = 0;
	my $setosver = 0;
	my $setbuildtemp = 0;
	my $comp = 0;
	my $osver = 0;
	my $setcurosver = 0;
	my $setfwmode = 0;
	my $patch_ofed=0;
	my $mofed_path="";
	my $auto_detect_gpu=0;

	if (scalar @ARGV >= 1) {
		foreach $arg (@ARGV) {
			if ( $setanswer ) {
				my @pair = split /=/,$arg;
				if ( scalar(@pair) != 2 ) {
					printf STDERR "Invalid --answer keyword=value: '$arg'\n";
					Usage;
				}
				set_answer($pair[0], $pair[1]);
				$setanswer=0;
			} elsif ( $setcomp ) {
				foreach $comp ( @Components )
				{
					if ( "$arg" eq "$comp" )
					{
						$Default_Components{$arg} = 1;
						$setcomp=0;
					}
				}
				if ( $setcomp )
				{
					my @comps = translate_comp($arg);
					# if empty list returned, we will not clear setcomp and
					# will get error below
					foreach $comp ( @comps )
					{
						$Default_Components{$comp} = 1;
						$setcomp=0;
					}
				}
				if ( $setcomp )
				{
					printf STDERR "Invalid component: $arg\n";
					Usage;
				}
			} elsif ( $setenabled ) {
				foreach $comp ( @Components, @SubComponents )
				{
					if ( "$arg" eq "$comp" )
					{
						$Default_EnabledComponents{$arg} = 1;
						$setenabled=0;
					}
				}
				if ( $setenabled )
				{
					my @comps = translate_comp($arg);
					# if empty list returned, we will not clear setcomp and
					# will get error below
					foreach $comp ( @comps )
					{
						$Default_EnabledComponents{$comp} = 1;
						$setenabled=0;
					}
				}
				if ( $setenabled )
				{
					printf STDERR "Invalid component: $arg\n";
					Usage;
				}
			} elsif ( $setdisabled ) {
				foreach $comp ( @Components, @SubComponents )
				{
					if ( "$arg" eq "$comp" )
					{
						$Default_DisabledComponents{$arg} = 1;
						$setdisabled=0;
					}
				}
				if ( $setdisabled )
				{
					my @comps = translate_comp($arg);
					# if empty list returned, we will not clear setcomp and
					# will get error below
					foreach $comp ( @comps )
					{
						$Default_DisabledComponents{$comp} = 1;
						$setdisabled=0;
					}
				}
				if ( $setdisabled )
				{
					printf STDERR "Invalid component: $arg\n";
					Usage;
				}
			} elsif ( $setosver ) {
				$Build_OsVer="$arg";
				$setosver=0;
			} elsif ( $setbuildtemp ) {
				$Build_Temp="$arg";
				$setbuildtemp=0;
#			} elsif ( $setfwmode ) {
#				if ( "$arg" eq "always" || "$arg" eq "asneeded") {
#					$Default_FirmwareUpgradeMode="$arg";
#				} else {
#					printf STDERR "Invalid --fwupdate mode: $arg\n";
#					Usage;
#				}
#				$setfwmode = 0;
			} elsif ( $setcurosver ) {
				$CUR_OS_VER="$arg";
				$setcurosver=0;
			} elsif ( "$arg" eq "-v" ) {
				$LogLevel=1;
			} elsif ( "$arg" eq "-vv" ) {
				$LogLevel=2;
#			} elsif ( "$arg" eq "-f" ) {
#				$Skip_FirmwareUpgrade=1;
			} elsif ( "$arg" eq "-i" ) {
				$Default_CompInstall=1;
				$Default_Prompt=1;
				$setcomp=1;
				if ($install_opt || $Default_CompUninstall || $Default_Build) {
					# can't mix -i with other install controls
					printf STDERR "Invalid combination of options: $arg not permitted with previous options\n";
					Usage;
				}
			} elsif ( "$arg" eq "-e" ) {
				$Default_CompUninstall=1;
				$Default_Prompt=1;
				$setcomp=1;
				if ($install_opt || $Default_CompInstall || $Default_Build) {
					# can't mix -e with other install controls
					printf STDERR "Invalid combination of options: $arg not permitted with previous options\n";
					Usage;
				}
			} elsif ( "$arg" eq "-E" ) {
				$Default_Autostart=1;
				$Default_EnableAutostart=1;
				$Default_Prompt=1;
				$setenabled=1;
				if ($Default_Build) {
					# can't mix -E with other install controls
					printf STDERR "Invalid combination of options: $arg not permitted with previous options\n";
					Usage;
				}
			} elsif ( "$arg" eq "-D" ) {
				$Default_Autostart=1;
				$Default_DisableAutostart=1;
				$Default_Prompt=1;
				$setdisabled=1;
				if ($Default_Build) {
					# can't mix -D with other install controls
					printf STDERR "Invalid combination of options: $arg not permitted with previous options\n";
					Usage;
				}
			} elsif ( "$arg" eq "--fwupdate" ) {
				$setfwmode=1;
			} elsif ( "$arg" eq "--answer" ) {
				$setanswer=1;
			} elsif ( "$arg" eq "--without-depcheck" ) {
				$rpm_check_dependencies=0;
			} elsif ( "$arg" eq "--user-space" ) {
				$user_space_only = 1;
			} elsif ( "$arg" eq "--force" ) {
				$Force_Install=1;
			} elsif ( "$arg" eq "-G") {
				if (defined $ENV{'NVIDIA_GPU_DIRECT'} && -e "$ENV{'NVIDIA_GPU_DIRECT'}/Module.symvers" ) {
					$GPU_Dir = $ENV{'NVIDIA_GPU_DIRECT'};
					$ComponentInfo{"openmpi_gcc_cuda_ofi"}{'Hidden'} = 0;
					$ComponentInfo{"openmpi_gcc_cuda_ofi"}{'Disabled'} = 0;
					$GPU_Install="NV_GPU";
				} elsif (defined $ENV{'INTEL_GPU_DIRECT'} && -e "$ENV{'INTEL_GPU_DIRECT'}/Module.symvers" ) {
					$GPU_Dir = $ENV{'INTEL_GPU_DIRECT'};
					$GPU_Install="INTEL_GPU";
				} elsif (defined $ENV{'INTEL_GPU_DIRECT'} && "$ENV{'INTEL_GPU_DIRECT'}" eq "CURRENT_KERNEL" ) {
					$GPU_Dir = $ENV{'INTEL_GPU_DIRECT'};
					$GPU_Install="INTEL_GPU";
				}

				# GPU direct not set by user, set flag to auto-detect after loop
				if ("$GPU_Install" eq "NONE") {
					$auto_detect_gpu = 1;
				}
			} elsif ( "$arg" eq "-C" ) {
				$To_Show_Comps = 1;
			} elsif ( "$arg" eq "-c" ) {
				# undocumented option to output detailed information on a component
				$Default_ShowCompInfo=1;
				$setcomp=1;
			} elsif ( "$arg" eq "-V" ) {
				printf "$VERSION\n";
				exit(0);
			} elsif ( "$arg" eq "-R" ) {
				$setcurosver=1;
			} elsif ( "$arg" eq "-B" ) {
				# undocumented option to do a build for specific OS
				$Default_Build=1;
				$Default_Prompt=1;
				$setosver=1;
				if ($install_opt || $Default_CompInstall || $Default_CompUninstall || $Default_Autostart) {
					# can't mix -B with install
					printf STDERR "Invalid combination of options: $arg not permitted with previous options\n";
					Usage;
				}
			} elsif ( "$arg" eq "-d" ) {
				# undocumented option to aid debug of build
				$Build_Debug=1;
				if ($install_opt || $Default_CompInstall || $Default_CompUninstall || $Default_Autostart) {
					# can't mix -d with install
					printf STDERR "Invalid combination of options: $arg not permitted with previous options\n";
					Usage;
				}
			} elsif ( "$arg" eq "-t" ) {
				# undocumented option to aid debug of build
				$setbuildtemp=1;
				if ($install_opt || $Default_CompInstall || $Default_CompUninstall || $Default_Autostart) {
					# can't mix -t with install
					printf STDERR "Invalid combination of options: $arg not permitted with previous options\n";
					Usage;
				}
			} elsif ( "$arg" eq "--rebuild" ) {
				# force rebuild
				$Build_Force=1;
				$OFED_force_rebuild=1;
			} elsif ( "$arg" eq "--user_queries" ) {
				$Default_UserQueries=1;
			} elsif ( "$arg" eq "--no_user_queries" ) {
				$Default_UserQueries=-1;
			} elsif ( "$arg" eq "--patch_ofed" ) {
				$patch_ofed=1;
			} else {
				# Install options
				if ( "$arg" eq "-a" ) {
					$Default_Install=1;
				} elsif ( "$arg" eq "-u" ) {
					$Default_Uninstall=1;
				} elsif ( "$arg" eq "-s" ) {
					$Default_Autostart=1;
				} elsif ( "$arg" eq "-n" ) {
					$Default_Install=1;
					$Default_SameAutostart=1;
				} elsif ( "$arg" eq "-U" ) {
					$Default_Upgrade=1;
					$Default_SameAutostart=1;
#				} elsif ( "$arg" eq "-F" ) {
#					$Default_FirmwareUpgrade=1;
				} elsif ("$arg" eq "-O") {
					$Default_RpmConfigKeepOld=1;
					$Default_RpmConfigUseNew=0;
				} elsif ("$arg" eq "-N") {
					$Default_RpmConfigKeepOld=0;
					$Default_RpmConfigUseNew=1;
				} else {
					printf STDERR "Invalid option: $arg\n";
					Usage;
				}
				if ($install_opt || $Default_CompInstall
					|| $Default_CompUninstall) {
					# only one of the above install selections
					printf STDERR "Invalid combination of options: $arg not permitted with previous options\n";
					Usage;
				}
				$install_opt=1;
				if ( $Default_RpmConfigKeepOld || $Default_RpmConfigUseNew) {
					$Default_Prompt=0;
				} else {
					$Default_Prompt=1;
				}
			}
			$last_arg=$arg;
		}
	}

	# GPU direct not set by user, attempt to set automatically
	# check for intel gpu: if oneapi-ze in package, check for oneapi installed
	if (1 == $auto_detect_gpu) {
		if (0 == find_dir("ONEAPI-ZE")) {
			# call get_kernel_module_for_intel_gpu which will set GPU_Install and GPU_Dir if valid
			if (get_kernel_module_for_intel_gpu($CUR_OS_VER, $GPU_Install, $GPU_Dir) == 1) {
				setup_env("INTEL_GPU_DIRECT", $GPU_Dir);
				NormalPrint("Using INTEL_GPU_DIRECT=$GPU_Dir\n")
			}
		}
		# check for nvidia gpu: if cuda in package, check for cuda installed
		if (("$GPU_Install" eq "NONE") && (0 == find_dir("CUDA"))) {
			# call get_kernel_module_for_nv_gpu which will set GPU_Install and GPU_Dir if valid
			if (get_kernel_module_for_nv_gpu($CUR_OS_VER, $GPU_Install, $GPU_Dir) == 1) {
				setup_env("NVIDIA_GPU_DIRECT", $GPU_Dir);
				$ComponentInfo{"openmpi_gcc_cuda_ofi"}{'Hidden'} = 0;
				$ComponentInfo{"openmpi_gcc_cuda_ofi"}{'Disabled'} = 0;
				NormalPrint("Using NVIDIA_GPU_DIRECT=$GPU_Dir\n");
			}
		}
		if ("$GPU_Install" eq "NONE") {
			printf STDERR "GPU Direct requested, and:\n";
			printf STDERR "  - Neither NVIDIA_GPU_DIRECT nor INTEL_GPU_DIRECT set\n";
			printf STDERR "                   -or-\n";
			printf STDERR "  - GPU support not automatically detected in OS installation\n";
			printf STDERR "                   -or-\n";
			printf STDERR "  - GPU support automatically detected but user rejected using it\n";
			printf STDERR "                   -or-\n";
			printf STDERR "  - GPU software not included in this package\n";
			Usage;
		}
	}
	if ($To_Show_Comps == 1) {
		ShowComponents;
		exit(0);
	}
	if ( $setcomp || $setenabled || $setdisabled  || $setosver || $setbuildtemp || $setfwmode || $setanswer) {
		printf STDERR "Missing argument for option: $last_arg\n";
		Usage;
	}
	if ( ($Default_Install || $Default_CompInstall || $Default_Upgrade
			|| $Force_Install)
         && ! $allow_install) {
		printf STDERR "Installation options not permitted in this mode\n";
		Usage;
	}
	if ( ($Default_Build || $OFED_force_rebuild ) && ! $allow_install) {
		printf STDERR "Build options not permitted in this mode\n";
		Usage;
	}
	if ($Default_Build || $OFED_force_rebuild) {
		# try to set MOFED_PATH if not set
		if (!defined $ENV{'MOFED_PATH'}) {
			if ( -d "/usr/src/ofa_kernel/x86_64/$CUR_OS_VER") {
				if (GetYesNo("Mofed installation discovered - set MOFED_PATH?", "y") == 1) {
					$mofed_path = `echo /usr/src/ofa_kernel/x86_64/$CUR_OS_VER)`;
					setup_env("MOFED_PATH", $mofed_path);
					printf STDERR "Using MOFED_PATH=$mofed_path\n";
				}
			}
		}
	}
}

my @INSTALL_CHOICES= ();
sub show_menu
{
	my $inp;
	my $max_inp;

	@INSTALL_CHOICES= ();
	if ( $Default_Install ) {
		NormalPrint ("Installing All Intel Software\n");
		@INSTALL_CHOICES = ( @INSTALL_CHOICES, 1);
	}
   	if ( $Default_CompInstall ) {
		NormalPrint ("Installing Selected Intel Software\n");
		@INSTALL_CHOICES = ( @INSTALL_CHOICES, 1);
	}
  	if ( $Default_Upgrade ) {
		NormalPrint ("Upgrading/Re-Installing Intel Software\n");
		@INSTALL_CHOICES = ( @INSTALL_CHOICES, 1);
	}
#   	if ( $Default_FirmwareUpgrade ) {
#		NormalPrint ("Upgrading HCA Firmware\n");
#		@INSTALL_CHOICES = ( @INSTALL_CHOICES, 4);
#	}
   	if ($Default_Uninstall ) {
		NormalPrint ("Uninstalling All Intel Software\n");
		@INSTALL_CHOICES = ( @INSTALL_CHOICES, 6);
	}
   	if ($Default_CompUninstall ) {
		NormalPrint ("Uninstalling Selected Intel Software\n");
		@INSTALL_CHOICES = ( @INSTALL_CHOICES, 6);
	}
   	if ($Default_Autostart) {
		NormalPrint ("Configuring Autostart for Selected Installed Intel Drivers\n");
		@INSTALL_CHOICES = ( @INSTALL_CHOICES, 3);
	}
	if (scalar(@INSTALL_CHOICES) > 0) {
		return;
	}
	system "clear";
	printf ("$BRAND $VERSION Software\n\n");
	if ($allow_install) {
		printf ("   1) Install/Uninstall Software\n");
	} else {
		printf ("   1) Show Installed Software\n");
	}
	printf ("   2) Reconfigure $ComponentInfo{'eth_roce'}{'Name'} (Ethernet only)\n");
	printf ("   3) Reconfigure Driver Autostart \n");
	printf ("   4) Generate Supporting Information for Problem Report\n");
	printf ("   5) FastFabric (Host/Admin)\n");
	$max_inp=5;
	if (!$allow_install)
	{
		printf ("   6) Uninstall Software\n");
		$max_inp=6;
	}
	printf ("\n   X) Exit\n");

	while( $inp < 1 || $inp > $max_inp) {
		$inp = getch();

		if ($inp =~ /[qQ]/ || $inp =~ /[Xx]/ ) {
			return 1;
		}
		if ($inp =~ /[0123456789abcdefABCDEF]/)
		{
			$inp = hex($inp);
		}
	}
	@INSTALL_CHOICES = ( $inp );
	return 0;
}

determine_os_version;
init_components;

# when this is used as main for a component specific INSTALL
# the component can provide some overrides of global settings such as Components
overrides;

process_args;
check_root_user;
if ( $Default_ShowCompInfo )
{
        ShowCompInfo();
        exit(0);
}
if ( ! $Default_Build ) {
	open_log("");
} else {
	open_log("./build.log");
}

if ( ! $Default_Build ) {
	verify_modtools;
	if ($allow_install) {
		verify_distrib_files;
	}
}

set_libdir;
init_delta_info($CUR_OS_VER);

do{
	if ($Default_Build) {
		$exit_code = build_all_components($Build_OsVer, $Build_Debug, $Build_Temp, $Build_Force);
		done();
	} else {
		if ( show_menu != 0) {
		done();
		}
	}

	foreach my $INSTALL_CHOICE ( @INSTALL_CHOICES )
	{
		if ($allow_install && $INSTALL_CHOICE == 1)
		{
			select_debug_release(".");
			show_install_menu(1);
			if ($Default_Prompt) {
				if ($exit_code == 0) {
					print "Done Installing $BRAND Software.\n"
				} else {
					print "Failed to install all $BRAND software.\n"
				}
			}
		}
		elsif ($INSTALL_CHOICE == 1) {
			show_installed(1);
		}
		elsif ($INSTALL_CHOICE == 6)
		{
			show_uninstall_menu(1);
			if ( $Default_Prompt ) {
				if ($exit_code == 0) {
					print "Done Uninstalling $BRAND Software.\n"
				} else {
					print "Failed to uninstall all $BRAND Software.\n"
				}
			}
		}
		elsif ($INSTALL_CHOICE == 2)
		{
			if (GetYesNo("Eth RoCE is for Ethernet only, are you sure you want to continue?", "y") == 1) {
				my $selected_comp = 'eth_roce';
				if ( $Default_SameAutostart ) {
					NormalPrint "Leaving configuration on $ComponentInfo{$selected_comp}{'Name'} at its previous value.\n";
				} else {
					my $prereqs_installed = 1;
					foreach my $comp ( @Components ) {
						if (comp_has_prereq_of($selected_comp, $comp) && ! comp_is_installed("$comp")) {
							$prereqs_installed = 0;
							last;
						}
					}
					if ($prereqs_installed == 1) {
						config_roce("y");
						config_lmtsel("$DEFAULT_LIMITS_SEL");
						Config_ifcfg();
						if (check_need_reboot()) {
							HitKeyCont;
						}
					} else {
						NormalPrint "Please install pre-required component(s) $ComponentInfo{$selected_comp}{'PreReq'} first.\n";
						HitKeyCont;
					}
				}
			}
		}
		elsif ($INSTALL_CHOICE == 3)
		{
			reconfig_autostart;
			if ( $Default_Prompt ) {
				print "Done Driver Autostart Configuration.\n"
			}
		}
		elsif ($INSTALL_CHOICE == 4)
		{
			# Generate Supporting Information for Problem Report
			capture_report($ComponentInfo{'eth_tools'}{'Name'});
		}
		elsif ($INSTALL_CHOICE == 5)
		{
			# FastFabric (Host/Switch Setup/Admin)
			run_fastfabric($ComponentInfo{'fastfabric'}{'Name'});
		}
	}
}while( !$Default_Prompt );
done();
sub done() {
	if ( not $user_space_only ) {
		do_rebuild_ramdisk;
	}
	close_log;
	exit $exit_code;
}
