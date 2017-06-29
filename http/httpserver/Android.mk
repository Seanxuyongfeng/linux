LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := httpserver.cpp

LOCAL_C_INCLUDES := system/core/include \
                frameworks/native/include

LOCAL_SHARED_LIBRARIES := \
    libevent \
    libcutils \
    liblog \
    libbinder \
    libutils \
    libdl

LOCAL_MODULE := httpserver

include $(BUILD_EXECUTABLE)