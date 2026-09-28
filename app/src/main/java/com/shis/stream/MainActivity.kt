package com.shis.stream

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.media.projection.MediaProjectionManager
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    private val viewModel: StreamViewModel by viewModels()

    private lateinit var statusText: TextView
    private lateinit var backendInput: EditText
    private lateinit var streamInput: EditText
    private lateinit var keyInput: EditText
    private lateinit var startButton: Button
    private lateinit var stopButton: Button

    private val micPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) { granted ->
        if (granted) beginConnection()
        else statusText.text = "Discord no está involucrado aquí, pero Android exige RECORD_AUDIO para capturar audio interno."
    }

    private val captureLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult(),
    ) { result ->
        val data = result.data
        if (result.resultCode == Activity.RESULT_OK && data != null) {
            viewModel.startScreenCapture(data)
        } else {
            statusText.text = "Captura cancelada"
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusText = findViewById(R.id.statusText)
        backendInput = findViewById(R.id.backendInput)
        streamInput = findViewById(R.id.streamInput)
        keyInput = findViewById(R.id.keyInput)
        startButton = findViewById(R.id.startButton)
        stopButton = findViewById(R.id.stopButton)

        val prefs = getSharedPreferences("shis_stream", Context.MODE_PRIVATE)
        backendInput.setText(prefs.getString("api_base", ""))
        streamInput.setText(prefs.getString("stream_name", "cris"))
        keyInput.setText(prefs.getString("stream_key", ""))

        startButton.setOnClickListener {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
                beginConnection()
            } else {
                micPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
            }
        }

        stopButton.setOnClickListener { viewModel.stopStreaming() }

        lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.uiState.collect { state ->
                    statusText.text = state.status
                    startButton.isEnabled = !state.busy && !state.streaming
                    stopButton.isEnabled = state.streaming || state.busy
                }
            }
        }
    }

    private fun beginConnection() {
        val apiBase = backendInput.text.toString().trim()
        val streamName = streamInput.text.toString().trim()
        val streamKey = keyInput.text.toString()

        getSharedPreferences("shis_stream", Context.MODE_PRIVATE)
            .edit()
            .putString("api_base", apiBase)
            .putString("stream_name", streamName)
            .putString("stream_key", streamKey)
            .apply()

        lifecycleScope.launch {
            if (!viewModel.prepareConnection(apiBase, streamName, streamKey)) return@launch

            val projectionManager = getSystemService(MediaProjectionManager::class.java)
            captureLauncher.launch(projectionManager.createScreenCaptureIntent())
        }
    }
}
