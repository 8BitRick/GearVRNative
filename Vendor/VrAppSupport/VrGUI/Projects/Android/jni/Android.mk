LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libvrgui.a
#
# VrGui
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

#APP_MODULE      := vrgui

LOCAL_MODULE    := vrgui			# generate libvrgui.a

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb
LOCAL_ARM_NEON  := true				# compile with neon support enabled

include $(LOCAL_PATH)/../../../../../cflags.mk

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../Src

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../Src

LOCAL_SRC_FILES := 	../../../Src/ActionComponents.cpp \
					../../../Src/AnimComponents.cpp \
					../../../Src/CollisionPrimitive.cpp \
					../../../Src/DefaultComponent.cpp \
					../../../Src/Fader.cpp \
					../../../Src/FolderBrowser.cpp \
					../../../Src/GazeCursor.cpp \
					../../../Src/GuiSys.cpp \
					../../../Src/MetaDataManager.cpp \
					../../../Src/ProgressBarComponent.cpp \
					../../../Src/ScrollBarComponent.cpp \
					../../../Src/ScrollManager.cpp \
					../../../Src/SoundLimiter.cpp \
					../../../Src/SwipeHintComponent.cpp \
					../../../Src/TextFade_Component.cpp \
					../../../Src/VolumePopup.cpp \
					../../../Src/VRMenu.cpp \
					../../../Src/VRMenuComponent.cpp \
					../../../Src/VRMenuEvent.cpp \
					../../../Src/VRMenuEventHandler.cpp \
					../../../Src/VRMenuMgr.cpp \
					../../../Src/VRMenuObject.cpp \
					../../../Src/SliderComponent.cpp \
					../../../Src/UI/UITexture.cpp \
                    ../../../Src/UI/UIMenu.cpp \
                    ../../../Src/UI/UIObject.cpp \
                    ../../../Src/UI/UIContainer.cpp \
                    ../../../Src/UI/UILabel.cpp \
                    ../../../Src/UI/UIImage.cpp \
                    ../../../Src/UI/UIButton.cpp \
                    ../../../Src/UI/UIProgressBar.cpp \
                    ../../../Src/UI/UINotification.cpp \
					../../../Src/UI/UISlider.cpp \
					../../../Src/UI/UIDiscreteSlider.cpp

LOCAL_STATIC_LIBRARIES := vrappframework systemutils

include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS

$(call import-module,Vendor/VrAppFramework/Projects/AndroidPrebuilt/jni)

# Note: Even though we depend on SystemUtils, we don't explicitly import it
# since our dependent projects may want either a prebuilt or from-source version.