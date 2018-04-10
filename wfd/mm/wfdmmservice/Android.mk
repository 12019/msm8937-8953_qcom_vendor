LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#-----------------------------------------------------------------
# Define
#-----------------------------------------------------------------
LOCAL_CFLAGS := -D_ANDROID_

#----------------------------------------------------------------
# SRC CODE
#----------------------------------------------------------------
LOCAL_SRC_FILES := src/IWFDMMService.cpp
LOCAL_SRC_FILES += src/WFDMMService.cpp
LOCAL_SRC_FILES += src/WFDMMServiceClient.cpp
LOCAL_SRC_FILES += src/IWFDMMListener.cpp

#----------------------------------------------------------------
# INCLUDE PATH
#----------------------------------------------------------------
LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-osal/include
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/common/inc
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../utils/inc
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../utils/inc
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-core/omxcore
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/include

# ---------------------------------------------------------------------------------
#            COPY INCLUDE FILES TO OUT
# ---------------------------------------------------------------------------------

LOCAL_COPY_HEADERS_TO := wfd/include
LOCAL_COPY_HEADERS := inc/WFDMMServiceClient.h
LOCAL_COPY_HEADERS += inc/IWFDMMService.h
LOCAL_COPY_HEADERS += inc/IWFDMMListener.h

LOCAL_SHARED_LIBRARIES := libmmosal
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libdl
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libbinder

LOCAL_MODULE:= libwfdmmservice

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qtl
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
