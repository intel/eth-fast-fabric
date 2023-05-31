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

# =============================================================================
# The functions and constants below assist in editing modules.conf or
# modprobe.conf to add IB specific entries

my $MODULE_CONF_FILE = "/etc/modules.conf";
my $MODULE_CONF_DIST_FILE;
my $DRACUT_EXE_FILE = "/usr/bin/dracut";
if (substr($CUR_OS_VER,0,3) eq "2.6")
{
	$MODULE_CONF_FILE = "/etc/modprobe.conf";
	$MODULE_CONF_DIST_FILE = "/etc/modprobe.conf.dist";
	$DRACUT_EXE_FILE = "/sbin/dracut";
}
if (-f "$(MODULE_CONF_FILE).local")
{
	$MODULE_CONF_FILE = "$(MODULE_CONF_FILE).local";
}
my $ETH_CONF="modules.conf";	## additions to modules.conf

# marker strings used in MODULES_CONF_FILE
# for entries added by installation
my $START_DRIVER_MARKER="Eth Drivers Start here";
my $END_DRIVER_MARKER="Eth Drivers End here";

# Keep track of whether we already did edits to avoid repeated edits
my $DidConfig=0;

# This code is from OFED, it removes lines related to IB from
# modprobe.conf.  Used to prevent distro specific effects on OFED.
sub disable_distro_ofed()
{
	if ( "$MODULE_CONF_DIST_FILE" ne "" && -f "$MODULE_CONF_DIST_FILE" ) {
		my $res;

		$res = open(MDIST, "$MODULE_CONF_DIST_FILE");
		if ( ! $res ) {
			NormalPrint("Can't open $MODULE_CONF_DIST_FILE for input: $!");
			return;
		}
		my @mdist_lines;
		while (<MDIST>) {
			push @mdist_lines, $_;
		}
		close(MDIST);

		$res = open(MDIST, ">$MODULE_CONF_DIST_FILE");
		if ( ! $res ) {
			NormalPrint("Can't open $MODULE_CONF_DIST_FILE for output: $!");
			return;
		}
		foreach my $line (@mdist_lines) {
			chomp $line;
			if ($line =~ /^\s*install ib_core|^\s*alias ib|^\s*alias net-pf-26 ib_sdp/) {
				print MDIST "# $line\n";
			} else {
				print MDIST "$line\n";
			}
		}
		close(MDIST);
	}
}

# =============================================================================
# The functions and constants below assist in editing limits.conf
# to add IB specific entries related to memory locking

my $LIMITS_CONF_FILE = "/etc/security/limits.conf";
my $LIMITS_CONF="limits.conf";	## additions to limits.conf

# marker strings used in LIMITS_CONF_FILE
# for entries added by installation
my $START_LIMITS_MARKER="Eth Settings Start here";
my $END_LIMITS_MARKER="Eth Settings End here";

# Keep track of whether we already did edits to avoid repeated edits
my $DidLimits=0;

# remove iba entries from modules.conf
sub remove_limits_conf()
{
	$DidLimits = 0;
	if ( -e "$LIMITS_CONF_FILE") {
		if (check_keep_config($LIMITS_CONF_FILE, "", "y"))
		{
			print "Keeping /$LIMITS_CONF_FILE changes ...\n";
		} else {
			print "Modifying $LIMITS_CONF_FILE ...\n";
			if ( -e "$ETH_SYSTEMCFG_FILE" ) {
			     system("$ETH_SYSTEMCFG_FILE --disable Memory_Limit");
			}
		}
	}
}

#
# Override the system's standard udev configuration to allow
# different access rights to some of the infiniband device files.
#
my $UDEV_RULES_DIR ="/etc/udev/rules.d";
my $UDEV_RULES_FILE = "05-opa.rules";
my $Default_UserQueries = 0;

my $udev_perm_string = "Allow non-root users to access the UMAD interface?";

#AddAnswerHelp("UserQueries", "$udev_perm_string");

sub install_udev_permissions($)
{
	my ($srcdir) = shift(); # source directory.
	my $SourceFile;
	my $Context;
	my $Cnt;

	if ($Default_UserQueries == 0) {
		$Default_UserQueries = GetYesNoWithMemory("UserQueries",0,"$udev_perm_string", "y");
	}

	if ($Default_UserQueries > 0) {
                # Installation of udev will be taken care during RPM installation, we just have to set
                setup_env("ETH_UDEV_RULES", 1);
	} elsif ( -e "$UDEV_RULES_DIR/$UDEV_RULES_FILE" ) {
		#update environment variable accordingly
		setup_env("ETH_UDEV_RULES", 0);
	} else {
		# do nothing
		setup_env("ETH_UDEV_RULES", -1);
	}
}

sub remove_udev_permissions()
{
	remove_file("$UDEV_RULES_DIR/$UDEV_RULES_FILE");
}

my $ARPTBL_TUNING_NAME = "ARPTABLE_TUNING";
my $ARPTBL_TUNING_DESC = 'Adjust kernel ARP table size for large fabrics';
AddAnswerHelp("$ARPTBL_TUNING_NAME", "$ARPTBL_TUNING_DESC");

sub config_arptbl_tunning()
{
	prompt_iefs_conf_param("$ARPTBL_TUNING_NAME", "$ARPTBL_TUNING_DESC", "y", 'ETH_ARPTABLE_TUNING');
}

my $ROCE_NAME="ROCE_ON";
my $ROCE_DESC="RoCE RDMA transport";
AddAnswerHelp("$ROCE_NAME", "$ROCE_DESC");

sub config_roce($)
{
	my $default = shift();
	if (! -e "$ETH_SYSTEMCFG_FILE")
	{
		NormalPrint("Couldn't find file $ETH_SYSTEMCFG_FILE\n");
		return;
	}

	my $status = `$ETH_SYSTEMCFG_FILE --status RoCE | tr -d '\n'`;
	prompt_iefs_conf_param("$ROCE_NAME", "$ROCE_DESC", "$default", 'ETH_ROCE_ON');
	if ("$ENV{'ETH_ROCE_ON'}" eq "1") {
		if ( "$status" ne "RoCE [ENABLED]" ) {
			system "$ETH_SYSTEMCFG_FILE --enable RoCE";
		}
	} else {
		if ( "$status" ne "RoCE [DISABLED]" ) {
			system "$ETH_SYSTEMCFG_FILE --disable RoCE";
		}
	}
}

my $LMTSEL_NAME="LIMITS_SEL";
my $LMTSEL_DESC="Resource Limits Selector";
AddAnswerHelp("$LMTSEL_NAME", "$LMTSEL_DESC");
my $LMTSEL_CONF="/etc/modprobe.d/irdma.conf";
my $LMTSEL_CONF_BAK="/etc/modprobe.d/irdma.conf.iefsbak";
my $LMTSEL_STR="options irdma limits_sel";

sub config_lmtsel($)
{
	my $default = shift();
	if ( exists $AnswerMemory{"$LMTSEL_NAME"}) {
		$default = $AnswerMemory{"$LMTSEL_NAME"};
	}

	my $sel = GetNumericValue("$LMTSEL_DESC (0-7)", $default, 0, 7);

	if ( -e $LMTSEL_CONF) {
		system "/bin/cp -f $LMTSEL_CONF $LMTSEL_CONF_BAK";
		my $cur_val = `grep '^\\s*options\\s\\+irdma\\s\\+limits_sel\\s*=.*\$' $LMTSEL_CONF | tail -n 1 | cut -d '=' -f 2`;
		chomp $cur_val;
		if ( "$cur_val" ne "" ) {
			if ( "$cur_val" ne "$sel") {
				system "sed -i 's/^\\s*options\\s\\+irdma\\s\\+limits_sel\\s*=.*\$/$LMTSEL_STR=$sel/g' $LMTSEL_CONF";
			}
			return;
		}
	}
	system "echo \"$LMTSEL_STR=$sel\" >> $LMTSEL_CONF";
}

sub restore_lmtsel()
{
	if (GetYesNo("Restore $LMTSEL_CONF?", "y") == 1) {
		if ( -e $LMTSEL_CONF_BAK) {
			if (system "diff $LMTSEL_CONF_BAK $LMTSEL_CONF > /dev/null") {
				system "mv -f $LMTSEL_CONF_BAK $LMTSEL_CONF";
			}
		} else {
			#shouldn't happen
			NormalPrint "Couldn't find $LMTSEL_CONF_BAK. Nothing to restore.\n";
		}
	}
}

#
# Ensures IEFS drivers are incorporated in the initial ram disk.
#
my $CallDracut = 0;

sub rebuild_ramdisk()
{
	# Just increase the count. We will do the rebuild at the end of the script.
	$CallDracut++;
}

sub do_rebuild_ramdisk()
{
	if ($CallDracut && -d '/boot' && $CUR_DISTRO_VENDOR ne "ubuntu") {
		my $cmd = $DRACUT_EXE_FILE . ' --stdlog 0';
		if ( -d '/dev') {
			$cmd = $DRACUT_EXE_FILE;
		}
		#name of initramfs may vary between distros, so need to get it from lsinitrd
		my $current_initrd = `lsinitrd 2>&1 | head -n 1`;
		my ($initrd_prefix) = $current_initrd =~ m/\/boot\/(\w+)-[\w\-\.]+.*/;
		my $initrd_suffix = "";
                if ($current_initrd =~ m/\.img[':]/) {
                        $initrd_suffix = ".img";
                }
		my $tmpfile = "/tmp/$initrd_prefix-$CUR_OS_VER$initrd_suffix";

		if ( -e $cmd ) {
			do {
				NormalPrint("Rebuilding boot image with \"$cmd -f $tmpfile $CUR_OS_VER\"...\n");
				# Try to build a temporary image first as a dry-run to make sure
				# a failed run will not destroy an existing image.
				if (system("$cmd -f $tmpfile $CUR_OS_VER") == 0 && system("mv -f $tmpfile /boot/") == 0) {
					NormalPrint("New initramfs installed in /boot.\n");
					NormalPrint("done.\n");
					return;
				} else {
					NormalPrint("failed.\n");
				}
			} while(GetYesNo("Do you want to retry?", "n"));
			$exit_code = 1;
		} else {
			NormalPrint("$cmd not found, cannot update initial ram disk.");
			$exit_code = 1;
		}
	} elsif ($CallDracut && -d '/boot' && $CUR_DISTRO_VENDOR eq "ubuntu") {
		LogPrint("Skip Ramdisk Rebuild on Ubuntu")
	}
}

