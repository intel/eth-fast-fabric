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

#

use strict;
use Term::ANSIColor;
use Term::ANSIColor qw(:constants);
use File::Basename;
use Math::BigInt;

#Setup some defaults
my $KEY_ESC=27;
my $KEY_CNTL_C=3;
my $KEY_ENTER=13;

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

# TARGET_STACK
# this is patched by Makefile to be IBACCESS or OPENIB, only OPENIB is supported
my $IB_STACK_TYPE="THIS_IS_THE_IB_STACK_TYPE";

if ( "$IB_STACK_TYPE" ne "OPENIB" ) {
	die "Fatal Error: STACK_TYPE not properly set.  ethfastfabric script corrupted\n";
}

my $FF_CONF_FILE;
my $OPA_CONFIG_DIR; # opa editable config scripts
my $SYS_CONFIG_DIR; # system editable config scripts
my $BIN_DIR;
my $TOOLS_DIR;
# where to install libraries
my $LIB_DIR;
my $USRLOCALLIB_DIR;
my $OWNER;
my $GROUP;

$SYS_CONFIG_DIR="/etc";
$OPA_CONFIG_DIR = "$SYS_CONFIG_DIR/eth-tools";
$LIB_DIR = "/lib";
$USRLOCALLIB_DIR = "/usr/local/lib";
if ( "$BIN_DIR" eq "" ) {
	$BIN_DIR = "/usr/sbin";
}
if ( "$TOOLS_DIR" eq "" ) {
	$TOOLS_DIR = "/usr/lib/eth-tools";
}
$FF_CONF_FILE = "$OPA_CONFIG_DIR/ethfastfabric.conf";
$OWNER = "root";
$GROUP = "root";

my $ROOT = "/";

my $MENU_CHOICE=0;

my $Default_Prompt=0;	 # use default values at prompts, non-interactive

	# full name of component for error messages
my %ComponentName = (
				"oftools" => "Eth Tools",
				"fastfabric" => "FastFabric",
				"mpiapps" => "eth-mpi-apps",	# a package name
				"mpisrc" => "MPI Source",
				);

my @FabricSetupSteps;
	# Names of fabric setup steps
@FabricSetupSteps = ( "config", "findgood", "setupssh",
				"copyhosts", "showuname", "install",
				"configsnmp", "buildapps", "reboot", "refreshssh",
				"rebuildmpi", "ethcmdall", "copyall", "viewres"
					);

	# full name of steps for prompts
my %FabricSetupStepsName = (
				"config" => "Edit Config and Select/Edit Host File",
				"findgood" => "Verify Hosts Pingable",
				"setupssh" => "Set Up Password-Less SSH/SCP",
				"copyhosts" => "Copy /etc/hosts to All Hosts",
				"showuname" => "Show uname -a for All Hosts",
				"install" => "Install/Upgrade Intel Ethernet Software",
				"configsnmp" => "Configure SNMP",
				"buildapps" => "Build Test Apps and Copy to Hosts",
				"reboot" => "Reboot Hosts",
				"refreshssh" => "Refresh SSH Known Hosts",
				"rebuildmpi" => "Rebuild MPI Library and Tools",
				"ethcmdall" => "Run a Command on All Hosts",
				"copyall" => "Copy a File to All Hosts",
				"viewres" => "View ethhostadmin Result Files"
				);
# what to output before each menu item as a delimiter/marker
my %FabricSetupStepsMenuDelimiter = (
				"config" => "setup",
				"findgood" => "",
				"setupssh" => "",
				"copyhosts" => "",
				"showuname" => "",
				"install" => "",
				"configsnmp" => "",
				"buildapps" => "",
				"reboot" => "",
				"refreshssh" => "admin",
				"rebuildmpi" => "",
				"ethcmdall" => "",
				"copyall" => "",
				"viewres" => "review",
				);
	# Names of fabric admin steps
my @FabricAdminSteps;
# ethpingall replaced with findgood
@FabricAdminSteps = ( "config", "fabric_info", "findgood", "singlehost",
				"showallports", "rping",
				"refreshssh", "mpiperf", "health", "cabletest", "ethcaptureall",
				"ethcmdall", "viewres"
					);
	# full name of steps for prompts
my %FabricAdminStepsName = (
				"config" => "Edit Config and Select/Edit Host File",
				"fabric_info" => "Summary of Fabric Components",
				"findgood" => "Verify Hosts Are Pingable, SSHable, and Active",
				"singlehost" => "Perform Single Host Verification",
				"showallports" => "Verify Eth Fabric Status and Topology",
				"rping" => "Verify Hosts Ping via RDMA",
				"refreshssh" => "Refresh SSH Known Hosts",
				"mpiperf" => "Check MPI Performance",
				"health" => "Check Overall Fabric Health",
				"cabletest" => "Start or Stop Bit Error Rate Cable Test",
				"ethcaptureall" => "Generate All Hosts Problem Report Info",
				"ethcmdall" => "Run a Command on All Hosts",
				"viewres" => "View ethhostadmin Result Files"
				);
# what to output before each menu item as a delimiter/marker
my %FabricAdminStepsMenuDelimiter = (
				"config" => "validation",
				"fabric_info" => "",
				#"ethpingall" => "",
				"findgood" => "",
				"singlehost" => "",
				"showallports" => "",
				"rping" => "",
				"refreshssh" => "",
				"mpiperf" => "",
				"health" => "",
				"cabletest" => "",
				"ethcaptureall" => "admin",
				"ethcmdall" => "",
				"viewres" => "review",
				);
my $FabricSetupHostsFile="$OPA_CONFIG_DIR/hosts";
# HOSTS_FILE overrides default
if ( "$ENV{HOSTS_FILE}" ne "" ) {
	$FabricSetupHostsFile="$ENV{HOSTS_FILE}";
}
my $PrevFabricSetupHostsFile=$FabricSetupHostsFile;
my $FabricSetupScpFromDir=".";
my $FabricAdminHostsFile="$OPA_CONFIG_DIR/allhosts";
my $PrevFabricAdminHostsFile="$OPA_CONFIG_DIR/allhosts";
# HOSTS_FILE overrides default
if ( "$ENV{HOSTS_FILE}" ne "" ) {
	$FabricAdminHostsFile="$ENV{HOSTS_FILE}";
}
my $FabricChassisFile="$OPA_CONFIG_DIR/chassis";
# CHASSIS_FILE overrides default
if ( "$ENV{CHASSIS_FILE}" ne "" ) {
	$FabricChassisFile="$ENV{CHASSIS_FILE}";
}
my $Editor="$ENV{EDITOR}";
if ( "$Editor" eq "" ) {
	$Editor="vi";
}

sub HitKeyCont;
sub remove_whitespace($);

sub set_libdir
{
	if ( -d "$ROOT/lib64" )
	{
		$LIB_DIR = "/lib64";
		$USRLOCALLIB_DIR = "/usr/local/lib64";
	}
}

sub getch
{
	my $c;
	system("stty -echo raw");
	$c=getc(STDIN);
	system("stty echo -raw");
	print "\n";
	return $c;
}

sub open_log
{
	open(LOG_FD, ">>$ROOT/var/log/iefs.log");
	print LOG_FD "-------------------------------------------------------------------------------\n";
	print LOG_FD basename($0) . " $INT_VERSION Run " . `/bin/date`;
	print LOG_FD "$0 @ARGV\n"
}

sub close_log
{
	close LOG_FD;
}

# show Skip/Perform status for a step in a menu
sub printStepSelected
{
	my $selected = $_[0];
	my $message  = $_[1];

	if (! $selected )
	{
		print   "[ Skip  ] ";
	} else {
		print GREEN, "[Perform] ", RESET;
	}
	print BOLD, RED "$message", RESET, "\n";
	return;
}

sub print_separator
{
	print "-------------------------------------------------------------------------------\n";
}

sub print_delimiter
{
	my $delimiter = "$_[0]";
	if ("$delimiter" eq "line" ) {
		print_separator;
	} elsif ("$delimiter" eq "blank" ) {
		print("\n");
	} elsif ("$delimiter" eq "admin" ) {
		print("Admin:\n");
	} elsif ("$delimiter" eq "setup" ) {
		print("Setup:\n");
	} elsif ("$delimiter" eq "validation" ) {
		print("Validation:\n");
	} elsif ("$delimiter" eq "review" ) {
		print("Review:\n");
	} elsif ("$delimiter" eq "other" ) {
		print("Other:\n");
	}
}

# remove any directory prefixes from path
sub basename
{
	my($path) = "$_[0]";

	$path =~ s/.*\/(.*)$/$1/;
	return $path
}

sub make_dir
{
	my($dir) = "$ROOT$_[0]";
	my($owner) = "$_[1]";
	my($group) = "$_[2]";
	my($mode) = "$_[3]";

	system "mkdir -p -m $mode $dir";
	system "chown $owner $dir";
	system "chgrp $group $dir";
}

sub check_dir
{
	my($dir) = "$_[0]";
	if (! -d "$ROOT$dir" ) 
	{
		#Creating directory 

		make_dir("$dir", "$OWNER", "$GROUP", "ugo=rx,u=rwx");
	}
}

sub copy_file
{
	my($src) = "$_[0]";
	my($dest) = "$ROOT$_[1]";
	my($owner) = "$_[2]";
	my($group) = "$_[3]";
	my($mode) = "$_[4]";
	# only copy file if source exists, this keep all those cp errors for litering
	# install for development.

	if ( -e $src)
	{               
		system "cp -rf $src $dest";
		system "chown $owner $dest";
		system "chgrp $group $dest";
		system "chmod $mode $dest";
	}
}

sub copy_data_file
{
	copy_file("$_[0]", "$_[1]", "$OWNER", "$GROUP", "ugo=r,u=rw");
}

sub GetChoice($$@)
{
	my($Question) = shift();
	my($default) = shift();

	my %choices;
	@choices{@_}=();  # Convert our choices to a hash table.

	my $c;

	if ( $Default_Prompt ) {
		print LOG_FD "$Question -> $default\n";
		$c=lc($default);
		if (exists $choices{$c}) {
			return $c;
		}
		# If the default is invalid, fall through.
	}

	while (1)
	{
		print "$Question [$default]: ";
		chomp($_ = <STDIN>);
		$_=remove_whitespace($_);
		if ("$_" eq "") {
			$_=$default;
		}
		$c = lc($_);
			
		if (exists $choices{$c})
		{
			print LOG_FD "$Question -> $c\n";
			return $c;
		}
	}
}

sub GetNumericValue
{
	my($retval) = 0;

	my($Question) = $_[0];
	my($default) = $_[1];
	my($minvalue) = $_[2];
	my($maxvalue) = $_[3];

        if ( $Default_Prompt ) {
		print "$Question -> $default\n";
		print LOG_FD "$Question -> $default\n";
		if (($default ge $minvalue) && ($default le $maxvalue)) {
			return $default;
		}
		# for invalid default, fall through and prompt
	}

	while (1)
	{
		print "$Question [$default]: ";
		chomp($retval = <STDIN>);
		$retval=remove_whitespace($retval);

		if (length($retval) == 0) {
			$retval=$default;
		}
		if (($retval >= $minvalue) && ($retval <= $maxvalue)) {
			print LOG_FD "$Question -> $retval\n";
			return $retval;
		}
		else {
			print "Value Out-of-Range ($minvalue to $maxvalue)\n";
		}
	}        
}

sub GetYesNo
{
	my($retval) = 1;
	my($answer) = 0;

	my($Question) = $_[0];
	my($default) = $_[1];

	if ( $Default_Prompt ) {
		print "$Question -> $default\n";
		print LOG_FD "$Question -> $default\n";
		if ( "$default" eq "y") {
			return 1;
		} elsif ("$default" eq "n") {
			return 0;
		}
		# for invalid default, fall through and prompt
	}

	while ($answer == 0)
	{
		print "$Question [$default]: ";
		chomp($_ = <STDIN>);
		$_=remove_whitespace($_);
		#$_ = getch();
		if ("$_" eq "") {
			$_=$default;
		}
		if (/^[Nn]/) 
		{
			print LOG_FD "$Question -> n\n";
			$retval = 0;
			$answer = 1;
		} elsif (/^[Yy]/ ) {
			print LOG_FD "$Question -> y\n";
			$retval = 1;
			$answer = 1;
		}
	}        
	return $retval;
}


sub HitKeyCont
{
	if ( $Default_Prompt )
	{
		return;
	}

	print "Hit any key to continue...";
	#$_=<STDIN>;
	getch();
	return;
}

sub HitKeyContAbortable
{
	my $c;

	if ( $Default_Prompt )
	{
		return;
	}

	print "Hit any key to continue (or ESC to abort)...";
	#$_=<STDIN>;
	$c = getch();
	# cntl-C is Abort character
	if ( ord($c) == $KEY_ESC || ord($c) == $KEY_CNTL_C )
	{
		return (GetYesNo("Abort?", "y") == 1);
	} else {
		return 0;
	}
}

# for security reason, we restrict to [a-zA-Z0-9_], '-', '.', ' ' and '/'
sub SanitizeFilename($)
{
	my($filename) = shift();
	if ($filename =~ /^[\w\-. \/]+$/)
	{
		return $filename;
	} else {
		print "Invalid filename\n";
		return "";
	}
}

# return 1 if the given config file has only comment and whitespace lines
sub empty_config_file($)
{
	my $file = $_[0];
	my $lines = `$BIN_DIR/ethexpandfile $file|wc -l`;	# let errors go to stderr
	return ( $lines == 0);
}

# check if the given config files exists and is non-empty
# return 1 if its good, 0 if not
sub valid_config_file($$)
{
	my $desc = $_[0];
	my $file = $_[1];

	if ( ! -e "$file" ) {
		print "$file: not found\n";
		print "You must create/select a $desc to proceed\n";
		HitKeyCont;
		return 0;	# failure
	}
	if (empty_config_file($file)) {
		print "$file: is empty\n";
		print "You must create/select a $desc to proceed\n";
		HitKeyCont;
		return 0;	# failure
	}
	return 1; # success
}

# provide a simple menu to select a single item from a list
# # returns one of selections or "" (to indicate user escaped menu)
sub selection_menu($$$@)
{
	my $title = shift();
	my $title2 = shift();
	my $prompt = shift();
	my @selections = @_;

	my $firstline = 0;
	my $maxlines=15;
	my $i;
	my $inp;

DO_MENU:
	system "clear";
	printf ("$title\n");
	printf ("$title2\n\n");
	my $screens = int((scalar(@selections) + $maxlines-1)/$maxlines);
	if ($screens > 1 ) {
		printf ("Please Select $prompt (screen %d of $screens):\n",
					$firstline/$maxlines+1);
	} else {
		printf ("Please Select $prompt:\n");
	}
	my $index=0;
	for($i=0; $i < scalar(@selections); $i++) {
		if ($index >= $firstline && $index < $firstline+$maxlines) {
			printf ("%x) %s\n", $index-$firstline, $selections[$i]);
		}
		$index++;
	}
	printf ("\n");
	if ($screens > 1 ) {
		printf ("N) Next Screen\n");
	}
	printf (  "X) Return to Previous Menu (or ESC or Q)\n");
	$inp = getch();
	
	if ($inp =~ /[qQ]/ || $inp =~ /[xX]/ || ord($inp) == $KEY_ESC) {
		return "";
	}

	if ($inp =~ /[nN]/ ) {
		if (scalar(@selections) > $maxlines) {
			$firstline += $maxlines;
			if ($firstline >= scalar(@selections)) {
				$firstline=0;
			}
		}
	} elsif ($inp =~ /[0123456789abcdefABCDEF]/) {
		my $value = hex($inp);
		my $index = ($firstline + $value);
		if ( $value < $maxlines && $index < scalar(@selections)) {
			return $selections[$index];
		}
	}
	goto DO_MENU;
}

# This function supports reading parameters from config file which are
# actually shell scripts.  openib.conf, fastfabric.conf and the Linux network
# config files are examples of such.
# If the file or parameter does not exist, returns ""
sub read_simple_config_param($$)
{
	my $config_file=shift();
	my $parameter=shift();

	if ( ! -e "$config_file" ) {
		return "";
	}
	my $value=`. $config_file >/dev/null 2>/dev/null; echo \$$parameter`;
	chomp($value);
	return "$value";
}

sub remove_whitespace($)
{
	my $string=shift();
	chomp($string);
	$string =~ s/^[[:space:]]*//;	# remove leading
	$string =~ s/[[:space:]]*$//;	# remove trailing
	return $string;
}

sub installed_mpiapps
{
	return (system("rpm -q eth-mpi-apps > /dev/null") == 0);
}


sub installed_mpisrc
{
	return (-e "$ROOT/usr/src/eth/MPI/do_build");
}

sub check_user
{
	my $user;
	$user=`/usr/bin/id -u`;
	if ($user != 0)
	{
		die "\n\nYou must be \"root\" to run this install program\n\n";
	}
}

sub Usage
{
	# don't document -r option, its only used for MPI do_build and
	# may not be appropriate anyway
	printf STDERR "Usage: $0 [--fromdir dir]\n";
	printf STDERR "       --fromdir dir - default directory to get install image from\n";
	exit (2);
}

sub process_args
{
	my $arg;
	my $last_arg;
	my $setroot = 0;
	my $setdir = 0;

	if (scalar @ARGV >= 1) {
		foreach $arg (@ARGV) {
			if ( $setdir ) {
				$FabricSetupScpFromDir="$arg";
				$setdir=0;
			} elsif ( $setroot ) {
				$ROOT="$arg";
				$setroot=0;
			} elsif ( "$arg" eq "--fromdir" ) {
				$setdir=1;
			} elsif ( "$arg" eq "-r" ) {
				$setroot=1;
			}
			else {
				printf STDERR "Invalid option: $arg\n";
				Usage;
			}
			$last_arg=$arg;
		}
	}
	if ( $setdir || $setroot) {
		printf STDERR "Missing argument for option: $last_arg\n";
		Usage;
	}

}

sub read_ffconfig_param
{
	my $param       = "$_[0]";	# ffconfig parameter

	my $result=`if [ -f $FF_CONF_FILE ]; then . $FF_CONF_FILE >/dev/null 2>&1; fi; . $TOOLS_DIR/ethfastfabric.conf.def >/dev/null 2>&1; . $TOOLS_DIR/ff_funcs >/dev/null 2>&1; echo \$$param`;
	chomp($result);
	return "$result";
}

sub setup_ffconfig
{
	do {
		if ( ! -e "$FF_CONF_FILE" ) {
			# replace missing file
			copy_data_file("$TOOLS_DIR/ethfastfabric.conf.def", "$FF_CONF_FILE");
		}
		print "You will now have a chance to edit/review the FastFabric Config File:\n";
		print "$FF_CONF_FILE\n";
		print "The values in this file will control the default operation of the\n";
		print "FastFabric Tools.  With the exception of the host file to use,\n";
		print "the values you specify for defaults will be used for all FastFabric\n";
		print "Operations performed via this menu system\n";
		print "Beware existing environment variables will override the values in this file.\n\n";

		print "About to: $Editor $FF_CONF_FILE\n";
		if ( HitKeyContAbortable() == 1) {
			return 1;
		}
		system("$Editor $FF_CONF_FILE");
		if ( ! -e "$FF_CONF_FILE" ) {
			print "You must have a $FF_CONF_FILE file to proceed\n\n";
		}
	} until ( -e "$FF_CONF_FILE");
	return 0;
}

#sub setup_ffports
#{
#	my $file = read_ffconfig_param("PORTS_FILE");
#	do {
#		if ( ! -e "$file" ) {
#			# replace missing file
#			copy_data_file("/usr/lib/eth-tools/ports", "$file");
#		}
#		print "You will now have a chance to edit/review the FastFabric PORTS_FILE:\n";
#		print "$file\n";
#		print "Some of the FastFabric operations which follow will use this file to\n";
#		print "specify the local NIC ports to use to access the fabric(s) to operate on\n";
#		print "Beware existing environment variables will override the values in this file.\n\n";
#
#		print "About to: $Editor $file\n";
#		if ( HitKeyContAbortable() == 1) {
#			return 1;
#		}
#		system("$Editor $file");
#		if ( ! -e "$file" ) {
#			print "You must have a $file file to proceed\n\n";
#		}
#	} until ( -e "$file");
#	return 0;
#}

sub run_fabric_cmd
{
	my $cmd       = "$_[0]";
	my $skip_prompt  = "$_[1]";
	print "Executing: $cmd\n";
	print LOG_FD "Executing: $cmd\n";
	system("$cmd");
	if ( "$skip_prompt" ne "skip_prompt" && HitKeyContAbortable() == 1) {
		return 1;
	}
	return 0;
}

sub run_fabric_ethcmdall
{
	my $DestFile       = "$_[0]";
	my $chassis       = "$_[1]";
	my $Sopt          = "$_[2]";
	my $other_opts    = "$_[3]";
	my $cmd;
	my $fabric_cmd;

	# valid_config_file has already been called by caller
	do {
		if ($chassis == 0) {
			print "Enter Command to run on all hosts (or none):";
		} else {
			print "Enter Command to run on all chassis (or none):";
		}
		chomp($cmd = <STDIN>);
		$cmd=remove_whitespace($cmd);
	} until ( length($cmd) != 0 );
	if ( "$cmd" ne "none" ) {
		if ($chassis == 0) {
			my $timelimit=GetNumericValue("Timelimit in minutes (0=unlimited):", 1, 0, 24*60) * 60;
			if ( $timelimit ) {
				$other_opts="$other_opts -T $timelimit";
			}
			if (GetYesNo("Run in parallel on all hosts?", "y") ) {
				$fabric_cmd="$BIN_DIR/ethcmdall -p $other_opts -f $DestFile '$cmd'";
			} else {
				$fabric_cmd="$BIN_DIR/ethcmdall $other_opts -f $DestFile '$cmd'";
			}
		} else {
			if (GetYesNo("Run in parallel on all chassis?", "y") ) {
				$fabric_cmd="$BIN_DIR/ethcmdall -C $Sopt -p $other_opts -F $DestFile '$cmd'";
			} else {
				$fabric_cmd="$BIN_DIR/ethcmdall -C $Sopt $other_opts -F $DestFile '$cmd'";
			}
		}
		if (GetYesNo("About to run: $fabric_cmd\nAre you sure you want to proceed?", "n") ) {
			return run_fabric_cmd("$fabric_cmd");
		}
	}
	return 0;
}

sub check_load($$$)
{
	my $hostfile       = "$_[0]";
	my $ropt       = "$_[1]"; # -r shows least busy hosts, default is busiest
	my $prompt       = "$_[2]";

	# valid_config_file has already been called by caller
	if (GetYesNo("View Load on hosts $prompt?", "y") ) {
		return run_fabric_cmd("$BIN_DIR/ethcheckload $ropt -f $hostfile");
	}
	return 0;
}

sub ff_viewres
{
	my $view_punchlist       = "$_[0]";
	my $view_verifyhosts      = "$_[1]";

	my $result_dir = read_ffconfig_param("FF_RESULT_DIR");
	my $files = "$result_dir/test.res $result_dir/test.log";
	my $rm_files = "test.res test.log";
	my $extra_rm_files = "test.res~ test.log~";
	if ( $view_verifyhosts) {
		$files= "$result_dir/verifyhosts.res $files";
		$rm_files= "verifyhosts.res $rm_files";
		$extra_rm_files = "verifyhosts.res~ $extra_rm_files";
	}
	if ( $view_punchlist) {
		$files= "$result_dir/punchlist.csv $files";
	}
	if ( ! -e "$result_dir/test.res" && ! -e "$result_dir/test.log"
		&& ! ($view_punchlist && -e "$result_dir/punchlist.csv")
	   	&& ! ($view_verifyhosts && -e "$result_dir/verifyhosts.res") ) {
		print "No result files in $result_dir\n";
		if ( HitKeyContAbortable() ) {
			return 1;
		}
	} else {
		print "Using $Editor (to select a different editor, export EDITOR).\n";
		print "About to: $Editor $files\n";
		if ( HitKeyContAbortable() ) {
			return 1;
		}
		system("$Editor $files");
		if (GetYesNo("Would you like to remove $rm_files test_tmp* and save_tmp\nin $result_dir ?", "n") ) {
			print("Executing: cd $result_dir && rm -rf $rm_files save_tmp test_tmp* $extra_rm_files\n");
			system("cd $result_dir && rm -rf $rm_files save_tmp test_tmp* $extra_rm_files");
			if ( HitKeyContAbortable() ) {
				return 1;
			}
		}
	}
	return 0;
}

sub fabricsetup_config
{
	my $inp;
	my $file;

	print "Using $Editor (to select a different editor, export EDITOR).\n";
	do {
		if (setup_ffconfig() ) {
			return 1;
		}
		do 
		{
			print "The FastFabric operations which follow will require a file\n";
			print "listing the hosts to operate on\n";
			print "You should select a file which OMITS this host\n";
			print "Select Host File to Use/Edit [$PrevFabricSetupHostsFile]: ";
			chomp($inp = <STDIN>);
			$inp=remove_whitespace($inp);

			if ( length($inp) == 0 ) 
			{
				$inp = $PrevFabricSetupHostsFile;
			}
			$file = SanitizeFilename($inp);
			if ( length($file) > 0 ) {
				print "About to: $Editor $file\n";
				if ( HitKeyContAbortable() ) {
					return 1;
				}
				system("$Editor '$file'");
				if ( ! -e "$file" ) {
					print "You must create a Host File to proceed\n\n";
				} else {
					$FabricSetupHostsFile=$file;
					$PrevFabricSetupHostsFile=$file;
				}
			}
		} until ( -e "$file");
		print "Selected Host File: $FabricSetupHostsFile\n";
	} until (! GetYesNo("Do you want to edit/review/change the files?", "y") );
	print LOG_FD "Selected Host File -> $FabricSetupHostsFile\n";
	return 0;
}

sub fabricsetup_ethpingall
{
	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	return run_fabric_cmd("$BIN_DIR/ethpingall -p -f $FabricSetupHostsFile");
}
sub fabricsetup_findgood
{
	my $findgood_opts="-A";

	if  ( -e "$PrevFabricSetupHostsFile"
		   && "$PrevFabricSetupHostsFile" ne "$FabricSetupHostsFile" ) {
		if (GetYesNo("Would you like to go back to using $PrevFabricSetupHostsFile?", "y") ) {
	    	$FabricSetupHostsFile = $PrevFabricSetupHostsFile;
		}
	}
	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	if (! GetYesNo("Would you like to verify hosts are ssh-able?", "n") ) {
		# skip ssh test
		$findgood_opts="$findgood_opts -R";
	}
	if ( run_fabric_cmd("$BIN_DIR/ethfindgood $findgood_opts -f $FabricSetupHostsFile")) {
		# serious failure
		return 1;
	}
	# ask even if non-bad since good file will be better sorted for cabletest
	#if (-s $OPA_CONFIG_DIR/bad) {
	if ( "$FabricSetupHostsFile" ne "$OPA_CONFIG_DIR/good") {
		if (GetYesNo("Would you like to now use $OPA_CONFIG_DIR/good as Host File?", "y") ) {
	    	$PrevFabricSetupHostsFile = "$FabricSetupHostsFile";
	    	$FabricSetupHostsFile = "$OPA_CONFIG_DIR/good";
		}
	}
	return 0;
}

sub fabricsetup_setupssh
{
	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	return run_fabric_cmd("$BIN_DIR/ethsetupssh -S -p -f $FabricSetupHostsFile");
}
sub fabricsetup_copyhosts
{
	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	return run_fabric_cmd("$BIN_DIR/ethscpall -p -f $FabricSetupHostsFile /etc/hosts /etc/hosts");
}
sub fabricsetup_showuname
{
	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	return run_fabric_cmd("$BIN_DIR/ethcmdall -T 60 -f $FabricSetupHostsFile 'uname -a'");
}
sub fabricsetup_install
{
	my $install_mode;
	my $dir="$FabricSetupScpFromDir";
	my $query=0;
	my $install_file;
	my $product = read_ffconfig_param("FF_PRODUCT");
	my $version = read_ffconfig_param("FF_PRODUCT_VERSION");
	my $packages = "";
	my $options;
 
	$install_file="$product.$version.tgz";

	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	do {
		do {
			if ( "$dir" ne "none" && ! -e "$dir/$install_file" ) {
				print "$dir/$install_file: not found\n";
				$query=1;
			} else {
				$query = ! GetYesNo("Do you want to use $dir/$install_file?", "y");
			}
			if ( $query ) {
				do {
					print "Enter Directory to get $install_file from (or none):";
					chomp($dir = <STDIN>);
					$dir=remove_whitespace($dir);
				} until ( length($dir) != 0 );
			}
		} until ( "$dir" eq "none" || -e "$dir/$install_file" );
		if ("$dir" eq "none" ) {
			print "You have selected to skip the installation/upgrade step\n";
			$install_mode="skip";
		} else {
			print "You have selected to use $dir/$install_file\n";
			$FabricSetupScpFromDir="$dir";
			print "\nAn upgrade/reinstall or an initial installation may be performed.\n\n";
			print "An upgrade/reinstall requires an existing installation of Intel Eth software\non the selected nodes and will upgrade or reinstall the existing packages.\n\n";
			print "An initial installation will first uninstall any existing Intel Eth software.\n";

			my $choice = GetChoice("Would you like to do a fresh [i]nstall, an [u]pgrade or [s]kip this step?", 
				"u", ("i", "u", "s"));

			if ("$choice" eq "u") {
				$install_mode="upgrade";
				print "You have selected to perform an upgrade installation\n";
				$options = read_ffconfig_param("FF_UPGRADE_OPTIONS");
				if ("$options" ne "") {
					if (! GetYesNo("Do you want to use the following upgrade options: '$options'?", "y")) {
						do {
							print "Enter upgrade options (or none):";
							chomp($options = <STDIN>);
							$options=remove_whitespace($options);
						} until ( length($options) != 0);
					}
					if ("$options" eq "none") {
						$options="";
					}
					else {
						$options = "-U \'$options\'";
					}
				}
			} elsif ("$choice" eq "i") {
				$install_mode="load";
				print "You have selected to perform an initial installation\n";
				print "This will uninstall any existing Intel Eth software on the selected nodes\n";
				$options = read_ffconfig_param("FF_INSTALL_OPTIONS");
				if ("$options" ne "") {
					if (! GetYesNo("Do you want to use the following install options: '$options'?", "y")) {
						do {
							print "Enter install options (or none):";
							chomp($options = <STDIN>);
							$options=remove_whitespace($options);
						} until ( length($options) != 0);
					}
					if ("$options" eq "none") {
						$options="";
					}
					else {
						$options = "-I \'$options\'";
						$packages = read_ffconfig_param("FF_PACKAGES");
						if ("$packages" ne "") {
							if (! GetYesNo("Do you want to install the following packages?\n    $packages\n", "y")) {
								do {
									print "Enter packages (or none):";
									chomp($packages = <STDIN>);
									$packages=remove_whitespace($packages);
								} until ( length($packages) != 0);
							}
							if ("$packages" eq "none") {
								$packages="";
							}
							else {
								$packages = "-P \'$packages\'";
							}
						}
					}
				}
			}
		}
	} until (GetYesNo("Are you sure you want to proceed?", "n") );
	if ( "$install_mode" ne "skip" ) {
		return run_fabric_cmd("$BIN_DIR/ethhostadmin -f $FabricSetupHostsFile -d $FabricSetupScpFromDir $options $packages $install_mode");
	}
	return 0;
}

sub fabricsetup_configsnmp
{
	if (! valid_config_file("Host File", $FabricAdminHostsFile) ) {
		return 1;
	}
	return run_fabric_cmd("$BIN_DIR/ethsetupsnmp -p -f $FabricAdminHostsFile");
}
sub fabricsetup_buildmpi
{
	my $mpi_apps_dir = "/usr/src/eth/mpi_apps";
	my $build_dir = read_ffconfig_param("FF_MPI_APPS_DIR");
	if (! -e "$mpi_apps_dir/Makefile") {
		# the makefile and some of apps are part of opa-fastfabric package
		print "$mpi_apps_dir/Makefile: not found\n";
		print "Make sure opa-fastfabric is properly installed\n";
		HitKeyCont;
		return 1;
	}
	# for now do not build NASA, etc, just basic stuff
	my $mode;
	my $mpich_prefix;
	my $inp;
	do {
		# get default MPI to use
		$mpich_prefix= `cd $mpi_apps_dir 2>/dev/null; . ./select_mpi 2>/dev/null; echo \$MPICH_PREFIX`;
		chomp $mpich_prefix;

		# identify other alternatives
		my $prefix="/usr";
		my $dirs=`find $prefix/mpi -maxdepth 2 -mindepth 2 -type d 2>/dev/null|sort`;
		$dirs=$dirs . `find /opt/intel/impi/*/intel64 -maxdepth 0 -type d 2>/dev/null|sort`;
		my @dirs = split /[[:space:]]+/, $dirs;

		#print "The following MPIs have been found on this system:\n";
		if ( $dirs !~ m|$mpich_prefix|) {
			# odd case, default is not in a normal location
			#print "       $mpich_prefix\n";
			@dirs = ($mpich_prefix, @dirs);
		}
		my @mpi_dirs = ();
		foreach my $d ( @dirs ) {
			next if ( ! -e "$d/bin/mpicc" ); # skip incomplete MPI dirs
			#print "       $d\n";
			@mpi_dirs = (@mpi_dirs, $d);
		}

		my $title1 = "Host Setup: $FabricSetupStepsName{'buildapps'}";
		my $title2 = "MPI Directory Selection";
		my $other_dir = "Enter Other Directory";
		if (@mpi_dirs == 0) {
			$title2 = "$title2\n\nWarning: No MPI found!";
			$other_dir = "$other_dir (e.g. /usr/mpi/src/openmpi-1.1.1)";
		}

		do {
			$inp = selection_menu(
				"$title1", "$title2", "MPI Directory",
			   	(@mpi_dirs, "$other_dir"));
			if ( "$inp" eq "" ) {
				$inp = "none";
			} elsif ( "$inp" eq "$other_dir" ) {
				do {
					print "Enter MPI directory location (or none):";
					chomp($inp = <STDIN>);
					$inp=remove_whitespace($inp);
				} until ( "$inp" ne "");
			}
			if ("$inp" ne "none" && ! -e "$inp" ) {
				print "$inp: not found\n";
				HitKeyCont;
			}
		} until ( "$inp" eq "none" || -e "$inp" );
		$mpich_prefix=$inp;
		if ("$mpich_prefix" eq "none" ) {
			print "You have selected to skip the building of the MPI Apps step\n";
			$mode="skip";
		} else {
			print "You have selected to use MPI: $mpich_prefix\n";
			$mode="build";
		}
	} until (GetYesNo("Are you sure you want to proceed?", "n") );
			
	if ( "$mode" ne "build" ) {
		return 0;
	}
	if ( -e "$build_dir/.filelist" ) {
		run_fabric_cmd("cd $build_dir; rm -rf `cat .filelist`", "skip_prompt");
	}
	run_fabric_cmd("mkdir -p $build_dir; cp -r -p $mpi_apps_dir/. $build_dir", "skip_prompt");
	run_fabric_cmd("cd $mpi_apps_dir; find . -mindepth 1 > $build_dir/.filelist", "skip_prompt");
	if (!installed_mpiapps()){
		print "Package $ComponentName{mpiapps} not installed. Only building subset of MPI Apps\n";
		HitKeyCont;
		if (run_fabric_cmd("cd $build_dir; MPICH_PREFIX=$mpich_prefix make clobber eth-base")) {
			return 1;
		}
	} else{
		if (run_fabric_cmd("cd $build_dir; MPICH_PREFIX=$mpich_prefix make clobber quick")) {
			return 1;
		}
	}
	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	my $fabric_cmd="$BIN_DIR/ethscpall -t -p -f $FabricSetupHostsFile $build_dir $build_dir";
	if (GetYesNo("About to run: $fabric_cmd\nAre you sure you want to proceed?","n") ) {
        return run_fabric_cmd("$fabric_cmd");
	}
	return 0;
}
sub fabricsetup_buildapps
{
	my $res = 0;
	if (GetYesNo("Do you want to build MPI Test Apps?", "y") ) {
		$res = fabricsetup_buildmpi();
	}
	return $res;
}

sub fabricsetup_reboot
{
	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	return run_fabric_cmd("$BIN_DIR/ethhostadmin -f $FabricSetupHostsFile reboot");
}
sub fabricsetup_refreshssh
{
	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	return run_fabric_cmd("$BIN_DIR/ethsetupssh -p -U -f $FabricSetupHostsFile");
}
sub fabricsetup_rebuildmpi
{
	if ( ! installed_mpisrc() ) {
		printf("$ComponentName{mpisrc} not installed on this system\n");
		printf("Unable to Rebuild MPI\n");
		HitKeyCont;
		return;
	}
	if (run_fabric_cmd("cd $ROOT/usr/src/eth/MPI; ./do_build")) {
		return 1;
	}
	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	# do in two steps so user can see results of build before scp starts
	# determine where MPI was built and copy needed files to all nodes
	my $mpich_prefix= read_simple_config_param("$ROOT/usr/src/eth/MPI/.mpiinfo", "MPICH_PREFIX");
	# instead of copy, copy the actual rpms and install them
	#return run_fabric_cmd("$BIN_DIR/ethscpall -t -p -f $FabricSetupHostsFile $mpich_prefix $mpich_prefix");
	my $mpi_rpms= read_simple_config_param("$ROOT/usr/src/eth/MPI/.mpiinfo", "MPI_RPMS");
	if (run_fabric_cmd("cd /usr/src/eth/MPI && $BIN_DIR/ethscpall -p -f $FabricSetupHostsFile $mpi_rpms /var/tmp")) {
		return 1;
	}
	# need force for reinstall case
	return run_fabric_cmd("$BIN_DIR/ethcmdall -p -f $FabricSetupHostsFile 'cd /var/tmp; rpm -U --force $mpi_rpms; rm -f $mpi_rpms'");
}
sub fabricsetup_ethcmdall
{
	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	return run_fabric_ethcmdall("$FabricSetupHostsFile", 0, "", "");
}
sub fabricsetup_copyall
{
	my $file;
	my $dir;
	my $fabric_cmd;

	if (! valid_config_file("Host File", $FabricSetupHostsFile) ) {
		return 1;
	}
	do {
		print "Enter File to copy to all hosts (or none):";
		chomp($file = <STDIN>);
		$file=remove_whitespace($file);
	} until ( length($file) != 0 );
	if ( "$file" ne "none" ) {
		if ( substr($file,0,1) ne "/" ) {
			chomp($dir=`pwd`);
			$file="$dir/$file";
		}
		$fabric_cmd="$BIN_DIR/ethscpall -p -f $FabricSetupHostsFile $file $file";
		if (GetYesNo("About to run: $fabric_cmd\nAre you sure you want to proceed?", "n") ) {
			return run_fabric_cmd("$fabric_cmd");
		}
	}
	return 0;
}
sub fabricsetup_viewres
{
	return ff_viewres(0,0);
}

# Host Setup
sub fabric_setup
{
	my $result;
	my $inp;
	my %selected = ();
	my %statusMessage = (); # TBD - not used
	my $step;
	my $i;

	foreach $step ( @FabricSetupSteps )
	{
		$selected{$step}= 0;
	}
DO_SETUP:
	system "clear";
	print color("bold");
	printf ("FastFabric Ethernet Host Setup Menu\n");
	printf ("Host File: $FabricSetupHostsFile\n");
	print color("reset");
	for($i=0; $i < scalar(@FabricSetupSteps); $i++)
	{
		$step = $FabricSetupSteps[$i];
		print_delimiter("$FabricSetupStepsMenuDelimiter{$step}");
		printf ("%x) %-41s ", $i, $FabricSetupStepsName{$step});
		printStepSelected($selected{$step}, $statusMessage{$step});
	}

	printf ("\nP) Perform the Selected Actions              N) Select None\n");
	printf (  "X) Return to Previous Menu (or ESC or Q)\n");
			
	%statusMessage = ();

	$inp = getch();
	
	if ($inp =~ /[qQ]/ || $inp =~ /[xX]/ || ord($inp) == $KEY_ESC)
	{
		return;
	}

	if ($inp =~ /[nN]/ )
	{
		foreach $step ( @FabricSetupSteps )
		{
			$selected{$step}= 0;
		}
	}
	elsif ($inp =~ /[0123456789abcdefABCDEF]/)
	{
		$step = $FabricSetupSteps[hex($inp)];
		$selected{$step}= ! $selected{$step};
	}
	elsif ($inp =~ /[pP]/)
	{
		# perform the fabric setup
		foreach $step ( @FabricSetupSteps )
		{
			if ($selected{$step} )
			{
				print LOG_FD "Performing Host Setup: $FabricSetupStepsName{$step}\n";
				print "\nPerforming Host Setup: $FabricSetupStepsName{$step}\n";
				$result = eval "fabricsetup_$step";
				$selected{$step} = 0;
				if ( $result ) {
					goto DO_SETUP
				}
			}
		}
	}

	goto DO_SETUP;
}

sub fabricadmin_config
{
	my $inp;
	my $file;

	print "Using $Editor (to select a different editor, export EDITOR).\n";
	do {
		if (setup_ffconfig() ) {
			return 1;
		}
		do
		{
			print "The FastFabric operations which follow will require a file\n";
			print "listing the hosts to operate on\n";
			print "You should select a file which INCLUDES this host\n";
			print "Select Host File to Use/Edit [$PrevFabricAdminHostsFile]: ";
			chomp($inp = <STDIN>);
			$inp=remove_whitespace($inp);

			if ( length($inp) == 0 ) 
			{
				$inp = $PrevFabricAdminHostsFile;
			}
			$file = SanitizeFilename($inp);
			if ( length($file) > 0 ) {
				print "About to: $Editor $file\n";
				if ( HitKeyContAbortable() ) {
					return 1;
				}
				system("$Editor '$file'");
				if ( ! -e "$file" ) {
					print "You must create a Host File to proceed\n\n";
				} else {
					$FabricAdminHostsFile=$file;
					$PrevFabricAdminHostsFile=$file;
				}
			}
		} until ( -e "$file");
		print "Selected Host File: $FabricAdminHostsFile\n";
	} until (! GetYesNo("Do you want to edit/review/change the files?", "y") );
	print LOG_FD "Selected Host File -> $FabricAdminHostsFile\n";
	return 0;
}

sub fabricadmin_fabric_info
{
	if ( ! -e "$BIN_DIR/ethfabricinfo" ) {
		print "ethfabricinfo requires $ComponentName{oftools} be installed\n";
		HitKeyCont;
		return 1;
	}
	return run_fabric_cmd("$BIN_DIR/ethfabricinfo");
}
#sub fabricadmin_ethpingall
#{
#	if ( ! -e "$FabricAdminHostsFile" ) {
#		print "$FabricAdminHostsFile: not found\n";
#		print "You must create/select a Host File to proceed\n";
#		HitKeyCont;
#		return 1;
#	}
#	return run_fabric_cmd("/sbin/ethpingall -p -f $FabricAdminHostsFile");
#}
sub fabricadmin_findgood
{
	my $findgood_opts="";

	if  ( -e "$PrevFabricAdminHostsFile"
		   && "$PrevFabricAdminHostsFile" ne "$FabricAdminHostsFile" ) {
		if (GetYesNo("Would you like to go back to using $PrevFabricAdminHostsFile?", "y") ) {
	    	$FabricAdminHostsFile = $PrevFabricAdminHostsFile;
		}
	}
	if (! valid_config_file("Host File", $FabricAdminHostsFile) ) {
		return 1;
	}
	if (! GetYesNo("Would you like to verify hosts are ssh-able?", "y") ) {
		# skip ssh test
		$findgood_opts="$findgood_opts -R";
	}
	if (! GetYesNo("Would you like to verify host RDMA ports are active?", "y") ) {
		# skip active test
		$findgood_opts="$findgood_opts -A";
	}
	if ( run_fabric_cmd("$BIN_DIR/ethfindgood $findgood_opts -f $FabricAdminHostsFile")) {
		return 1;
	}
	# ask even if non-bad since good file will be better sorted for cabletest
	#if (-s $OPA_CONFIG_DIR/bad) {
	if ( "$FabricAdminHostsFile" ne "$OPA_CONFIG_DIR/good") {
		if (GetYesNo("Would you like to now use $OPA_CONFIG_DIR/good as Host File?", "y") ) {
	    	$PrevFabricAdminHostsFile = "$FabricAdminHostsFile";
	    	$FabricAdminHostsFile = "$OPA_CONFIG_DIR/good";
		}
	}
	return 0;
}

sub fabricadmin_singlehost
{
	my $verifyhosts_opts="";
	my $verifyhosts_tests="";
	my $result_dir = read_ffconfig_param("FF_RESULT_DIR");
	my $hostverify_sample = "/usr/share/eth-tools/samples/hostverify.sh";
	my $hostverify_default = read_ffconfig_param("FF_HOSTVERIFY_DIR") . "/hostverify_default.sh";
	my $hostverify = "";
	my $hostverify_res = "hostverify.res";
	my $inp;
	my $timelimit = 1;
	my $hostverify_specific = read_ffconfig_param("FF_HOSTVERIFY_DIR") . "/hostverify_" . basename($FabricAdminHostsFile) . ".sh";
	my $use_specific = 0;

	if (! valid_config_file("Host File", $FabricAdminHostsFile) ) {
		return 1;
	}

	print "\nFastFabric needs to create a working file for verification\n";
	if (GetYesNo("Would you like to use $hostverify_specific?", "y") ) {
		$use_specific = 1;
		$hostverify = $hostverify_specific;
	} else {
		$hostverify = $hostverify_default;
		print "Use default $hostverify as working file\n";
	}

	if ( ! -e "$hostverify" ) {
		copy_data_file("$hostverify_sample", "$hostverify");
	} else {
		print "\nFile $hostverify already exists\n";
		if (GetYesNo("Would you like to copy $hostverify_sample to $hostverify?", "n") ) {
			copy_data_file("$hostverify_sample", "$hostverify");
		}
	}

	print "\nIn below file, leaving NIC_IFS empty will use the interfaces defined in $FabricAdminHostsFile.\n";
	if (GetYesNo("Would you like to edit $hostverify?", "y") ) {

		print "About to: $Editor $hostverify\n";
		if ( HitKeyContAbortable() == 1) {
			return 1;
		}
		system("$Editor $hostverify");
		if (! $use_specific) {
			if (GetYesNo("Would you like to save $hostverify locally as $hostverify_specific?", "n") ) {
				copy_data_file("$hostverify", "$hostverify_specific");
			}
		} else {
			if (GetYesNo("Would you like to save $hostverify locally as $hostverify_default?", "n") ) {
				copy_data_file("$hostverify", "$hostverify_default");
			}
		}
	}

	print "\nChoose n below only if $hostverify on hosts has not changed \n";
	if (GetYesNo("Would you like to copy $hostverify to hosts?", "y") ) {
		$verifyhosts_opts="-c"; # copy the hostverify.sh file
        }

	if (GetYesNo("Would you like to specify tests to run?", "n") ) {
		print "Enter space separated list of hostverify tests [default hpl]: ";
		chomp($inp = <STDIN>);
		$inp=remove_whitespace($inp);
		if ( length($inp) != 0 ) 
		{
			$verifyhosts_tests="$inp";
		} else {
			$verifyhosts_tests="default hpl";
		}
	}

	#If HPL test is to be run, make sure mpi-tests package is installed
	if (index($verifyhosts_tests,"hpl") != -1) {
		if (!installed_mpiapps()) {
			print "Package $ComponentName{mpiapps} not installed. Cannot run HPL test.\n";
        	HitKeyCont;
			return 1;
		}
	}

	my $upload_dir = `
		[ -e /etc/eth-tools/ethfastfabric.conf ] && . /etc/eth-tools/ethfastfabric.conf 2>/dev/null;
		. /usr/lib/eth-tools/ethfastfabric.conf.def 2>/dev/null;
		echo \$UPLOADS_DIR`;
	chomp $upload_dir;
	print "\nVerification will run on each host locally and upload results to $upload_dir/<hostname>/\n";
	print "Enter filename for upload destination file [$hostverify_res]: ";
	chomp($inp = <STDIN>);
	$inp=remove_whitespace($inp);

	if ( length($inp) != 0 ) 
	{
		$hostverify_res = $inp;
	}

	#Get timelimit, and suggest a higher default if HPL test is being run. 
	if (index($verifyhosts_tests,"hpl") != -1) {
		$timelimit=GetNumericValue("Timelimit in minutes:", 5, 1, 100) * 60;
	} else {
		$timelimit=GetNumericValue("Timelimit in minutes:", 1, 1, 100) * 60;
	}


	if (check_load($FabricAdminHostsFile, "", "prior to verification") ) {
		return 1;
	}
	print "\n";
	run_fabric_cmd("$BIN_DIR/ethverifyhosts -k $verifyhosts_opts -u $hostverify_res -T $timelimit -f $FabricAdminHostsFile -F $hostverify $verifyhosts_tests", "skip_prompt");
	print "About to: $Editor $result_dir/verifyhosts.res\n";
	if ( HitKeyContAbortable() ) {
		return 1;
	}
	system("$Editor '$result_dir/verifyhosts.res'");
	return 0;
}

sub fabricadmin_showallports
{
	my $linkanalysis=0;
	my $linkanalysis_opts="";
	my $linkanalysis_reports="";
	my $topology=0;
	my $result_dir = read_ffconfig_param("FF_RESULT_DIR");
	my $linkanalysis_file = "$result_dir/linkanalysis.res";
	my $inp;

	if (GetYesNo("Would you like to perform fabric error analysis?", "y") ) {
		$linkanalysis=1;
		$linkanalysis_reports="$linkanalysis_reports errors";
#		if (GetYesNo("Clear error counters after generating report?", "n") ) {
#			$linkanalysis_reports="$linkanalysis_reports clearerrors";
#			if (GetYesNo("Force Clear of hardware error counters after generating report?", "n") ) {
#				$linkanalysis_reports="$linkanalysis_reports clearhwerrors";
#			}
#		}
	}
	if (GetYesNo("Would you like to perform fabric link speed error analysis?", "y") ) {
		$linkanalysis=1;
		$linkanalysis_reports="$linkanalysis_reports slowlinks";
		if (GetYesNo("Check for links configured to run slower than supported?", "n") ) {
			$linkanalysis_reports="$linkanalysis_reports misconfiglinks";
		}
		if (GetYesNo("Check for links connected with mismatched speed potential?", "n") ) {
			$linkanalysis_reports="$linkanalysis_reports misconnlinks";
		}
	}
	print "\nThe fabric deployment can be verified against the planned topology.\n";
	print "Typically the planned topology will have been converted to an XML topology\n";
	print "file using ethxlattopology or a customized variation.\n";
	print "If this step has been done and a topology file has been placed in the\n";
	print "location specified in Ethernet Mgt config file, mgt_config.xml, then\n";
	print "a topology verification can be performed.\n";
	print "Refer to the FastFabric CLI reference guide for more info.\n\n";
	if (GetYesNo("Would you like to verify fabric topology?", "y") ) {
		# TBD - check for presence of topology files
		if (GetYesNo("Verify all aspects of topology (links, nodes)?", "y") ) {
			$linkanalysis=1;
			$topology=1;
			$linkanalysis_reports="$linkanalysis_reports verifyall";
		} else {
			# given venn diagram of types of links checked in verify*links
			# reasonable choices are:
			# verifylinks
			#     or
			# verifyextlinks
			#     or
			# verfyniclinks and/or ( verifyislinks or verifyextislinks)
			# Note we do not have a verifyextniclinks since most fi links are ext
			if (GetYesNo("Verify all link topology (backplanes and cables)?", "y") ) {
				$linkanalysis=1;
				$topology=1;
				$linkanalysis_reports="$linkanalysis_reports verifylinks";
			} elsif (GetYesNo("Verify cable link topology (NICs and switches)?", "y") ) {
				$linkanalysis=1;
				$topology=1;
				$linkanalysis_reports="$linkanalysis_reports verifyextlinks";
			} else {
				if (GetYesNo("Verify inter-switch link topology (backplanes and cables)?", "y") ) {
					$linkanalysis=1;
					$topology=1;
					$linkanalysis_reports="$linkanalysis_reports verifyislinks";
				} elsif (GetYesNo("Verify cable inter-switch link topology?", "y") ) {
					$linkanalysis=1;
					$topology=1;
					$linkanalysis_reports="$linkanalysis_reports verifyextislinks";
				}
				if (GetYesNo("Verify NIC link topology?", "y") ) {
					$linkanalysis=1;
					$topology=1;
					$linkanalysis_reports="$linkanalysis_reports verifyniclinks";
				}
			}
			if (GetYesNo("Verify all nodes?", "y") ) {
				$linkanalysis=1;
				$topology=1;
				$linkanalysis_reports="$linkanalysis_reports verifynodes";
			}
		}
		if ($topology) {
			if (! GetYesNo("Include unexpected devices in punchlist?", "y") ) {
				$linkanalysis_opts="$linkanalysis_opts -U";
			}
		}
	}

	do {
		print "Enter filename for results [$linkanalysis_file]: ";
		chomp($inp = <STDIN>);
		$inp=remove_whitespace($inp);
		if (length($inp) == 0) {
			$inp = $linkanalysis_file;
		}
		$inp = SanitizeFilename($inp);
	} while (length($inp) == 0);
	$linkanalysis_file = $inp;
	if ($linkanalysis) {
		my $snapshot_suffix=`date +%Y%m%d-%H%M%S`;
		chop $snapshot_suffix;
		if ( run_fabric_cmd("$BIN_DIR/ethlinkanalysis $linkanalysis_opts -x '$snapshot_suffix' $linkanalysis_reports > $linkanalysis_file 2>&1", "skip_prompt") ) {
			return 1;
		}
	} else  {
		if (! valid_config_file("Host File", $FabricAdminHostsFile) ) {
			return 1;
		}
		if ( run_fabric_cmd("$BIN_DIR/ethshowallports -f $FabricAdminHostsFile > $linkanalysis_file 2>&1", "skip_prompt") ) {
			return 1;
		}
	}
	print "About to: $Editor $linkanalysis_file\n";
	if ( HitKeyContAbortable() ) {
		return 1;
	}
	system("$Editor '$linkanalysis_file'");
	return 0;
}
sub fabricadmin_rping
{
	if (! valid_config_file("Host File", $FabricAdminHostsFile) ) {
		return 1;
	}
	return run_fabric_cmd("$BIN_DIR/ethhostadmin -f $FabricAdminHostsFile rping");
}
sub fabricadmin_refreshssh
{
	if (! valid_config_file("Host File", $FabricAdminHostsFile) ) {
		return 1;
	}
	return run_fabric_cmd("$BIN_DIR/ethsetupssh -p -U -f $FabricAdminHostsFile");
}
sub fabricadmin_mpiperf
{
	if (! valid_config_file("Host File", $FabricAdminHostsFile) ) {
		return 1;
	}
	if (GetYesNo("Test Latency and Bandwidth deviation between all hosts?", "y") ) {
		if (check_load($FabricAdminHostsFile, "", "prior to test")) {
			return 1;
		}
		return run_fabric_cmd("$BIN_DIR/ethhostadmin -f $FabricAdminHostsFile mpiperfdeviation");
	} else {
	
		if (!installed_mpiapps()) {
			print "Package $ComponentName{mpiapps} not installed.\n";
			print "Cannot run the mpiperf tests. Deviation tests can still be used. Please try again. \n";
			HitKeyCont;
			return 1;
		}

		if (check_load($FabricAdminHostsFile, "", "prior to test")) {
			return 1;
		}
		return run_fabric_cmd("$BIN_DIR/ethhostadmin -f $FabricAdminHostsFile mpiperf");
	}
}
sub fabricadmin_health
{
	# TBD - esm_chassis and chassis file
	if (GetYesNo("Baseline present configuration?", "n") ) {
		return run_fabric_cmd("$BIN_DIR/ethallanalysis -b");
	} else {
		return run_fabric_cmd("$BIN_DIR/ethallanalysis");
	}
}
sub fabricadmin_cabletest
{
	my $IFS_FM_BASE="/usr/lib/opa-fm";
	my $rundir="$IFS_FM_BASE/bin";
	my $cabletest_opts = "";
	my $cabletest_file = "";
	my $cabletest_cmds = "";
	my $check_load_before = 0;

	do {
		if (GetYesNo("Stop or cleanup any already running Cable Test?", "y") ) {
			if ( ! -e "$FabricAdminHostsFile" ) {
				print "$FabricAdminHostsFile: not found\n";
				print "You must create/select a Host File to proceed\n";
				HitKeyCont;
				return 1;
			}
			$cabletest_file="-f '$FabricAdminHostsFile'";
			$cabletest_cmds="$cabletest_cmds stop";
		}

		if (GetYesNo("Start Cable Test?", "y") ) {
			if ( ! -e "$FabricAdminHostsFile" ) {
				print "$FabricAdminHostsFile: not found\n";
				print "You must create/select a Host File to proceed\n";
				HitKeyCont;
				return 1;
			}
			my $numprocs=GetNumericValue("Number of Processes per host:", 3, 1, 64);
			$cabletest_opts="$cabletest_opts -n $numprocs";
			$cabletest_cmds="$cabletest_cmds start";
			$cabletest_file="-f '$FabricAdminHostsFile'";
			$check_load_before = 1;
		}
		if ( $check_load_before) {
			if (check_load($FabricAdminHostsFile, "", "prior to test")) {
				return 1;
			}
		}
		if ( "$cabletest_opts" ne "" || "$cabletest_cmds" ne "" ) {
			print "About to run: $BIN_DIR/ethcabletest $cabletest_opts $cabletest_file $cabletest_cmds\n";
			if ( HitKeyContAbortable() ) {
				return 1;
			}
			return run_fabric_cmd("$BIN_DIR/ethcabletest $cabletest_opts $cabletest_file $cabletest_cmds");
		}
	} until (GetYesNo("No selection made. Are you sure you want to proceed?", "n") );
}
sub fabricadmin_ethcaptureall
{
	my $detail;

	if (! valid_config_file("Host File", $FabricAdminHostsFile) ) {
		return 1;
	}
	$detail=GetNumericValue("Capture detail level (1-Normal 2-Fabric 3-Analysis):", 3, 1, 3);
	return run_fabric_cmd("$BIN_DIR/ethcaptureall -p -D $detail -f $FabricAdminHostsFile");
}
sub fabricadmin_ethcmdall
{
	if (! valid_config_file("Host File", $FabricAdminHostsFile) ) {
		return 1;
	}
	return run_fabric_ethcmdall("$FabricAdminHostsFile", 0, "", "");
}
sub fabricadmin_viewres
{
	return ff_viewres(1,1);
}

# Host Admin
sub fabric_admin
{
	my $result;
	my $inp;
	my %selected = ();
	my %statusMessage = (); # TBD - not used
	my $step;
	my $i;

	foreach $step ( @FabricAdminSteps )
	{
		$selected{$step}= 0;
	}
DO_SETUP:
	system "clear";
	print color("bold");
	printf ("FastFabric Ethernet Host Verification/Admin Menu\n");
	printf ("Host File: $FabricAdminHostsFile\n");
	print color("reset");
	for($i=0; $i < scalar(@FabricAdminSteps); $i++)
	{
		$step = $FabricAdminSteps[$i];
		print_delimiter("$FabricAdminStepsMenuDelimiter{$step}");
		printf ("%x) %-48s ", $i, $FabricAdminStepsName{$step});
		printStepSelected($selected{$step}, $statusMessage{$step});
	}

	printf ("\nP) Perform the Selected Actions              N) Select None\n");
	printf (  "X) Return to Previous Menu (or ESC or Q)\n");
			
	%statusMessage = ();

	$inp = getch();
	
	if ($inp =~ /[qQ]/ || $inp =~ /[xX]/ || ord($inp) == $KEY_ESC)
	{
		return;
	}

	if ($inp =~ /[nN]/ )
	{
		foreach $step ( @FabricAdminSteps )
		{
			$selected{$step}= 0;
		}
	}
	elsif ($inp =~ /[0123456789abcdefABCDEF]/)
	{
		$step = $FabricAdminSteps[hex($inp)];
		$selected{$step}= ! $selected{$step};
	}
	elsif ($inp =~ /[pP]/)
	{
		# perform the fabric admin
		foreach $step ( @FabricAdminSteps )
		{
			if ($selected{$step} )
			{
				print LOG_FD "Performing Host Admin: $FabricAdminStepsName{$step}\n";
				print "\nPerforming Host Admin: $FabricAdminStepsName{$step}\n";
				$result = eval "fabricadmin_$step";
				$selected{$step} = 0;
				if ( $result )
				{
					goto DO_SETUP
				}
			}
		}
	}

	goto DO_SETUP;
}

# Fabric Monitor
sub fabricmonitor_opatop
{
	return run_fabric_cmd("$BIN_DIR/opatop");
}
#sub fabric_monitor
#{
#	my $result;
#	my $inp;
#	my %selected = ();
#	my %statusMessage = (); # TBD - not used
#	my $step;
#	my $i;
#
#	foreach $step ( @FabricMonitorSteps )
#	{
#		$selected{$step}= 0;
#	}
#DO_SETUP:
#	system "clear";
#	print color("bold");
#	printf ("FastFabric OPA Fabric Monitoring Menu\n\n");
#	print color("reset");
#	for($i=0; $i < scalar(@FabricMonitorSteps); $i++)
#	{
#		$step = $FabricMonitorSteps[$i];
#		print_delimiter("$FabricMonitorStepsMenuDelimiter{$step}");
#		printf ("%x) %-41s ", $i, $FabricMonitorStepsName{$step});
#		printStepSelected($selected{$step}, $statusMessage{$step});
#	}
#
#	printf ("\nP) Perform the Selected Actions              N) Select None\n");
#	printf (  "X) Return to Previous Menu (or ESC or Q)\n");
#			
#	%statusMessage = ();
#
#	$inp = getch();
#	
#	if ($inp =~ /[qQ]/ || $inp =~ /[xX]/ || ord($inp) == $KEY_ESC)
#	{
#		return;
#	}
#
#	if ($inp =~ /[nN]/ )
#	{
#		foreach $step ( @FabricMonitorSteps )
#		{
#			$selected{$step}= 0;
#		}
#	}
#	elsif ($inp =~ /[0123456789abcdefABCDEF]/)
#	{
#		$step = $FabricMonitorSteps[hex($inp)];
#		$selected{$step}= ! $selected{$step};
#	}
#	elsif ($inp =~ /[pP]/)
#	{
#		# perform the fabric monitor
#		foreach $step ( @FabricMonitorSteps )
#		{
#			if ($selected{$step} )
#			{
#				print LOG_FD "Performing Fabric Monitoring: $FabricMonitorStepsName{$step}\n";
#				print "\nPerforming Fabric Monitoring: $FabricMonitorStepsName{$step}\n";
#				$result = eval "fabricmonitor_$step";
#				$selected{$step} = 0;
#				if ( $result )
#				{
#					goto DO_SETUP
#				}
#			}
#		}
#	}
#
#	goto DO_SETUP;
#}
#
#sub chassis_config
#{
#	my $inp;
#	my $file;
#
#	print "Using $Editor (to select a different editor, export EDITOR).\n";
#	do {
#		if (setup_ffconfig() ) {
#			return 1;
#		}
#		if (setup_ffports() ) {
#			return 1;
#		}
#		do 
#		{
#			print "The FastFabric operations which follow will require a file\n";
#			print "listing the chassis to operate on\n";
#			print "Select Chassis File to Use/Edit [$FabricChassisFile]: ";
#			chomp($inp = <STDIN>);
#			$inp=remove_whitespace($inp);
#
#			if ( length($inp) == 0 ) 
#			{
#				$file = $FabricChassisFile;
#			} else {
#				$file = $inp;
#			}
#			print "About to: $Editor $file\n";
#			if ( HitKeyContAbortable() ) {
#				return 1;
#			}
#			system("$Editor '$file'");
#			if ( ! -e "$file" ) {
#				print "You must create a Chassis File to proceed\n\n";
#			} else {
#				$FabricChassisFile=$file;
#			}
#		} until ( -e "$file");
#		print "Selected Chassis File: $FabricChassisFile\n";
#	} until (! GetYesNo("Do you want to edit/review/change the files?", "y") );
#	print LOG_FD "Selected Chassis File -> $FabricChassisFile\n";
#	return 0;
#}

# expand directory list, expands any ~ characters
sub expand_pathnames
{
	my $pathnames = $_[0];
	my $all_pathnames="";
	my $first=1;
	foreach my $pathname (split(/[[:space:]]+/, "$pathnames")) {
		# file or directory (can be wildcards)
		# expand directory, also filters files without .dpkg or .spkg suffix
		my $expanded = `echo -n $pathname`;
		if ( $expanded ne "" ) {
			if ( $first ) {
				$all_pathnames="$expanded";
				$first=0;
			} else {
				$all_pathnames="$all_pathnames $expanded";
			}
		}
	}
	return "$all_pathnames";
}

# expand package list, returns "" if "none" or has invalid entries
sub chassis_expand_fwpackages
{
	my $packages = $_[0];
	my $all_packages="";
	if ( "$packages" eq "none" ) {
		return "";
	}
	foreach my $package (split(/[[:space:]]+/, "$packages")) {
		# file or directory (can be wildcards)
		# expand directory, also filters files without .dpkg or .spkg suffix
		my $expanded = `find $package -type f -name '*.[ds]pkg' 2>/dev/null`;
		if ( $expanded eq "" ) {
			print "$package: No .dpkg nor .spkg files found\n";
			return "";
		}
		$all_packages="$all_packages$expanded";
	}
	return "$all_packages";
}

sub chassis_showallports
{
	my $linkanalysis=0;
	my $linkanalysis_opts="";
	my $linkanalysis_reports="";
	my $Sopt="";
	my $result_dir = read_ffconfig_param("FF_RESULT_DIR");
	my $linkanalysis_file = "$result_dir/linkanalysis.res";
	my $inp;

	if (GetYesNo("Would you like to perform fabric error analysis?", "y") ) {
		$linkanalysis=1;
		$linkanalysis_reports="$linkanalysis_reports errors";
#		if (GetYesNo("Clear error counters after generating report?", "n") ) {
#			$linkanalysis_reports="$linkanalysis_reports clearerrors";
#			if (GetYesNo("Force Clear of hardware error counters after generating report?", "n") ) {
#				$linkanalysis_reports="$linkanalysis_reports clearhwerrors";
#			}
#		}
	}
	if (GetYesNo("Would you like to perform fabric link speed error analysis?", "y") ) {
		$linkanalysis=1;
		$linkanalysis_reports="$linkanalysis_reports slowlinks";
		if (GetYesNo("Check for links configured to run slower than supported?", "n") ) {
			$linkanalysis_reports="$linkanalysis_reports misconfiglinks";
		}
		if (GetYesNo("Check for links connected with mismatched speed potential?", "n") ) {
			$linkanalysis_reports="$linkanalysis_reports misconnlinks";
		}
	}

	do {
		print "Enter filename for results [$linkanalysis_file]: ";
		chomp($inp = <STDIN>);
		$inp=remove_whitespace($inp);
		if (length($inp) == 0) {
			$inp = $linkanalysis_file;
		}
		$inp = SanitizeFilename($inp);
	} while (length($inp) == 0);
	$linkanalysis_file = $inp;
	if ($linkanalysis) {
		#my $snapshot_suffix=`date +%Y%m%d-%H%M%S`;
		#chop $snapshot_suffix;
		if ( run_fabric_cmd("$BIN_DIR/ethlinkanalysis $linkanalysis_opts $linkanalysis_reports > $linkanalysis_file 2>&1", "skip_prompt") ) {
			return 1;
		}
	} else  {
		if (! valid_config_file("Chassis File", $FabricChassisFile) ) {
			return 1;
		}
		if (GetYesNo("Would you like to be prompted for chassis' password?", "n") ) {
			$Sopt="-S";
		}
		if ( run_fabric_cmd("$BIN_DIR/ethshowallports -C $Sopt -F $FabricChassisFile > $linkanalysis_file 2>&1", "skip_prompt") ) {
			return 1;
		}
	}
	print "About to: $Editor $linkanalysis_file\n";
	if ( HitKeyContAbortable() ) {
		return 1;
	}
	system("$Editor '$linkanalysis_file'");
	return 0;
}

sub show_menu
{
	my $inp;
	my $max_inp = 2;

START:	
	system "clear";
	print color("bold");
	printf ("$BRAND Ethernet FastFabric Tools\n");
	printf ("Version: $VERSION\n\n");
	print color("reset");
	printf ("   1) Host Setup\n");
	printf ("   2) Host Verification/Admin\n");
	#printf ("   3) Fabric Monitoring\n");
	printf ("\n   X) Exit (or Q)\n");

	$inp = getch();      

	if ($inp =~ /[qQ]/ || $inp =~ /[Xx]/ ) {
		die "Exiting\n";
	}
	if (ord($inp) == $KEY_ENTER) {           
		goto START;
	}

	if ($inp =~ /[0123456789abcdefABCDEF]/)
	{
		$inp = hex($inp);
	}

	if ($inp < 1 || $inp > $max_inp) 
	{
#		printf ("Invalid choice...Try again\n");
		goto START;
	}
	$MENU_CHOICE = $inp;
}

process_args;
check_user;
open_log;
set_libdir;

RESTART:
show_menu;

if ($MENU_CHOICE == 1) 
{
	# Host Setup
	fabric_setup;
	goto RESTART;
}
elsif ($MENU_CHOICE == 2) 
{
	# Host Verification/Admin
	fabric_admin;
	goto RESTART;
}
#elsif ($MENU_CHOICE == 3) 
#{
#	# Fabric Monitoring
#	fabric_monitor;
#	goto RESTART;
#}
close_log;
