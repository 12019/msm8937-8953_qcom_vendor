LOCAL_PATH:= $(call my-dir)

########################################################
#Test Iris Dev
include $(CLEAR_VARS)
LOCAL_CFLAGS += -Werror -Wno-unused-parameter
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../daemon/hardware/
LOCAL_SRC_FILES:=  test_iris_dev.cpp
LOCAL_SHARED_LIBRARIES := libirisdev libutils
LOCAL_MODULE:= iris_dev_test
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_OWNER := qti
LOCAL_MULTILIB := 32
include $(BUILD_EXECUTABLE)

########################################################
#Test Iris Daemon
include $(CLEAR_VARS)
LOCAL_CFLAGS += -Werror -Wno-unused-parameter
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../daemon/service/ \
		$(LOCAL_PATH)/../../daemon/hardware/
LOCAL_SRC_FILES:=  test_iris_daemon.cpp
LOCAL_SHARED_LIBRARIES := \
	libbinder \
        libandroid_runtime \
        libc \
        libcutils \
	liblog \
	libutils \
	libkeystore_binder \
	libirisdev \
	libiris_daemon

LOCAL_MODULE:= iris_daemon_test
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_OWNER := qti
LOCAL_MULTILIB := 32
include $(BUILD_EXECUTABLE)

########################################################
#Test Iris TZ API
include $(CLEAR_VARS)
LOCAL_CFLAGS += -Werror -Wno-unused-parameter
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
                    vendor/qcom/proprietary/securemsm/QSEEComAPI \
                    $(TARGET_OUT_HEADERS)/common/inc \
                    hardware/libhardware/include \
                    $(LOCAL_PATH)/../../daemon/hardware/
LOCAL_SRC_FILES:= test_tz_api.cpp
LOCAL_SHARED_LIBRARIES := libutils libbinder liblog libQSEEComAPI libirisdev
LOCAL_MODULE:= test_tz_api
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_OWNER := qti
LOCAL_MULTILIB := 32
include $(BUILD_EXECUTABLE)

########################################################
#Test Iris Iris Camera
include $(CLEAR_VARS)
LOCAL_CFLAGS += -Werror -Wno-unused-parameter
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
                    vendor/qcom/proprietary/securemsm/QSEEComAPI \
                    $(TARGET_OUT_HEADERS)/common/inc \
                    hardware/libhardware/include \
                    $(LOCAL_PATH)/../../daemon/hardware/ \
                    $(LOCAL_PATH)/../../daemon/iris_camera/inc \
                    hardware/qcom/camera/QCamera2/stack/common \
                    hardware/qcom/camera/QCamera2/stack/mm-camera-interface/inc

LOCAL_SRC_FILES:= test_iris_camera_source.cpp
LOCAL_SHARED_LIBRARIES := libutils libbinder liblog libQSEEComAPI libirisdev  libgui libcamera_client
LOCAL_MODULE:= test_iris_camera_source
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_OWNER := qti
LOCAL_MULTILIB := 32
include $(BUILD_EXECUTABLE)
