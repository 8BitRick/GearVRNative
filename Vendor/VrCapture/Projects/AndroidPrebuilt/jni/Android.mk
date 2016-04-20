LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libvrcapture.a
#
# VrCapture
#--------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := vrcapture

# TODO: use this path for importing pre-built libraries...
LOCAL_SRC_FILES := ../../../Libs/Android/$(TARGET_ARCH_ABI)/libvrcapture.a

#LOCAL_SRC_FILES := ../../Android/obj/local/$(TARGET_ARCH_ABI)/libvrcapture.a

# only export public headers
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../Include

ifneq (,$(wildcard $(LOCAL_PATH)/$(LOCAL_SRC_FILES)))
  include $(PREBUILT_STATIC_LIBRARY)
endif
