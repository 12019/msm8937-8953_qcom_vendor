LOCAL_PATH := $(call my-dir)

# ------------------------------------------------------------------------------
#       Make the Shared library (libhardware_wigig)
# ------------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_C_INCLUDES =
LOCAL_SHARED_LIBRARIES:= libcrypto libwpa_client liblog libcutils
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES:= wigig.c
LOCAL_MODULE := libhardware_wigig

include $(BUILD_SHARED_LIBRARY)

# ------------------------------------------------------------------------------
#       deploy wigig_supplicant.conf
# ------------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE       := wigig_supplicant.conf
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH  := $(TARGET_OUT_ETC)/wifi
LOCAL_SRC_FILES    := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

# ------------------------------------------------------------------------------
#       deploy wigig_p2p_supplicant.conf
# ------------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE       := wigig_p2p_supplicant.conf
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH  := $(TARGET_OUT_ETC)/wifi
LOCAL_SRC_FILES    := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)