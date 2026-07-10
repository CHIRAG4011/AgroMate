package com.agromate.app.data.repository

import com.agromate.app.data.remote.WebSocketClient
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class ChatRepository @Inject constructor(
    private val wsClient: WebSocketClient
) {
    fun connectChat(serverUrl: String, onMessage: (String) -> Unit, onError: (Throwable) -> Unit) {
        wsClient.connect("$serverUrl/ws/chat", onMessage, onError)
    }

    fun sendMessage(text: String) {
        wsClient.sendMessage(text)
    }

    fun disconnect() {
        wsClient.disconnect()
    }
}
