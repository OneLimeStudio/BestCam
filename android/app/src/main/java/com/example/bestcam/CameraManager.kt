package com.example.bestcam

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.ImageFormat
import android.graphics.Rect
import android.graphics.YuvImage
import android.util.Log
import android.util.Size
import androidx.camera.core.*
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class CameraManager(
    private val context: Context,
    private val lifecycleOwner: LifecycleOwner,
    private val previewView: PreviewView,
    private val server: MjpegServer
) {
    private var cameraProvider: ProcessCameraProvider? = null
    private var camera: androidx.camera.core.Camera? = null
    private var lensFacing = CameraSelector.LENS_FACING_BACK
    private var imageAnalyzer: ImageAnalysis? = null
    private var preview: Preview? = null
    
    private val cameraExecutor: ExecutorService = Executors.newFixedThreadPool(2)
    
    private var targetResolution = Size(1920, 1080)
    var quality: Int = 55
    var isBeautyFilterEnabled = false

    private var nv21Buffer: ByteArray? = null
    private val jpegOutStream = ThreadLocal.withInitial { ByteArrayOutputStream(200_000) }

    fun startCamera() {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(context)
        cameraProviderFuture.addListener({
            try {
                cameraProvider = cameraProviderFuture.get()
                bindCameraUseCases()
            } catch (e: Exception) {
                Log.e("CameraManager", "Error starting camera", e)
            }
        }, ContextCompat.getMainExecutor(context))
    }

    private fun bindCameraUseCases() {
        val cameraProvider = cameraProvider ?: return
        
        val cameraSelector = CameraSelector.Builder()
            .requireLensFacing(lensFacing)
            .build()

        preview = Preview.Builder()
            .setTargetResolution(targetResolution)
            .build()
            .also {
                it.setSurfaceProvider(previewView.surfaceProvider)
            }

        imageAnalyzer = ImageAnalysis.Builder()
            .setTargetResolution(targetResolution)
            .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
            .build()
            .also {
                it.setAnalyzer(cameraExecutor) { imageProxy ->
                    try {
                        processImage(imageProxy)
                    } catch (e: Exception) {
                        Log.e("CameraManager", "Processing error", e)
                        imageProxy.close()
                    }
                }
            }

        try {
            cameraProvider.unbindAll()
            camera = cameraProvider.bindToLifecycle(
                lifecycleOwner,
                cameraSelector,
                preview,
                imageAnalyzer
            )
        } catch (e: Exception) {
            Log.e("CameraManager", "Use case binding failed", e)
        }
    }

    @SuppressLint("UnsafeOptInUsageError")
    private fun processImage(imageProxy: ImageProxy) {
        val jpegData = imageProxy.toJpeg()
        if (jpegData != null) {
            server.sendFrame(jpegData)
        }
        imageProxy.close()
    }

    private fun ImageProxy.toJpeg(): ByteArray? {
        return try {
            val width = this.width
            val height = this.height
            
            // Allocate or reuse buffer
            val requiredSize = width * height * 3 / 2
            if (nv21Buffer == null || nv21Buffer!!.size != requiredSize) {
                nv21Buffer = ByteArray(requiredSize)
            }
            val nv21 = nv21Buffer!!

            yuv420ToNv21(this, nv21)
            
            if (isBeautyFilterEnabled) {
                applyBeautyFilter(nv21, width, height)
            }

            val yuvImage = YuvImage(nv21, ImageFormat.NV21, width, height, null)
            val out = jpegOutStream.get()!!
            out.reset()
            yuvImage.compressToJpeg(Rect(0, 0, width, height), quality, out)
            out.toByteArray()
        } catch (e: Exception) {
            Log.e("CameraManager", "JPEG conversion failed", e)
            null
        }
    }

    private fun yuv420ToNv21(image: ImageProxy, nv21: ByteArray) {
        val width = image.width
        val height = image.height
        val planes = image.planes
        val yBuffer = planes[0].buffer
        val uBuffer = planes[1].buffer
        val vBuffer = planes[2].buffer

        // Copy Y plane
        val yRowStride = planes[0].rowStride
        if (yRowStride == width) {
            yBuffer.get(nv21, 0, width * height)
        } else {
            for (row in 0 until height) {
                yBuffer.position(row * yRowStride)
                yBuffer.get(nv21, row * width, width)
            }
        }

        // Copy interleaved U/V planes (NV21 format is YYYY... VUVU...)
        val vRowStride = planes[2].rowStride
        val vPixelStride = planes[2].pixelStride
        val uRowStride = planes[1].rowStride
        val uPixelStride = planes[1].pixelStride
        
        var pos = width * height
        
        if (vPixelStride == 2 && vBuffer.remaining() == (width * height / 2 - 1)) {
            vBuffer.get(nv21, pos, vBuffer.remaining())
        } else {
            for (row in 0 until height / 2) {
                for (col in 0 until width / 2) {
                    val vIdx = row * vRowStride + col * vPixelStride
                    val uIdx = row * uRowStride + col * uPixelStride
                    nv21[pos++] = vBuffer.get(vIdx)
                    nv21[pos++] = uBuffer.get(uIdx)
                }
            }
        }
    }

    private fun applyBeautyFilter(nv21: ByteArray, width: Int, height: Int): ByteArray {
        val brightnessBoost = 25
        
        // Dynamic skip: Only skip if resolution is high (1080p+)
        // This fixes the 720p performance dip by processing more detail at lower res
        val skip = if (width >= 1920) 2 else 1
        
        for (y in 0 until height step skip) {
            val offset = y * width
            for (x in 0 until width step skip) {
                val idx = offset + x
                val yVal = nv21[idx].toInt() and 0xFF
                
                // 1. Brightness
                val boostedY = Math.min(255, yVal + brightnessBoost).toByte()
                
                // 2. Horizontal Smoothing
                nv21[idx] = boostedY
                if (x + 1 < width && skip == 2) {
                    nv21[idx + 1] = boostedY
                }
            }
        }
        
        return nv21
    }

    fun switchCamera() {
        lensFacing = if (lensFacing == CameraSelector.LENS_FACING_BACK) {
            CameraSelector.LENS_FACING_FRONT
        } else {
            CameraSelector.LENS_FACING_BACK
        }
        bindCameraUseCases()
    }

    fun toggleFlash(on: Boolean) {
        camera?.cameraControl?.enableTorch(on)
    }

    fun setZoom(ratio: Float) {
        camera?.cameraControl?.setZoomRatio(ratio)
    }

    fun setResolution(width: Int, height: Int) {
        targetResolution = Size(width, height)
        if (cameraProvider != null) {
            bindCameraUseCases()
        } else {
            startCamera()
        }
    }

    fun shutdown() {
        cameraExecutor.shutdown()
        cameraProvider?.unbindAll()
    }
}
