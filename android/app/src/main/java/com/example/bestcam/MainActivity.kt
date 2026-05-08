package com.example.bestcam

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Color
import android.net.wifi.WifiManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.text.format.Formatter
import android.view.WindowManager
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.example.bestcam.databinding.ActivityMainBinding
import com.google.android.material.dialog.MaterialAlertDialogBuilder

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private var server: MjpegServer? = null
    private var cameraManager: CameraManager? = null
    private var isStreaming = false
    private var is1080p = false // Set default to 720p
    private var isBeautyOn = false

    private val handler = Handler(Looper.getMainLooper())
    private val updateStatsTask = object : Runnable {
        override fun run() {
            updateStats()
            handler.postDelayed(this, 1000)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        if (allPermissionsGranted()) {
            startApp()
            checkUsbDebugging()
        } else {
            ActivityCompat.requestPermissions(
                this, REQUIRED_PERMISSIONS, REQUEST_CODE_PERMISSIONS
            )
        }

        setupUI()
    }

    private fun setupUI() {
        binding.btnStartStop.setOnClickListener {
            toggleServer()
        }

        binding.btnSwitchCamera.setOnClickListener {
            cameraManager?.switchCamera()
        }

        binding.btnFilter.setOnClickListener {
            isBeautyOn = !isBeautyOn
            cameraManager?.isBeautyFilterEnabled = isBeautyOn
            binding.btnFilter.setBackgroundColor(if (isBeautyOn) Color.MAGENTA else Color.TRANSPARENT)
            val status = if (isBeautyOn) "Enabled" else "Disabled"
            Toast.makeText(this, "Beauty Filter: $status", Toast.LENGTH_SHORT).show()
        }

        binding.btnSettings.setOnClickListener {
            is1080p = !is1080p
            val width = if (is1080p) 1920 else 1280
            val height = if (is1080p) 1080 else 720
            cameraManager?.setResolution(width, height)
            
            val resText = if (is1080p) "1080p" else "720p"
            binding.btnSettings.text = resText
            Toast.makeText(this, "Resolution: $resText", Toast.LENGTH_SHORT).show()
        }

        binding.zoomSlider.addOnChangeListener { _, value, _ ->
            cameraManager?.setZoom(value)
            binding.tvZoom.text = "Zoom: ${String.format("%.1f", value)}x"
        }

        val ip = getLocalIpAddress()
        binding.tvConnectionInfo.text = "http://$ip:8080"
    }

    private fun startApp() {
        server = MjpegServer(8080)
        server?.start()
        cameraManager = CameraManager(this, this, binding.previewView, server!!)
        
        // Apply current resolution
        val width = if (is1080p) 1920 else 1280
        val height = if (is1080p) 1080 else 720
        cameraManager?.setResolution(width, height) // This also calls startCamera via bindCameraUseCases
        
        handler.post(updateStatsTask)
        
        isStreaming = true
        updateStreamingUI()
    }

    private fun toggleServer() {
        if (isStreaming) {
            server?.stopServer()
            isStreaming = false
        } else {
            if (server == null) {
                server = MjpegServer(8080)
                cameraManager = CameraManager(this, this, binding.previewView, server!!)
                val width = if (is1080p) 1920 else 1280
                val height = if (is1080p) 1080 else 720
                cameraManager?.setResolution(width, height)
            }
            server?.start()
            isStreaming = true
        }
        updateStreamingUI()
    }

    private fun updateStreamingUI() {
        if (isStreaming) {
            binding.btnStartStop.text = "STOP"
            binding.btnStartStop.setBackgroundColor(Color.RED)
            binding.tvStatus.text = "Status: Streaming"
        } else {
            binding.btnStartStop.text = "START"
            binding.btnStartStop.setBackgroundColor(Color.parseColor("#4CAF50"))
            binding.tvStatus.text = "Status: Stopped"
        }
    }

    private fun updateStats() {
        server?.let {
            val clients = it.getClientCount()
            val frames = it.getFrameCount()
            binding.tvStats.text = "Frames: $frames | Clients: $clients"
        }
    }

    private fun checkUsbDebugging() {
        val adbEnabled = Settings.Global.getInt(contentResolver, Settings.Global.ADB_ENABLED, 0) > 0
        if (!adbEnabled) {
            showUsbDebuggingGuide()
        }
    }

    private fun showUsbDebuggingGuide() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Enable USB Debugging")
            .setMessage("To use this app as a high-quality webcam, USB debugging is recommended for low latency.\n\n" +
                    "Steps:\n" +
                    "1. Go to Settings > About Phone\n" +
                    "2. Tap 'Build Number' 7 times\n" +
                    "3. Go to System > Developer Options\n" +
                    "4. Enable 'USB Debugging'")
            .setPositiveButton("Got it", null)
            .show()
    }

    private fun getLocalIpAddress(): String {
        return try {
            val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
            Formatter.formatIpAddress(wifiManager.connectionInfo.ipAddress)
        } catch (e: Exception) {
            "0.0.0.0"
        }
    }

    private fun allPermissionsGranted() = REQUIRED_PERMISSIONS.all {
        ContextCompat.checkSelfPermission(baseContext, it) == PackageManager.PERMISSION_GRANTED
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_CODE_PERMISSIONS) {
            if (allPermissionsGranted()) {
                startApp()
            } else {
                Toast.makeText(this, "Permissions not granted by the user.", Toast.LENGTH_SHORT).show()
                finish()
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        handler.removeCallbacks(updateStatsTask)
        cameraManager?.shutdown()
        server?.stopServer()
    }

    companion object {
        private const val REQUEST_CODE_PERMISSIONS = 10
        private val REQUIRED_PERMISSIONS = arrayOf(Manifest.permission.CAMERA)
    }
}
