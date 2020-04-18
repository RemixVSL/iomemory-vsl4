# IOMemory-VSL4
This is an "unsupported" updated, and cleaned up version of the original driver
source for newer generation FusionIO cards. It comes with no warranty, it may
cause DATA LOSS or CORRUPTION. Therefore it is NOT meant for production use,
just for testing purposes.

## Background
Driver support for FusionIO cards has been lagging behind kernel
releases, effectively making these cards an expensive paperweight
when running a distribution like Ubuntu / Arch / Fedora / ProxMox which
all supply newer kernels than supported.

## Current version
The current driver version is derived from iomemory-vsl-4.3.7.1205, and has
gone through rigorous rewriting and cleaning of redundant, unused, and old code.
This driver is aimed to only support Linux kernels from 5.0 and upwards.

## Important notes!!!
At this moment the driver has been tested with kernel 5.0 to 5.6. Tests are
run on an LVM volume with an ext4 filesystem. Workload testing is done with
VM's, Containers, FIO and normal desktop usage.

## Building
There are several ways to build and package the module.

### From Source
```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl4/
git checkout <release-tag>
cd root/usr/src/iomemory-vsl4-4.3.7
make gpl
sudo insmod iomemory-vsl4.ko
```

### .deb Ubuntu / Debian
```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl4/
git checkout <release-tag>
cd root/usr/src/iomemory-vsl4-4.3.7
make dpkg
```

### .rpm CentOS / RHEL
```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl4/
git checkout <release-tag>
rpmbuild -ba fio-driver.spec
```

## Installation
Installation can be done with created packages, DKMS or other options described
in the original README.

## DKMS
Dynamic Kernel Module Support automates away the requirement of having to
repackage the kernel module with every kernel and headers update that takes
place on the system.
```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl4/
git checkout <release-tag>
cd root/usr/src/iomemory-vsl4-4.3.7
sudo make dkms
```

# Utils
With fio-utils installed you should see the following kind of...,:
```
snuf@scipio:~/Documents/iodrive4/fio-util-4/usr/bin$ sudo ./fio-status -a

Note: 2 unmanaged ioMemory devices found requiring a v3.x driver.
   Install the driver package to see device information.
   Note that only one driver package can be installed at a time.

Found 1 VSL driver package:
   4.3.6 build 1173 Driver: not loaded

Found 1 ioMemory device in this system

Adapter: ioMono  (driver 4.3.6)
	ioMemory SX300-1300, Product Number:F13-004-1300-CS-0001, SN:1446G1531, FIO SN:1446G1531
	ioMemory Adapter Controller, PN:PA006002103
	Product UUID:d3e0a40c-7e14-5264-842f-316d3f353492
	PCIe Power limit threshold: Disabled
	PCIe slot available power: 0.24W
	PCIe negotiated link: 8 lanes at 5.0 Gt/sec each, 4000.00 MBytes/sec total
	Connected ioMemory modules:
	  08:00.0:	Product Number:F13-004-1300-CS-0001, SN:1446G1531

08:00.0	ioMemory Adapter Controller, Product Number:F13-004-1300-CS-0001, SN:1446G1531
	ioMemory Adapter Controller, PN:PA006002103
	Microcode Versions: App:0.0.44.0
	PCI:08:00.0
	Vendor:1aed, Device:3002, Sub vendor:1aed, Sub device:3002
	Firmware v8.9.5, rev 20160412 Public
	UEFI Option ROM v4.2.5, rev 1266 Disabled
	PCIe slot available power: 0.24W
	PCIe negotiated link: 8 lanes at 5.0 Gt/sec each, 4000.00 MBytes/sec total
	Internal temperature: 63.49 degC, max 79.24 degC
	Internal voltage: avg 1.01V, max 1.02V
	Aux voltage: avg 1.80V, max 1.83V
```

## Support
Join us on the Discord Server in the Wiki, or create a bug report

## Other notes
Installing fio-util, fio-common, fio-preinstall and fio-sysvinit is recommended.
