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

mkdir -p ${DESTDIR}/usr/bin
mkdir -p ${DESTDIR}/usr/sbin
mkdir -p ${DESTDIR}/usr/lib/eth-tools
mkdir -p ${DESTDIR}/usr/share/eth-tools/{samples,help}
#mkdir -p ${DESTDIR}/${DSAP_LIBDIR}/ibacm
mkdir -p ${DESTDIR}/etc/rdma
mkdir -p ${DESTDIR}/etc/eth-tools
#mkdir -p ${DESTDIR}/etc/cron.d
#mkdir -p ${DESTDIR}/usr/include/infiniband
mkdir -p ${DESTDIR}/usr/share/man/man1
mkdir -p ${DESTDIR}/usr/share/man/man8
mkdir -p ${DESTDIR}/usr/src/eth/mpi_apps

#Binaries and scripts installing (basic tools)
#cd builtbin.OPENIB_FF.release
cd $(cat ${BUILDDIR}/RELEASE_PATH)

cd bin
[ -n "$basic_tools_sbin" ] && cp -t ${DESTDIR}/usr/sbin $basic_tools_sbin
[ -n "$basic_tools_opt" ] && cp -t ${DESTDIR}/usr/lib/eth-tools/ $basic_tools_opt
#ln -s ./opaportinfo ${DESTDIR}/usr/sbin/opaportconfig

cd ../bin
[ -n "$ff_tools_opt" ] && cp -t ${DESTDIR}/usr/lib/eth-tools/ $ff_tools_opt
[ -n "$opasnapconfig_bin" ] && cp -t ${DESTDIR}/usr/lib/eth-tools/ $opasnapconfig_bin

cd ../fastfabric
[ -n "$ff_tools_sbin" ] && cp -t ${DESTDIR}/usr/sbin $ff_tools_sbin
[ -n "$ff_tools_misc" ] && cp -t ${DESTDIR}/usr/lib/eth-tools/ $ff_tools_misc
[ -n "$help_doc" ] && cp -t ${DESTDIR}/usr/share/eth-tools/help $help_doc
[ -n "$basic_configs" ] && cp -t ${DESTDIR}/etc/eth-tools $basic_configs

cd ../etc
#cd cron.d
#cp -t ${DESTDIR}/etc/cron.d opa-cablehealth
#cd ..

cd ../fastfabric/samples
([ -n "$ff_iba_samples" ] || [ -n "$basic_samples" ]) && cp -t ${DESTDIR}/usr/share/eth-tools/samples $ff_iba_samples $basic_samples
cd ..

cd ../fastfabric/tools
[ -n "$ff_tools_exp" ] && cp -t ${DESTDIR}/usr/lib/eth-tools/ $ff_tools_exp
[ -n "$ff_libs_misc" ] && cp -t ${DESTDIR}/usr/lib/eth-tools/ $ff_libs_misc
cp -t ${DESTDIR}/usr/lib/eth-tools/ osid_wrapper
cp -t ${DESTDIR}/etc/eth-tools allhosts chassis hosts switches
cd ..

cd ../man/man1
[ -n "$basic_mans" ] && cp -t ${DESTDIR}/usr/share/man/man1 $basic_mans
[ -n "$opasadb_mans" ] && cp -t ${DESTDIR}/usr/share/man/man1 $opasadb_mans
cd ../man8
[ -n "$ff_mans" ] && cp -t ${DESTDIR}/usr/share/man/man8 $ff_mans
cd ..

cd ../src/mpi/mpi_apps
tar -xzf mpi_apps.tgz -C ${DESTDIR}/usr/src/eth/mpi_apps/
cd ../../

#Config files
cd ../config
cp -t ${DESTDIR}/etc/eth-tools ethmon.conf ethmon.si.conf

#Libraries installing
#cd ../builtlibs.OPENIB_FF.release
cd $(cat $BUILDDIR/LIB_PATH)


# Now that we've put everything in the buildroot, copy any default config files to their expected location for user
# to edit. To prevent nuking existing user configs, the files section of this spec file will reference these as noreplace
# config files.
cp ${DESTDIR}/usr/lib/eth-tools/ethfastfabric.conf.def ${DESTDIR}/etc/eth-tools/ethfastfabric.conf

