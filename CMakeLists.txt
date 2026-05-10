cmake_minimum_required(VERSION 3.20)
project(BestCam LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows SDK path (adjust if needed to match your SDK version)
# The MFCreateVirtualCamera API requires Windows 11 22H2 (SDK 10.0.22621.0 or later).
set(WIN_SDK_LIB "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64")
set(WIN_SDK_INC "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um")
set(WIN_SDK_SHARED "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared")

# Shared compiler definitions
add_compile_definitions(
    _WIN32_WINNT=0x0A00
    NTDDI_VERSION=0x0A00000B
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

# Common include directories
set(COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${WIN_SDK_INC}"
    "${WIN_SDK_SHARED}"
)

# 1. Build the Virtual Camera COM DLL (BestCamSource.dll)
add_library(BestCamSource SHARED
    DllMain.cpp
    MediaSource.cpp
    MediaStream.cpp
    FrameServer.cpp
    BestCamSource.def
)
target_include_directories(BestCamSource PRIVATE ${COMMON_INCLUDES})
target_link_directories(BestCamSource PRIVATE "${WIN_SDK_LIB}")
target_link_libraries(BestCamSource PRIVATE
    mf.lib
    mfplat.lib
    mfuuid.lib
    user32.lib
    ole32.lib
    advapi32.lib
)

# 2. Build the Host Executable (BestCamHost.exe)
add_executable(BestCamHost BestCamHost.cpp)
target_include_directories(BestCamHost PRIVATE ${COMMON_INCLUDES})
target_link_directories(BestCamHost PRIVATE "${WIN_SDK_LIB}")
target_link_libraries(BestCamHost PRIVATE
    mf.lib
    mfplat.lib
    mfuuid.lib
    mfsensorgroup.lib
    ole32.lib
)
