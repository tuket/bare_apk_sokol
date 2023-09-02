C:\Program Files\Java\jdk-20\bin\keytool.exe
C:\Program Files\Java\jdk-20\bin\jarsigner.exe
https://stackoverflow.com/questions/50705658/how-to-sign-an-apk-through-command-line
https://hero.handmade.network/forums/code-discussion/t/3016-guide_-_how_to_build_a_native_android_app_using_a_single_batch_file_for_visual_studio
https://stackoverflow.com/questions/39091845/create-android-apk-manually-via-command-line-makefile
https://connortumbleson.com/2018/02/19/taking-a-look-at-aapt2/

https://medium.com/mindorks/how-i-decreased-my-app-size-to-70-using-apk-analyser-4a6f79512072
https://interrupt.memfault.com/blog/best-and-worst-gcc-clang-compiler-flags#-ffunction-sections--fdata-sections----gc-sections

Sample native activity (%0 java): https://developer.android.com/ndk/samples/sample_na.html


---------------------------------

NDK = C:\Users\tuket\Documents\APPS\android_sdk\ndk\25.2.9519653

CC_ARM64 = C:\Users\tuket\Documents\APPS\android_sdk\ndk\25.2.9519653\toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android26-clang.cmd

CFLAGS+= -I$(RAWDRAWANDROID)/rawdraw -I$(NDK)/sysroot/usr/include -I$(NDK)/sysroot/usr/include/android -I$(NDK)/toolchains/llvm/prebuilt/$(OS_NAME)/sysroot/usr/include -I$(NDK)/toolchains/llvm/prebuilt/$(OS_NAME)/sysroot/usr/include/android -fPIC -I$(RAWDRAWANDROID) -DANDROIDVERSION=$(ANDROIDVERSION)

CFLAGS+= -I$(NDK)/toolchains/llvm/prebuilt/$(OS_NAME)/sysroot/usr/include -fPIC -DANDROIDVERSION=21

C:\Users\tuket\Documents\APPS\android_sdk\ndk\25.2.9519653\toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android26-clang.cmd -IC:\Users\tuket\Documents\APPS\android_sdk\ndk\25.2.9519653\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\include -fPIC -DANDROIDVERSION=21 -m64 -o build/libtest.so main.c -LC:\Users\tuket\Documents\APPS\android_sdk\ndk\25.2.9519653\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\aarch64-linux-android\21 -s -lm -lGLESv3 -lEGL -landroid -llog -lOpenSLES -shared -uANativeActivity_onCreategcc

C:\Users\tuket\Documents\APPS\android_sdk\ndk\25.2.9519653\toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android26-clang.cmd
-IC:\Users\tuket\Documents\APPS\android_sdk\ndk\25.2.9519653\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\include -fPIC -DANDROIDVERSION=21
-m64
-o build/libtest.so
main.c
-LC:\Users\tuket\Documents\APPS\android_sdk\ndk\25.2.9519653\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\aarch64-linux-android\21
-Wl,--gc-sections -Wl,-Map=output.map -s
-lm -lGLESv3 -lEGL -landroid -llog -lOpenSLES
-shared -uANativeActivity_onCreategcc

------

C:\Users\tuket\Documents\APPS\android_sdk\build-tools\34.0.0-rc3\aapt.exe package -f -M .\AndroidManifest.xml -S .\res\ -I C:\Users\tuket\Documents\APPS\android_sdk\platforms\android-21\android.jar -F .\build\test.unaligned.apk .\build\tmp\

& "C:\Program Files\Java\jdk-20\bin\keytool.exe" -genkey -keystore test.keystore -storepass android -alias test -keyalg RSA -keysize 2048 -validity 50000

& 'c:\Program Files\Java\jdk-20\bin\jarsigner.exe' -sigalg SHA1withRSA -digestalg SHA1 -storepass android -keypass android -keystore .\test.keystore -signedJar .\build\test.signed.apk .\build\test.unaligned.apk test

C:\Users\tuket\Documents\APPS\android_sdk\platform-tools\adb.exe install .\build\test.signed.apk