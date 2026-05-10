// ============================================================================
// FrameServer.h — Cross-session shared memory reader
// Defines the protocol for reading NV12 frames sent from the companion script.
// ============================================================================
#pragma once
#include <windows.h>

// We use the Global\ namespace so the shared memory is accessible across 
// session boundaries (from Session 0 where Frame Server runs, to the user session).
#define SHARED_MEM_NAME L"Global\\BestCam_SharedMem"
#define MUTEX_NAME      L"Global\\BestCam_Mutex"

// Data layout of the shared memory mapping
#pragma pack(push, 1)
struct SharedMemHeader
{
    UINT32  width;
    UINT32  height;
    UINT32  stride;
    UINT32  frameSize;
    UINT64  frameIndex; // Monotonically increasing index for each new frame
    UINT8   data[1];    // Start of the NV12 frame buffer
};
#pragma pack(pop)

class FrameServer
{
public:
    FrameServer();
    ~FrameServer();

    HRESULT Initialize();
    HRESULT GetLatestFrame(BYTE** data, DWORD* length, UINT64* frameIndex);

    UINT32 GetWidth() const;
    UINT32 GetHeight() const;

private:
    HANDLE _hMapFile;
    HANDLE _hMutex;
    SharedMemHeader* _header;
    UINT64 _lastIndex;
};
