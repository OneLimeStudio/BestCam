# BestCam — Turn Your Android Phone Into a Pro Webcam

BestCam transforms your Android smartphone into a high-performance, low-latency webcam for Windows. It shows up natively in Zoom, Discord, OBS, Microsoft Teams, and the Windows Camera app — exactly like a physical USB webcam.

> **Note:** Source code for both the Android app and Windows components will be published here soon.

---

## How It Works

BestCam has two parts that work together:

**Android App** — Captures video using CameraX and streams it as MJPEG over HTTP on port 8080. Currently connects over USB via ADB (Android Debug Bridge).

**Windows App** — Receives the stream, decodes each frame, and feeds it into a virtual camera driver that Windows recognizes as a real webcam. Three components handle this under the hood:

- `BestCam.exe` — The main app. Manages ADB, decodes the video stream, and writes frames to shared memory.
- `BestCamHost.exe` — Registers a virtual camera device with Windows using the Media Foundation Virtual Camera API.
- `BestCamSource.dll` — The COM driver loaded by Windows whenever an app requests a video frame.

Frames travel from your phone to the Windows camera pipeline entirely through RAM — no intermediate files, no network roundtrips on the PC side. This is what keeps latency low.

---

## Features

- **30+ FPS ** 720p 
- **25+ FPS ** 1080p
- **USB via ADB** — ~20–100ms end-to-end latency over a physical cable
- **Native Windows integration** — appears as a real camera in every app, no OBS or virtual cable needed
- **Beauty filter** — one-tap luma boost and skin smoothing for dim environments
- **720p / 1080p switching** — toggle resolution on the fly from the Android app
- **Lightweight** — Android app is ~5MB; Windows app requires no kernel driver installation
- **Privacy first** — local-only streaming, no data leaves your network

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

## Requirements

**Android**
- Android 8.0 (API 26) or higher

**Windows**
- Windows 11 22H2 (build 22621) or higher
- The MFVirtualCamera API used by BestCam does not exist on Windows 10

---

## Performance

| Metric | USB (ADB) |
|---|---|
| End-to-end latency | ~20–100ms |
| Target frame rate | 30 FPS |
| PC CPU usage (1080p) | ~15–25% total |
| Shared memory per frame | 3.1 MB (RAM only) |

---

## Getting Started

## Getting Started

1.  **Download** the latest release from the [Releases](../../releases) page.
2.  **Install** the BestCam APK on your Android device.
3.  **Run** `BestCam.exe` on your Windows PC.
4.  **Connect** your phone via USB (ensure **USB Debugging** is enabled in your phone's Developer Options).
5.  **Select Camera:** In your video conferencing app of choice (Zoom, Teams, Discord, etc.), select **"BestCam Virtual Webcam"** as your video source.

---

## Roadmap

- [ ] Publish Android source code
- [ ] Publish Windows source code (Python + C++)
- [ ] **Wi-Fi streaming** — move away from ADB dependency to direct Wi-Fi connection with auto-discovery
- [ ] Audio streaming support
- [ ] 60 FPS mode

---

## License

MIT
