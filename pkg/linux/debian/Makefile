BUILD_DIR := .
OUT_DIR := ..
UPSTREAM_NAME ?= $(shell dpkg-parsechangelog --show-field=Source)
UPSTREAM_VERSION ?= $(shell dpkg-parsechangelog --show-field=Version | sed 's/[-+~].*$$//')

build: prepare
	cd $(OUT_DIR)/$(UPSTREAM_NAME)-$(UPSTREAM_VERSION) && \
        pdebuild --use-pdebuild-internal --buildresult ..

clean:
	-rm -rf $(OUT_DIR)/$(UPSTREAM_NAME)-$(UPSTREAM_VERSION)
	-rm -f $(OUT_DIR)/*.build
	-rm -f $(OUT_DIR)/*.changes
	-rm -f $(OUT_DIR)/*.deb
	-rm -f $(OUT_DIR)/*.debian.tar.xz
	-rm -f $(OUT_DIR)/*.dsc

diff: prepare
	cd $(OUT_DIR)/$(UPSTREAM_NAME)-$(UPSTREAM_VERSION) && \
        debuild -S -sd

prepare: clean
	uscan --verbose --force-download

source: prepare
	cd $(OUT_DIR)/$(UPSTREAM_NAME)-$(UPSTREAM_VERSION) && \
        debuild -S -sa

.PHONY: build clean diff prepare source
