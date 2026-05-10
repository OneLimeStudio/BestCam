# BestCam — Turn Your Android Phone Into a Pro Webcam

BestCam transforms your Android smartphone into a high-performance, low-latency webcam for Windows. It shows up natively in Zoom, Discord, OBS, Microsoft Teams, and the Windows Camera app — exactly like a physical USB webcam.

Under the hood, this repository is also **the open-source implementation of Microsoft's [MFCreateVirtualCamera](https://learn.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/nf-mfvirtualcamera-mfcreatevirtualcamera) API.**

![Windows 11](https://img.shields.io/badge/Windows%2011-22H2%2B-0078D4?logo=windows11)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![License](https://img.shields.io/badge/License-GPL%20v2-blue)
![Media Foundation](https://img.shields.io/badge/API-MF%20Virtual%20Camera-blueviolet)

> **Note:** The driver and Windows Host source code are fully open-sourced here. The complete BestCam Android app and GUI will be published soon.

---

## Features

- **30+ FPS** 720p 
- **25+ FPS** 1080p
- **USB via ADB** — ~20–100ms end-to-end latency over a physical cable
- **Native Windows integration** — appears as a real camera in every app, no OBS or virtual cable needed
- **Modern API** — Uses Windows 11 Media Foundation, bypassing legacy DirectShow and unsigned kernel driver issues.
- **Privacy first** — local-only streaming, no data leaves your network.

---

## How It Works & Architecture

BestCam has two parts that work together:

1. **Android App** — Captures video using CameraX and streams it as MJPEG over HTTP on port 8080. Connects over USB via ADB (Android Debug Bridge).
2. **Windows App** — Receives the stream, decodes each frame (using TurboJPEG for speed), and feeds it into a virtual camera driver via Shared Memory.

### The Virtual Camera Driver

Every existing open-source virtual camera (OBS, Softcam, VCamdroid) uses **DirectShow** — a legacy API Microsoft deprecated years ago. The modern replacement is the **Media Foundation Virtual Camera API**, introduced in Windows 11 22H2.

BestCam provides a complete, working reference implementation of this API you can learn from, fork, or build on.

```text
┌─────────────────────────────────────────────────────────┐
│  Android Phone                                          │
│  Camera → MJPEG stream over HTTP :8080                  │
└──────────────┬──────────────────────────────────────────┘
               │ USB (ADB forward) or Wi-Fi
               ▼
┌─────────────────────────────────────────────────────────┐
│  companion.py  (User Session)                           │
│  TCP recv → JPEG decode → BGR→NV12 → shared memory     │
│                                                         │
│  Writes to: Global\BestCam_SharedMem                    │
│  Syncs via: Global\BestCam_Mutex                        │
└──────────────┬──────────────────────────────────────────┘
               │ Cross-session shared memory
               │ (Global\ namespace, NULL DACL)
               ▼
┌─────────────────────────────────────────────────────────┐
│  BestCamSource.dll  (Session 0, loaded by mfpmp.exe)    │
│                                                         │
│  IMFMediaSource → reads NV12 from shared memory         │
│  Delivers IMFSample via MEMediaSample events            │
└──────────────┬──────────────────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────────────────┐
│  BestCamHost.exe                                        │
│  MFCreateVirtualCamera() → Start() → getchar() → Stop()│
│  Keeps the virtual camera alive for the session         │
└──────────────┬──────────────────────────────────────────┘
               │
               ▼
        Zoom / Teams / OBS / Discord / Camera app
```

### Shared Memory Layout

```text
Offset  Size    Type      Field
──────  ────    ────      ─────
0       4       uint32    width      (1920)
4       4       uint32    height     (1080)
8       4       uint32    stride     (1920)
12      4       uint32    frameSize  (3,110,400 = 1920×1080×1.5)
16      8       uint64    frameIndex (monotonic counter)
24      ~3.1MB  uint8[]   NV12 data  (Y plane + interleaved UV)
```

---

## Technical Highlights

| Layer | Technology |
|---|---|
| Android video capture | CameraX, YUV_420_888 |
| Streaming protocol | MJPEG over HTTP (multipart/x-mixed-replace) |
| PC-side JPEG decode | libjpeg-turbo (TurboJPEG) — 2–3x faster than OpenCV |
| Frame format to Windows | NV12 (native Windows camera pipeline format) |
| Python to C++ bridge | Named shared memory (`Global\BestCam_SharedMem`) + Win32 mutex |
| Virtual camera API | Windows Media Foundation MFCreateVirtualCamera (Windows 11 22H2+) |
| USB transport | Android Debug Bridge (ADB) port forwarding |

---

## Building the Driver

### Prerequisites

- Windows 11 SDK (10.0.22621.0 or later)
- Visual Studio 2022 with C++ Desktop workload
- CMake 3.20+

### Build the DLL and Host

```powershell
# Configure
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release
```

This produces:
- `BestCamSource.dll` — the COM media source (loaded by Windows)
- `BestCamHost.exe` — registers and starts the virtual camera

### Register the DLL

```powershell
# Run as Administrator
regsvr32 build\Release\BestCamSource.dll
```

### Run

```powershell
# Run as Administrator (MFCreateVirtualCamera requires elevation)
build\Release\BestCamHost.exe
```

The camera appears system-wide. Run `reference_companion.py` (or your own frame producer) to push frames.

---

## Reference Companion Script

[`reference_companion.py`](reference_companion.py) is a minimal Python script that demonstrates the shared memory protocol. It does not require an Android phone. Instead, it generates a moving test pattern and pushes it into the driver.

```powershell
pip install numpy opencv-python
python reference_companion.py
```

This is intentionally simple — ~80 lines, no optimizations, no GUI. See the [Shared Memory Layout](#shared-memory-layout) section to build your own producer in any language.

---

## Key Implementation Details

**Why HKLM, not HKCR?** — `DllRegisterServer` writes to `HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID` directly. Writing to `HKEY_CLASSES_ROOT` under UAC silently redirects to `HKCU`, which is invisible to `SYSTEM` and `LOCAL SERVICE` — causing `0x80070005` at runtime.

**Why Global\\ prefix?** — The DLL runs inside `mfpmp.exe` (Session 0, LOCAL SERVICE). The companion script runs in the user session (Session 1+). Windows isolates kernel object namespaces per session. `Global\` puts shared memory and mutexes in the cross-session namespace.

**Why NULL DACL?** — The shared memory needs to be accessible from both Session 0 (LOCAL SERVICE) and the interactive user. A NULL DACL grants all access — acceptable for a local-only IPC channel.

**Why NV12?** — It's the native format of the Windows camera pipeline and hardware video encoders. Delivering NV12 directly avoids any in-driver color conversion.

---

## Requirements

**Android**
- Android 8.0 (API 26) or higher

**Windows**
- **Windows 11 22H2** (build 22621) or later — the `MFCreateVirtualCamera` API does not exist on Windows 10
- **Administrator privileges** — `MFCreateVirtualCamera()` requires an elevated token
- **Visual C++ Redistributable 2022** — for VCRUNTIME140.dll on end-user machines

---

## FAQ

**Q: Does this work on Windows 10?**
No. The MF Virtual Camera API (`MFCreateVirtualCamera`) was introduced in Windows 11 22H2. There is no polyfill or workaround. For Windows 10 support, you need a DirectShow filter.

**Q: Do I need to sign the driver?**
No. This is a user-mode COM DLL, not a kernel driver. No WHQL, no driver signing, no Windows Update.

**Q: Can I use this without an Android phone?**
Yes. The driver reads NV12 frames from shared memory — it doesn't care where they come from. Write your own producer (screen capture, AI-generated video, test pattern, etc.) using the [shared memory protocol](#shared-memory-layout).

**Q: Why not DirectShow?**
DirectShow is deprecated. Microsoft's modern camera pipeline uses Media Foundation. MF Virtual Camera devices are first-class citizens in Windows 11 — they appear in Device Manager, respect camera privacy settings, and work with Frame Server sharing (multiple apps can use the camera simultaneously).

---

## License

This project is licensed under the **GNU General Public License v2.0** — see [LICENSE](LICENSE) for details.
