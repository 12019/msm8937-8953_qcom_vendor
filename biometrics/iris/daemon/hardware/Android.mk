LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -Werror -Wno-unused-parameter
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
                    vendor/qcom/proprietary/securemsm/QSEEComAPI \
                    $(TARGET_OUT_HEADERS)/common/inc \
                    hardware/libhardware/include \
                    hardware/qcom/media/libstagefrighthw \
                    $(LOCAL_PATH)/../iris_camera/inc \
                    hardware/qcom/camera/QCamera2/stack/common \
                    hardware/qcom/camera/QCamera2/stack/mm-camera-test/inc \
                    hardware/qcom/camera/QCamera2/stack/mm-camera-interface/inc
LOCAL_SRC_FILES:=  iris_tz_api.cpp  iris_dev.cpp  iris_enroll_task.cpp iris_task.cpp iris_tz_comm.cpp iris_socket_comm.cpp \
LOCAL_SRC_FILES += iris_camera_source.cpp iris_auth_task.cpp iris_fd_socket_mapper.cpp iris_remove_task.cpp
LOCAL_SHARED_LIBRARIES := libutils libbinder liblog libQSEEComAPI libgui  libiriscamera libcutils
LOCAL_MODULE:= libirisdev
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_OWNER := qti
LOCAL_MULTILIB := 32
CPPFLAGS += -I$(SYSCORE_DIR)/include
include $(BUILD_SHARED_LIBRARY)
