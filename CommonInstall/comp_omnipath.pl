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

#[ICS VERSION STRING: unknown]
use strict;
use File::Basename;

#############################################################################
##
##    MPI installation generic functions

# these functions can be used by the MPI specific install functions

# installs PSM based MPI component.  also handles reinstall on top of
# existing installation and upgrade.
sub install_generic_mpi
{
    my $install_list = $_[0];
    my $installing_list = $_[1];
    my $mpiname = $_[2];
    my $compiler = $_[3];
    my $suffix = $_[4];
    my $mpifullname = "$mpiname"."_$compiler";
    if ( "$suffix" ne "") {
        $mpifullname = "$mpifullname" . "_" . "$suffix";
    }

    if (!exists($ComponentInfo{$mpifullname})) {
        NormalPrint "ERROR: no component $mpifullname!\n";
        exit 1;
    }

    my $srcdir = $ComponentInfo{$mpifullname}{'SrcDir'};
    my $version = eval "media_version_$mpifullname()";

    printf ("Installing $ComponentInfo{$mpifullname}{'Name'} $version...\n");
    LogPrint ("Installing $ComponentInfo{$mpifullname}{'Name'} $version for $CUR_OS_VER\n");
    # make sure any old potentially custom built versions of mpi are uninstalled
    uninstall_comp_rpms($mpifullname, ' --nodeps ', $install_list, $installing_list, 'silent');
    # cleanup from older installs just in case
    system ("rm -rf /usr/lib/eth-tools/.comp_$mpifullname.pl");

    my $rpmfile = rpm_resolve("$srcdir/$mpifullname", "any");
    if ( "$rpmfile" ne "" && -e "$rpmfile" ) {
        my $mpich_prefix= "/usr/mpi/$compiler/$mpiname-"
            . rpm_query_attr($rpmfile, "VERSION");
        if ( "$suffix" ne "") {
            $mpich_prefix= "$mpich_prefix" . "-" . "$suffix";
        }
        if ( -d "$mpich_prefix" ) {
            if (GetYesNo ("Remove $mpich_prefix directory?", "y")) {
                LogPrint "rm -rf $mpich_prefix\n";
                system("rm -rf $mpich_prefix");
            }
        }
    }
    install_comp_rpms($mpifullname, " -U --nodeps ", $install_list);

    $ComponentWasInstalled{$mpifullname} = 1;
}

sub installed_generic_mpi
{
    my $mpiname = $_[0];
    my $compiler = $_[1];
    my $suffix = $_[2];
    my $mpifullname = "$mpiname"."_$compiler";
    if ( "$suffix" ne "") {
        $mpifullname = "$mpifullname" . "_" . "$suffix";
    }

    return (rpm_is_installed ($mpifullname, "user") );
}

sub uninstall_generic_mpi
{
    my $install_list = $_[0];
    my $installing_list = $_[1];
    my $mpiname = $_[2];
    my $compiler = $_[3];
    my $suffix = $_[4];
    my $mpifullname = "$mpiname"."_$compiler";
    if ( "$suffix" ne "") {
        $mpifullname = "$mpifullname" . "_" . "$suffix";
    }

    if (!exists($ComponentInfo{$mpifullname})) {
        NormalPrint "ERROR: no component $mpifullname!\n";
        exit 1;
    }

    my $mpich_prefix= "/usr/mpi/$compiler/$mpiname-"
        . rpm_query_attr_pkg("$mpifullname", "VERSION");
    if ( "$suffix" ne "") {
        $mpich_prefix= "$mpich_prefix" . "-" . "$suffix";
    }
    my $rc;
    my $top;
    
    NormalPrint ("Uninstalling $ComponentInfo{$mpifullname}{'Name'}...\n");
	$top = rpm_query_attr_pkg("$mpifullname", "INSTALLPREFIX");
    if ($top eq "" || $top =~ /is not installed/) {
		$top = undef;
    } else {
		$top = `dirname $top`;
		chomp $top;
    }
    
    # uninstall tests in case built by do_build
    uninstall_comp_rpms($mpifullname, '', $install_list, $installing_list, 'verbose');

	# unfortunately mpi and mpitests can leave empty directories on uninstall
	# this can confuse IFS MPI tools because correct MPI to use
	# cannot be identified.  This remove such empty directories
	if ( -d "/$mpich_prefix" ) {
		system("cd '/$mpich_prefix'; rmdir -p tests/* >/dev/null 2>&1");
	}
    if ( -d $top ) {
		my @files = glob("$top/*");
		my $num = scalar (@files);
		if ( $num == 0 ) {
			system ("rm -rf $top");
		}
    }
    
	# cleanup from older installs just in case
    system ("rm -rf /usr/lib/eth-tools/.comp_$mpifullname.pl");
    system ("rmdir /usr/lib/eth-tools 2>/dev/null"); # remove only if empty
    $ComponentWasInstalled{$mpifullname} = 0;
}

#############################################################################
##
##    OpenMPI GCC Verbs

# is component X available on the install media (use of this
# allows for optional components in packaging or limited availability if a
# component isn't available on some OS/CPU combos)
sub get_rpms_dir_openmpi_gcc
{
    return $ComponentInfo{'openmpi_gcc'}{'SrcDir'};
}

sub available_openmpi_gcc
{
    my $srcdir = $ComponentInfo{'openmpi_gcc'}{'SrcDir'};
    return rpm_exists ("$srcdir/openmpi_gcc", "user");
}

# is component X presently installed on the system.  This is
# a quick check, not a "verify"
sub installed_openmpi_gcc
{
    return  installed_generic_mpi("openmpi", "gcc", "");
}

# what is the version installed on system.  Only
# called if installed_X is true.  versions are short strings displayed and
# logged, no operations are done (eg. only compare for equality)
sub installed_version_openmpi_gcc
{
    return rpm_query_version_release_pkg ("openmpi_gcc");
}

# only called if available_X.  Indicates version on
# media.  Will be compared with installed_version_X to determine if
# present installation is up to date.  Should return exact same format for
# version string so comparison of equality is possible.
sub media_version_openmpi_gcc
{
    my $srcdir = $ComponentInfo{'openmpi_gcc'}{'SrcDir'};
    my $rpm = rpm_resolve ("$srcdir/openmpi_gcc", "user");
    return rpm_query_version_release ($rpm);
}

# used to build/rebuild component on local system (if
# supported).  We support this for many items in comp_ofed.pl
# Other components (like SM) are
# not available in source and hence do not support this and simply
# implement a noop.
sub build_openmpi_gcc
{
    my $osver = $_[0];
    my $debug = $_[1];
    my $build_temp = $_[2];
    my $force = $_[3];

    return 0;
}

# does this need to be reinstalled.  Mainly used for
# ofed due to subtle changes such as install prefix or kernel options
# which may force a reinstall.  You'll find this is a noop in most others.
sub need_reinstall_openmpi_gcc
{
    my $install_list = shift ();
    my $installing_list = shift ();

    return "no";
}

# called for all components before they are installed.  Use to verify OS
# has proper dependent rpms installed.
sub check_os_prereqs_openmpi_gcc
{
	return rpm_check_os_prereqs("openmpi_gcc", "user");
}

# called for all components before they are installed.  Use
# to build things if needed, etc.
sub preinstall_openmpi_gcc
{
    my $install_list = $_[0];
    my $installing_list = $_[1];

    my $full = "";
    my $rc;

    return 0;
}

# installs component.  also handles reinstall on top of
# existing installation and upgrade.
sub install_openmpi_gcc
{
    install_generic_mpi("$_[0]", "$_[1]", "openmpi", "gcc", "");
}

# called after all components are installed.
sub postinstall_openmpi_gcc
{
    my $install_list = $_[0];     # total that will be installed when done
    my $installing_list = $_[1];  # what items are being installed/reinstalled
}

# uninstalls component.  May be called even if component is
# partially or not installed at all in which case should do its best to
# get rid or what might remain of component from a previously aborted
# uninstall or failed install
sub uninstall_openmpi_gcc
{
    uninstall_generic_mpi("$_[0]", "$_[1]", "openmpi", "gcc", "");
}

#############################################################################
##
##    OpenMPI GCC PSM

# is component X available on the install media (use of this
# allows for optional components in packaging or limited availability if a
# component isn't available on some OS/CPU combos)

sub get_rpms_dir_openmpi_gcc_ofi
{
    return $ComponentInfo{'openmpi_gcc_ofi'}{'SrcDir'};
}

sub available_openmpi_gcc_ofi
{
    my $srcdir = $ComponentInfo{'openmpi_gcc_ofi'}{'SrcDir'};
    return rpm_exists ("$srcdir/openmpi_gcc_ofi", "user");
}

# is component X presently installed on the system.  This is
# a quick check, not a "verify"
sub installed_openmpi_gcc_ofi
{
    return  installed_generic_mpi("openmpi", "gcc", "ofi");
}

# what is the version installed on system.  Only
# called if installed_X is true.  versions are short strings displayed and
# logged, no operations are done (eg. only compare for equality)
sub installed_version_openmpi_gcc_ofi
{
    return rpm_query_version_release_pkg ("openmpi_gcc_ofi");
}

# only called if available_X.  Indicates version on
# media.  Will be compared with installed_version_X to determine if
# present installation is up to date.  Should return exact same format for
# version string so comparison of equality is possible.
sub media_version_openmpi_gcc_ofi
{
    my $srcdir = $ComponentInfo{'openmpi_gcc_ofi'}{'SrcDir'};
    my $rpm = rpm_resolve ("$srcdir/openmpi_gcc_ofi", "user");
    return rpm_query_version_release ($rpm);
}

# used to build/rebuild component on local system (if
# supported).  We support this for many items in comp_ofed.pl
# Other components (like SM) are
# not available in source and hence do not support this and simply
# implement a noop.
sub build_openmpi_gcc_ofi
{
    my $osver = $_[0];
    my $debug = $_[1];
    my $build_temp = $_[2];
    my $force = $_[3];

    return 0;
}

# does this need to be reinstalled.  Mainly used for
# ofed due to subtle changes such as install prefix or kernel options
# which may force a reinstall.  You'll find this is a noop in most others.
sub need_reinstall_openmpi_gcc_ofi
{
    my $install_list = shift ();
    my $installing_list = shift ();

    return "no";
}

# called for all components before they are installed.  Use to verify OS
# has proper dependent rpms installed.
sub check_os_prereqs_openmpi_gcc_ofi
{
	return rpm_check_os_prereqs("openmpi_gcc_ofi", "user");
}

# called for all components before they are installed.  Use
# to build things if needed, etc.
sub preinstall_openmpi_gcc_ofi
{
    my $install_list = $_[0];
    my $installing_list = $_[1];

    my $full = "";
    my $rc;

    return 0;
}

# installs component.  also handles reinstall on top of
# existing installation and upgrade.
sub install_openmpi_gcc_ofi
{
    install_generic_mpi("$_[0]", "$_[1]", "openmpi", "gcc", "ofi");
}

# called after all components are installed.
sub postinstall_openmpi_gcc_ofi
{
    my $install_list = $_[0];     # total that will be installed when done
    my $installing_list = $_[1];  # what items are being installed/reinstalled
}

# uninstalls component.  May be called even if component is
# partially or not installed at all in which case should do its best to
# get rid or what might remain of component from a previously aborted
# uninstall or failed install
sub uninstall_openmpi_gcc_ofi
{
    uninstall_generic_mpi("$_[0]", "$_[1]", "openmpi", "gcc", "ofi");
}

#############################################################################
##
##    OpenMPI Intel PSM

# is component X available on the install media (use of this
# allows for optional components in packaging or limited availability if a
# component isn't available on some OS/CPU combos)

sub get_rpms_dir_openmpi_intel_ofi
{
    return $ComponentInfo{'openmpi_intel_ofi'}{'SrcDir'};
}

sub available_openmpi_intel_ofi
{
    my $srcdir = $ComponentInfo{'openmpi_intel_ofi'}{'SrcDir'};
    return rpm_exists ("$srcdir/openmpi_intel_ofi", "user");
}

# is component X presently installed on the system.  This is
# a quick check, not a "verify"
sub installed_openmpi_intel_ofi
{
    return  installed_generic_mpi("openmpi", "intel", "ofi");
}

# what is the version installed on system.  Only
# called if installed_X is true.  versions are short strings displayed and
# logged, no operations are done (eg. only compare for equality)
sub installed_version_openmpi_intel_ofi
{
    return rpm_query_version_release_pkg ("openmpi_intel_ofi");
}

# only called if available_X.  Indicates version on
# media.  Will be compared with installed_version_X to determine if
# present installation is up to date.  Should return exact same format for
# version string so comparison of equality is possible.
sub media_version_openmpi_intel_ofi
{
    my $srcdir = $ComponentInfo{'openmpi_intel_ofi'}{'SrcDir'};
    my $rpm = rpm_resolve ("$srcdir/openmpi_intel_ofi", "user");
    return rpm_query_version_release ($rpm);
}

# used to build/rebuild component on local system (if
# supported).  We support this for many items in comp_ofed.pl
# Other components (like SM) are
# not available in source and hence do not support this and simply
# implement a noop.
sub build_openmpi_intel_ofi
{
    my $osver = $_[0];
    my $debug = $_[1];
    my $build_temp = $_[2];
    my $force = $_[3];

    return 0;
}

# does this need to be reinstalled.  Mainly used for
# ofed due to subtle changes such as install prefix or kernel options
# which may force a reinstall.  You'll find this is a noop in most others.
sub need_reinstall_openmpi_intel_ofi
{
    my $install_list = shift ();
    my $installing_list = shift ();

    return "no";
}

# called for all components before they are installed.  Use to verify OS
# has proper dependent rpms installed.
sub check_os_prereqs_openmpi_intel_ofi
{
	return rpm_check_os_prereqs("openmpi_intel_ofi", "user");
}

# called for all components before they are installed.  Use
# to build things if needed, etc.
sub preinstall_openmpi_intel_ofi
{
    my $install_list = $_[0];
    my $installing_list = $_[1];

    my $full = "";
    my $rc;

    return 0;
}

# installs component.  also handles reinstall on top of
# existing installation and upgrade.
sub install_openmpi_intel_ofi
{
    install_generic_mpi("$_[0]", "$_[1]", "openmpi", "intel", "ofi");
}

# called after all components are installed.
sub postinstall_openmpi_intel_ofi
{
    my $install_list = $_[0];     # total that will be installed when done
    my $installing_list = $_[1];  # what items are being installed/reinstalled
}

# uninstalls component.  May be called even if component is
# partially or not installed at all in which case should do its best to
# get rid or what might remain of component from a previously aborted
# uninstall or failed install
sub uninstall_openmpi_intel_ofi
{
    uninstall_generic_mpi("$_[0]", "$_[1]", "openmpi", "intel", "ofi");
}

#############################################################################
##
##    OpenMPI PGI PSM

# is component X available on the install media (use of this
# allows for optional components in packaging or limited availability if a
# component isn't available on some OS/CPU combos)

sub get_rpms_dir_openmpi_pgi_ofi
{
    return $ComponentInfo{'openmpi_pgi_ofi'}{'SrcDir'};
}

sub available_openmpi_pgi_ofi
{
    my $srcdir = $ComponentInfo{'openmpi_pgi_ofi'}{'SrcDir'};
    return rpm_exists ("$srcdir/openmpi_pgi_ofi", "user");
}

# is component X presently installed on the system.  This is
# a quick check, not a "verify"
sub installed_openmpi_pgi_ofi
{
    return  installed_generic_mpi("openmpi", "pgi", "ofi");
}

# what is the version installed on system.  Only
# called if installed_X is true.  versions are short strings displayed and
# logged, no operations are done (eg. only compare for equality)
sub installed_version_openmpi_pgi_ofi
{
    return rpm_query_version_release_pkg ("openmpi_pgi_ofi");
}

# only called if available_X.  Indicates version on
# media.  Will be compared with installed_version_X to determine if
# present installation is up to date.  Should return exact same format for
# version string so comparison of equality is possible.
sub media_version_openmpi_pgi_ofi
{
    my $srcdir = $ComponentInfo{'openmpi_pgi_ofi'}{'SrcDir'};
    my $rpm = rpm_resolve ("$srcdir/openmpi_pgi_ofi", "user");
    return rpm_query_version_release ($rpm);
}

# used to build/rebuild component on local system (if
# supported).  We support this for many items in comp_ofed.pl
# Other components (like SM) are
# not available in source and hence do not support this and simply
# implement a noop.
sub build_openmpi_pgi_ofi
{
    my $osver = $_[0];
    my $debug = $_[1];
    my $build_temp = $_[2];
    my $force = $_[3];

    return 0;
}

# does this need to be reinstalled.  Mainly used for
# ofed due to subtle changes such as install prefix or kernel options
# which may force a reinstall.  You'll find this is a noop in most others.
sub need_reinstall_openmpi_pgi_ofi
{
    my $install_list = shift ();
    my $installing_list = shift ();

    return "no";
}

# called for all components before they are installed.  Use to verify OS
# has proper dependent rpms installed.
sub check_os_prereqs_openmpi_pgi_ofi
{
	return rpm_check_os_prereqs("openmpi_pgi_ofi", "user");
}

# called for all components before they are installed.  Use
# to build things if needed, etc.
sub preinstall_openmpi_pgi_ofi
{
    my $install_list = $_[0];
    my $installing_list = $_[1];

    my $full = "";
    my $rc;

    return 0;
}

# installs component.  also handles reinstall on top of
# existing installation and upgrade.
sub install_openmpi_pgi_ofi
{
	install_generic_mpi("$_[0]", "$_[1]", "openmpi", "pgi", "ofi");
}

# called after all components are installed.
sub postinstall_openmpi_pgi_ofi
{
    my $install_list = $_[0];     # total that will be installed when done
    my $installing_list = $_[1];  # what items are being installed/reinstalled
}

# uninstalls component.  May be called even if component is
# partially or not installed at all in which case should do its best to
# get rid or what might remain of component from a previously aborted
# uninstall or failed install
sub uninstall_openmpi_pgi_ofi
{
    uninstall_generic_mpi("$_[0]", "$_[1]", "openmpi", "pgi", "ofi");
}

#############################################################################
###
###    OpenMPI GCC CUDA
#

# is component X available on the install media (use of this
# allows for optional components in packaging or limited availability if a
# component isn't available on some OS/CPU combos)

sub get_rpms_dir_openmpi_gcc_cuda_ofi
{
    return $ComponentInfo{'openmpi_gcc_cuda_ofi'}{'SrcDir'};
}

sub available_openmpi_gcc_cuda_ofi
{
    my $srcdir = $ComponentInfo{'openmpi_gcc_cuda_ofi'}{'SrcDir'};
    return rpm_exists ("$srcdir/openmpi_gcc_cuda_ofi", "user");
}

# is component X presently installed on the system.  This is
# a quick check, not a "verify"
sub installed_openmpi_gcc_cuda_ofi
{
    return  installed_generic_mpi("openmpi", "gcc_cuda", "ofi");
}

# what is the version installed on system.  Only
# called if installed_X is true.  versions are short strings displayed and
# logged, no operations are done (eg. only compare for equality)
sub installed_version_openmpi_gcc_cuda_ofi
{
    return rpm_query_version_release_pkg ("openmpi_gcc_cuda_ofi");
}

# only called if available_X.  Indicates version on
# media.  Will be compared with installed_version_X to determine if
# present installation is up to date.  Should return exact same format for
# version string so comparison of equality is possible.
sub media_version_openmpi_gcc_cuda_ofi
{
    my $srcdir = $ComponentInfo{'openmpi_gcc_cuda_ofi'}{'SrcDir'};
    my $rpm = rpm_resolve ("$srcdir/openmpi_gcc_cuda_ofi", "user");
    return rpm_query_version_release ($rpm);
}

# used to build/rebuild component on local system (if
# supported).  We support this for many items in comp_ofed.pl
# Other components (like SM) are
# not available in source and hence do not support this and simply
# implement a noop.
sub build_openmpi_gcc_cuda_ofi
{
    my $osver = $_[0];
    my $debug = $_[1];
    my $build_temp = $_[2];
    my $force = $_[3];

    return 0;
}

# does this need to be reinstalled.  Mainly used for
# ofed due to subtle changes such as install prefix or kernel options
# which may force a reinstall.  You'll find this is a noop in most others.
sub need_reinstall_openmpi_gcc_cuda_ofi
{
    my $install_list = shift ();
    my $installing_list = shift ();

    return "no";
}

# called for all components before they are installed.  Use to verify OS
# has proper dependent rpms installed.
sub check_os_prereqs_openmpi_gcc_cuda_ofi
{
       return rpm_check_os_prereqs("openmpi_gcc_cuda_ofi", "user");
}

# called for all components before they are installed.  Use
# to build things if needed, etc.
sub preinstall_openmpi_gcc_cuda_ofi
{
    my $install_list = $_[0];
    my $installing_list = $_[1];

    my $full = "";
    my $rc;

    return 0;
}

# installs component.  also handles reinstall on top of
# existing installation and upgrade.
sub install_openmpi_gcc_cuda_ofi
{
    install_generic_mpi("$_[0]", "$_[1]", "openmpi", "gcc_cuda", "ofi");
}

# called after all components are installed.
sub postinstall_openmpi_gcc_cuda_ofi
{
    my $install_list = $_[0];     # total that will be installed when done
    my $installing_list = $_[1];  # what items are being installed/reinstalled
}

# uninstalls component.  May be called even if component is
# partially or not installed at all in which case should do its best to
# get rid or what might remain of component from a previously aborted
# uninstall or failed install
sub uninstall_openmpi_gcc_cuda_ofi
{
    uninstall_generic_mpi("$_[0]", "$_[1]", "openmpi", "gcc_cuda", "ofi");
}

#############################################################################
###
###    MPI Source

sub available_mpisrc()
{
	my $srcdir=$ComponentInfo{'mpisrc'}{'SrcDir'};
	return has_mpisrc($srcdir);
}

sub installed_mpisrc()
{
	my $srcdir = $ExtraMpisrcInfo{'Dest'};
	my $old_srcdir = "/usr/src/eth/MPI";
	return (has_mpisrc($srcdir) || has_mpisrc($old_srcdir));
}

sub has_mpisrc($)
{
	my $srcdir = shift();
	foreach my $srpm (@{$ExtraMpisrcInfo{'SrcRpms'}}) {
		if (file_glob("$srcdir/${srpm}*.src.rpm") eq "") {
			return 0;
		}
	}
	return 1;
}

# only called if installed_mpisrc is true
sub installed_version_mpisrc()
{
	return `cat $ExtraMpisrcInfo{'Dest'}/.version`;
}

# only called if available_mpisrc is true
sub media_version_mpisrc()
{
	my $srcdir=$ComponentInfo{'mpisrc'}{'SrcDir'};
	return `cat "$srcdir/version"`;
}

sub build_mpisrc($$$$)
{
	my $osver = shift();
	my $debug = shift();	# enable extra debug of build itself
	my $build_temp = shift();	# temp area for use by build
	my $force = shift();	# force a rebuild
	return 0;	# success
}

sub need_reinstall_mpisrc($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

    return "no";
}

sub check_os_prereqs_mpisrc
{
	return rpm_check_os_prereqs("mpisrc", "any");
}

sub preinstall_mpisrc($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

    return 0;
}

sub install_mpisrc($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled

	my $srcdir=$ComponentInfo{'mpisrc'}{'SrcDir'};
	my $version = media_version_mpisrc();
	chomp $version;

	printf ("Installing $ComponentInfo{'mpisrc'}{'Name'} $version...\n");
	LogPrint ("Installing $ComponentInfo{'mpisrc'}{'Name'} $version for $CUR_OS_VER\n");

	my $destdir = $ExtraMpisrcInfo{'Dest'};
	check_dir($destdir);
	# remove old versions (.src.rpm and built .rpm files too)
	system "rm -f $destdir/mvapich[-_]*.rpm 2>/dev/null";
	foreach my $srpm (@{$ExtraMpisrcInfo{'SrcRpms'}})
	{
		system "rm -f $destdir/$srpm-*.rpm 2>/dev/null";
	}
	foreach my $file (@{$ExtraMpisrcInfo{'DirtyFiles'}})
	{
		system "rm -f $destdir/$file 2>/dev/null";
	}

	# install new versions
	foreach my $srpm (@{$ExtraMpisrcInfo{'SrcRpms'}}) {
		my $srpmfile = file_glob("$srcdir/${srpm}-*.src.rpm");
		if ( "$srpmfile" ne "" ) {
			my $file = my_basename($srpmfile);
			copy_data_file($srpmfile, "$destdir/$file");
		}
	}
	foreach my $script (@{$ExtraMpisrcInfo{'BuildScripts'}}) {
		copy_systool_file("$srcdir/$script", "$destdir/$script");
	}
	foreach my $file (@{$ExtraMpisrcInfo{'MiscFiles'}}) {
		my $src = ${$file}{'Src'};
		my $dest = ${$file}{'Dest'};
		copy_data_file("$srcdir/$src", "$destdir/$dest");
	}

	$ComponentWasInstalled{'mpisrc'}=1;
}

sub postinstall_mpisrc($$)
{
	my $install_list = shift();	# total that will be installed when done
	my $installing_list = shift();	# what items are being installed/reinstalled
}

sub uninstall_mpisrc($$)
{
	my $install_list = shift();	# total that will be left installed when done
	my $uninstalling_list = shift();	# what items are being uninstalled

	NormalPrint ("Uninstalling $ComponentInfo{'mpisrc'}{'Name'}...\n");

	# try to uninstall meta pkg if it exists
	if (rpm_is_installed("ethmeta_mpisrc", "any") ||
	    rpm_is_installed("ethmeta_mpisrc_userspace", "any")) {
		rpm_uninstall_matches("ethmeta_mpisrc", "ethmeta_mpisrc", "", "");
	} else {
		my $destdir = $ExtraMpisrcInfo{'Dest'};
		# remove old versions (.src.rpm and built .rpm files too)
		foreach my $srpm (@{$ExtraMpisrcInfo{'SrcRpms'}}) {
			system "rm -f $destdir/$srpm-*.rpm 2>/dev/null";
		}
		foreach my $script (@{$ExtraMpisrcInfo{'BuildScripts'}}) {
			system "rm -f $destdir/$script 2>/dev/null";
		}
		foreach my $file (@{$ExtraMpisrcInfo{'MiscFiles'}}) {
			my $destfile = ${$file}{'Dest'};
			system "rm -f $destdir/$destfile 2>/dev/null";
		}
		foreach my $file (@{$ExtraMpisrcInfo{'DirtyFiles'}}) {
			system "rm -f $destdir/$file 2>/dev/null";
		}

		system "rmdir $destdir 2>/dev/null"; # remove only if empty
		system "rmdir /usr/src/eth 2>/dev/null"; # remove only if empty
	}

	$ComponentWasInstalled{'mpisrc'}=0;
}
