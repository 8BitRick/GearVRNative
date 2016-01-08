LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libvrcapture.a
#
# VrCapture
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE    := vrcapture		# generate libvrcapture.a

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_ARM_NEON  := true			# compile with neon support enabled
endif

include $(LOCAL_PATH)/../../../../cflags.mk

# Don't export any symbols from VrCapture!
LOCAL_CFLAGS += -fvisibility=hidden

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../Include

# only export public headers
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES  := ../../../Src/OVR_Capture.cpp \
                    ../../../Src/OVR_Capture_AsyncStream.cpp \
                    ../../../Src/OVR_Capture_FileIO.cpp \
                    ../../../Src/OVR_Capture_GLES3.cpp \
                    ../../../Src/OVR_Capture_Socket.cpp \
                    ../../../Src/OVR_Capture_StandardSensors.cpp \
                    ../../../Src/OVR_Capture_Thread.cpp \
                    ../../../Src/OVR_Capture_Variable.cpp 

# Enable use of OpenGL Loader vs implicit linking to GLES3
LOCAL_STATIC_LIBRARIES += openglloader
LOCAL_CFLAGS           += -DOVR_CAPTURE_HAS_OPENGL_LOADER

include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS

$(call import-module,Vendor/1stParty/OpenGL_Loader/Projects/Android/jni)