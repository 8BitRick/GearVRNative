LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := stb
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

include $(LOCAL_PATH)/../../../../../cflags.mk

LOCAL_SRC_FILES := \
  $(LOCAL_PATH)/../../../src/stb_image.c \
  $(LOCAL_PATH)/../../../src/stb_image_write.c \
  $(LOCAL_PATH)/../../../src/stb_vorbis.c

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../src

include $(BUILD_STATIC_LIBRARY)
