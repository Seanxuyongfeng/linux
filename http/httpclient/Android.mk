LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := httpclient_curl.cpp

LOCAL_C_INCLUDES := system/core/include \
                frameworks/native/include

LOCAL_SHARED_LIBRARIES := \
    libcurl \
    libcutils \
    liblog \
    libbinder \
    libutils \
    libdl

LOCAL_MODULE := client

include $(BUILD_EXECUTABLE)