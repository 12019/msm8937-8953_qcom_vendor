TOP_LOCAL_PATH := $(call my-dir)

#
# Build JNI library
#

include $(call all-subdir-makefiles)

#
# Build sample application package
#

LOCAL_PATH:= $(TOP_LOCAL_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/data/qcom/scve
LOCAL_JNI_SHARED_LIBRARIES  := liballocationimage \
                               libFaceRecognitionJNI \
                               libsocialcamera_util \
                               libsocialcamera_khr_egl_extensions \
                               libsocialcamera_khr_gles_extensions
ifeq ($(SCVE_MAKE_STANDALONE_APK), true)
   LOCAL_JNI_SHARED_LIBRARIES += libscveCommon \
                                 libscveFaceRecognition \
                                 libc++ \
                                 libmmcamera_faceproc \
                                 libfastcvopt \
                                 libadsprpc \
                                 libscveCommon_stub
endif

LOCAL_SRC_FILES             := $(call all-java-files-under, java/android) \
                               $(call all-java-files-under, java/com/qualcomm/qti/facerecognition) \
                               $(call all-java-files-under, java/com/qualcomm/qti/fr_camera) \
                               $(call all-java-files-under, java/com/qualcomm/qti/khronos) \
                               $(call all-renderscript-files-under, rs)

ifeq ($(call is-platform-sdk-version-at-least,23), true)
LOCAL_SRC_FILES             += $(call all-java-files-under, java/com/qualcomm/qti/permission/request)
else
LOCAL_SRC_FILES             += $(call all-java-files-under, java/com/qualcomm/qti/permission/dummy)
endif


LOCAL_PACKAGE_NAME          := SocialCamera
#TARGET_BUILD_APPS           := true
LOCAL_PROGUARD_FLAG_FILES   := proguard-project.txt

LOCAL_RENDERSCRIPT_TARGET_API := 21

include vendor/qcom/proprietary/scve/reference/common.mk

include $(BUILD_PACKAGE)
