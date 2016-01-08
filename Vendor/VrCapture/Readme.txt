VrCapture Quick Integration Guide.

------------------------------------------------------------------------------------------------------
About

    VrCapture is a library that is already built into VrApi that allows remote access to performance 
    and debugging metrics in real time via the OVRMonitor client tool. For more information, please 
    refer to the documentation at https://developer.oculus.com/documentation/mobilesdk/latest

    Beyond capturing data from inside VrApi, it is also possible to integrate VrCapture
    directly into your application to capture application specific data. But this is 
    entirely optional and is what this document addresses.

    VrCapture will do nothing (no sockets, threads, allocations, etc) until you initialize it.

------------------------------------------------------------------------------------------------------
Enable Network Access

    The first prerequisite is to make sure your Android Manifest has "android.permission.INTERNET"
    permission enabled. You can do this by adding the following line...

    <uses-permission android:name="android.permission.INTERNET" />


------------------------------------------------------------------------------------------------------
Linking/Including the ndk-build way

    If your application is using ndk-build based makefiles, linking to VrCapture is easy.

    1) Include at the bottom of your Android.mk...
       include <PATH_TO_VRCAPTURE>/Projects/Android/jni/Android.mk

    2) Between "include $(CLEAR_VARS)" and "include $(BUILD_SHARED_LIBRARY)" add...
       LOCAL_STATIC_LIBRARIES += vrcapture

    And thats it, VrCapture will now be linked into your application and header search paths
    setup properly. This is also convenient because it will automatically compile VrCapture
    to your target architecture.

------------------------------------------------------------------------------------------------------
Linking/Including manually

    In the case you have a custom build system...

    1) First, build VrCapture library...
       cd <PATH_TO_VRCAPTURE>/Projects/Android
       ndk-build -j16

    2) Add the header search path to your compiler line...
       -I<PATH_TO_VRCAPTURE>/Include

    3) Add the library to your link line...
       -L<PATH_TO_VRCAPTURE>/Projects/Android/obj/local/<TARGET_ARCHITECTURE> -lvrcapture


------------------------------------------------------------------------------------------------------
Init and Shutdown For Remote Capture over socket

    Capturing over socket is a quick and convenient way of analysing/monitor applications in real-time
    or for connecting to sessions already in progress when things go wrong.

    1) Include the primary VrCapture header...
         #include <OVR_Capture.h>

    2) Somewhere near the beginning of execution of your application...
         OVR::Capture::InitForRemoteCapture();

    3) Somewhere near the end of execution of your application...
         OVR::Capture::Shutdown();

------------------------------------------------------------------------------------------------------
Init and Shutdown For Local Capture to disk

    Capturing straight to disk is convenient for scenarios where network performance does not allow
    for robust real-time capturing or for setting up automated performance testing.

    1) Include the primary VrCapture header...
         #include <OVR_Capture.h>

    2) Somewhere near the beginning of execution of your application...
         OVR::Capture::InitForLocalCapture("/sdcard/capture.dat");

    3) Somewhere near the end of execution of your application...
         OVR::Capture::Shutdown();

    4) OVRMonitor expects capture file to be gzip compressed, so before downloading from the
       device make sure you compress your capture.dat file.
         adb shell gzip -9 /sdcard/capture.dat

    5) Download your compressed capture file.
         adb pull /sdcard/capture.dat.gz capture.dat

------------------------------------------------------------------------------------------------------
Enabling VrCapture only when requested

    VrApi does not call Init or Shutdown unless Vr Developer Mode is enabled and a developer
    configuration file has capture enabled. More information can be found in the SDK docs.

    But it is suggested that your application does not ship with capture turned on. VrApi provides
    a mechanism for you to query if Vr Developer Mode and Capture is enabled in VrApi_LocalPrefs.h.

    ovr_GetLocalPreferenceValueForKey(LOCAL_PREF_VRAPI_ENABLE_CAPTURE);

------------------------------------------------------------------------------------------------------
CPU Zones

    One of the fundamental pieces of OVRMonitor is its ability to handle a large number of performance
    zones. Zones can be dropped in simply with a single line and have extremely low overhead. e.g....

    void SomeFunction(void)
    {
        OVR_CAPTURE_CPU_ZONE(SomeFunction);
        // Do Something Expensive Here
    }

------------------------------------------------------------------------------------------------------
Capturing Log Output

    Routing debug messages through OVR::Capture::Logf() allows the remote monitoring tool to not only
    record log messages, but also records their time and thread id which when coupled with detailed
    CPU Zones allows for very precise information on when and where messages/errors/warnings occur.

    But in applications that use VrApi, all logcat messages originating from the current process are
    already intercepted. So typical applications do not need to do anything special to capture their
    log output.
