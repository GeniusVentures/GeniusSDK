This is the repo for the Genius Ventures Gaming SDK to interface with the Genius Tokens system

## Download GeniusSDK project

```bash
git clone git@github.com:GeniusVentures/GeniusSDK.git
cd GeniusSDK
git checkout develop
git submodule update --init --recursive
```

## Download SuperGenius project

```bash
cd ..
git clone git@github.com:GeniusVentures/SuperGenius.git  
cd SuperGenius
git checkout develop
git submodule update --init --recursive
```
## [Build SuperGenius project](../../../SuperGenius/blob/main/Readme.md)

Ideally the folder structure should be as follows

```bash
.
├── SuperGenius                          # SuperGenius project
│   └── build                            # build directory
│        ├── Android                     # Android build directory
│        ├── Linux                       # Linux build directory
│        ├── Windows                     # Windows build directory
│        ├── OSX                         # OSX build directory
|            └── Release                 # Release build of OSX (Created when building for OSX Release)
|        └── iOS                         # iOS build directory
└── GeniusSDK   
|   ├── Readme.MD                        # readme
|   └── CMakeList.txt                    # CMake file
│   └── build                            # build directory
│        ├── Android                     # Android build directory
│        ├── Linux                       # Linux build directory
│        ├── Windows                     # Windows build directory
│        ├── OSX                         # OSX build directory
|            └── Release                 # Release build of OSX (Created when building for OSX Release)
|        └── iOS                         # iOS build directory
```

## Building

### Windows

Using Visual Studio 17 2022 to compile GeniusSDK project. Chose the CMAKE_BUILD_TYPE according to the desired configuration (Debug or Release).
    
```bash
cd build
cd Windows 
md [Debug or Release] 
cd [Debug or Release]
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=[Debug or Release] -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_PROJECT] -DSUPERGENIUS_SRC_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT]
cmake --build . --config [Debug or Release]
```
### Linux

```bash
cd build/Linux
mkdir [Debug or Release] 
cd [Debug or Release]
cmake .. -DCMAKE_BUILD_TYPE=[Debug or Release] -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_PROJECT] -DSUPERGENIUS_SRC_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT]
cmake --build . --config [Debug or Release]
```

### Android Cross-Compile on Linux/OSX Hosts

#### Preinstall
- CMake 
- Android NDK Latest LTS Version (r25b) [(link)](https://github.com/android/ndk/wiki/Unsupported-Downloads)


#### Building
```bash
export ANDROID_NDK=/path/to/android-ndk-r25b
export ANDROID_TOOLCHAIN="$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
export PATH="$ANDROID_TOOLCHAIN":"$PATH" 
```

* armeabi-v7a

```bash
cd build/Android
mkdir -p [Debug or Release]/armeabi-v7a
cd [Debug or Release]/armeabi-v7a
cmake ../../ -DANDROID_ABI="armeabi-v7a" -DCMAKE_ANDROID_NDK=$ANDROID_NDK -DANDROID_TOOLCHAIN=clang -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_PROJECT] -DSUPERGENIUS_SRC_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT]
cmake --build . --config [Debug or Release]
```

* arm64-v8a

```bash
cd build/Android
mkdir -p [Debug or Release]/arm64-v8a
cd [Debug or Release]/arm64-v8a
cmake ../../ -DANDROID_ABI="arm64-v8a" -DCMAKE_ANDROID_NDK=$ANDROID_NDK -DANDROID_TOOLCHAIN=clang -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_PROJECT] -DSUPERGENIUS_SRC_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT]
cmake --build . --config [Debug or Release]
```

* x86

```bash
cd build/Android
mkdir -p [Debug or Release]/x86
cd [Debug or Release]/x86
cmake ../../ -DANDROID_ABI="x86" -DCMAKE_ANDROID_NDK=$ANDROID_NDK -DANDROID_TOOLCHAIN=clang -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_PROJECT] -DSUPERGENIUS_SRC_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT]
cmake --build . --config [Debug or Release]
```

* x86_64
```bash
cd build/Android
mkdir -p [Debug or Release]/x86_64
cd [Debug or Release]/x86_64
cmake ../../ -DANDROID_ABI="x86_64" -DCMAKE_ANDROID_NDK=$ANDROID_NDK -DANDROID_TOOLCHAIN=clang -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_PROJECT] -DSUPERGENIUS_SRC_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT]
cmake --build . --config [Debug or Release]
```

### OSX (x86_64 & Arm64) 

```bash
cd build/OSX
mkdir [Debug or Release] 
cd [Debug or Release]
cmake .. -DCMAKE_BUILD_TYPE=[Debug or Release] -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_PROJECT] -DSUPERGENIUS_SRC_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT]
cmake --build . --config [Debug or Release]
```

#### For iOS cross compile 

```bash
cd build/iOS
mkdir [Debug or Release] 
cd [Debug or Release]
cmake .. -DCMAKE_BUILD_TYPE=[Debug or Release] -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_BUILD_RELEASE] -DCMAKE_TOOLCHAIN_FILE=../iOS.cmake -DiOS_ABI=arm64-v8a -DIOS_ARCH="arm64" -DENABLE_ARC=0 -DENABLE_BITCODE=0 -DENABLE_VISIBILITY=1  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_SYSTEM_PROCESSOR=arm64 -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_PROJECT] -DSUPERGENIUS_SRC_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT]
cmake --build . --config [Debug or Release]
```

## Installing
To install, navigate to the build folder (Debug or /Relese) and use the command:

```bash
cmake --install .
```


