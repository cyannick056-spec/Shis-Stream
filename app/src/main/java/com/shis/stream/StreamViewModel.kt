package com.shis.stream

import android.Manifest
import android.app.Application
import android.content.Intent
import android.content.pm.PackageManager
import androidx.core.app.ActivityCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import io.livekit.android.LiveKit
import io.livekit.android.audio.ScreenAudioCapturer
import io.livekit.android.room.track.LocalAudioTrack
import io.livekit.android.room.track.LocalVideoTrack
import io.livekit.android.room.track.Track
import io.livekit.android.room.track.screencapture.ScreenCaptureParams
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import livekit.org.webrtc.audio.JavaAudioDeviceModule

data class StreamUiState(
    val status: String = "Listo",
    val busy: Boolean = false,
    val streaming: Boolean = false,
)

class StreamViewModel(application: Application) : AndroidViewModel(application) {
    private val room = LiveKit.create(application)
    private var screenAudioCapturer: ScreenAudioCapturer? = null
    private var internalAudioEnabled = false

    private val _uiState = MutableStateFlow(StreamUiState())
    val uiState: StateFlow<StreamUiState> = _uiState.asStateFlow()

    suspend fun prepareConnection(apiBase: String, streamName: String, streamKey: String): Boolean {
        if (apiBase.isBlank() || streamName.isBlank() || streamKey.isBlank()) {
            _uiState.value = StreamUiState(status = "Falta servidor, nombre o clave")
            return false
        }

        _uiState.value = StreamUiState(status = "Conectando…", busy = true)
        return try {
            val credentials = withContext(Dispatchers.IO) {
                TokenApi.fetchPublisherCredentials(apiBase, streamName, streamKey)
            }
            withContext(Dispatchers.IO) {
                runCatching { room.disconnect() }
                room.connect(credentials.serverUrl, credentials.token)
            }
            _uiState.value = StreamUiState(
                status = "Conectado a ${credentials.roomName}. Autoriza la captura de pantalla.",
                busy = false,
            )
            true
        } catch (t: Throwable) {
            _uiState.value = StreamUiState(status = "No se pudo conectar: ${t.message ?: t.javaClass.simpleName}")
            false
        }
    }

    fun startScreenCapture(permissionData: Intent, captureInternalAudio: Boolean) {
        val app = getApplication<Application>()
        if (
            captureInternalAudio &&
            ActivityCompat.checkSelfPermission(app, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED
        ) {
            _uiState.value = StreamUiState(status = "Falta permiso de audio")
            return
        }

        val serviceIntent = Intent(app, StreamForegroundService::class.java)
            .putExtra(StreamForegroundService.EXTRA_INTERNAL_AUDIO, captureInternalAudio)
        app.startForegroundService(serviceIntent)

        _uiState.value = StreamUiState(
            status = if (captureInternalAudio) "Iniciando video + audio experimental…" else "Iniciando video seguro…",
            busy = true,
        )

        viewModelScope.launch(Dispatchers.IO) {
            try {
                internalAudioEnabled = false
                room.localParticipant.setScreenShareEnabled(
                    true,
                    ScreenCaptureParams(permissionData),
                )

                if (!captureInternalAudio) {
                    _uiState.value = StreamUiState(
                        status = "● TRANSMITIENDO — SOLO VIDEO (modo seguro)",
                        streaming = true,
                    )
                    return@launch
                }

                // Give Switchroot time to stabilize MediaProjection before touching audio.
                delay(800)

                // Experimental path. This is intentionally isolated so Solo video never opens AudioRecord.
                (room.lkObjects.audioDeviceModule as? JavaAudioDeviceModule)
                    ?.setAudioRecordEnabled(false)

                room.localParticipant.setMicrophoneEnabled(true)

                val screenTrack = room.localParticipant
                    .getTrackPublication(Track.Source.SCREEN_SHARE)
                    ?.track as? LocalVideoTrack
                    ?: error("No se creó la pista de pantalla")

                val audioTrack = room.localParticipant
                    .getTrackPublication(Track.Source.MICROPHONE)
                    ?.track as? LocalAudioTrack
                    ?: error("No se creó la pista de audio")

                screenAudioCapturer = ScreenAudioCapturer.createFromScreenShareTrack(screenTrack)
                    ?: error("Android no entregó MediaProjection a la captura de audio")

                screenAudioCapturer?.gain = 1.0f
                audioTrack.setAudioBufferCallback(screenAudioCapturer!!)
                internalAudioEnabled = true

                _uiState.value = StreamUiState(
                    status = "● TRANSMITIENDO — VIDEO + AUDIO INTERNO (experimental)",
                    streaming = true,
                )
            } catch (t: Throwable) {
                if (captureInternalAudio) {
                    // If the audio setup fails with a recoverable Java/Kotlin error, keep video alive.
                    runCatching { disableInternalAudioOnly() }
                    _uiState.value = StreamUiState(
                        status = "● VIDEO ACTIVO — audio interno falló: ${t.message ?: t.javaClass.simpleName}",
                        streaming = true,
                    )
                } else {
                    try {
                        stopInternal()
                    } catch (_: Throwable) {
                        // Keep the original startup error for the UI.
                    }
                    _uiState.value = StreamUiState(status = "Error al iniciar video: ${t.message ?: t.javaClass.simpleName}")
                }
            }
        }
    }

    fun stopStreaming() {
        _uiState.value = _uiState.value.copy(status = "Deteniendo…", busy = true)
        viewModelScope.launch(Dispatchers.IO) {
            stopInternal()
            _uiState.value = StreamUiState(status = "Stream detenido")
        }
    }

    private suspend fun disableInternalAudioOnly() {
        runCatching {
            (room.localParticipant.getTrackPublication(Track.Source.MICROPHONE)?.track as? LocalAudioTrack)
                ?.setAudioBufferCallback(null)
        }
        runCatching { room.localParticipant.setMicrophoneEnabled(false) }
        screenAudioCapturer?.releaseAudioResources()
        screenAudioCapturer = null
        internalAudioEnabled = false
    }

    private suspend fun stopInternal() {
        if (internalAudioEnabled || screenAudioCapturer != null) {
            disableInternalAudioOnly()
        } else {
            runCatching { room.localParticipant.setMicrophoneEnabled(false) }
        }
        runCatching { room.localParticipant.setScreenShareEnabled(false) }
        releaseLocalResources()
        runCatching { room.disconnect() }
    }

    private fun releaseLocalResources() {
        screenAudioCapturer?.releaseAudioResources()
        screenAudioCapturer = null
        internalAudioEnabled = false
        getApplication<Application>().stopService(Intent(getApplication(), StreamForegroundService::class.java))
    }

    override fun onCleared() {
        // ViewModel scope is already being cancelled here, so only perform synchronous cleanup.
        releaseLocalResources()
        runCatching { room.disconnect() }
        room.release()
        super.onCleared()
    }
}
