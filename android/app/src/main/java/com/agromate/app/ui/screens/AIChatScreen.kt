package com.agromate.app.ui.screens

import android.annotation.SuppressLint
import android.content.Context
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.media.MediaPlayer
import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDownward
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.Send
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material.icons.filled.VolumeUp
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.agromate.app.data.NetworkConfig
import com.agromate.app.data.Localizer
import com.agromate.app.data.remote.ChatRequest
import com.agromate.app.data.remote.DetailedRequest
import com.agromate.app.data.remote.TTSRequest
import com.agromate.app.ui.theme.GreenPrimary
import com.agromate.app.ui.theme.GreenSecondary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.MultipartBody
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileOutputStream
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

data class ChatMessage(
    val text: String,
    val isUser: Boolean,
    val timestamp: String,
    val hasDetailed: Boolean = false,
    val responseType: String = "summary"
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AIChatScreen(
    modifier: Modifier = Modifier,
    messages: List<ChatMessage>,
    onMessagesChange: (List<ChatMessage>) -> Unit,
    chatPreloadMessage: String? = null,
    onPreloadHandled: () -> Unit = {}
) {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    val database = remember { com.agromate.app.data.local.AppDatabase.getDatabase(context) }
    val dao = remember { database.dao() }
    val localMessages by dao.getAllChatMessages().collectAsState(initial = emptyList())
    var isSending by remember { mutableStateOf(false) }

    var inputText by remember { mutableStateOf("") }
    var isListening by remember { mutableStateOf(false) }

    val lazyListState = rememberLazyListState()
    val recorder = remember { AudioRecorder() }
    
    var speakingMessageIndex by remember { mutableStateOf<Int?>(null) }

    val showScrollToBottomButton by remember {
        derivedStateOf {
            lazyListState.firstVisibleItemIndex > 0
        }
    }

    // Auto scroll to bottom
    LaunchedEffect(messages.size) {
        if (messages.isNotEmpty()) {
            lazyListState.animateScrollToItem(messages.size - 1)
        }
    }

    // Preload triggers (e.g. from camera scan screen)
    LaunchedEffect(chatPreloadMessage) {
        val preloadMsg = chatPreloadMessage
        if (!preloadMsg.isNullOrBlank()) {
            val timeString = SimpleDateFormat("hh:mm a", Locale.getDefault()).format(Date())
            coroutineScope.launch {
                dao.insertChatMessage(
                    com.agromate.app.data.local.LocalChatMessage(
                        text = preloadMsg,
                        isUser = true,
                        timestamp = timeString
                    )
                )
            }

            isSending = true
            try {
                val apiService = NetworkConfig.getApiService()
                val response = apiService.sendChatMessage(
                    ChatRequest(
                        message = preloadMsg,
                        language = Localizer.currentLanguage,
                        response_type = "summary"
                    )
                )
                val botResp = response.response
                val textToSpeak = botResp.short_summary ?: botResp.text
                coroutineScope.launch {
                    dao.insertChatMessage(
                        com.agromate.app.data.local.LocalChatMessage(
                            text = textToSpeak,
                            isUser = false,
                            timestamp = timeString,
                            hasDetailed = botResp.has_detailed,
                            responseType = botResp.response_type ?: "summary"
                        )
                    )
                }
                
                val botMsgIndex = chatMessages.size + 1
                speakingMessageIndex = botMsgIndex
                playTTSSpeech(context, textToSpeak, Localizer.currentLanguage) {
                    if (speakingMessageIndex == botMsgIndex) {
                        speakingMessageIndex = null
                    }
                }
            } catch (e: Exception) {
                coroutineScope.launch {
                    dao.insertChatMessage(
                        com.agromate.app.data.local.LocalChatMessage(
                            text = "Error: Could not connect to Laptop AI Server (${e.localizedMessage}). Please verify connection settings.",
                            isUser = false,
                            timestamp = timeString
                        )
                    )
                }
            } finally {
                isSending = false
                onPreloadHandled()
            }
        }
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        // Chat Header
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text(text = Localizer.t("nav_chat"), style = MaterialTheme.typography.titleLarge)
                val currentLangName = Localizer.supportedLanguages.find { it.code == Localizer.currentLanguage }?.name ?: "English"
                Text(text = "Language: $currentLangName", style = MaterialTheme.typography.bodySmall)
            }
            if (speakingMessageIndex != null) {
                IconButton(
                    onClick = {
                        stopCurrentSpeech()
                        speakingMessageIndex = null
                    },
                    colors = IconButtonDefaults.iconButtonColors(
                        containerColor = MaterialTheme.colorScheme.errorContainer
                    )
                ) {
                    Icon(
                        imageVector = Icons.Default.Stop,
                        contentDescription = "Stop Speech",
                        tint = MaterialTheme.colorScheme.error
                    )
                }
            }
        }

        // Messages List
        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
        ) {
            LazyColumn(
                state = lazyListState,
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.spacedBy(12.dp),
                contentPadding = PaddingValues(vertical = 8.dp)
            ) {
                itemsIndexed(messages) { index, message ->
                    Box(
                        modifier = Modifier.fillMaxWidth(),
                        contentAlignment = if (message.isUser) Alignment.CenterEnd else Alignment.CenterStart
                    ) {
                        Column(
                            horizontalAlignment = if (message.isUser) Alignment.End else Alignment.Start
                        ) {
                            Box(
                                modifier = Modifier
                                    .clip(
                                        RoundedCornerShape(
                                            topStart = 16.dp,
                                            topEnd = 16.dp,
                                            bottomStart = if (message.isUser) 16.dp else 4.dp,
                                            bottomEnd = if (message.isUser) 4.dp else 16.dp
                                        )
                                    )
                                    .background(
                                        if (message.isUser) GreenSecondary else MaterialTheme.colorScheme.surfaceVariant
                                    )
                                    .padding(12.dp)
                                    .widthIn(max = 280.dp)
                            ) {
                                Column {
                                    if (!message.isUser) {
                                        Row(
                                            modifier = Modifier.fillMaxWidth(),
                                            horizontalArrangement = Arrangement.SpaceBetween,
                                            verticalAlignment = Alignment.CenterVertically
                                        ) {
                                            Text(
                                                text = "🌱 AgroMate AI",
                                                fontWeight = FontWeight.Bold,
                                                fontSize = 11.sp,
                                                color = GreenPrimary
                                            )
                                            
                                            val isThisMessageSpeaking = speakingMessageIndex == index
                                            
                                            IconButton(
                                                onClick = {
                                                    coroutineScope.launch {
                                                        if (isThisMessageSpeaking) {
                                                            stopCurrentSpeech()
                                                            speakingMessageIndex = null
                                                        } else {
                                                            speakingMessageIndex = index
                                                            playTTSSpeech(context, message.text, Localizer.currentLanguage) {
                                                                if (speakingMessageIndex == index) {
                                                                    speakingMessageIndex = null
                                                                }
                                                            }
                                                        }
                                                    }
                                                },
                                                modifier = Modifier.size(24.dp)
                                            ) {
                                                Icon(
                                                    imageVector = if (isThisMessageSpeaking) Icons.Default.Stop else Icons.Default.VolumeUp,
                                                    contentDescription = if (isThisMessageSpeaking) "Stop Speech" else "Speak Response",
                                                    tint = if (isThisMessageSpeaking) GreenPrimary else Color.Gray,
                                                    modifier = Modifier.size(16.dp)
                                                )
                                            }
                                        }
                                        Spacer(modifier = Modifier.height(4.dp))
                                    }
                                    Text(
                                        text = message.text,
                                        color = if (message.isUser) Color.White else MaterialTheme.colorScheme.onSurfaceVariant,
                                        fontSize = 14.sp
                                    )

                                    // V2 Two-Tier More Details Button
                                    if (!message.isUser && message.hasDetailed && message.responseType == "summary") {
                                        Spacer(modifier = Modifier.height(10.dp))
                                        Button(
                                            onClick = {
                                                isSending = true
                                                coroutineScope.launch {
                                                    try {
                                                        val apiService = NetworkConfig.getApiService()
                                                        val response = apiService.getDetailedResponse(
                                                            DetailedRequest(
                                                                session_id = "default",
                                                                language = Localizer.currentLanguage
                                                            )
                                                        )
                                                        val botResponse = response.response
                                                        if (index < localMessages.size) {
                                                            val origMsg = localMessages[index]
                                                            val updatedLocal = origMsg.copy(
                                                                text = botResponse.text,
                                                                hasDetailed = botResponse.has_detailed,
                                                                responseType = botResponse.response_type ?: "detailed"
                                                            )
                                                            dao.insertChatMessage(updatedLocal)
                                                        }
                                                    } catch (e: Exception) {
                                                        Toast.makeText(context, "Failed to load detailed report", Toast.LENGTH_SHORT).show()
                                                    } finally {
                                                        isSending = false
                                                    }
                                                }
                                            },
                                            colors = ButtonDefaults.buttonColors(containerColor = GreenPrimary),
                                            shape = RoundedCornerShape(8.dp),
                                            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp),
                                            modifier = Modifier.height(32.dp)
                                        ) {
                                            Text(text = Localizer.t("more_details"), fontSize = 11.sp)
                                        }
                                    }
                                }
                            }
                            Spacer(modifier = Modifier.height(2.dp))
                            Text(
                                text = message.timestamp,
                                fontSize = 10.sp,
                                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.5f)
                            )
                        }
                    }
                }
            }

            // Scroll to bottom FAB
            if (showScrollToBottomButton) {
                FloatingActionButton(
                    onClick = {
                        coroutineScope.launch {
                            if (messages.isNotEmpty()) {
                                lazyListState.animateScrollToItem(messages.size - 1)
                            }
                        }
                    },
                    modifier = Modifier
                        .align(Alignment.BottomEnd)
                        .padding(16.dp)
                        .size(40.dp),
                    containerColor = GreenPrimary,
                    contentColor = Color.White,
                    shape = RoundedCornerShape(20.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.ArrowDownward,
                        contentDescription = "Scroll down",
                        modifier = Modifier.size(20.dp)
                    )
                }
            }
        }

        // State displays
        if (isListening) {
            Text(
                text = "🎙️ Listening... speak now",
                color = MaterialTheme.colorScheme.error,
                fontSize = 12.sp,
                modifier = Modifier.align(Alignment.CenterHorizontally)
            )
        } else if (isSending) {
            Text(
                text = "AgroMate is thinking...",
                color = GreenPrimary,
                fontSize = 12.sp,
                modifier = Modifier.align(Alignment.Start).padding(start = 8.dp)
            )
        }

        // Input controls
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            IconButton(
                onClick = { 
                    if (isListening) {
                        isListening = false
                        coroutineScope.launch {
                            val wavData = recorder.stopRecording()
                            if (wavData != null && wavData.isNotEmpty()) {
                                try {
                                    val requestBody = wavData.toRequestBody("audio/wav".toMediaTypeOrNull())
                                    val multipartBody = MultipartBody.Part.createFormData("file", "recording.wav", requestBody)
                                    
                                    val apiService = NetworkConfig.getApiService()
                                    val response = apiService.speechToText(multipartBody, Localizer.currentLanguage)
                                    if (response.status == "success" && response.text.isNotBlank()) {
                                        inputText = response.text
                                    }
                                } catch (e: Exception) {
                                    Toast.makeText(context, "Voice transcription failed: ${e.localizedMessage}", Toast.LENGTH_SHORT).show()
                                }
                            }
                        }
                    } else {
                        stopCurrentSpeech()
                        speakingMessageIndex = null
                        isListening = true
                        recorder.startRecording()
                    }
                },
                colors = IconButtonDefaults.iconButtonColors(
                    containerColor = if (isListening) MaterialTheme.colorScheme.errorContainer else MaterialTheme.colorScheme.surfaceVariant
                )
            ) {
                Icon(
                    imageVector = if (isListening) Icons.Default.Stop else Icons.Default.Mic, 
                    contentDescription = "Voice Input",
                    tint = if (isListening) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant
                )
            }

            TextField(
                value = inputText,
                onValueChange = { inputText = it },
                modifier = Modifier.weight(1f),
                placeholder = { Text(Localizer.t("type_message"), fontSize = 14.sp) },
                shape = RoundedCornerShape(24.dp),
                colors = TextFieldDefaults.colors(
                    focusedIndicatorColor = Color.Transparent,
                    unfocusedIndicatorColor = Color.Transparent
                ),
                enabled = !isSending
            )

            IconButton(
                onClick = {
                    if (inputText.isNotBlank() && !isSending) {
                        val userMsgText = inputText
                        inputText = ""
                        val timeString = SimpleDateFormat("hh:mm a", Locale.getDefault()).format(Date())
                        coroutineScope.launch {
                            dao.insertChatMessage(
                                com.agromate.app.data.local.LocalChatMessage(
                                    text = userMsgText,
                                    isUser = true,
                                    timestamp = timeString
                                )
                            )
                        }

                        isSending = true
                        coroutineScope.launch {
                            try {
                                val apiService = NetworkConfig.getApiService()
                                val response = apiService.sendChatMessage(
                                    ChatRequest(
                                        message = userMsgText,
                                        language = Localizer.currentLanguage,
                                        response_type = "summary"
                                    )
                                )
                                val botResp = response.response
                                val textToSpeak = botResp.short_summary ?: botResp.text
                                coroutineScope.launch {
                                    dao.insertChatMessage(
                                        com.agromate.app.data.local.LocalChatMessage(
                                            text = textToSpeak,
                                            isUser = false,
                                            timestamp = timeString,
                                            hasDetailed = botResp.has_detailed,
                                            responseType = botResp.response_type ?: "summary"
                                        )
                                    )
                                }
                                val botMsgIndex = chatMessages.size + 1
                                speakingMessageIndex = botMsgIndex
                                playTTSSpeech(context, textToSpeak, Localizer.currentLanguage) {
                                    if (speakingMessageIndex == botMsgIndex) {
                                        speakingMessageIndex = null
                                    }
                                }
                            } catch (e: Exception) {
                                coroutineScope.launch {
                                    dao.insertChatMessage(
                                        com.agromate.app.data.local.LocalChatMessage(
                                            text = "Error: Could not connect to Laptop AI Server (${e.localizedMessage}). Please verify connection settings.",
                                            isUser = false,
                                            timestamp = timeString
                                        )
                                    )
                                }
                            } finally {
                                isSending = false
                            }
                        }
                    }
                },
                colors = IconButtonDefaults.iconButtonColors(
                    containerColor = GreenPrimary
                ),
                enabled = inputText.isNotBlank() && !isSending
            ) {
                Icon(imageVector = Icons.Default.Send, contentDescription = "Send", tint = Color.White)
            }
        }
    }
}

// ── Text-to-Speech Player Manager ──
private var mediaPlayer: MediaPlayer? = null

suspend fun playTTSSpeech(
    context: Context,
    text: String,
    languageCode: String,
    onFinished: () -> Unit
) {
    try {
        stopCurrentSpeech()
        val cleanText = text.replace(Regex("[*#`_\\-]"), " ").replace(Regex("\\[.*?]\\(.*?\\)"), " ")
        withContext(Dispatchers.IO) {
            val prefs = context.getSharedPreferences("agromate_prefs", Context.MODE_PRIVATE)
            val engine = prefs.getString("voice_engine", "edge-tts") ?: "edge-tts"
            val gender = prefs.getString("voice_gender", "female") ?: "female"
            
            val apiService = NetworkConfig.getApiService()
            val responseBody = apiService.textToSpeech(
                TTSRequest(
                    text = cleanText,
                    language = languageCode,
                    gender = gender,
                    engine = engine
                )
            )
            val bytes = responseBody.bytes()
            
            val tempFile = File(context.cacheDir, "speech_temp.mp3")
            if (tempFile.exists()) tempFile.delete()
            
            FileOutputStream(tempFile).use { fos ->
                fos.write(bytes)
            }
            
            withContext(Dispatchers.Main) {
                val player = MediaPlayer()
                mediaPlayer = player
                player.setDataSource(tempFile.absolutePath)
                player.prepare()
                player.setOnCompletionListener {
                    it.release()
                    mediaPlayer = null
                    onFinished()
                }
                player.start()
            }
        }
    } catch (e: Exception) {
        e.printStackTrace()
        onFinished()
    }
}

fun stopCurrentSpeech() {
    try {
        mediaPlayer?.let {
            if (it.isPlaying) {
                it.stop()
            }
            it.release()
        }
    } catch (e: Exception) {
        e.printStackTrace()
    } finally {
        mediaPlayer = null
    }
}

// ── AudioRecorder Helper for WAV capturing ──
class AudioRecorder {
    private var audioRecord: AudioRecord? = null
    private var isRecording = false
    private val sampleRate = 16000
    private val channelConfig = AudioFormat.CHANNEL_IN_MONO
    private val audioFormat = AudioFormat.ENCODING_PCM_16BIT
    private val bufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat)
    private val outputStream = ByteArrayOutputStream()

    @SuppressLint("MissingPermission")
    fun startRecording() {
        if (isRecording) return
        try {
            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.MIC,
                sampleRate,
                channelConfig,
                audioFormat,
                bufferSize
            )
            audioRecord?.startRecording()
            isRecording = true
            outputStream.reset()
            
            Thread {
                val buffer = ShortArray(bufferSize)
                while (isRecording) {
                    val read = audioRecord?.read(buffer, 0, buffer.size) ?: 0
                    if (read > 0) {
                        synchronized(outputStream) {
                            for (i in 0 until read) {
                                val value = buffer[i]
                                outputStream.write(value.toInt() and 0xFF)
                                outputStream.write((value.toInt() shr 8) and 0xFF)
                            }
                        }
                    }
                }
            }.start()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun stopRecording(): ByteArray? {
        if (!isRecording) return null
        isRecording = false
        try {
            audioRecord?.stop()
            audioRecord?.release()
        } catch (e: Exception) {
            e.printStackTrace()
        }
        audioRecord = null
        
        val pcmData = synchronized(outputStream) {
            outputStream.toByteArray()
        }
        if (pcmData.isEmpty()) return null
        return pcmToWav(pcmData, sampleRate)
    }

    private fun pcmToWav(pcmData: ByteArray, sampleRate: Int): ByteArray {
        val header = ByteArray(44)
        val totalDataLen = pcmData.size + 36
        val byteRate = sampleRate * 2

        header[0] = 'R'.code.toByte()
        header[1] = 'I'.code.toByte()
        header[2] = 'F'.code.toByte()
        header[3] = 'F'.code.toByte()
        
        header[4] = (totalDataLen and 0xff).toByte()
        header[5] = ((totalDataLen shr 8) and 0xff).toByte()
        header[6] = ((totalDataLen shr 16) and 0xff).toByte()
        header[7] = ((totalDataLen shr 24) and 0xff).toByte()

        header[8] = 'W'.code.toByte()
        header[9] = 'A'.code.toByte()
        header[10] = 'V'.code.toByte()
        header[11] = 'E'.code.toByte()

        header[12] = 'f'.code.toByte()
        header[13] = 'm'.code.toByte()
        header[14] = 't'.code.toByte()
        header[15] = ' '.code.toByte()

        header[16] = 16
        header[17] = 0
        header[18] = 0
        header[19] = 0

        header[20] = 1
        header[21] = 0
        header[22] = 1
        header[23] = 0

        header[24] = (sampleRate and 0xff).toByte()
        header[25] = ((sampleRate shr 8) and 0xff).toByte()
        header[26] = ((sampleRate shr 16) and 0xff).toByte()
        header[27] = ((sampleRate shr 24) and 0xff).toByte()

        header[28] = (byteRate and 0xff).toByte()
        header[29] = ((byteRate shr 8) and 0xff).toByte()
        header[30] = ((byteRate shr 16) and 0xff).toByte()
        header[31] = ((byteRate shr 24) and 0xff).toByte()

        header[32] = 2
        header[33] = 0
        header[34] = 16
        header[35] = 0

        header[36] = 'd'.code.toByte()
        header[37] = 'a'.code.toByte()
        header[38] = 't'.code.toByte()
        header[39] = 'a'.code.toByte()

        header[40] = (pcmData.size and 0xff).toByte()
        header[41] = ((pcmData.size shr 8) and 0xff).toByte()
        header[42] = ((pcmData.size shr 16) and 0xff).toByte()
        header[43] = ((pcmData.size shr 24) and 0xff).toByte()

        val wavData = ByteArray(44 + pcmData.size)
        System.arraycopy(header, 0, wavData, 0, 44)
        System.arraycopy(pcmData, 0, wavData, 44, pcmData.size)
        return wavData
    }
}
