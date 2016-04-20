# GearVRNative
Gear VR Native base project with FMOD - better suited for starting new projects

## What is this?
This project was created to have a better starting place to build a native Gear VR project.
There are still a few places that could be cleaner but overall stuff has been structured and cut down to mostly just the essentials.
Specifically, a lot of work was put in to separating the Oculus libraries from main application code.
Also, I really wanted **Intellisense** so a working Visual Studio project and solution are included.
This is based on the VrCubeWorld_Framework project.

## How is this structured?
* GearVRNative - This is your module (C++ stuff is at this level)
* GearVRNative/Projects/Android - This is the Android project (Java stuff is at this level)
* Vendor - The Oculus libraries are in here

## How do I make this my own?
**Search and replace**
* yourcomp - this is current name of company in project, replace it with your company name
* GearVRNative - replace these with your project name
* GVR - This is the namespace used, replace with your namespace
* Gear VR Native - Replace this with the human readable name of your app

## Other thoughts
At the moment, "Intellisense" in Android Studio is broken for C++. 
So I recommend using Visual Studio to write your C++ and Android Studio to do your building.
You can have both open at the same time without any problems.

## Contact
You can email me directly or message me on the Oculus forums.
- On Oculus forums: 8bit
- Email: rickalo801@gmail.com
