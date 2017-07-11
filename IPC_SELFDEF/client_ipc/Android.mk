LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := main.cpp

LOCAL_C_INCLUDES := system/core/include \
                frameworks/native/include

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libbinder \
    libutils \
    libdl

LOCAL_MODULE := client_dev

include $(BUILD_EXECUTABLE)