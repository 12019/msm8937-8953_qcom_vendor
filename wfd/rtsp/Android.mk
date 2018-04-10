ifneq ($(BUILD_TINY_ANDROID),true)

ROOT_DIR := $(call my-dir)
LOCAL_PATH := $(ROOT_DIR)
include $(CLEAR_VARS)

BUILD_TEST_PROGRAMS := true

LOCAL_COPY_HEADERS_TO := wfd/rtsplib/include
LOCAL_COPY_HEADERS += rtsplib/inc/RTSPStringStream.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_api.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_client.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_common.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_helper.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_parser.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_server.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_session.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_sink.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_source.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_state.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_wfd.h
LOCAL_COPY_HEADERS += rtsplib/inc/rtsp_base.h

include $(BUILD_COPY_HEADERS)

PREBUILT_HY11_STATS_PATH := $(LOCAL_PATH)/../../prebuilt_HY11
PREBUILT_HY22_STATS_PATH := $(LOCAL_PATH)/../../prebuilt_HY22
PREBUILT_GREASE_STATS_PATH := $(LOCAL_PATH)/../../prebuilt_grease
HAS_PREBUILT_PATH := false
HAS_PREBUILT_PATH := $(shell if [ -d $(PREBUILT_HY11_STATS_PATH) ] ; then echo true; else echo false; fi)
ifeq ($(HAS_PREBUILT_PATH),false)
  HAS_PREBUILT_PATH := $(shell if [ -d $(PREBUILT_HY22_STATS_PATH) ] ; then echo true; else echo false; fi)
endif

ifeq ($(HAS_PREBUILT_PATH),false)
  HAS_PREBUILT_PATH := $(shell if [ -d $(PREBUILT_GREASE_STATS_PATH) ] ; then echo true; else echo false; fi)
endif

ifneq ($(HAS_PREBUILT_PATH),true)
# ---------------------------------------------------------------------------------
#                 Common definitons
# ---------------------------------------------------------------------------------

rtsp-def := -DRTSP_DBG_ALL -D_ANDROID_
rtsp-inc := $(TARGET_OUT_HEADERS)/wfd/rtsplib/include
rtsp-inc += external/libcxx/include
rtsp-inc += $(TARGET_OUT_HEADERS)/mm-osal/include/
rtsp-inc += $(TARGET_OUT_HEADERS)/common/inc
# ---------------------------------------------------------------------------------
#             Make the apps
# ---------------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE := libwfdrtsp
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(rtsp-def)
LOCAL_C_INCLUDES := $(rtsp-inc)
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := libc++
LOCAL_SRC_FILES := rtsplib/src/rtsp_base.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_state.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_source.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_sink.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_session.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_linux.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_api.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_parser.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_helper.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_server.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_client.cpp
LOCAL_SRC_FILES += rtsplib/src/rtsp_wfd.cpp
LOCAL_SRC_FILES += rtsplib/src/RTSPStringStream.cpp

LOCAL_SHARED_LIBRARIES += libcutils libutils libmmosal libwfdcommonutils
LOCAL_LDLIBS += -llog
LOCAL_MODULE_OWNER := qti
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)

ifeq ($(BUILD_TEST_PROGRAMS),true)

include $(CLEAR_VARS)
LOCAL_MODULE := rtspclient
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(rtsp-def)
LOCAL_C_INCLUDES := $(rtsp-inc)
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := libwfdrtsp libc++ libmmosal libwfdcommonutils
LOCAL_SRC_FILES    := rtsptest/sink/src/sink.cpp
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := rtspserver
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(rtsp-def)
LOCAL_C_INCLUDES := $(rtsp-inc)
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := libwfdrtsp libc++ libmmosal libwfdcommonutils
LOCAL_SRC_FILES    := rtsptest/source/src/source.cpp
include $(BUILD_EXECUTABLE)

endif #BUILD_TEST_PROGRAMS
endif #Prebuilt Check
endif #BUILD_TINY_ANDROID

# ---------------------------------------------------------------------------------
#                     END
# ---------------------------------------------------------------------------------
