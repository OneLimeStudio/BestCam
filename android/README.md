# BestCam

BestCam is a high-performance Android application that transforms your smartphone into a high-fidelity, low-latency webcam for PCs and servers. It streams a smooth 30 FPS MJPEG video over HTTP at up to 1080p resolution.

## Features

- **High-Performance Streaming**: Rock-solid 30 FPS at 1080p using optimized memory management and asynchronous frame delivery.
- **Dynamic Resolution**: Easily switch between 720p and 1080p directly from the app.
- **Real-Time Filters**: Built-in beauty filter that brightens the image and smooths skin tones without dropping frames.
- **Zero-Config Setup**: Built-in MJPEG server compatible with web browsers, VLC, OBS, OpenCV, and more.
- **Developer Friendly**: Includes a guide to enable USB Debugging for the lowest possible latency.

## Getting Started

### Prerequisites

- Android 8.0 (API level 26) or higher.
- A physical Android device (emulators may not support all camera features).

### Installation

1. Clone the repository.
2. Open the project in Android Studio.
3. Build and run the app on your device.

### Usage

1. Open the app and grant camera permissions.
2. Tap the **START** button to begin streaming.
3. Type the displayed URL (e.g., `http://192.168.1.5:8080`) into your PC's browser or video software.

## Technical Details

- **Protocol**: MJPEG over HTTP (`multipart/x-mixed-replace`).
- **Camera API**: Jetpack CameraX.
- **Optimizations**: Zero-allocation buffer pooling, bulk memory copies, and decoupled worker-thread architecture.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
