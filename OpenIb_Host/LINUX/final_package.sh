#!/bin/bash

set -e
mkdir -p ${RELEASE_DIR}
cd ${RPMDIR}
packaged_files_path=${RPM_BUILD_DIR}/eth-fast-fabric-${MKRPM_VER}/packaged_files
dist_files_path=${RPM_BUILD_DIR}/eth-fast-fabric-${MKRPM_VER}/dist_files
dir=`grep 'IntelEth-Tools-FF\.' ${packaged_files_path} | xargs dirname`
filename=`grep 'IntelEth-Tools-FF\.' ${packaged_files_path} | xargs basename`
subdir=${filename%.tgz}
rpmdir=$dir/$subdir/${BIN_OUTPUT_DIR}/${DIR_ARCH}
srpmdir=$dir/$subdir/${SRC_OUTPUT_DIR}
basicdir=`grep 'IntelEth-Tools\.' ${packaged_files_path} | xargs dirname`
basic=`grep 'IntelEth-Tools\.' ${packaged_files_path} | xargs basename`
basicrpmdir=$basicdir/${basic%.tgz}/${BIN_OUTPUT_DIR}/${DIR_ARCH}
basicsrpmdir=$basicdir/${basic%.tgz}/${SRC_OUTPUT_DIR}
mkdir -p $srpmdir
mkdir -p $rpmdir
rm -rf ${basicdir}/${basic} ${basicdir}/${basic%.tgz}
cp -rp ${dir}/${subdir} ${basicdir}/${basic%.tgz}

# all "eval" trickery due to the fact that ${XXX_EXT} can contain things like {.ext1,.ext2,.ext3}
mpiapps_src=$(eval echo ${SRPMSDIR}/eth-mpi-apps${SEPARATOR}${MKRPM_VER}-${MKRPM_REL}${SRPM_EXT})
mpiapps_bin=$(eval echo ${RPMSDIR}/eth-mpi-apps${SEPARATOR}${MKRPM_VER}-${MKRPM_REL}${RPM_EXT})

ff_src=$(eval echo ${SRPMSDIR}/eth-tools${SEPARATOR}${MKRPM_VER}-${MKRPM_REL}${SRPM_EXT})
ff_basic_bin=$(eval echo ${RPMSDIR}/eth-tools-basic${SEPARATOR}${MKRPM_VER}-${MKRPM_REL}${RPM_EXT})
ff_bin=$(eval echo ${RPMSDIR}/eth-tools-fastfabric${SEPARATOR}${MKRPM_VER}-${MKRPM_REL}${RPM_EXT})

cp ${mpiapps_src}  $srpmdir/
cp ${ff_src}       $srpmdir/
cp ${ff_src}       $basicsrpmdir/
cp ${ff_basic_bin} $rpmdir/
cp ${ff_bin}       $rpmdir/
cp ${ff_basic_bin} $basicrpmdir/
cp ${mpiapps_bin} $rpmdir/

if [ "${OPA_FEATURE_SET}" != "opa10" ]; then
	snapconfig=$(eval echo ${RPMSDIR}/opa-snapconfig${SEPARATOR}${MKRPM_VER}-${MKRPM_REL}${RPM_EXT})
	cp ${snapconfig} $dir/
	cp ${snapconfig} ${RELEASE_DIR}/
fi
if [ "${BUILD_TARGET_OS_VENDOR}" = "redhat" ]; then
	rh_debuginfo=$(eval echo ${RPMSDIR}/eth-tools-${DEBUGINFO}${SEPARATOR}${MKRPM_VER}-${MKRPM_REL}${DEBUG_EXT} )
	cp ${rh_debuginfo} $rpmdir/
fi
if [ "${BUILD_TARGET_OS_VENDOR}" = "SuSE" ]; then
	suse_debuginfo=$(eval echo ${RPMSDIR}/eth-tools-debugsource${SEPARATOR}${MKRPM_VER}-${MKRPM_REL}${RPM_EXT})
	cp ${suse_debuginfo} $rpmdir/
fi
if [ "${BUILD_TARGET_OS_VENDOR}" = "SuSE" -o "${BUILD_TARGET_OS_VENDOR}" = "ubuntu" ]; then
	basic_debuginfo=$(eval echo ${RPMSDIR}/eth-tools-basic-${DEBUGINFO}${SEPARATOR}${MKRPM_VER}-${MKRPM_REL}${DEBUG_EXT})
	ff_debuginfo=$(eval echo ${RPMSDIR}/eth-tools-fastfabric-${DEBUGINFO}${SEPARATOR}${MKRPM_VER}-${MKRPM_REL}${DEBUG_EXT})
	cp ${basic_debuginfo} $rpmdir/
	cp ${basic_debuginfo} $basicrpmdir/
	cp ${ff_debuginfo}    $rpmdir/
fi
cd $dir
echo "Packaging $dir/$filename ..."
echo "GZIP=[$GZIP]"
GZIP= tar -cvz --file="${filename}" "${subdir}"
echo "Packaging $basicdir/$basic ..."
GZIP= tar -cvz --file="${basic}" --exclude mpi --exclude shmem "${basic%.tgz}"

cp "${RPMDIR}/${packaged_files_path}" "${TL_DIR}/${PROJ_FILE_DIR}/"
cp "${RPMDIR}/${dist_files_path}"     "${TL_DIR}/${PROJ_FILE_DIR}/"

exit 0