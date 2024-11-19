Name: eth-tools
Version: 11.7.0.2
Release: 2%{?dist}
Summary: Intel Ethernet Fabric Suite basic tools and libraries for fabric management

License: BSD-3-Clause
Url: https://github.com/intel/eth-fast-fabric
Source: %url/releases/download/v%{version_no_tilde}/eth-fast-fabric-%{version_no_tilde}.tar.gz
ExclusiveArch: x86_64
# The Intel(R) Ethernet Fabric Suite product line is only available on x86_64 platforms at this time.

Epoch: 1

%description
This package contains the tools necessary to manage an Intel Ethernet fabric.

%package basic
Summary: Management level tools and scripts

Requires: rdma bc

Requires: expect%{?_isa}, tcl%{?_isa}, libibverbs-utils%{?_isa}, librdmacm-utils%{?_isa}, net-snmp%{?_isa}, net-snmp-utils%{?_isa}
BuildRequires: make
BuildRequires: expat-devel
BuildRequires: gcc-c++
BuildRequires: tcl-devel
BuildRequires: rdma-core-devel
BuildRequires: net-snmp-devel


%description basic
Contains basic tools for fabric management necessary on all compute nodes.

%package fastfabric
Summary: Management level tools and scripts
Requires: eth-tools-basic%{?_isa} >= %{version}-%{release}

BuildRequires: perl-generators

%description fastfabric
Contains tools for managing fabric on a management node.

%prep
%autosetup -cn eth-fast-fabric-%{version_no_tilde}

%build
cd OpenIb_Host
OPA_FEATURE_SET=opa10 CLOCAL='%build_cflags' CCLOCAL='%build_cxxflags' LDLOCAL='%build_ldflags' ./ff_build.sh %{_builddir}

%install
BUILDDIR=%{_builddir} DESTDIR=%{buildroot} LIBDIR=%{_prefix}/lib DSAP_LIBDIR=%{_libdir} OpenIb_Host/ff_install.sh

%files basic
%dir %{_sysconfdir}/eth-tools/
%{_sbindir}/ethbw
%{_sbindir}/ethcapture
%{_sbindir}/ethshmcleanup
%{_prefix}/lib/eth-tools/setup_self_ssh
%{_prefix}/lib/eth-tools/usemem
%{_prefix}/lib/eth-tools/ethipcalc
%{_prefix}/lib/eth-tools/stream
%{_prefix}/lib/eth-tools/ethudstress
%{_mandir}/man1/ethbw.1*
%{_mandir}/man1/ethcapture.1*
%{_mandir}/man1/ethshmcleanup.1*
%{_datadir}/eth-tools/samples/dsa_setup
%{_datadir}/eth-tools/samples/dsa.service
%{_datadir}/eth-tools/samples/mgt_config.xml-sample
%config(noreplace) %{_sysconfdir}/eth-tools/mgt_config.xml

%files fastfabric
%{_sbindir}/*
%exclude %{_sbindir}/ethbw
%exclude %{_sbindir}/ethcapture
%exclude %{_sbindir}/ethshmcleanup
%{_prefix}/lib/eth-tools/*
%exclude %{_prefix}/lib/eth-tools/setup_self_ssh
%exclude %{_prefix}/lib/eth-tools/usemem
%exclude %{_prefix}/lib/eth-tools/ethipcalc
%exclude %{_prefix}/lib/eth-tools/stream
%exclude %{_prefix}/lib/eth-tools/ethudstress
%{_datadir}/eth-tools/*
%exclude %{_datadir}/eth-tools/samples/dsa_setup
%exclude %{_datadir}/eth-tools/samples/dsa.service
%exclude %{_datadir}/eth-tools/samples/mgt_config.xml-sample
%{_mandir}/man8/eth*.8*
%{_usrsrc}/eth/*
%config(noreplace) %{_sysconfdir}/eth-tools/ethfastfabric.conf
%config(noreplace) %{_sysconfdir}/eth-tools/ethmon.conf
%config(noreplace) %{_sysconfdir}/eth-tools/allhosts
%config(noreplace) %{_sysconfdir}/eth-tools/hosts
%config(noreplace) %{_sysconfdir}/eth-tools/switches
%{_sysconfdir}/eth-tools/ethmon.si.conf
%config(noreplace) /usr/lib/eth-tools/osid_wrapper

%changelog
* Mon Nov 18 2024 Jijun Wang <jijun.wang@intel.com> - 11.7.0.2-2
- Fixed ethudstress cq poll regression issue

* Tue Jul 16 2024 Jijun Wang <jijun.wang@intel.com> - 11.7.0.0-110
- Improved to support RHEL 9.4/8.10 and SLES 15.6
- Merged duplicate OS prereq files for major OS versions where possible
- Added ability to compile IMB-MPI1-GPU with cuda support

* Tue Jul 16 2024 Jijun Wang <jijun.wang@intel.com> - 11.7.0.0-82
- Cleaned up not needed files
- Added config on unprivileged access to UserFaultFD
- Added GPU auto-detection feature
- Fixed permission and enumeration issues on dsa_setup
- Improved ethudstress stability to handle unmatched client/server config
- Improved MpiApps to allow concurrent mpi_apps runs with SLURM
- Fixed FI_PROVIDER_PATH overrode issue on mpi_apps

* Mon Mar 11 2024 Jijun Wang <jijun.wang@intel.com> - 11.6.0.0-223
- Updated host verification to check UD QPs, and skip VT-d disabled
  check by default
- Updated compiler options to enhance security of FastFabric
- Improved MpiApps README file
- Improved help for tools ethbw and ethextract*
- Fixed HPL build for newer version oneAPI

* Mon Mar 11 2024 Jijun Wang <jijun.wang@intel.com> - 11.6.0.0-211
- Enhanced MpiApps scripts for easier use with SLURM
- Introduced PSM3_PRINT_STATS_PREFIX to specify PSM3 stats files location
- Improved MpiApps to support oneCCL benchmark
- Easy of use improvement on MpiApps
-- Improved parameter files and scripts description
-- Reorganized PSM3 parameter files
-- Improved script printout to include system env
-- Moved MpiApps parameter settings mostly into env vs on MPI cmd line

* Mon Mar 11 2024 Jijun Wang <jijun.wang@intel.com> - 11.6.0.0-189
- Improved to support RHEL 8.9 and RHEL 9.3
- Added dsa_setup man page

* Tue Oct 03 2023 Jijun Wang <jijun.wang@intel.com> - 11.5.1.0-3
- Improved help text and man page for all tools

* Tue Oct 03 2023 Jijun Wang <jijun.wang@intel.com> - 11.5.1.0-1
- Improved to support SLES 15.5
- Replaced fgrep and egrep with grep -F and grep -E
- Improved ethlinkanalysis/ethfabricanalysis to use first enabled
  plane when it's not specified
- Fixed version comparison issue on install script that happens on
  CUDA version components

* Thu Jun 01 2023 Jijun Wang <jijun.wang@intel.com> - 11.5.0.0-173
- Changed FastFabric package build to use dynamically generated
  perl dependencies for RedHat family OSes
- Fixed build issue caused by EXPORT_ALL_VARIABLES on Fedora 38

* Wed May 31 2023 Jijun Wang <jijun.wang@intel.com> - 11.5.0.0-167
- Improved to support RHEL 8.8 and 9.2
- Improved dsa_setup to support shared workqueue
- Cleaned OS detection code

* Wed May 31 2023 Jijun Wang <jijun.wang@intel.com> - 11.5.0.0-125
- Added build support on OpenCloud and Oracle Linux OSes
- Improved ethcapture to use journalctl if rsyslog not installed
- Improved to support Basic-IB package
- Refactoried package build scripts to better support different OSes

* Wed May 31 2023 Jijun Wang <jijun.wang@intel.com> - 11.5.0.0-89
- Changed script shebang to use /bin/bash rather than /bin/sh

* Wed May 31 2023 Jijun Wang <jijun.wang@intel.com> - 11.5.0.0-86
- Added tool dsa_setup to aid creation of DSA devices
- Added Ubuntu support on building fastfabric package

* Wed May 31 2023 Jijun Wang <jijun.wang@intel.com> - 11.5.0.0-73
- Improved to support RHEL 9.1
- Fixed RV rebuild issue under SLES 15.4 to support Intel GPU
- Fixed installation issue under Ubuntu when there are conflict files
- Improved stability on ethcabletest and ethfindgood

* Wed May 31 2023 Jijun Wang <jijun.wang@intel.com> - 11.5.0.0-53
- Improved to support RHEL 8.7
- Added Ubuntu support on RV and mpiapps
- Fixed minor bugs on nodeverify.sh, ethshowallports and ethfindgood
- Improved below tools to support fabric plane
-- ethcabletest.sh
-- ethshowallports.sh
-- ethhostadmin

* Thu Dec 1 2022 Jijun Wang <jijun.wang@intel.com> - 11.4.0.0-198
- Added Ubuntu support to INSTALL script
- Updated man pages for FastFabric tools

* Thu Dec 1 2022 Jijun Wang <jijun.wang@intel.com> - 11.4.0.0-194
- Changed FastFabric to setup PFC using software DCB (open lldp)
- Enhanced host verify to check passive mode intel_pstate, loopback rping

* Wed Nov 30 2022 Jijun Wang <jijun.wang@intel.com> - 11.4.0.0-144
- Fixed MpiApps build script to allow building HPL with Intel MPI and MKL
- Updated MpiApps scripts to pass exe params to commands

* Wed Nov 30 2022 Jijun Wang <jijun.wang@intel.com> - 11.4.0.0-139
- Removed dead code and unsupported tools

* Wed Nov 30 2022 Jijun Wang <jijun.wang@intel.com> - 11.4.0.0-137
- Introduced tool ethbw to monitor BW per NIC
- Added support on Intel GPU

* Wed Nov 30 2022 Jijun Wang <jijun.wang@intel.com> - 11.4.0.0-124
- Improved to support RHEL 9.0
- Improved FastFabric to support fabric plane
- improved ethreport with below new features:
-- -P/-H: include only persist or hardware data
-- -A/-o otherports: include all or other (inactive) ports
-- -s: include performance data
- Improved node verification to check NIC firmware and driver version

* Wed Nov 30 2022 Jijun Wang <jijun.wang@intel.com> - 11.4.0.0-81
- Improved to support SLES 15.4
- Improved performance on ethreport, rping and pfctest

* Wed Nov 30 2022 Jijun Wang <jijun.wang@intel.com> - 11.4.0.0-55
- Improved ethreport to support more than 256 ports

* Mon Jun 13 2022 Jijun Wang <jijun.wang@intel.com> - 11.3.0.0-130
- Improved to support RHEL 8.6
- Improved to enable almalinux and circle Linux
- Added tool ethshmcleanup.sh for obsoleted shm file clean up
- Update MpiApps to use OSU 5.9

* Mon Jun 13 2022 Jijun Wang <jijun.wang@intel.com> - 11.2.0.0-259
- Improved to support RHEL 8.5

* Mon Jan 10 2022 Jijun Wang <jijun.wang@intel.com> - 11.1.0.1-7
- Fixed false negative issue on ethfindgood

* Fri Jan 07 2022 Jijun Wang <jijun.wang@intel.com> - 11.1.0.1-6
- Improved ethfindgood to be more stable on finding irdma dev name

* Mon Nov 01 2021 Jijun Wang <jijun.wang@intel.com> - 11.1.0.1-3
- Enabled builds on Rocky Linux

* Mon Aug 16 2021 Jijun Wang <jijun.wang@intel.com> - 11.1.0.0-179
- Renamed 'chassis' to 'switch'
- Added suport on GPU and PortID
- Added empirical PFC testing

* Thu Aug 12 2021 Jijun Wang <jijun.wang@intel.com> - 11.0.0.0-166
- Added perl as Requires for fastfabric package

* Thu Aug 12 2021 Jijun Wang <jijun.wang@intel.com> - 11.2.0.0-61
- Added perl as Requires for fastfabric package

* Fri Feb 05 2021 Jijun Wang <jijun.wang@intel.com> - 11.0.0.0-163
- Cleaned up for upstream

* Mon Feb 26 2018 Jijun Wang <jijun.wang@intel.com> - 10.8.0.0-1
- Added epoch for RHEL address-resolution, basic and fastfabric
- Added component information in description for all rpms

* Thu Apr 13 2017 Scott Breyer <scott.j.breyer@intel.com> - 10.5.0.0-1
- Updates for spec file cleanup

* Fri Oct 10 2014 Erik E. Kahn <erik.kahn@intel.com> - 1.0.0-ifs-1
- Initial version

