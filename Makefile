all: help

.PHONY: dkms
dkms:
	cd root/usr/src/$(shell ls root/usr/src) && \
		$(MAKE) dkms

.PHONY: dpkg
dpkg:
	# patch fio_version, fio_short_version in debian/fio_values
	cd $(shell git rev-parse --show-toplevel) && \
		dpkg-buildpackage -rfakeroot --no-check-builddeps --no-sign

.PHONY: rpm
rpm:
	@echo cough

.PHONY: module
module:
	cd root/usr/src/$(shell ls root/usr/src) && \
  	$(MAKE) gpl

clean:
	cd root/usr/src/$(shell ls root/usr/src) && \
  	$(MAKE) clean

define usage
@echo Stub for making dkms, dpkg, the module and clean
@echo usage: make "(dkms|dpkg|rpm|module|clean)"
endef
help:
	$(usage)
