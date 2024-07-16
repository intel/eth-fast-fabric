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

# ============================================================================
# Driver file management

my $RunDepmod=0;	# do we need to run depmod
sub	verify_modtools()
{
	my $look_str;
	my $look_len;
	my $ver;
	my $mod_ver;

	$look_str = "insmod version ";
	$look_len = length ($look_str);
	$ver = `/sbin/insmod -V 2>&1 | grep \"$look_str\"`;
	chomp $ver;
	$mod_ver = substr ($ver, $look_len - 1, length($ver)-$look_len+1);
	$_ = $mod_ver;
	/2\.[34]\.[0-9]+/;
	#$MOD_UTILS_REV;
	if ($& eq "")
	{
		NormalPrint "Unable to proceed, modutils tool old: $mod_ver\n";
		Abort "Install newer version of modutils 2.3.15 or greater" ;
	}
}

sub check_depmod()
{
	if ($RunDepmod == 1 )
	{
		print_separator;
		print "Generating module dependencies...\n";
		LogPrint "Generating module dependencies: /sbin/depmod -aev\n";
		system "/sbin/depmod -aev > /dev/null 2>&1";
		$RunDepmod=0;
		return 1;
	}
	return 0;
}

sub find_shared_lib
{
	# 1 - name of shared library file

	my ($libname) = shift();

	# search in standard location using ld cache
	if (0 == system("ldconfig -p | grep -q $libname")) {
		return 0;
	}

	# not found, try additional paths in env
	my $pathstring = $ENV{'LD_LIBRARY_PATH'};
	if ( "$pathstring" ne "" ) {
		my @pathlist = split(":", $pathstring);
		foreach my $dir ( @pathlist )
		{
			if ( -e "$dir/$libname" ) {
				return 0;
			}
		}
	}

	return 1;

}

sub get_gpu_choice
{
	# 1 - directory for search match
	# 2 - directory for regex version extract
	# 3 - brand
	# 4 - Gpu_Dir to be returned - real directory of Module.symver or "CURRENT_KERNEL" or ""

	my ($search_dir) = shift();
	my ($regex_dir) = shift();
	my ($brand) = shift();
	my $num_versions;
	my @gpu_versions;
	my @gpu_symvers;
	my $i = 0;;
	my $inp = 0;
	my $retry_inp = 0;
	my $gpu_dir = "";

	@gpu_symvers=`find /usr/src/ -name Module.symvers | grep $search_dir | sort -Vr`;
	$num_versions = scalar @gpu_symvers;
	if  ( $num_versions > 1 ) {
		# Multiple versions discovered; arrange a sorted list of versions (latest first)
		printf "Multiple versions of $brand GPU support discovered; please select the version to use:\n", $brand;
		for($i = 0; $i < $num_versions; $i++){
			my $s = @gpu_symvers[$i];
			$s =~ /($regex_dir-[-A-Za-z0-9\.]+)/;
			@gpu_versions[$i] = $1;
			printf "%d -  %s\n", $i+1, @gpu_versions[$i];
		}
		$inp = GetNumericValue("Please enter a number 1-$num_versions:", 1, 1, $num_versions);
		NormalPrint("\nSelected $inp: @gpu_versions[$inp - 1]\n");
		sleep 2; # give user a chance to see selection
		# Make selection, subtract one from index since numbering started at 1
		$gpu_dir = dirname(@gpu_symvers[$inp - 1]);
		chomp($gpu_dir);
	} else {
		# Just one version discovered; just use it
		chomp($gpu_dir = `find /usr/src/ -name Module.symvers | grep dmabuf | xargs dirname`);
	}

	$_[0] = $gpu_dir;
}

sub get_kernel_module_for_intel_gpu
{
	# 1 - kernel ver
	# 2 - Gpu_Install to be returned - "INTEL_GPU", "NV_VPU", or "NONE"
	# 3 - Gpu_Dir to be returned - real directory of Module.symver or "CURRENT_KERNEL" or ""

	my ($Kver) = shift();
	my $gpu_install;
	my $gpu_dir = "";
	my $retval = 1;

	if ((0 == find_shared_lib("libze_loader.so.1")) && (0 == find_shared_lib("libze_intel_gpu.so.1"))) {
		# use Module.symvers if present, else CURRENT_KERNEL
		if (GetYesNo("Support for Intel GPU discovered - install for Intel GPU?", "y") == 1) {
			if (0 == system("find /usr/src/ -name Module.symvers | grep -iq dmabuf")) {
				if ( $Default_Prompt ) {
					# for non-interactive just get the latest version
					chomp($gpu_dir = `find /usr/src/ -name Module.symvers | grep dmabuf | sort -V | tail -1 | xargs dirname`);
				} else {
					# for - interactive, check for multiple versions then give a choice if so
					get_gpu_choice("dmabuf", "intel-dmabuf", "Intel", $gpu_dir);
				}
			} else {
				$gpu_dir = "CURRENT_KERNEL";
			}
			$gpu_install = "INTEL_GPU";
		} else {
			$gpu_install = "NONE";
			$retval = 0;
		}
	} else {
		$gpu_install = "NONE";
		$retval = 0;
	}

	$_[0] = $gpu_install;
	$_[1] = $gpu_dir;

	return $retval;
}

sub get_kernel_module_for_nv_gpu
{
	# 1 - kernel ver
	# 2 - Gpu_Install to be returned - "INTEL_GPU", "NV_VPU", or "NONE"
	# 3 - Gpu_Dir to be returned - real directory of Module.symver or "CURRENT_KERNEL" or ""

	my ($Kver) = shift();
	my $gpu_install;
	my $gpu_dir = "";
	my $retval = 1;

	# use Module.symvers if present
	if (0 == system("find /usr/src/ -name Module.symvers | grep -iq nvidia")) {
		if (GetYesNo("Support for NVIDIA GPU discovered - install for NVIDIA GPU?", "y") == 1) {
			if ( $Default_Prompt ) {
				# for non-interactive just get the latest version
				chomp($gpu_dir = `find /usr/src/ -name Module.symvers | grep nvidia | sort -V | tail -1 | xargs dirname`);
			} else {
				# for - interactive, check for multiple versions then give a choice if so
				get_gpu_choice("nvidia", "nvidia", "NVIDIA", $gpu_dir);
			}
			$gpu_install = "NV_GPU";
		} else {
			$gpu_install = "NONE";
			$retval = 0;
		}
	} else {
		$gpu_install = "NONE";
		$retval = 0;
	}

	$_[0] = $gpu_install;
	$_[1] = $gpu_dir;

	return $retval;
}
