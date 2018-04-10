LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    qti-game_trigger.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libutils \
    libxml2

LOCAL_CFLAGS += \
    -Wall \
    -DQC_DEBUG=0

LOCAL_C_INCLUDES := external/connectivity/stlport/stlport \
                    external/libxml2/include \
                    external/icu/icu4c/source/common \
                    vendor/qcom/proprietary/android-perf/mp-ctl

LOCAL_MODULE := libqti-gt
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
