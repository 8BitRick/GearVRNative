LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libMediaSurfacePlugin.so
#
# MediaSurface Plugin
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE    := OculusMediaSurface		# generate OculusMediaSurface.so

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb
LOCAL_ARM_NEON  := true				# compile with neon support enabled

include $(LOCAL_PATH)/../../../../../cflags.mk

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../../../../VrAppFramework/Include

LOCAL_SRC_FILES  := ../../../../../VrAppFramework/Src/GlProgram.cpp \
					../../../../../VrAppFramework/Src/GlGeometry.cpp \
					../../../Src/MediaSurfacePlugin.cpp \
                    ../../../Src/SurfaceTexture.cpp \
                    ../../../Src/MediaSurface.cpp

# OpenGL ES 3.0
LOCAL_LDLIBS := -lGLESv3
# GL platform interface
LOCAL_LDLIBS += -lEGL
# logging
LOCAL_LDLIBS += -llog

LOCAL_STATIC_LIBRARIES += libovrkernel

include $(BUILD_SHARED_LIBRARY)		# start building based on everything since CLEAR_VARS

ifneq (,$(wildcard $(LOCAL_PATH)/../../../../../LibOVRKernel/Projects/Android))
$(call import-module,Vendor/LibOVRKernel/Projects/Android/jni)
else
$(call import-module,Vendor/LibOVRKernel/Projects/AndroidPrebuilt/jni)
endif
