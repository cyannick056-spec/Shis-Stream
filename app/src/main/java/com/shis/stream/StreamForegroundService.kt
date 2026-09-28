package com.shis.stream

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.IBinder

class StreamForegroundService : Service() {
    override fun onCreate() {
        super.onCreate()
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL_ID,
                "SHIS Stream",
                NotificationManager.IMPORTANCE_LOW,
            ),
        )

        val notification = Notification.Builder(this, CHANNEL_ID)
            .setContentTitle("SHIS Stream")
            .setContentText("Transmitiendo pantalla y audio del juego")
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setOngoing(true)
            .build()

        startForeground(NOTIFICATION_ID, notification)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int = START_STICKY

    override fun onBind(intent: Intent?): IBinder? = null

    companion object {
        private const val CHANNEL_ID = "shis_stream_capture"
        private const val NOTIFICATION_ID = 1406
    }
}
