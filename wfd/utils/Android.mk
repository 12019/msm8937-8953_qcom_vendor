LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# ------------------------------------------------------------------------------
#            Common definitons
# ------------------------------------------------------------------------------

LOCAL_CFLAGS := -D_ANDROID_
ifeq ($(call is-android-codename,ICECREAM_SANDWICH),true)
LOCAL_CFLAGS += -DWFD_ICS
endif

ifeq ($(call is-board-platform-in-list,msm8996),true)
LOCAL_CFLAGS += -DDISPLAY_DS_APIS_AVAILABLE
endif

# ------------------------------------------------------------------------------
#            WFD COMMON UTILS SRC
# ------------------------------------------------------------------------------

LOCAL_SRC_FILES := \
    src/wfd_cfg_parser.cpp \
    src/wfd_netutils.cpp\
    src/WFDMMThreads.cpp\
    src/WFDUtils.cpp\
    src/WFDMMSourceStatistics.cpp\
    src/WFDMessages.cpp\
    src/MMIonMemory.cpp\
    src/WFDMMTouchEventObserver.cpp\
    src/WFDMMSourceSignalQueue.cpp\
    src/wfd_util_queue.c\
    src/wfd_util_mutex.c\
    src/wfd_util_signal.c\
    src/wfdmmsource_queue.cpp\
    src/WFDMMSourceMutex.cpp\
    src/WFDMMSourceQueue.cpp\
    src/WFDMMSourceSignal.cpp\
    src/CursorHandler.cpp

# ------------------------------------------------------------------------------
#             WFD COMMON UTILS INC
# ------------------------------------------------------------------------------

LOCAL_C_INCLUDES := $(LOCAL_PATH)/./inc
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../uibc/interface/inc
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../mm/interface/inc
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../mm/source/utils/inc
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/common/inc
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-osal/include
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-core/omxcore
LOCAL_C_INCLUDES += external/libcxx/include
LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/display/libqservice/
LOCAL_C_INCLUDES += external/libnl/include
LOCAL_C_INCLUDES += $(TOP)/system/core/include/utils
LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/display/libqdutils
LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/display/libgralloc

# ---------------------------------------------------------------------------------
#            COPY INCLUDE FILES TO OUT
# ---------------------------------------------------------------------------------

LOCAL_COPY_HEADERS_TO := wfd/include
LOCAL_COPY_HEADERS := inc/WFDMMLogs.h
LOCAL_COPY_HEADERS += inc/WFDMessages.h
LOCAL_COPY_HEADERS += inc/WFDMMThreads.h
LOCAL_COPY_HEADERS += inc/WFDUtils.h
LOCAL_COPY_HEADERS += inc/MMIonMemory.h
LOCAL_COPY_HEADERS += inc/WFDMMSourceSignalQueue.h

# ------------------------------------------------------------------------------
#            WFD COMMON UTILS SHARED LIB
# ------------------------------------------------------------------------------

LOCAL_SHARED_LIBRARIES := libmmosal
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libqservice
LOCAL_SHARED_LIBRARIES += libqdutils
LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libnl
LOCAL_LDLIBS += -llog

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libwfdcommonutils

LOCAL_MODULE_OWNER := qcom
LOCAL_PROPRIETARY_MODULE := true
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
include $(BUILD_SHARED_LIBRARY)
include $(call all-makefiles-under,$(LOCAL_PATH))

# ------------------------------------------------------------------------------
#            END
# ------------------------------------------------------------------------------
