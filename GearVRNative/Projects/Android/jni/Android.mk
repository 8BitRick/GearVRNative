LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libvrcubeworld.so
#--------------------------------------------------------
include $(CLEAR_VARS)
LOCAL_MODULE := fmod
LOCAL_SRC_FILES := ../../../Libs/lib$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := fmodL
LOCAL_SRC_FILES := ../../../Libs/lib$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

include ../../../cflags.mk

LOCAL_MODULE			:= gearvrnative
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../Include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../Include/fmod
LOCAL_SRC_FILES			:= ../../../Src/GearVRNative.cpp
LOCAL_SRC_FILES			+= ../../../Src/VrCubeWorld.cpp
LOCAL_SRC_FILES			+= ../../../Src/GVRAudioMgr.cpp
LOCAL_STATIC_LIBRARIES	+= systemutils vrsound vrlocale vrgui vrappframework libovrkernel
LOCAL_SHARED_LIBRARIES	+= vrapi fmodL

include $(BUILD_SHARED_LIBRARY)

$(call import-module,Vendor/LibOVRKernel/Projects/AndroidPrebuilt/jni)
$(call import-module,Vendor/VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,Vendor/VrAppFramework/Projects/AndroidPrebuilt/jni)
$(call import-module,Vendor/VrAppSupport/SystemUtils/Projects/AndroidPrebuilt/jni)
$(call import-module,Vendor/VrAppSupport/VrGui/Projects/AndroidPrebuilt/jni)
$(call import-module,Vendor/VrAppSupport/VrLocale/Projects/AndroidPrebuilt/jni)
$(call import-module,Vendor/VrAppSupport/VrSound/Projects/AndroidPrebuilt/jni)
