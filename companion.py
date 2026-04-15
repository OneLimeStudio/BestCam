import sys
import os
import subprocess
import threading
import time
import logging
import urllib.request
import numpy as np
import cv2
import pyvirtualcam
from pyvirtualcam import PixelFormat
import pystray
from PIL import Image, ImageDraw
import tkinter as tk
from tkinter import ttk

if getattr(sys, 'frozen', False):
    base_path = os.path.dirname(sys.executable)
else:
    base_path = os.path.dirname(os.path.abspath(__file__))

logging.basicConfig(
    filename=os.path.join(base_path, 'webcambridge.log'),
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s'
)

class WebcamBridge:
    def __init__(self, on_status_change=None):
        self.running = False
        self.stream_thread = None
        self.adb_path = os.path.join(base_path, "platform-tools", "adb.exe")
        self._status = "Idle"
        self.fps = 0
        self.on_status_change = on_status_change
        self.adb_restarted = False  # Only restart ADB server once per session

    @property
    def status(self):
        return self._status

    @status.setter
    def status(self, value):
        self._status = value
        logging.info(f"Status changed: {value}")
        if self.on_status_change:
            self.on_status_change(value)

    def get_adb_cmd(self):
        """Returns the adb command to use based on what's available."""
        # 1. Bundled inside app folder
        if os.path.exists(self.adb_path):
            logging.info(f"Using bundled ADB: {self.adb_path}")
            return [self.adb_path]

        # 2. Android Studio SDK path
        android_studio_path = os.path.join(
            os.environ.get("LOCALAPPDATA", ""),
            "Android", "Sdk", "platform-tools", "adb.exe"
        )
        if os.path.exists(android_studio_path):
            logging.info(f"Using Android Studio ADB: {android_studio_path}")
            return [android_studio_path]

        # 3. System PATH
        try:
            CREATE_NO_WINDOW = 0x08000000 if os.name == 'nt' else 0
            result = subprocess.run(
                ["where", "adb"],
                capture_output=True, text=True,
                creationflags=CREATE_NO_WINDOW
            )
            if result.returncode == 0:
                path = result.stdout.strip().splitlines()[0]
                logging.info(f"Using system ADB: {path}")
                return [path]
        except Exception:
            pass

        # 4. Fallback — hope it's in PATH
        logging.warning("ADB not found in known locations, falling back to 'adb'")
        return ["adb"]

    def get_device_state(self):
        """Check connected device authorization state. Returns 'device', 'unauthorized', 'offline', or None."""
        try:
            adb_cmd = self.get_adb_cmd()
            CREATE_NO_WINDOW = 0x08000000 if os.name == 'nt' else 0
            result = subprocess.run(
                adb_cmd + ["devices"],
                capture_output=True, text=True,
                timeout=5,
                creationflags=CREATE_NO_WINDOW
            )
            for line in result.stdout.strip().splitlines()[1:]:  # Skip header
                parts = line.split()
                if len(parts) >= 2:
                    return parts[1]  # 'device', 'unauthorized', 'offline', etc.
            return None  # No device connected
        except Exception as e:
            logging.error(f"Failed to check device state: {e}")
            return None

    def restart_adb_server(self):
        """Kill and restart ADB server — forces the USB debugging prompt to reappear on the phone."""
        try:
            adb_cmd = self.get_adb_cmd()
            CREATE_NO_WINDOW = 0x08000000 if os.name == 'nt' else 0

            logging.info("Killing ADB server...")
            self.status = "Killing ADB server..."

            subprocess.run(
                adb_cmd + ["kill-server"],
                capture_output=True, text=True,
                timeout=10,
                creationflags=CREATE_NO_WINDOW
            )
            time.sleep(2)  # Let the server fully die & USB stack reset

            logging.info("Starting ADB server...")
            self.status = "Starting ADB server..."

            subprocess.run(
                adb_cmd + ["start-server"],
                capture_output=True, text=True,
                timeout=15,
                creationflags=CREATE_NO_WINDOW
            )
            time.sleep(2)  # Let server boot and probe USB devices

            # Now wait for the user to accept the USB debugging prompt on the phone
            self.status = "Waiting for USB debug authorization on phone..."
            logging.info("Waiting for USB debug authorization (up to 30s)...")

            for i in range(30):  # Wait up to 30 seconds
                state = self.get_device_state()
                if state == "device":
                    logging.info("Device authorized successfully!")
                    self.status = "Device authorized"
                    return
                elif state == "unauthorized":
                    # Prompt is showing on phone — user needs to tap Allow
                    self.status = "Tap 'Allow USB debugging' on your phone!"
                    logging.info(f"Device unauthorized — waiting for user to accept (attempt {i+1}/30)")
                elif state == "offline":
                    self.status = "Device offline, waiting..."
                    logging.info(f"Device offline — waiting (attempt {i+1}/30)")
                else:
                    self.status = "Plug in your phone via USB..."
                    logging.info(f"No device detected — waiting (attempt {i+1}/30)")
                time.sleep(1)

            # Timed out
            final_state = self.get_device_state()
            if final_state == "device":
                logging.info("Device authorized (just in time).")
            else:
                logging.warning(f"Timed out waiting for authorization. Device state: {final_state}")
                self.status = f"USB auth timeout (state: {final_state})"

        except Exception as e:
            logging.error(f"Failed to restart ADB server: {e}")

    def start_adb_forward(self):
        try:
            adb_cmd = self.get_adb_cmd()
            CREATE_NO_WINDOW = 0x08000000 if os.name == 'nt' else 0

            # Restart ADB server once per session to pick up fresh USB drivers
            if not self.adb_restarted:
                self.restart_adb_server()
                self.adb_restarted = True

            result = subprocess.run(
                adb_cmd + ["forward", "tcp:4747", "tcp:4747"],
                capture_output=True,
                text=True,
                timeout=10,
                creationflags=CREATE_NO_WINDOW
            )
            if result.returncode != 0:
                logging.error(f"ADB forward failed: {result.stderr}")
                return False

            logging.info("ADB forward tcp:4747 success.")
            return True
        except Exception as e:
            logging.error(f"Exception running ADB: {e}")
            return False

    def _read_mjpeg_stream(self, url):
        """
        Manually reads an MJPEG HTTP stream and yields decoded BGR frames.
        Bypasses FFMPEG entirely — fixes Samsung YUV scanline/green-tint encoding bugs.
        Finds raw JPEG frames by scanning for SOI (FFD8) and EOI (FFD9) markers.
        """
        req = urllib.request.urlopen(url, timeout=10)
        buf = b''
        SOI = b'\xff\xd8'  # JPEG Start Of Image
        EOI = b'\xff\xd9'  # JPEG End Of Image

        while self.running:
            chunk = req.read(65536)
            if not chunk:
                break
            buf += chunk

            # Extract all complete JPEG frames from the buffer
            while True:
                start = buf.find(SOI)
                if start == -1:
                    buf = b''   # No SOI yet, discard junk
                    break
                end = buf.find(EOI, start + 2)
                if end == -1:
                    buf = buf[start:]  # Keep from SOI, wait for more data
                    break

                jpeg_bytes = buf[start:end + 2]
                buf = buf[end + 2:]  # Advance past this frame

                arr = np.frombuffer(jpeg_bytes, dtype=np.uint8)
                frame = cv2.imdecode(arr, cv2.IMREAD_COLOR)
                if frame is not None:
                    yield frame

    def stream_loop(self):
        while self.running:
            self.status = "Connecting to ADB..."
            self.start_adb_forward()

            cam = None
            try:
                self.status = "Connecting to stream..."

                # Get the first frame to determine dimensions
                gen = self._read_mjpeg_stream("http://localhost:4747/video")
                first_frame = next(gen, None)

                if first_frame is None:
                    self.status = "Waiting for camera data..."
                    time.sleep(2)
                    continue

                height, width = first_frame.shape[:2]
                target_fps = 30  # DroidCam/WebcamBridge default

                cam = pyvirtualcam.Camera(
                    width=width,
                    height=height,
                    fps=target_fps,
                    fmt=PixelFormat.RGBA,
                    backend='unitycapture'
                )

                self.status = "Streaming"
                logging.info(f"Streaming started: {width}x{height} @ {target_fps}fps (manual MJPEG decoder)")

                frames = 0
                start_time = time.time()

                # Send first frame
                cam.send(cv2.cvtColor(first_frame, cv2.COLOR_BGR2RGBA))
                cam.sleep_until_next_frame()

                for frame in gen:
                    if not self.running:
                        break
                    cam.send(cv2.cvtColor(frame, cv2.COLOR_BGR2RGBA))
                    cam.sleep_until_next_frame()

                    frames += 1
                    t = time.time()
                    if t - start_time >= 1.0:
                        self.fps = frames
                        frames = 0
                        start_time = t

                logging.warning("Stream ended or disconnected.")

            except Exception as e:
                import traceback
                logging.error(f"Streaming error:\n{traceback.format_exc()}")
                self.status = "Error during streaming"
            finally:
                if cam:
                    cam.close()
                self.fps = 0

            if self.running:
                self.adb_restarted = False
                self.status = "Waiting for phone"
                time.sleep(2)

    def start(self):
        if not self.running:
            self.running = True
            self.adb_restarted = False  # Allow ADB restart on manual restart too
            self.stream_thread = threading.Thread(target=self.stream_loop, daemon=True)
            self.stream_thread.start()
            logging.info("WebcamBridge started.")

    def stop(self):
        if self.running:
            self.running = False
            self.status = "Idle"
            if self.stream_thread:
                self.stream_thread.join(timeout=3)
            logging.info("WebcamBridge stopped.")

    def manual_restart_adb(self):
        """Manually restart ADB server from UI — runs in a background thread."""
        def _do_restart():
            self.restart_adb_server()
            # After restart, also reset the flag so the stream loop won't skip it
            self.adb_restarted = True
        threading.Thread(target=_do_restart, daemon=True).start()


def create_icon_image():
    image = Image.new('RGBA', (64, 64), (0, 0, 0, 0))
    dc = ImageDraw.Draw(image)
    dc.ellipse((4, 4, 60, 60), fill=(0, 255, 0))
    return image


if __name__ == '__main__':
    root = tk.Tk()
    root.title("WebcamBridge")
    root.geometry("260x220")
    root.resizable(False, False)
    root.attributes('-toolwindow', True)
    root.withdraw()

    tray_icon = None

    def update_tray_status(new_status):
        if tray_icon is not None:
            tray_icon.title = f"WebcamBridge: {new_status}"
            tray_icon.update_menu()

    bridge = WebcamBridge(on_status_change=update_tray_status)

    def show_window(icon=None, item=None):
        root.after(0, root.deiconify)

    def hide_window():
        root.withdraw()

    def on_quit(icon=None, item=None):
        bridge.stop()
        if tray_icon:
            tray_icon.stop()
        root.after(0, root.destroy)

    root.protocol("WM_DELETE_WINDOW", hide_window)

    menu = pystray.Menu(
        pystray.MenuItem('Show Window', show_window, default=True),
        pystray.MenuItem('Start', lambda: bridge.start(), enabled=lambda item: not bridge.running),
        pystray.MenuItem('Stop', lambda: bridge.stop(), enabled=lambda item: bridge.running),
        pystray.MenuItem('Restart ADB', lambda: bridge.manual_restart_adb()),
        pystray.MenuItem(lambda text: f"Status: {bridge.status}", None, enabled=False),
        pystray.Menu.SEPARATOR,
        pystray.MenuItem('Quit', on_quit)
    )

    tray_icon = pystray.Icon(
        name="WebcamBridge",
        icon=create_icon_image(),
        title="WebcamBridge: Starting...",
        menu=menu
    )

    ttk.Label(root, text="WebcamBridge", font=("Helvetica", 12, "bold")).pack(pady=10)

    lbl_status = ttk.Label(root, text="Status: Idle")
    lbl_status.pack(pady=2)

    lbl_fps = ttk.Label(root, text="FPS: 0")
    lbl_fps.pack(pady=2)

    btn_frame = ttk.Frame(root)
    btn_frame.pack(pady=5)

    btn_start = ttk.Button(btn_frame, text="Start", command=bridge.start)
    btn_start.pack(side=tk.LEFT, padx=5)

    btn_stop = ttk.Button(btn_frame, text="Stop", command=bridge.stop)
    btn_stop.pack(side=tk.LEFT, padx=5)

    btn_adb = ttk.Button(btn_frame, text="Restart ADB", command=bridge.manual_restart_adb)
    btn_adb.pack(side=tk.LEFT, padx=5)

    btn_hide = ttk.Button(root, text="Minimize to tray", command=hide_window)
    btn_hide.pack(pady=10)

    def ui_poll():
        lbl_status.config(text=f"Status: {bridge.status}")
        lbl_fps.config(text=f"FPS: {bridge.fps}")
        if tray_icon:
            tray_icon.update_menu()
        root.after(1000, ui_poll)

    ui_poll()
    bridge.start()

    threading.Thread(target=tray_icon.run, daemon=True).start()

    root.mainloop()

    sys.exit(0)