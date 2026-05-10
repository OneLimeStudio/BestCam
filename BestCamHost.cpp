// ============================================================================
// BestCamHost.exe
// Registers the virtual camera to the OS via MFCreateVirtualCamera.
// This process must remain running for the virtual camera to stay active.
// Requires Administrator privileges to run.
// ============================================================================
#include <windows.h>
#include <mfvirtualcamera.h>
#include <mfapi.h>
#include <mfidl.h>
#include <wrl/client.h>
#include <stdio.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfsensorgroup.lib")
#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

// The CLSID of the COM media source matching the one in DllMain.cpp
static const WCHAR* SOURCE_CLSID = L"{B5A2E580-4AB3-4506-B711-DC9E0F3B228C}";

int wmain()
{
    printf("[BestCam] Initializing Virtual Webcam Host...\n");

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        printf("[ERROR] CoInitialize failed: 0x%08X\n", hr);
        return 1;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        printf("[ERROR] MFStartup failed: 0x%08X\n", hr);
        return 1;
    }

    // Verify virtual camera support (Requires Windows 11 22H2+)
    BOOL supported = FALSE;
    hr = MFIsVirtualCameraTypeSupported(MFVirtualCameraType_SoftwareCameraSource, &supported);
    if (FAILED(hr) || !supported) {
        printf("[ERROR] Virtual cameras are not supported on this system.\n");
        printf("        Ensure you are running Windows 11 22H2 or later.\n");
        MFShutdown();
        CoUninitialize();
        return 1;
    }
    printf("[OK] System supports virtual cameras.\n");

    // Instantiate the virtual camera
    ComPtr<IMFVirtualCamera> virtualCamera;
    hr = MFCreateVirtualCamera(
        MFVirtualCameraType_SoftwareCameraSource,
        MFVirtualCameraLifetime_Session,        // Active only while this host is running
        MFVirtualCameraAccess_CurrentUser,
        L"BestCam Virtual Webcam",              // Device name shown in apps
        SOURCE_CLSID,
        nullptr,
        0,
        &virtualCamera
    );

    if (FAILED(hr)) {
        printf("[ERROR] MFCreateVirtualCamera failed: 0x%08X\n", hr);
        printf("        Did you run 'regsvr32 BestCamSource.dll' as Administrator?\n");
        MFShutdown();
        CoUninitialize();
        return 1;
    }
    printf("[OK] Virtual camera created successfully.\n");

    // Start publishing the camera to the system
    hr = virtualCamera->Start(nullptr);
    if (FAILED(hr)) {
        printf("[ERROR] Failed to start virtual camera: 0x%08X\n", hr);
        virtualCamera->Shutdown();
        MFShutdown();
        CoUninitialize();
        return 1;
    }

    printf("\n");
    printf("========================================================\n");
    printf("  BestCam Virtual Webcam is LIVE!\n");
    printf("========================================================\n");
    printf("  Camera should now be visible in OBS, Zoom, Teams, etc.\n");
    printf("  Run the companion script to feed frames to the driver.\n");
    printf("  Press ENTER to stop the camera and exit.\n");
    printf("========================================================\n");

    getchar();

    printf("[BestCam] Shutting down...\n");
    virtualCamera->Stop();
    virtualCamera->Shutdown();
    virtualCamera.Reset();

    MFShutdown();
    CoUninitialize();

    return 0;
}
