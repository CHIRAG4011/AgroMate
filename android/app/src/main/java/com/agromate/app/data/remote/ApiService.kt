package com.agromate.app.data.remote

import okhttp3.MultipartBody
import retrofit2.http.Body
import retrofit2.http.GET
import retrofit2.http.Multipart
import retrofit2.http.POST
import retrofit2.http.Part
import retrofit2.http.Path
import retrofit2.http.Query

data class LoginRequest(val username: String, val email: String)
data class AuthResponse(val accessToken: String, val tokenType: String)
data class ScanResponse(val id: String, val plantName: String, val condition: String, val confidence: Double)
data class RobotStatus(val id: String, val name: String, val isConnected: Boolean, val battery: Int)
data class RobotsResponse(val status: String, val robots: List<RobotStatus>)

data class ChatRequest(
    val message: String,
    val session_id: String = "default",
    val language: String = "en",
    val response_type: String = "summary",
    val scan_context: Map<String, Any>? = null
)

data class ChatResponseData(
    val text: String,
    val short_summary: String?,
    val response_type: String?,
    val has_detailed: Boolean,
    val language: String
)

data class ChatApiResponse(val status: String, val response: ChatResponseData)

data class DetailedRequest(
    val session_id: String = "default",
    val language: String = "en"
)

data class ContextRequest(
    val session_id: String = "default",
    val scan_context: Map<String, Any>
)

data class PlantIdentifyResult(
    val plant_name: String,
    val confidence: Double,
    val scientific_name: String?,
    val family: String?,
    val common_names: List<String>?,
    val model_used: String?,
    val processing_time_ms: Double?,
    val ai_summary: String? = null
)

data class PlantIdentifyResponse(
    val status: String,
    val result: PlantIdentifyResult
)

data class PlantDiagnoseResult(
    val plant_name: String,
    val disease_name: String,
    val is_healthy: Boolean,
    val confidence: Double,
    val severity: String,
    val affected_area_percent: Double?,
    val treatment: String?,
    val organic_treatment: String?,
    val model_used: String?,
    val processing_time_ms: Double?,
    val ai_summary: String? = null
)

data class PlantDiagnoseResponse(
    val status: String,
    val result: PlantDiagnoseResult
)

data class TranslationResponse(
    val status: String,
    val translations: Map<String, String>
)

data class AnalyticsDbSummary(
    val total_scans: Int,
    val total_diagnoses: Int,
    val total_chats: Int
)

data class AnalyticsResponse(
    val status: String,
    val database: AnalyticsDbSummary
)

data class RobotSyncRequest(
    val robot_id: String,
    val name: String,
    val mac_address: String,
    val connected: Boolean,
    val battery: Int = 89,
    val signal_strength: String = "Excellent",
    val scans_today: Int = 124
)

interface ApiService {
    @GET("api/analytics")
    suspend fun getAnalytics(@Query("days") days: Int = 30): AnalyticsResponse

    @POST("api/robots/sync")
    suspend fun syncRobot(@Body request: RobotSyncRequest): Map<String, Any>

    @POST("api/auth/login")
    suspend fun login(@Body request: LoginRequest): AuthResponse

    @GET("api/robots")
    suspend fun getRobots(): RobotsResponse

    @POST("api/scan")
    suspend fun submitScan(@Query("robot_id") robotId: String, @Body imageBytes: ByteArray): ScanResponse

    @GET("api/analytics/weather")
    suspend fun getWeather(
        @Query("lat") lat: Double,
        @Query("lon") lon: Double,
        @Query("city") city: String?
    ): Map<String, Any>

    @GET("api/scans")
    suspend fun getScans(
        @Query("limit") limit: Int = 50,
        @Query("offset") offset: Int = 0,
        @Query("scan_type") scanType: String? = null
    ): ScanHistoryResponse

    @POST("api/chat/")
    suspend fun sendChatMessage(@Body request: ChatRequest): ChatApiResponse

    @POST("api/chat/details")
    suspend fun getDetailedResponse(@Body request: DetailedRequest): ChatApiResponse

    @POST("api/chat/context")
    suspend fun setSessionContext(@Body request: ContextRequest): Map<String, Any>

    @GET("api/chat/history")
    suspend fun getChatHistory(@Query("session_id") sessionId: String = "default"): ChatHistoryResponse


    @Multipart
    @POST("api/plants/identify")
    suspend fun identifyPlant(
        @Part image: MultipartBody.Part,
        @Query("language") language: String
    ): PlantIdentifyResponse

    @Multipart
    @POST("api/plants/diagnose")
    suspend fun diagnosePlant(
        @Part image: MultipartBody.Part,
        @Query("plant_name") plantName: String?,
        @Query("language") language: String
    ): PlantDiagnoseResponse

    @POST("api/voice/tts")
    suspend fun textToSpeech(@Body request: TTSRequest): okhttp3.ResponseBody

    @Multipart
    @POST("api/voice/stt")
    suspend fun speechToText(
        @Part file: MultipartBody.Part,
        @Query("language") language: String
    ): STTResponse

    @GET("api/i18n/translations/{language}")
    suspend fun getTranslations(@Path("language") language: String): TranslationResponse
}

data class DBMessage(val role: String, val content: String, val created_at: String)
data class ChatHistoryResponse(val status: String, val messages: List<DBMessage>)

data class TTSRequest(
    val text: String,
    val language: String = "en",
    val gender: String = "female",
    val engine: String = "edge-tts"
)

data class STTResponse(val status: String, val text: String, val language: String, val confidence: Double)

data class ScanHistoryResponse(
    val status: String,
    val total: Int,
    val scans: List<DBScanItem>
)

data class DBScanItem(
    val id: Int,
    val scan_type: String,
    val image_path: String,
    val created_at: String,
    val diagnoses: List<DBDiagnosisItem>
)

data class DBDiagnosisItem(
    val plant_name: String?,
    val disease_name: String?,
    val confidence: Double,
    val severity: String?,
    val treatment: String? = null,
    val organic_treatment: String? = null
)

