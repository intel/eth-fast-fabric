#!/bin/bash

function os_release_file() {
    if [ -e /etc/os-release ] && grep -q '^ID=' /etc/os-release 2>/dev/null; then
        echo "/etc/os-release"
    elif [ -e /usr/lib/os-release ] && grep -q '^ID=' /usr/lib/os-release 2>/dev/null; then
        # Fall Back for known Bug in Rocky 8.6 GA where /etc/ file was broken
        echo "/usr/lib/os-release"
    fi
}
declare -A vendor_map=(
    [rhel]="rhel"
    [centos]="rhel"
    [rocky]="rhel"
    [almalinux]="rhel"
    [navix]="rhel"
    [circle]="rhel"
    [ol]="ol"
    [opencloudos]="ocs"
    [sles]="sles"
    [sle_hpc]="sles"
    [ubuntu]="ubuntu"
)

# Get 'os-release' file
os_file=$(os_release_file)
if [ "${os_file}" ] && [ -e "${os_file}" ]
then
    . ${os_file}
    if [ "${vendor_map[${ID}]+found}" == "found" ]
    then
        id=${vendor_map[${ID}]}
    else
        # If not in map, just use ID
        id=${ID}
    fi
	versionid=${VERSION_ID}
fi

echo $id $versionid
exit 0
