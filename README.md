<br />
<p align="center">

  <!-- <a href="https://github.com/snuf/iomemory-docs">
    <img src="https://github.com/snuf/iomemory-docs/images/logo.png" alt="Logo" width="80" height="80">
  </a> -->

  <h3 align="center"></h3>

  <p align="center">
    <a href="../../wiki">Wiki</a>
    .
    <a href="../../issues">Report Bug</a>
    ·
    <a href="../../issues">Request Feature</a>
  </p>
</p>

# IOMemory-VSL4
This is an "unsupported" updated, and cleaned up version of the original driver
source for newer generation FusionIO cards. It comes with no warranty, it may
cause DATA LOSS or CORRUPTION.

# IOMemory-VSL(3)
For the iomemory-vsl driver please go to the [iomemory-vsl](https://github.com/snuf/iomemory-vsl) repo.

# How to Identify your Fusion-io Card (including VSL version)
 Please refer to the [Fusion-io and OEM Part Numbers Matrix](https://docs.google.com/spreadsheets/d/e/2PACX-1vQMd40liekOCeftUYQx6GeofHgjU5SSDT-jHWid03JCfswQxHAhVee3rW-04baqKg1qN2fp7wEzuFm6/pubhtml).

## Background
Driver support for FusionIO cards has been lagging behind kernel
releases, effectively making these cards an expensive paperweight
when running a distribution like Ubuntu / Arch / Fedora / ProxMox which
all supply newer kernels than supported.

## Current version
The current driver version is derived from iomemory-vsl-4.3.7.1205, and has
gone through rigorous rewriting and cleaning of redundant, unused, and old code.
This driver is aimed to support Linux kernels from 5.0 and upwards.

### Releases
We've abandoned the notion of releases, Generally `main` should be checked out. `main` is completely backwards compatible for all **5**, and **6** kernels. The latest working tested kernel is **6.5**.

Historically releases were tagged, and were be checked out by their tag. The release tags follow Linux Kernel versions. E.g. **v5.12.1 (Boop Noodle)** will work on all 5.x kernels that are 5.12 and lower, but is not guaranteed to work on 5.13. **v4.20.2 - Big Ole Nope Rope** supports most kernels that pre-date 5.0. Again this way of following kernel releases has been abandoned. Please follow **main**.

| Tag | Codename |
| --- | --- |
| main | |
| v5.12.1 | Boop Noodle |
| v5.10.0 | Spicy Nope Rope |
| v5.6.1 | Danger Noodle |
| v4.20.1 | Big Ole Nope Rope |

## Important Note for newer Linux Kernels
Starting with Linux kernel 5.4.0, significant changes to the kernel were made that require additional boot time kernel flags for this driver to work. These affect AMD CPUs starting with 5.4.0, and Intel CPUs after about kernel 5.8.0. 

Add the following to your /etc/default/grub by looking for `GRUB_CMDLINE_LINUX_DEFAULT=""` and adding additional parameters inside the quotes.
For AMD systems:
```
amd_iommu=on iommu=pt
```
For Intel system:
```
iommu=pt
```

Example:
```GRUB_CMDLINE_LINUX_DEFAULT="quiet iommu=pt"```

### Supported Hardware
Here's a not so exhaustive list of iomemory cards. I have only tested the 3.2TB card, and was able to crossflash back to OEM. The rest below all seem to be SX350s or PX600s. While they should all work, we don't have any PX or SX300 cards to test with.

| OEM Models |
| --- |
| ioMemory SX300 |
| ioMemory SX350 |
| ioMemory PX600 |

| HPE PCIe Workload Accelerators | Part Number | Capacity | Notes | tested firmware |
| --- | --- | --- | --- | --- |
| HP Light Endurance (LE) Workload Accelerator | 775666-B21 | 1.0TB | Unknown | gen3_tangerine_fusion |
| HP Light Endurance (LE) Workload Accelerator | 775672-B21 | 5.2TB | Unknown | Unknown |
| HP Value Endurance (VE) Workload Accelerator | 763834-B21 | 1.3TB | Likely SX350 | Unknown |
| HP Value Endurance (VE) Workload Accelerator | 763836-B21 | 3.2TB | Rebranded SX350 | gen3_tangerine_fusion |
| HP Value Endurance (VE) Workload Accelerator | 763840-B21 | 6.4TB | Likely SX350 | Unknown |

| Cisco UCS Storage Accelerators | Part Number | Capacity | Notes |
| --- | --- | --- | --- | 
| UCS Rack PCIe Storage 1300 GB SanDisk SX350 Medium Endurance | UCSC-F-S13002 | 1.3TB | 
| UCS Rack PCIe Storage 1600 GB SanDisk SX350 Medium Endurance | UCSC-F-S16002 | 1.6TB |
| UCS Rack PCIe Storage 3200 GB SanDisk SX350 Medium Endurance | UCSC-F-S32002 | 3.2TB |
| UCS Rack PCIe Storage 6400 GB SanDisk SX350 Medium Endurance | UCSC-F-S64002 | 6.4TB |
| UCS 1000 GB Fusion ioMemory3 PX Performance line for Rack M4 | UCSC-F-FIO-1000PS | 1.0TB | PX600 |
| UCS 1300 GB Fusion ioMemory3 PX Performance line for Rack M4 | UCSC-F-FIO-1300PS | 1.3TB | PX600 |
| UCS 2600 GB Fusion ioMemory3 PX Performance line for Rack M4 | UCSC-F-FIO-2600PS | 2.6TB | PX600 |
| UCS 5200 GB Fusion ioMemory3 PX Performance line for Rack M4 | UCSC-F-FIO-5200PS | 5.2TB | PX600 |

## Important notes!!!
At this moment the driver has been tested over time with different kernel version. 
Tests are run on an LVM volume with an ext4 filesystem. Workload testing is done with
VM's, Containers, FIO and normal desktop usage.

## Building
There are several ways to build and package the module.

Note! For many systems, the best option is to use DKMS, using the [DKMS instructions below](https://github.com/RemixVSL/iomemory-vsl4/blob/main/README.md#dkms). If you prefer to build the module directly, or to create a dpkg or rpm package, you can proceed with the options below.

Please make sure that the required dependencies are installed, as mentioned in this [README](https://github.com/RemixVSL/iomemory-vsl4/blob/main/README)

### From Source
```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl4/
# OPTIONAL: Checkout a specific release. Usually using main branch is correct.
# DO THIS ONLY IF YOU'VE BEEN TOLD IT'S REQUIRED!
# git checkout <release-tag>
make module
cd root/usr/src/iomemory-vsl4-4.3.7
sudo insmod iomemory-vsl4.ko
```

### .deb Ubuntu / Debian
```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl4/
# OPTIONAL: Checkout a specific release. Usually using main branch is correct.
# DO THIS ONLY IF YOU'VE BEEN TOLD IT'S REQUIRED!
# git checkout <release-tag>
make dpkg
```

### .rpm CentOS / RHEL
```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl4/
# OPTIONAL: Checkout a specific release. Usually using main branch is correct.
# DO THIS ONLY IF YOU'VE BEEN TOLD IT'S REQUIRED!
# git checkout <release-tag>
make rpm
```

## Installation
Installation can be done with created packages, DKMS or other options described
in the original README.

## DKMS
Dynamic Kernel Module Support automates away the requirement of having to
repackage the kernel module with every kernel and headers update that takes
place on the system.

Try building from `main` first as it works with most modern kernels up to about 5.14:

```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl4/
make dkms
```

If you know you need to build a specific branch based on a specific recommendation, use:

```
git clone https://github.com/snuf/iomemory-vsl4
cd iomemory-vsl4/
git checkout <release-tag>
make dkms
```

# Utils
With fio-utils installed you should see the following kind of...,:
```
.deb https://www.dropbox.com/s/d18dokktt0g2aau/fio-util_4.3.7.1205-1.0_amd64.deb
.rpm https://www.dropbox.com/s/ba2vcxhmf9p2pb2/fio-util-4.3.7.1205-1.el7.x86_64.rpm

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

# How to Get Help
- Open an issue in this Github repo
- Join our Discord server at https://discord.gg/VxDvVy3EdS

# Acknowledgements
The support and maintenance of this driver is made possible by the people that actively contribute or contributed to its code base or by supporting the project in other ways.

| Name | Discord |  |
|---|---|---|
| Demetrius Cassidy | | Long nights of cleaning the codebase, setting up the Discord channel and guiding people through firmware upgrades |
| Vince Fleming |  | Donating a 1.2TB IoDrive2 for debugging problems |

Obviously all the regulars on the Discord channel, notably @bplein, @AcquaCow and @Tear.

Oh yes and the folks that were at <b>Fusion IO</b> for creating this product that was way ahead of its time and delivering the integration shim sources with the driver core.

## Resources
Tools and resources often used to figure out what changed, and why things are not working as they are supposed to.
| Source | |
| --- | --- |
| [Elixir](https://elixir.bootlin.com/linux/latest/source) | Making the Linux source code browsable and easy to compare changes over time | 
| [ZOL](https://zfsonlinux.org/) |  A source of inspiration for changes in the block layers of the linux kernel that we get to deal with | 
| [LKML](https://lkml.org/) | Sometimes the first or last resort to figure out why something changed in the kernel, or where |
| [The Nvidia Forum](https://forums.developer.nvidia.com/c/gpu-unix-graphics/linux/148) | Because they are hot to trot they encounter problems before we do, so we get to ride on their coat tails.... sometimes....though  often not | 
| [Ghidra](https://ghidra-sre.org/) | The Ghidra project from the NSA that allows a look under the covers to figure out things inside non-sourcy libs|

## Other notes
Installing fio-util, fio-common, fio-preinstall and fio-sysvinit is recommended.
