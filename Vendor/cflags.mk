# Use the global cflags.mk at root of project
TEMP_BUILD_LOCAL_PATH := $(call my-dir)
include $(TEMP_BUILD_LOCAL_PATH)/../cflags.mk
