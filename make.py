import sys
import os
import subprocess
import shutil

APP_NAME = "test"
ANDROID_MIN_VERSION = 24 # 24 is needed for using camera2 feature of the NDK. But it's not working anyways, and we are using the Java API through JNI. So we could actually reduce the version to 21
ANDROID_TARGET_VERSION = 33
SDK_PATH = "C:/Android"
JDK_PATH = "C:/Program Files/Java/jdk-20"

arch = "arm32"
#arch = "arm64"
isArm32 = arch == "arm32"
isArm64 = arch == "arm64"

shutil.rmtree("build")
os.mkdir("build")
os.mkdir("build/tmp/")
os.mkdir("build/tmp/lib")
OUTPUT_SO_FOLDER = "build/tmp/lib/arm64-v8a" if isArm64 else "build/tmp/lib/armeabi-v7a"
os.mkdir(OUTPUT_SO_FOLDER)

ndk_versions = os.listdir(SDK_PATH + "/ndk")
if len(ndk_versions) == 0:
    print("Error: no NDKs installed")
    exit()

# locate the most recent NDK installed
ndk_versions = [tuple(map(int, str.split("."))) for str in ndk_versions]
ndk_versions = sorted(ndk_versions, reverse=True)
NDK_PATH = SDK_PATH + "/ndk/" + ".".join([str(x) for x in ndk_versions[0]])

LLVM_PATH = NDK_PATH + "/toolchains/llvm/prebuilt/windows-x86_64"
COMPILER_PATH = LLVM_PATH + f"/bin/{'aarch64' if isArm64 else 'armv7a'}-linux-android{'eabi' if isArm32 else ''}{ANDROID_MIN_VERSION}-clang.cmd"
print(COMPILER_PATH)
INCLUDE_PATH = LLVM_PATH + "/sysroot/usr/include"
LIB_PATH = LLVM_PATH + "/sysroot/usr/lib/" + ("aarch64-linux-android/" if isArm64 else "arm-linux-androideabi/") + str(ANDROID_MIN_VERSION)
# compile the .so file
subprocess.run([
    COMPILER_PATH,
    "-I" + INCLUDE_PATH,
    "-fPIC",
    "-DAPP_NAME=\"" + APP_NAME + '"',
    "-DNDEBUG",
    "-DANDROID",
    "-DANDROIDVERSION=" + str(ANDROID_MIN_VERSION),
    "-m64" if isArm64 else "-m32",

    #"-Os", # optimize for size
    #"-target", "armv7-none-linux-androideabi" + str(ANDROID_TARGET_VERSION),
    "-fdata-sections",
    "-ffunction-sections",
    "-flto",
    #"-fstack-protector-strong",
    #"-funwind-tables",
    #"-mthumb",
    #"-Wl,--build-id=sha1",
    #"-Wl,--no-rosegment",
    #"-g",

    "-o", OUTPUT_SO_FOLDER + "/libtest.so",
    "main_sokol.c",
    "-L" + LIB_PATH,
    "-Wl,--gc-sections",
    #"-Wl,-Map=output.map",
    "-s",
    "-lc", "-lm", "-lGLESv3", "-lEGL", "-landroid", "-llog", "-lOpenSLES",
    "-latomic",
    "-lcamera2ndk",
    "-shared",
    "-u", "ANativeActivity_onCreate"
]).check_returncode()

# pack everyting into the .apk file using aapt
BUILD_TOOLS_PATH = SDK_PATH + "/build-tools/34.0.0-rc3"
AAPT_EXE = BUILD_TOOLS_PATH + "/aapt.exe"
subprocess.run([
    AAPT_EXE,
    "package",
    "-f",
    "-M", "./AndroidManifest.xml",
    "-S", "./res/",
    "-I", SDK_PATH + "/platforms/android-" + str(ANDROID_MIN_VERSION) + "/android.jar",
    "-F", "./build/test.unaligned.apk",
    "-A", "assets",
    "./build/tmp"
]).check_returncode()

# sign the apk (jarsigner)
cur_apk_name = "./build/test.unaligned.apk"
use_apksigner = True
if not use_apksigner:
    JARSIGNER_PATH = JDK_PATH + "/bin/jarsigner.exe"
    subprocess.run([
        JARSIGNER_PATH,
        "-sigalg", "SHA256withRSA",
        "-digestalg", "SHA256",
        "-storepass", "android",
        "-keypass", "android", 
        "-keystore" ,"./test.keystore",
        "-signedJar", "./build/test.signed.apk",
        cur_apk_name,
        "test"
    ]).check_returncode()
    cur_apk_name = "./build/test.signed.apk"

# (optional) align the apk for better performance in loading
ZIPALIGN_PATH = BUILD_TOOLS_PATH + "/zipalign.exe" # https://developer.android.com/tools/zipalign
subprocess.run([
    ZIPALIGN_PATH,
    "-p", # Page-aligns uncompressed .so files
    "-f", # overwrite existing output file
    #"-z", # recompress using the zopfli algorithm, which has better compression ratios but is slower
    "4", # 4 byte alignment
    cur_apk_name, # input
    "./build/test.apk" # output
]).check_returncode()
cur_apk_name = "./build/test.apk"

# sign apk (apksigner)
# unlike jarsigner, apksigner must be run *after* aligning
if use_apksigner:
    APKSIGNER_PATH = BUILD_TOOLS_PATH + "/apksigner.bat"
    subprocess.run([
        APKSIGNER_PATH,
        "sign",
        "--ks-key-alias", "test",
        "--ks", "test.keystore",
        "--ks-pass", "pass:android",
        cur_apk_name
    ]).check_returncode()

gotta_run = len(sys.argv) > 1 and sys.argv[1] == "run"
gotta_install = len(sys.argv) > 1 and sys.argv[1] == "install"
gotta_install = gotta_install or gotta_run

# install in the device
ADB_PATH = SDK_PATH + "/platform-tools/adb.exe"
if gotta_install:
    subprocess.run([
        ADB_PATH,
        "install",
        "-r",
        cur_apk_name
    ]).check_returncode()

# run in the device
if gotta_run:
    print("run!\n")
    subprocess.run([
        ADB_PATH,
        "shell", "am", "start",
        "-n",
        "org.my_organization.test/android.app.NativeActivity"
    ]).check_returncode()
    if 1:
        f = open("log.txt", "w")
        subprocess.run([
            ADB_PATH,
            "shell", "logcat",
            #"-s", "BARE_APK"
        ], stdout=f).check_returncode()