#Turn on kernel engineering build as default when TARGET_BUILD_VARIANT is eng, to disable it, add ENG_BLD=0 in build command
ifeq ($(TARGET_BUILD_VARIANT), eng)
ENG_BLD := 1
endif
ifeq ($(TARGET_BUILD_VARIANT), userdebug)
ENG_BLD := 1
endif

MOTO_MOD_INSTALL=$(TARGET_OUT)/lib/modules
KERNEL_OUT_DIR=$(PRODUCT_OUT)/obj/PARTITIONS/kernel_intermediates
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT_DIR)/build/arch/arm/boot/zImage
WLAN_DRV_PATH = $(TOPDIR)system/wlan/ti/wilink_6_1/platforms/os/linux
MOTO_BSP_PREBUILT = $(TOPDIR)motorola/bsp/prebuilt/target/images

ifneq ($(DO_NOT_REBUILD_THE_KERNEL),1)
.PHONY: $(TARGET_PREBUILT_KERNEL)
endif

ifeq ($(TARGET_PRODUCT), cdma_social)
TEST_MUDFLAP=1
BLD_CONF=msm7627_social
else
BLD_CONF=mapphone
endif

$(TARGET_PREBUILT_KERNEL) += $(MOTO_MOD_INSTALL)/dummy.ko
$(TARGET_PREBUILT_KERNEL): $(HOST_OUT_EXECUTABLES)/depmod$(HOST_EXECUTABLE_SUFFIX)
ifeq ($(TARGET_PRODUCT), cdma_social)
	make -f kernel/kernel.mk TEST_MUDFLAP=$(TEST_MUDFLAP) BLD_CONF=$(BLD_CONF) KERNEL_OUT_DIR=$(KERNEL_OUT_DIR) DEPMOD=$(HOST_OUT_EXECUTABLES)/depmod$(HOST_EXECUTABLE_SUFFIX) KERNEL_CROSS_COMPILE=$(shell pwd)/prebuilt/$(HOST_PREBUILT_TAG)/toolchain/arm-eabi-4.4.0/bin/arm-eabi- ENG_BLD=
else
	make -f kernel/kernel.mk BLD_CONF=$(BLD_CONF) KERNEL_OUT_DIR=$(KERNEL_OUT_DIR) DEPMOD=$(HOST_OUT_EXECUTABLES)/depmod$(HOST_EXECUTABLE_SUFFIX) KERNEL_CROSS_COMPILE=$(shell pwd)/prebuilt/$(HOST_PREBUILT_TAG)/toolchain/arm-eabi-4.4.0/bin/arm-eabi- ENG_BLD=$(ENG_BLD)
	mkdir -p $(MOTO_MOD_INSTALL)
	rm -f $(MOTO_MOD_INSTALL)/dummy.ko
	find $(KERNEL_OUT_DIR)/build/lib/modules -name "*.ko" -exec cp -f {} \
		$(MOTO_MOD_INSTALL) \; || true
	cp $(WLAN_DRV_PATH)/tiwlan_drv.ko $(MOTO_MOD_INSTALL)
	$(TOPDIR)prebuilt/$(HOST_OS)-$(HOST_ARCH)/toolchain/arm-eabi-4.4.0/bin/arm-eabi-strip --strip-debug $(MOTO_MOD_INSTALL)/*.ko
	cp $(MOTO_MOD_INSTALL)/*.ko $(MOTO_BSP_PREBUILT)/system/lib/modules
	cp $(TARGET_PREBUILT_KERNEL) $(MOTO_BSP_PREBUILT)
	touch $(MOTO_MOD_INSTALL)/dummy.ko
endif

file := $(INSTALLED_KERNEL_TARGET)
ALL_PREBUILT += $(file)
$(file): $(TARGET_PREBUILT_KERNEL) | $(ACP)
	$(transform-prebuilt-to-target)

.PHONY: kernel
kernel: $(TARGET_PREBUILT_KERNEL)
