LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -Werror -Wno-unused-parameter
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
                    $(TARGET_OUT_HEADERS)/common/inc
LOCAL_SRC_FILES:=  test_iris_server.c iris_tz_main.c iris_tz_api.c iris_parser.c iris_manager_dummy.c qcosal.c
LOCAL_SHARED_LIBRARIES := libutils liblog
LOCAL_MODULE:= iris_tz_simulator
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
include $(BUILD_EXECUTABLE)