
########################################################
#Iris Daemon lib

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS := -Wall -Wextra -Werror -Wunused -Wno-unused-parameter
LOCAL_SRC_FILES := \
	BpIrisDaemon.cpp \
	BnIrisDaemonCallback.cpp \
	BpIrisDaemonCallback.cpp
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../hardware/ \
		hardware/libhardware/include
LOCAL_MODULE := libiris_daemon
LOCAL_SHARED_LIBRARIES := \
	libbinder \
	liblog \
	libutils

LOCAL_MODULE_OWNER := qti
LOCAL_MULTILIB := 32
include $(BUILD_SHARED_LIBRARY)

########################################################
#Iris Daemon main

include $(CLEAR_VARS)
LOCAL_CFLAGS := -Wall -Wextra -Werror -Wunused -Wno-unused-parameter
LOCAL_SRC_FILES := irisd_main.cpp BnIrisDaemon.cpp BpIrisDaemon.cpp IrisDaemonProxy.cpp BpIrisDaemonCallback.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../hardware/ \
		hardware/libhardware/include
LOCAL_MODULE := irisdaemon
LOCAL_SHARED_LIBRARIES := \
	libbinder \
	liblog \
	libutils \
	libkeystore_binder \
	libirisdev \
	libcutils 
LOCAL_MODULE_OWNER := qti
LOCAL_MULTILIB := 32
include $(BUILD_EXECUTABLE)

