package com.agromate.app.data.remote

import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString

class WebSocketClient(private val client: OkHttpClient) {
    private var webSocket: WebSocket? = null

    fun connect(url: String, onMessageReceived: (String) -> Unit, onError: (Throwable) -> Unit) {
        val request = Request.Builder().url(url).build()
        webSocket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onMessage(webSocket: WebSocket, text: String) {
                onMessageReceived(text)
            }

            override fun onMessage(webSocket: WebSocket, bytes: ByteString) {
                onMessageReceived(bytes.utf8())
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                onError(t)
            }

            override fun onClosing(webSocket: WebSocket, code: Int, reason: String) {
                webSocket.close(1000, null)
            }
        })
    }

    fun sendMessage(text: String) {
        webSocket?.send(text)
    }

    fun disconnect() {
        webSocket?.close(1000, "App closed")
        webSocket = null
    }
}
