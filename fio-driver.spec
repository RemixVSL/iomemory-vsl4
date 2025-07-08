%define fio_version        4.3.7.1205
%define fio_sysconfdir     /etc
%define fio_oem_name       fusionio
%define fio_oem_name_short fio
%define fio_sourcedir      /usr/src
%define fio_driver_name    iomemory-vsl4

%define has_kver %{?rpm_kernel_version: 1} %{?!rpm_kernel_version: 0}
%if !%{has_kver}
%define rpm_kernel_version %(uname -r)
%endif

%define has_nice_kver %{?rpm_nice_kernel_version: 1} %{?!rpm_nice_kernel_version: 0}
%if !%{has_nice_kver}
%define rpm_nice_kernel_version %(echo %{rpm_kernel_version} | sed -e 's/-/_/g')
%endif

%define fio_tar_version %{fio_version}
%{!?dist:%define dist %nil}
%{!?kernel_module_package:%define kernel_module_package %nil}
%{!?kernel_module_package_buildreqs:%define kernel_module_package_buildreqs %nil}

%define firehose_shipped_object %{?fio_shipped_object: FIREHOSE_SHIPPED_OBJECT=%{fio_shipped_object}}
%define fio_release 1%{dist}


# Turn off silly debug packages
%define debug_package %{nil}


Summary: Driver for SanDisk Fusion ioMemory devices
Name: iomemory-vsl4
Vendor: SanDisk
Version: %{fio_version}
Release: %{fio_release}
Obsoletes: iodrive-driver-kmod, iodrive-driver, fio-driver
License: Proprietary
Group: System Environment/Kernel
URL: http://support.fusionio.com/
Source0: %{name}-%{fio_tar_version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildArch: x86_64
%if "%{_vendor}" == "suse"
BuildRequires: %kernel_module_package_buildreqs rsync tar gcc make kernel-source
%else
%if "%{_vendor}" == "redhat"
BuildRequires: %kernel_module_package_buildreqs rsync tar gcc make kernel-devel rpm-build
# Add the below macro later (causes build failures on too many systems)
#kernel_module_package
%endif
%endif


%description
Driver for SanDisk Fusion ioMemory devices


%prep
%setup -q -n %{name}-%{fio_tar_version}


%build
rsync -rv root/usr/src/iomemory-vsl4-4.3.7/ driver_source/
if ! %{__make} \
        -C driver_source \
        KERNELVER=%{rpm_kernel_version} \
        FIO_DRIVER_NAME=%{fio_driver_name} \
        %{firehose_shipped_object} \
        modules
        then
        set +x
        cat <<EOF >&2
ERROR:

EOF
                exit 1
fi


%install
[ "$(cd "${RPM_BUILD_ROOT}" && pwd)" != "/" ] && rm -rf ${RPM_BUILD_ROOT}
rsync -a root/ "${RPM_BUILD_ROOT}/"
mkdir -p "${RPM_BUILD_ROOT}/lib/modules/%{rpm_kernel_version}/extra/%{fio_oem_name_short}"
cp driver_source/iomemory-vsl4.ko \
    "${RPM_BUILD_ROOT}/lib/modules/%{rpm_kernel_version}/extra/%{fio_oem_name_short}"

# Ensure the docdir has the driver version (prevents collisions when multiple drivers are installed)
mv "${RPM_BUILD_ROOT}/usr/share/doc/iomemory-vsl4" \
    "${RPM_BUILD_ROOT}/usr/share/doc/iomemory-vsl4-%{rpm_kernel_version}"

mkdir -p "${RPM_BUILD_ROOT}/usr/src/iomemory-vsl4-4.3.7/include/fio/port/linux"
touch -a "driver_source/Module.symvers"
cp "driver_source/Module.symvers" "${RPM_BUILD_ROOT}/usr/src/iomemory-vsl4-4.3.7/"
cp "driver_source/include/fio/port/linux/kfio_config.h" "${RPM_BUILD_ROOT}/usr/src/iomemory-vsl4-4.3.7/include/fio/port/linux/"

%clean
make clean

%pre
%ifarch i386
wrong_version() {
    echo "iomemory-vsl4 requires the i686 kernel to be installed.  Due to problems with architecture detection in anaconda, "
    echo "it appears that the wrong kernel is installed.  Please see"
    echo
    echo "http://fedoraproject.org/wiki/Bugs/FC6Common#head-e0676100ebd965b92fbaa7111097983a3822f143"
    echo
    echo "for more information."
    echo
    exit -1
}

echo "Checking kernel version..."

if [ `uname -m` != 'i686' ] ; then
    wrong_version
fi

if [ `rpm -q --queryformat="%{ARCH}\\n" kernel | sort -u` == 'i586' ] ; then
    echo "Found via rpm (uname reports i686):"
    echo
    wrong_version
fi

%endif


%package -n %{name}-%{rpm_kernel_version}
Summary: Driver for SanDisk Fusion ioMemory devices
Group: System Environment/Kernel
%define _wrong_version_format_terminate_build 0
Provides: iomemory-vsl4, iomemory-vsl, libvsl, kernel-modules = %{rpm_kernel_version}
Provides: iomemory-vsl4-%{fio_version}
Obsoletes: iodrive-driver, fio-driver
Conflicts:  iomemory-vsl4-source

%description -n %{name}-%{rpm_kernel_version}
Driver for fio devices


%post -n %{name}-%{rpm_kernel_version}
if [ -x "/sbin/weak-modules" ]; then
    echo "/lib/modules/%{rpm_kernel_version}/extra/%{fio_oem_name_short}/iomemory-vsl4.ko" \
        | /sbin/weak-modules --add-modules
fi
/sbin/depmod -a %{rpm_kernel_version}
if [ -a /etc/init.d/fio-agent ]; then #does fio-agent start script exist
    if [ "$(pidof fio-agent)" ] # fio-agent running
    then
        /etc/init.d/fio-agent restart
    fi
fi
if hash dracut &> /dev/null
then
    dracut -f
fi
ln -sf /usr/lib/fio/libvsl_4.so /usr/lib/fio/libvsl.so
ldconfig

%preun -n %{name}-%{rpm_kernel_version}
if [ "$1" -eq 0 ]; then
    rm -f /usr/lib/fio/libvsl.so
fi
if [ -f "/usr/lib/fio/libvsl_4.so" ]; then
    cp /usr/lib/fio/libvsl_4.so /usr/lib/fio/libvsl-prev.so
fi

%postun -n %{name}-%{rpm_kernel_version}
if [ -x "/sbin/weak-modules" ]; then
    echo "/lib/modules/%{rpm_kernel_version}/extra/%{fio_oem_name_short}/iomemory-vsl4.ko" \
        | /sbin/weak-modules --remove-modules
fi
/sbin/depmod -a %{rpm_kernel_version}
if [ "$1" -eq 0 ]; then
    /sbin/ldconfig
fi
if hash dracut &> /dev/null
then
    dracut -f
fi


%files -n %{name}-%{rpm_kernel_version}
%defattr(-, root, root)
/lib/modules/%{rpm_kernel_version}/extra/%{fio_oem_name_short}/iomemory-vsl4.ko
/usr/lib/fio/libvsl_4.so
/usr/share/doc/fio/NOTICE.libvsl_4
/etc/ld.so.conf.d/fio.conf
/usr/share/doc/iomemory-vsl4-%{rpm_kernel_version}/License
/usr/share/doc/iomemory-vsl4-%{rpm_kernel_version}/NOTICE.iomemory-vsl


%package -n %{name}-config-%{rpm_kernel_version}
Summary: Configuration of %{name} for FIO drivers %{rpm_kernel_version}
Group: System Environment/Kernel
Provides: iomemory-vsl4-config
Provides: iomemory-vsl4-config-%{fio_version}

%description -n %{name}-config-%{rpm_kernel_version}
Configuration of %{name} for FIO drivers %{rpm_kernel_version}


%files -n %{name}-config-%{rpm_kernel_version}
%defattr(-, root, root)
/usr/src/iomemory-vsl4-4.3.7/Module.symvers
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/linux/kfio_config.h


%package source
Summary: Source to build driver for SanDisk Fusion ioMemory devices
Group: System Environment/Kernel
Release: %{fio_release}
Obsoletes: iodrive-driver-source, fio-driver-source
Provides: iomemory-vsl4, iomemory-vsl, iomemory-vsl4-%{fio_version}, libvsl
Obsoletes: iodrive-driver, fio-driver
Conflicts: iomemory-vsl4

%description source
Source to build driver for SanDisk Fusion ioMemory devices


%files source
%defattr(-, root, root)
/usr/lib/fio/libvsl_4.so
/usr/share/doc/fio/NOTICE.libvsl_4
%config /etc/ld.so.conf.d/fio.conf
/usr/src/iomemory-vsl4-4.3.7/Kbuild
/usr/src/iomemory-vsl4-4.3.7/Makefile
/usr/src/iomemory-vsl4-4.3.7/dkms.conf.example
/usr/src/iomemory-vsl4-4.3.7/cdev.c
/usr/src/iomemory-vsl4-4.3.7/common_kinfo.c
/usr/src/iomemory-vsl4-4.3.7/dbgset.c
/usr/src/iomemory-vsl4-4.3.7/driver_init.c
/usr/src/iomemory-vsl4-4.3.7/check_target_kernel.sh
/usr/src/iomemory-vsl4-4.3.7/errno.c
/usr/src/iomemory-vsl4-4.3.7/kblock.c
/usr/src/iomemory-vsl4-4.3.7/kcache.c
/usr/src/iomemory-vsl4-4.3.7/kcondvar.c
/usr/src/iomemory-vsl4-4.3.7/kcpu.c
/usr/src/iomemory-vsl4-4.3.7/kcsr.c
/usr/src/iomemory-vsl4-4.3.7/kfile.c
/usr/src/iomemory-vsl4-4.3.7/kfio.c
/usr/src/iomemory-vsl4-4.3.7/kfio_config.sh
/usr/src/iomemory-vsl4-4.3.7/khotplug.c
/usr/src/iomemory-vsl4-4.3.7/kinfo.c
/usr/src/iomemory-vsl4-4.3.7/kinit.c
/usr/src/iomemory-vsl4-4.3.7/kmem.c
/usr/src/iomemory-vsl4-4.3.7/kscatter.c
/usr/src/iomemory-vsl4-4.3.7/kscsi.c
/usr/src/iomemory-vsl4-4.3.7/kscsi_host.c
/usr/src/iomemory-vsl4-4.3.7/ktime.c
/usr/src/iomemory-vsl4-4.3.7/kmsg.c
/usr/src/iomemory-vsl4-4.3.7/kfio_common.c
/usr/src/iomemory-vsl4-4.3.7/license.c
/usr/src/iomemory-vsl4-4.3.7/main.c
/usr/src/iomemory-vsl4-4.3.7/module_param.c
/usr/src/iomemory-vsl4-4.3.7/pci.c
/usr/src/iomemory-vsl4-4.3.7/port-internal.h
/usr/src/iomemory-vsl4-4.3.7/port-internal-boss.h
/usr/src/iomemory-vsl4-4.3.7/sched.c
/usr/src/iomemory-vsl4-4.3.7/six_lock.c
/usr/src/iomemory-vsl4-4.3.7/state.c
/usr/src/iomemory-vsl4-4.3.7/sysrq.c
/usr/src/iomemory-vsl4-4.3.7/*.conf
/usr/src/iomemory-vsl4-4.3.7/module_operations.sh
/usr/src/iomemory-vsl4-4.3.7/objtool_wrapper
/usr/src/iomemory-vsl4-4.3.7/kfio/*
/usr/src/iomemory-vsl4-4.3.7/include/kblock_meta.h
/usr/src/iomemory-vsl4-4.3.7/include/kfile_meta.h
/usr/src/iomemory-vsl4-4.3.7/include/sysrq_meta.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/arch/ppc/atomic.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/arch/ppc/bits.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/arch/ppc/cache.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/arch/x86_common/atomic.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/arch/x86_common/bits.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/arch/x86_common/cache.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/common/kinfo.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/common/tsc.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/common/uuid.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/align.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/atomic_list.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/bitops.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/byteswap.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/cdev.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/commontypes.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/compiler.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/port_config.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/port_config_macros.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/port_config_macros_clear.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/port_config_vars_externs.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/csr_simulator.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/dbgset.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/errno.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/message_id_ranges.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/message_ids.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/fio-poppack.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/fio-port.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/fio-pshpack1.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/fio-stat.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/fiostring.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/ifio.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/ioctl.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kbio.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kblock.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kcache.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kcondvar.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kcpu.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kcsr.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kfio.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kfio_config.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kglobal.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kinfo.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kmem.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kmsg.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kpci.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kscatter.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/kscsi.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/ktime.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/ktypes.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/ktypes_32.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/ktypes_64.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/libgen.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/list.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/nanddev.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/nexus.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/pci.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/porting_cdev.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/ranges.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/sched.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/six_lock.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/state.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/stdint.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/types.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/unaligned.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/ufio.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/utypes.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/vararg.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/version.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/commontypes.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/message_ids.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/div64.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/errno.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/kassert.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/kblock.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/kcondvar.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/kfile.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/kfio.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/kpci.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/kscsi.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/kscsi_config.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/stdint.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/common-linux/ufio.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/linux/ktypes.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/linux/utypes.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/gcc/align.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/port/gcc/compiler.h
/usr/src/iomemory-vsl4-4.3.7/include/fio/public/fioapi.h


%changelog
* Mon Dec 07 2009 23:19:12 -0700 Support <support@fusionio.com>
- Initial build.
