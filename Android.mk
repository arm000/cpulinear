LOCAL_PATH := $(call my-dir)


include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := cpulinear

include $(LOCAL_PATH)/../Android.common.mk

LOCAL_SRC_FILES += cpulinear.cpp
#LOCAL_STATIC_LIBRARIES += libm

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libEGL \
    libGLESv2 \
    libui \
    libgui \
    libutils \
    libz \
    libstlport

include $(NVIDIA_EXECUTABLE)
