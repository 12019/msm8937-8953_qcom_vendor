ROOT_DIR := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_PATH := $(ROOT_DIR)

# ---------------------------------------------------------------------------------
#          Common definitons
# ---------------------------------------------------------------------------------

libmm-venc-def := -DQCOM_OMX_VENC_EXT
libmm-venc-def += -DWIDI_720p_ENCODE_ENABLE
libmm-venc-def += -O3
libmm-venc-def += -D_ANDROID_
libmm-venc-def += -D_ANDROID_LOG_
libmm-venc-def += -D_ANDROID_LOG_ERROR
libmm-venc-def += -D_ANDROID_LOG_PROFILE
libmm-venc-def += -Du32="unsigned int"

# ---------------------------------------------------------------------------------
#          MM-SRC APP
# ---------------------------------------------------------------------------------

include $(CLEAR_VARS)

mm-venc-test-inc := $(LOCAL_PATH)/../../interface/inc
mm-venc-test-inc += $(LOCAL_PATH)/../utils/inc
mm-venc-test-inc += $(LOCAL_PATH)/../../Framework/inc
mm-venc-test-inc += $(LOCAL_PATH)/../../uibc/source/inc
mm-venc-test-inc += $(TARGET_OUT_HEADERS)/mm-core/omxcore
mm-venc-test-inc += $(TARGET_OUT_HEADERS)/common/inc
mm-venc-test-inc += $(TARGET_OUT_HEADERS)/mm-osal/include

LOCAL_MODULE := venc-widi
LOCAL_CFLAGS := $(libmm-venc-def)
LOCAL_CFLAGS += -Wconversion
LOCAL_C_INCLUDES := $(mm-venc-test-inc)

# ---------------------------------------------------------------------------------
#          MM-SRC APP SHARED LIB
# ---------------------------------------------------------------------------------

LOCAL_SHARED_LIBRARIES := libmmosal
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libmmrtpencoder
LOCAL_SHARED_LIBRARIES += libmmwfdinterface
LOCAL_SHARED_LIBRARIES += libwfdmmsrc
LOCAL_SHARED_LIBRARIES += libcutils

LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES :=./src/WFDMMSourceApp.cpp

include $(BUILD_EXECUTABLE)


# ---------------------------------------------------------------------------------
#          OVERLAY TEST APP
# ---------------------------------------------------------------------------------
include $(CLEAR_VARS)
LOCAL_PATH := $(ROOT_DIR)

libmm-ovrly-def += -O3
libmm-ovrly-def += -D_ANDROID_
libmm-ovrly-def += -D_ANDROID_LOG_
libmm-ovrly-def += -D_ANDROID_LOG_ERROR
libmm-ovrly-def += -D_ANDROID_LOG_PROFILE
libmm-ovrly-def += -Du32="unsigned int"

mm-ovrly-test-inc += $(LOCAL_PATH)/../utils/inc
mm-ovrly-test-inc += $(LOCAL_PATH)/../../../utils/inc
mm-ovrly-test-inc += $(LOCAL_PATH)/inc
mm-ovrly-test-inc += $(LOCAL_PATH)/../framework/inc
mm-ovrly-test-inc += $(TARGET_OUT_HEADERS)/mm-core/omxcore
mm-ovrly-test-inc += $(TARGET_OUT_HEADERS)/common/inc
mm-ovrly-test-inc += $(TARGET_OUT_HEADERS)/mm-osal/include
mm-ovrly-test-inc += $(TOP)/external/skia/include/core/
mm-ovrly-test-inc += $(TOP)/external/libpng/
mm-ovrly-test-inc += $(TOP)/frameworks/native/include/ui
mm-ovrly-test-inc += $(TOP)/hardware/qcom/display/libgralloc/

LOCAL_MODULE := WFDOvrly-app
LOCAL_CFLAGS := $(libmm-ovrly-def)
LOCAL_CFLAGS += -Wconversion
LOCAL_C_INCLUDES := $(mm-ovrly-test-inc)

LOCAL_SHARED_LIBRARIES := libmmosal
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libskia
LOCAL_SHARED_LIBRARIES += libOmxCore
LOCAL_SHARED_LIBRARIES += libmedia
LOCAL_SHARED_LIBRARIES += libstagefright
LOCAL_SHARED_LIBRARIES += libwfdmmsrc


LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES :=./src/WFDMMSourceOverlayApp.cpp


include $(BUILD_EXECUTABLE)

# ---------------------------------------------------------------------------------
#             END
# ---------------------------------------------------------------------------------
