# This is a legacy library which is depended on by some old hidl-generated
# makefiles. Now, '-java' libraries can work as both static and non-static
# libraries.

################################################################################
LOCAL_PATH:= $(call my-dir)

# TODO(b/68433855): re-enable building this in the PDK
ifneq ($(TARGET_BUILD_PDK),true)

include $(CLEAR_VARS)
LOCAL_MODULE := android.hidl.base-V1.0-java-static
LOCAL_MODULE_CLASS := JAVA_LIBRARIES

intermediates := $(call local-generated-sources-dir, COMMON)

HIDL := $(HOST_OUT_EXECUTABLES)/hidl-gen$(HOST_EXECUTABLE_SUFFIX)
LOCAL_NO_STANDARD_LIBRARIES := true
LOCAL_JAVA_LIBRARIES := core-oj hwbinder

#
# Build types.hal (DebugInfo)
#
GEN := $(intermediates)/android/hidl/base/V1_0/DebugInfo.java
$(GEN): $(HIDL)
$(GEN): PRIVATE_HIDL := $(HIDL)
$(GEN): PRIVATE_DEPS := $(LOCAL_PATH)/base/1.0/types.hal
$(GEN): PRIVATE_OUTPUT_DIR := $(intermediates)
$(GEN): PRIVATE_CUSTOM_TOOL = \
        $(PRIVATE_HIDL) -o $(PRIVATE_OUTPUT_DIR) \
        -Ljava \
        -randroid.hidl:system/libhidl/transport \
        android.hidl.base@1.0::types.DebugInfo

$(GEN): $(LOCAL_PATH)/base/1.0/types.hal
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

#
# Build IBase.hal
#
GEN := $(intermediates)/android/hidl/base/V1_0/IBase.java
$(GEN): $(HIDL)
$(GEN): PRIVATE_HIDL := $(HIDL)
$(GEN): PRIVATE_DEPS := $(LOCAL_PATH)/base/1.0/IBase.hal
$(GEN): PRIVATE_DEPS += $(LOCAL_PATH)/base/1.0/types.hal
$(GEN): $(LOCAL_PATH)/base/1.0/types.hal
$(GEN): PRIVATE_OUTPUT_DIR := $(intermediates)
$(GEN): PRIVATE_CUSTOM_TOOL = \
        $(PRIVATE_HIDL) -o $(PRIVATE_OUTPUT_DIR) \
        -Ljava \
        -randroid.hidl:system/libhidl/transport \
        android.hidl.base@1.0::IBase

$(GEN): $(LOCAL_PATH)/base/1.0/IBase.hal
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
include $(BUILD_STATIC_JAVA_LIBRARY)

endif # TARGET_BUILD_PDK not true
