WFD_PLATFORM_LIST := msm8974 apq8084 msm8994 msm8916 msm8992 msm8996 msm8937 msm8953 msmcobalt
ifeq ($(call is-board-platform-in-list,$(WFD_PLATFORM_LIST)),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# ---------------------------------------------------------------------------------
#            COPY THE CAPABILITY.XML FILE TO /data
# ---------------------------------------------------------------------------------

LOCAL_MODULE:= capability.xml
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)

# ---------------------------------------------------------------------------------
#            COPY THE WFDCONFIG.XML FILE TO /data
# ---------------------------------------------------------------------------------

LOCAL_MODULE:= wfdconfig.xml
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
include $(BUILD_PREBUILT)

# ---------------------------------------------------------------------------------
#            COPY THE WFDCONFIGSINK.XML FILE TO /system/etc
# ---------------------------------------------------------------------------------
include $(CLEAR_VARS)
LOCAL_MODULE:= wfdconfigsink.xml
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
include $(BUILD_PREBUILT)

# ---------------------------------------------------------------------------------
#            COPY THE WFDWHITELIST.XML FILE TO /system/etc
# ---------------------------------------------------------------------------------
include $(CLEAR_VARS)
LOCAL_MODULE:= wfddswhitelist.xml
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
include $(BUILD_PREBUILT)

# ---------------------------------------------------------------------------------
#            COPY THE WFDCONFIGSINK.XML FILE TO /system/etc
# ---------------------------------------------------------------------------------
#include $(CLEAR_VARS)
#LOCAL_MODULE:= wfdmessage.bin
#LOCAL_MODULE_CLASS := EXECUTABLES
#LOCAL_SRC_FILES := $(LOCAL_MODULE)
#LOCAL_MODULE_TAGS := optional
#LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)
#include $(BUILD_PREBUILT)
endif

# ---------------------------------------------------------------------------------
#            END
# ---------------------------------------------------------------------------------
