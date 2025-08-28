TOPDIR := ../../../../../out/build/kernel
KERN_DIR := $(TOPDIR)
SRC := gpio_key_drv.c
MODULE_NAME := gpio_key_drv
OUT_DIR := output

all:
	@mkdir -p $(OUT_DIR)
	cp $(SRC) $(OUT_DIR)/
	echo "obj-m := $(MODULE_NAME).o" > $(OUT_DIR)/Makefile
	make -C $(KERN_DIR) M=$(shell pwd)/$(OUT_DIR) modules
	"${TOPDIR}"/scripts/sign-file sha512 \
		"${TOPDIR}"/certs/signing_key.pem \
		"${TOPDIR}"/certs/signing_key.x509 \
		$(OUT_DIR)/$(MODULE_NAME).ko $(OUT_DIR)/$(MODULE_NAME)_sign.ko

clean:
	@if [ -d "$(OUT_DIR)" ]; then \
		make -C $(KERN_DIR) M=$(shell pwd)/$(OUT_DIR) clean; \
	fi
	rm -rf $(OUT_DIR)
