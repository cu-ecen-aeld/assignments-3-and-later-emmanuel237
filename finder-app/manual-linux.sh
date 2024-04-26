#!/bin/bash
# Script outline to install and build kernel.
# Author: Emmanuel Kiegaing.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
SYSROOT=$(aarch64-none-linux-gnu-gcc -print-sysroot)

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper #clean any previously built artefacts
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j$(nproc) ARCH=${ARCH}  CROSS_COMPILE=${CROSS_COMPILE} all
    #make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules --no kernel module is needed for this simple example
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
ROOTFS_BASE_DIR=${OUTDIR}/rootfs
mkdir -p ${ROOTFS_BASE_DIR}
mkdir -p ${ROOTFS_BASE_DIR}/bin ${ROOTFS_BASE_DIR}/dev ${ROOTFS_BASE_DIR}/etc ${ROOTFS_BASE_DIR}/home ${ROOTFS_BASE_DIR}/lib ${ROOTFS_BASE_DIR}/lib64 ${ROOTFS_BASE_DIR}/proc ${ROOTFS_BASE_DIR}/sbin ${ROOTFS_BASE_DIR}/sys ${ROOTFS_BASE_DIR}/tmp ${ROOTFS_BASE_DIR}/usr ${ROOTFS_BASE_DIR}/var
mkdir -p ${ROOTFS_BASE_DIR}/usr/bin ${ROOTFS_BASE_DIR}/usr/lib ${ROOTFS_BASE_DIR}/usr/sbin
mkdir -p ${ROOTFS_BASE_DIR}/var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    
else
    cd busybox
    make distclean
fi

# TODO: Make and install busybox
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${ROOTFS_BASE_DIR} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${ROOTFS_BASE_DIR}/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${ROOTFS_BASE_DIR}/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${ROOTFS_BASE_DIR}/lib
cp -a ${SYSROOT}/lib64/ld-2.33.so ${ROOTFS_BASE_DIR}/lib64
cp -a ${SYSROOT}/lib64/libm.so.6 ${ROOTFS_BASE_DIR}/lib64
cp -a ${SYSROOT}/lib64/libm-2.33.so ${ROOTFS_BASE_DIR}/lib64
cp -a ${SYSROOT}/lib64/libresolv.so.2 ${ROOTFS_BASE_DIR}/lib64
cp -a ${SYSROOT}/lib64/libresolv-2.33.so ${ROOTFS_BASE_DIR}/lib64
cp -a ${SYSROOT}/lib64/libc.so.6 ${ROOTFS_BASE_DIR}/lib64
cp -a ${SYSROOT}/lib64/libc-2.33.so ${ROOTFS_BASE_DIR}/lib64


# TODO: Make device nodes
sudo mknod -m 666 ${ROOTFS_BASE_DIR}/dev/null c 1 3
sudo mknod -m 600 ${ROOTFS_BASE_DIR}/dev/console c 5 1
# TODO: Clean and build the writer utility
cd "$FINDER_APP_DIR"
make CROSS_COMPILE=${CROSS_COMPILE} clean
make CROSS_COMPILE=${CROSS_COMPILE}
 
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -a ${FINDER_APP_DIR}/finder.sh ${ROOTFS_BASE_DIR}/home
cp -a ${FINDER_APP_DIR}/finder-test.sh ${ROOTFS_BASE_DIR}/home
cp -a ${FINDER_APP_DIR}/writer ${ROOTFS_BASE_DIR}/home
cp -a ${FINDER_APP_DIR}/autorun-qemu.sh ${ROOTFS_BASE_DIR}/home 
mkdir -p ${ROOTFS_BASE_DIR}/home/conf
cp -a ${FINDER_APP_DIR}/conf/username.txt ${ROOTFS_BASE_DIR}/home/conf 
cp -a ${FINDER_APP_DIR}/conf/assignment.txt ${ROOTFS_BASE_DIR}/home/conf 


# TODO: Chown the root directory
cd "$ROOTFS_BASE_DIR"/
sudo chown -R root:root *
# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd "$OUTDIR"/ 
gzip -f initramfs.cpio