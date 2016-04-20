LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := minizip
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

include $(LOCAL_PATH)/../../../../../cflags.mk

LOCAL_SRC_FILES := \
  $(LOCAL_PATH)/../../../../src/ioapi.c \
  $(LOCAL_PATH)/../../../../src/miniunz.c \
  $(LOCAL_PATH)/../../../../src/mztools.c \
  $(LOCAL_PATH)/../../../../src/unzip.c \
  $(LOCAL_PATH)/../../../../src/zip.c

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../src

include $(BUILD_STATIC_LIBRARY)
