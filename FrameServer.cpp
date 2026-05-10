// ============================================================================
// FrameServer.cpp — Implementation of the shared memory reader
// ============================================================================
#include "FrameServer.h"

// 24 bytes header + NV12 frame size (1920x1080)
static const DWORD TOTAL_MEM_SIZE = 24 + 1920 * 1080 * 3 / 2;

FrameServer::FrameServer()
    : _hMapFile(nullptr)
    , _hMutex(nullptr)
    , _header(nullptr)
    , _lastIndex(0)
{
}

FrameServer::~FrameServer()
{
    if (_header)
        UnmapViewOfFile(_header);
    if (_hMapFile)
        CloseHandle(_hMapFile);
    if (_hMutex)
        CloseHandle(_hMutex);
}

HRESULT FrameServer::Initialize()
{
    // Create a security descriptor with a NULL DACL to allow full access.
    // This is necessary because this DLL runs in Session 0 (LOCAL SERVICE), 
    // and the companion script runs in the interactive user session.
    SECURITY_DESCRIPTOR sd;
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
        return HRESULT_FROM_WIN32(GetLastError());

    if (!SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE))
        return HRESULT_FROM_WIN32(GetLastError());

    SECURITY_ATTRIBUTES sa;
    sa.nLength              = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle       = FALSE;

    // Create the global file mapping object
    _hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,   // Backed by the system paging file
        &sa,
        PAGE_READWRITE,
        0,
        TOTAL_MEM_SIZE,
        SHARED_MEM_NAME         // L"Global\\BestCam_SharedMem"
    );

    if (!_hMapFile)
        return HRESULT_FROM_WIN32(GetLastError());

    // Map the view into our process address space
    _header = (SharedMemHeader*)MapViewOfFile(
        _hMapFile, 
        FILE_MAP_READ | FILE_MAP_WRITE, 
        0, 
        0, 
        0
    );

    if (!_header)
        return HRESULT_FROM_WIN32(GetLastError());

    // Zero-initialize and set up default metadata so the companion script 
    // knows the dimensions we expect.
    ZeroMemory(_header, sizeof(SharedMemHeader));
    _header->width     = 1920;
    _header->height    = 1080;
    _header->stride    = 1920;
    _header->frameSize = 1920 * 1080 * 3 / 2;

    // Create the cross-process mutex (non-fatal if this fails, we can run lockless)
    _hMutex = CreateMutexW(&sa, FALSE, MUTEX_NAME);

    return S_OK;
}

HRESULT FrameServer::GetLatestFrame(BYTE** data, DWORD* length, UINT64* frameIndex)
{
    if (!_header)
        return E_FAIL;

    if (_hMutex)
        WaitForSingleObject(_hMutex, 5);

    // If the frame index hasn't changed, there is no new frame
    if (_header->frameIndex == _lastIndex)
    {
        if (_hMutex) ReleaseMutex(_hMutex);
        return E_PENDING;
    }

    // Pass back pointers directly into the shared memory segment
    *data       = _header->data;
    *length     = _header->frameSize;
    *frameIndex = _header->frameIndex;
    _lastIndex  = _header->frameIndex;

    if (_hMutex) ReleaseMutex(_hMutex);
    return S_OK;
}

UINT32 FrameServer::GetWidth()  const { return _header ? _header->width  : 0; }
UINT32 FrameServer::GetHeight() const { return _header ? _header->height : 0; }
