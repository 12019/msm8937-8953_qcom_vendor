LOCAL_PATH := $(call my-dir)

# Make the JNI part
# ============================================================
include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wno-unused-parameter

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../hal
LOCAL_SHARED_LIBRARIES += \
       libnativehelper \
       libcutils \
       libandroid_runtime \
       libhardware_wigig
LOCAL_SRC_FILES := \
        jni/com_qualcomm_qti_server_wigig_WigigNative.cpp
LOCAL_MODULE := libwigig-service
include $(BUILD_SHARED_LIBRARY)

# Build the java code
# ============================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, java)

LOCAL_STATIC_JAVA_LIBRARIES := wigig_manager
LOCAL_JAVA_LIBRARIES := services
LOCAL_REQUIRED_MODULES := services
LOCAL_MODULE := wigig-service

include $(BUILD_JAVA_LIBRARY)
