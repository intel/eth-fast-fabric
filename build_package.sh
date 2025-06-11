#! /bin/bash

read id versionid <<< $(OpenIb_Host/get_id_and_versionid.sh)

case $id in
	rhel|centos|rocky|almalinux|navix|circle|ol)
		spec_ff="OpenIb_Host/eth-tools.spec.rh"	;;
	fedora)
		spec_ff="OpenIb_Host/eth-tools.spec.fedora" ;;
	sles)
		spec_ff="OpenIb_Host/eth-tools.spec.sles" ;;
	ocs)
		spec_ff="OpenIb_Host/eth-tools.spec.ocs" ;;
	ubuntu)
		build_deb="yes" ;;
	*)
		echo "ERROR: Unsupported distribution: $id $versionid"
		exit 1
	;;
esac

if [ "$build_deb" = "yes" ]; then
	pkg_ver_release=$(dpkg-parsechangelog --show-field Version)
	pkg_ver=${pkg_ver_release%%-*}
	output_dir="$HOME/debbuild/eth-tools-${pkg_ver}"
	mkdir -p "${output_dir}"
	cp -r ./* "${output_dir}"
	pushd "${output_dir}"
		dpkg-buildpackage -us -uc
	popd
else
	mkdir -p $HOME/rpmbuild/{SOURCES,RPMS,SRPMS,SPECS}
	cp ${spec_ff} $HOME/rpmbuild/SPECS/eth-tools.spec
	pkg_ver=$(rpmspec --query --srpm --queryformat="%{version}" ${spec_ff})
	tar czf $HOME/rpmbuild/SOURCES/eth-fast-fabric-${pkg_ver}.tar.gz --exclude-vcs .
	rpmbuild -ba $HOME/rpmbuild/SPECS/eth-tools.spec
fi
