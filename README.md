# IOMemory-VSL4

This is an unsupported update of the original driver source for newer generation FusionIO cards. It comes with no warranty, it may cause DATA LOSS or CORRUPTION.
Therefore it is NOT meant for production use, just for testing purposes.

## Current version
The current version is derived from iomemory-vsl-4.3.7.

## Important notes!!!
At this moment the driver has only been tested with kernel 5.3, and ext4fs, by installing / playing Goat Simulator from steam, and running the FIO tests from the iomemory-vsl test suite on a logical volume.

The original driver does some GCC specific things and will not compile with -Werror, when turning warnings into errors, due to -Wvla (variable length arrays) and some other dirty bits. It still contains some unsavory things that need cleaning, it does however "work" in the setting described above.

## Background
Driver support for FusionIO cards has been lagging behind kernel
releases, effectively making also these cards an expensive paperweight
when running a distribution like Ubuntu which supplies newer kernels.
Deemed a trivial task to update the drivers and actually make them work
with said newer kernels, and putting the expensive paperweight to use again
so I could access my data..., set forking and fixing the code in motion
quite a while ago originally on the older generation of FusionIO cards, due
to community requests also the newer.

## Building
### Source
```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl4/
git checkout backport-iomemory-vsl
cd root/usr/src/iomemory-vsl4-4.3.7
make gpl
sudo insmod iomemory-vsl4.ko
```
### Ubuntu / Debian

### CentOS / RHEL
If you are on CentOS or similiar distribution simply run
```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl/
git checkout next_generation
rpmbuild -ba fio-driver.spec
```
Otherwise module building can be done according to the original README.

## Installation
Installation can be done according to the original README.

## DKMS
A dkms.conf file is supplied, so it should be plug and play:
```
sudo cp -r iomemory-vsl/root/usr/src/iomemory-vsl-4.3.7 /usr/src/
sudo mkdir -p /var/lib/dkms/iomemory-vsl/4.3.7/build
sudo ln -s /usr/src/iomemory-vsl-4.3.7 /var/lib/dkms/iomemory-vsl/4.3.6/source
sudo dkms build -m iomemory-vsl -v 3.2.15
sudo dkms install -m iomemory-vsl -v 3.2.15
sudo modprobe iomemory-vsl
```
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

## Other notes
Installing fio-util, fio-common, fio-preinstall and fio-sysvinit is recommended.
