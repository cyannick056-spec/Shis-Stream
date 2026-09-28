package com.shis.stream

import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder

internal data class PublisherCredentials(
    val serverUrl: String,
    val token: String,
    val roomName: String,
)

internal object TokenApi {
    fun fetchPublisherCredentials(
        apiBaseUrl: String,
        streamName: String,
        streamKey: String,
    ): PublisherCredentials {
        val base = apiBaseUrl.trim().trimEnd('/')
        require(base.startsWith("https://") || base.startsWith("http://")) {
            "La URL del servidor no es válida"
        }

        val stream = URLEncoder.encode(streamName.trim(), "UTF-8")
        val connection = (URL("$base/api/publisher-token?stream=$stream").openConnection() as HttpURLConnection).apply {
            requestMethod = "GET"
            connectTimeout = 10_000
            readTimeout = 10_000
            setRequestProperty("Accept", "application/json")
            setRequestProperty("X-Stream-Key", streamKey)
        }

        try {
            val code = connection.responseCode
            val body = (if (code in 200..299) connection.inputStream else connection.errorStream)
                ?.bufferedReader()
                ?.use { it.readText() }
                .orEmpty()

            if (code !in 200..299) {
                val message = runCatching { JSONObject(body).optString("error") }.getOrNull()
                throw IllegalStateException(message?.takeIf { it.isNotBlank() } ?: "Servidor respondió HTTP $code")
            }

            val json = JSONObject(body)
            return PublisherCredentials(
                serverUrl = json.getString("serverUrl"),
                token = json.getString("token"),
                roomName = json.getString("roomName"),
            )
        } finally {
            connection.disconnect()
        }
    }
}
