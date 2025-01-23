#!/bin/bash
set -x

[ -z "${BUILDDIR}" ] && BUILDDIR="."
[ -z "${DESTDIR}" ] && DESTDIR="/"
[ -z "${LIBDIR}" ] && LIBDIR=/usr/lib
#[ -z "${DSAP_LIBDIR}" ] && DSAP_LIBDIR="$DSAP_LIBDIR"
#
#if [ -n ${DSAP_LIBDIR}]; then
#	DSAP_LIBDIR="/usr/lib"
#fi

if [ ! -f "${BUILDDIR}/RELEASE_PATH" ]; then
    echo "Wrong BUILDDIR? No such file ${BUILDDIR}/RELEASE_PATH"
    exit 1
fi

source OpenIb_Host/ff_filegroups.sh

release_string="IntelEth-Tools-FF.$BUILD_TARGET_OS_ID.$MODULEVERSION"
sbindir="$(rpm --eval "%{_sbindir}" | sed -e 's/^\///')"
bindir="$(rpm --eval "%{_bindir}" | sed -e 's/^\///')"
prefix="$(rpm --eval "%{_prefix}" | sed -e 's/^\///')"
mandir="$(rpm --eval "%{_mandir}" | sed -e 's/^\///')"
datadir="$(rpm --eval "%{_datadir}" | sed -e 's/^\///')"
usrsrc="$(rpm --eval "%{_usrsrc}" | sed -e 's/^\///')"
sysconfdir="$(rpm --eval "%{_sysconfdir}" | sed -e 's/^\///')"

mkdir -p ${DESTDIR}/${bindir}
mkdir -p ${DESTDIR}/${sbindir}
mkdir -p ${DESTDIR}/${prefix}/lib/eth-tools
mkdir -p ${DESTDIR}/${datadir}/eth-tools/{samples,help}
#mkdir -p ${DESTDIR}/${DSAP_LIBDIR}/ibacm
mkdir -p ${DESTDIR}/${sysconfdir}/rdma
mkdir -p ${DESTDIR}/${sysconfdir}/eth-tools
#mkdir -p ${DESTDIR}/etc/cron.d
#mkdir -p ${DESTDIR}/usr/include/infiniband
mkdir -p ${DESTDIR}/${mandir}/man1
mkdir -p ${DESTDIR}/${mandir}/man8
mkdir -p ${DESTDIR}/${usrsrc}/eth/mpi_apps

#Binaries and scripts installing (basic tools)
#cd builtbin.OPENIB_FF.release
cd $(cat ${BUILDDIR}/RELEASE_PATH)

cd bin
[ -n "$basic_tools_sbin" ] && cp -t ${DESTDIR}/${sbindir} $basic_tools_sbin
[ -n "$basic_tools_opt" ] && cp -t ${DESTDIR}/${prefix}/lib/eth-tools/ $basic_tools_opt
#ln -s ./opaportinfo ${DESTDIR}/usr/sbin/opaportconfig

cd ../bin
[ -n "$ff_tools_opt" ] && cp -t ${DESTDIR}/${prefix}/lib/eth-tools/ $ff_tools_opt
[ -n "$opasnapconfig_bin" ] && cp -t ${DESTDIR}/${prefix}/lib/eth-tools/ $opasnapconfig_bin

cd ../fastfabric
[ -n "$ff_tools_sbin" ] && cp -t ${DESTDIR}/${sbindir} $ff_tools_sbin
[ -n "$ff_tools_misc" ] && cp -t ${DESTDIR}/${prefix}/lib/eth-tools/ $ff_tools_misc
[ -n "$help_doc" ] && cp -t ${DESTDIR}/${datadir}/eth-tools/help $help_doc
[ -n "$basic_configs" ] && cp -t ${DESTDIR}/${sysconfdir}/eth-tools $basic_configs

cd ../etc
#cd cron.d
#cp -t ${DESTDIR}/etc/cron.d opa-cablehealth
#cd ..

cd ../fastfabric/samples
([ -n "$ff_iba_samples" ] || [ -n "$basic_samples" ]) && cp -t ${DESTDIR}/${datadir}/eth-tools/samples $ff_iba_samples $basic_samples
cd ..

cd ../fastfabric/tools
[ -n "$ff_tools_exp" ] && cp -t ${DESTDIR}/${prefix}/lib/eth-tools/ $ff_tools_exp
[ -n "$ff_libs_misc" ] && cp -t ${DESTDIR}/${prefix}/lib/eth-tools/ $ff_libs_misc
cp -t ${DESTDIR}/${prefix}/lib/eth-tools/ osid_wrapper
cp -t ${DESTDIR}/${sysconfdir}/eth-tools allhosts hosts switches
cd ..

cd ../man/man1
[ -n "$basic_mans" ] && cp -t ${DESTDIR}/${mandir}/man1 $basic_mans
[ -n "$opasadb_mans" ] && cp -t ${DESTDIR}/${mandir}/man1 $opasadb_mans
cd ../man8
[ -n "$ff_mans" ] && cp -t ${DESTDIR}/${mandir}/man8 $ff_mans
cd ..

cd ../src/mpi/mpi_apps
tar -xzf mpi_apps.tgz -C ${DESTDIR}/${usrsrc}/eth/mpi_apps/
cd ../../

#Config files
cd ../config
cp -t ${DESTDIR}/${sysconfdir}/eth-tools ethmon.conf ethmon.si.conf

#Libraries installing
#cd ../builtlibs.OPENIB_FF.release
cd $(cat $BUILDDIR/LIB_PATH)


# Now that we've put everything in the buildroot, copy any default config files to their expected location for user
# to edit. To prevent nuking existing user configs, the files section of this spec file will reference these as noreplace
# config files.
cp ${DESTDIR}/usr/lib/eth-tools/ethfastfabric.conf.def ${DESTDIR}/${sysconfdir}/eth-tools/ethfastfabric.conf

