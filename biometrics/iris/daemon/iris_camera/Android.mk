OLD_LOCAL_PATH := $(LOCAL_PATH)
LOCAL_PATH:=$(call my-dir)

include $(CLEAR_VARS)

LOCAL_CFLAGS += -DCAMERA_ION_HEAP_ID=ION_IOMMU_HEAP_ID

LOCAL_SRC_FILES:= src/iris_camera.c

LOCAL_C_INCLUDES:=$(LOCAL_PATH)/inc \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
        hardware/qcom/camera/QCamera2/stack/common \
        hardware/qcom/camera/QCamera2/stack/mm-camera-interface/inc

LOCAL_CFLAGS += -Wall -Wextra -Werror
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_SHARED_LIBRARIES:= \
         libcutils libdl libmmcamera_interface

LOCAL_MODULE_TAGS := optional

LOCAL_32_BIT_ONLY := $(BOARD_QTI_CAMERA_32BIT_ONLY)

LOCAL_MODULE:= libiriscamera
LOCAL_CLANG := false

include $(BUILD_SHARED_LIBRARY)

LOCAL_PATH := $(OLD_LOCAL_PATH)
