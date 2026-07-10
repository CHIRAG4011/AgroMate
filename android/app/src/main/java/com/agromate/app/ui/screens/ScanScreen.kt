package com.agromate.app.ui.screens

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.content.Context
import android.widget.Toast
import com.agromate.app.data.Localizer
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.result.launch
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.*
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.CameraAlt
import androidx.compose.material.icons.filled.Chat
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.PhotoLibrary
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material.icons.filled.VolumeUp
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.agromate.app.data.NetworkConfig
import com.agromate.app.data.remote.PlantDiagnoseResult
import com.agromate.app.data.remote.PlantIdentifyResult
import com.agromate.app.data.remote.ContextRequest
import com.agromate.app.ui.theme.AccentOrange
import com.agromate.app.ui.theme.GreenPrimary
import com.agromate.app.ui.theme.GreenSecondary
import com.agromate.app.ui.theme.RedAlert
import kotlinx.coroutines.launch
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.MultipartBody
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.ByteArrayOutputStream
import java.io.InputStream

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ScanScreen(
    modifier: Modifier = Modifier,
    onNavigateToChat: (String) -> Unit
) {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    var isSpeaking by remember { mutableStateOf(false) }

    val infiniteTransition = rememberInfiniteTransition(label = "ScannerAnimation")
    val scanProgress by infiniteTransition.animateFloat(
        initialValue = 0.05f,
        targetValue = 0.95f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 1800, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "scanProgress"
    )
    val blinkAlpha by infiniteTransition.animateFloat(
        initialValue = 0.3f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 600, easing = EaseInOutSine),
            repeatMode = RepeatMode.Reverse
        ),
        label = "blinkAlpha"
    )

    // Result holds either PlantIdentifyResult or PlantDiagnoseResult
    var identifyResult by remember { mutableStateOf<PlantIdentifyResult?>(null) }
    var diagnoseResult by remember { mutableStateOf<PlantDiagnoseResult?>(null) }

    // Auto-TTS trigger when scan completes
    LaunchedEffect(identifyResult, diagnoseResult) {
        val summary = identifyResult?.ai_summary ?: diagnoseResult?.ai_summary
        if (!summary.isNullOrBlank()) {
            val prefs = context.getSharedPreferences("agromate_prefs", Context.MODE_PRIVATE)
            val autoplay = prefs.getBoolean("voice_autoplay", true)
            if (autoplay) {
                isSpeaking = true
                playTTSSpeech(context, summary, Localizer.currentLanguage) {
                    isSpeaking = false
                }
            }
        }
    }

    DisposableEffect(Unit) {
        onDispose {
            stopCurrentSpeech()
        }
    }

    var imageBitmap by remember { mutableStateOf<Bitmap?>(null) }
    var imageBytes by remember { mutableStateOf<ByteArray?>(null) }
    
    var plantNameInput by remember { mutableStateOf("") }
    var isScanning by remember { mutableStateOf(false) }
    var scanStatusText by remember { mutableStateOf("") }
    
    // Track if camera was already auto-launched this session
    var cameraAutoLaunched by remember { mutableStateOf(false) }

    // Auto-analyze function: runs identify + diagnose sequentially
    fun performFullAnalysis(bytes: ByteArray) {
        isScanning = true
        identifyResult = null
        diagnoseResult = null
        scanStatusText = "Identifying plant..."
        
        coroutineScope.launch {
            try {
                val requestBody = bytes.toRequestBody("image/jpeg".toMediaTypeOrNull())
                val multipartBody = MultipartBody.Part.createFormData("image", "scan.jpg", requestBody)
                
                val apiService = NetworkConfig.getApiService()
                
                // Step 1: Identify the plant
                try {
                    val identifyResponse = apiService.identifyPlant(multipartBody, Localizer.currentLanguage)
                    if (identifyResponse.status == "success") {
                        identifyResult = identifyResponse.result
                        plantNameInput = identifyResponse.result.plant_name
                    }
                } catch (e: Exception) {
                    // Identification failed, continue to diagnosis
                }
                
                // Step 2: Diagnose disease (re-create request body since stream was consumed)
                scanStatusText = "Diagnosing diseases..."
                try {
                    val diagRequestBody = bytes.toRequestBody("image/jpeg".toMediaTypeOrNull())
                    val diagMultipart = MultipartBody.Part.createFormData("image", "scan.jpg", diagRequestBody)
                    
                    val diagResponse = apiService.diagnosePlant(
                        diagMultipart,
                        if (plantNameInput.isBlank()) null else plantNameInput,
                        Localizer.currentLanguage
                    )
                    if (diagResponse.status == "success") {
                        diagnoseResult = diagResponse.result
                    }
                } catch (e: Exception) {
                    // Diagnosis failed
                }

                // Step 3: Set session context on backend
                try {
                    val contextMap = mutableMapOf<String, Any>()
                    diagnoseResult?.let {
                        contextMap["plant_name"] = it.plant_name
                        contextMap["disease_name"] = it.disease_name
                        contextMap["is_healthy"] = it.is_healthy
                        contextMap["confidence"] = it.confidence
                        contextMap["severity"] = it.severity
                    } ?: identifyResult?.let {
                        contextMap["plant_name"] = it.plant_name
                        contextMap["scientific_name"] = it.scientific_name ?: ""
                        contextMap["confidence"] = it.confidence
                        contextMap["is_healthy"] = true
                    }
                    if (contextMap.isNotEmpty()) {
                        apiService.setSessionContext(ContextRequest("default", contextMap))
                    }
                } catch (ctxErr: Exception) {
                    ctxErr.printStackTrace()
                }
                
                if (identifyResult == null && diagnoseResult == null) {
                    Toast.makeText(context, "Could not analyze the image. Please try again.", Toast.LENGTH_LONG).show()
                }
            } catch (e: Exception) {
                Toast.makeText(context, "Error: ${e.localizedMessage}", Toast.LENGTH_LONG).show()
            } finally {
                isScanning = false
                scanStatusText = ""
            }
        }
    }

    // Camera Launcher — auto-analyze after capture
    val cameraLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.TakePicturePreview()
    ) { bitmap ->
        if (bitmap != null) {
            imageBitmap = bitmap
            val stream = ByteArrayOutputStream()
            bitmap.compress(Bitmap.CompressFormat.JPEG, 90, stream)
            val bytes = stream.toByteArray()
            imageBytes = bytes
            
            // Clear previous results
            identifyResult = null
            diagnoseResult = null
            
            // Auto-analyze immediately
            performFullAnalysis(bytes)
        }
    }

    // Gallery Picker Launcher — auto-analyze after selection
    val galleryLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.PickVisualMedia()
    ) { uri ->
        if (uri != null) {
            try {
                val inputStream: InputStream? = context.contentResolver.openInputStream(uri)
                val bitmap = BitmapFactory.decodeStream(inputStream)
                imageBitmap = bitmap
                
                val byteStream = ByteArrayOutputStream()
                bitmap.compress(Bitmap.CompressFormat.JPEG, 90, byteStream)
                val bytes = byteStream.toByteArray()
                imageBytes = bytes
                
                // Clear previous results
                identifyResult = null
                diagnoseResult = null
                
                // Auto-analyze immediately
                performFullAnalysis(bytes)
            } catch (e: Exception) {
                Toast.makeText(context, "Failed to load image: ${e.localizedMessage}", Toast.LENGTH_SHORT).show()
            }
        }
    }

    // Auto-launch camera when entering the scan screen
    LaunchedEffect(Unit) {
        if (!cameraAutoLaunched) {
            cameraAutoLaunched = true
            try {
                cameraLauncher.launch()
            } catch (e: Exception) {
                // Camera not available, user can use gallery
            }
        }
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Premium Scan Header with green gradient
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(110.dp)
                .clip(RoundedCornerShape(20.dp))
                .background(
                    Brush.horizontalGradient(
                        colors = listOf(GreenPrimary, GreenSecondary)
                    )
                )
                .padding(20.dp),
            contentAlignment = Alignment.CenterStart
        ) {
            Column {
                Text(
                    text = "AI Plant Scanner",
                    color = Color.White,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold
                )
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    text = "Auto-identifies plant & diagnoses diseases",
                    color = Color.White.copy(alpha = 0.85f),
                    fontSize = 13.sp
                )
            }
        }

        // Image Preview & Capture Area
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .height(260.dp),
            shape = RoundedCornerShape(20.dp),
            border = BorderStroke(1.dp, if (isScanning) Color(0xFF00E676) else MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)),
            colors = CardDefaults.cardColors(containerColor = Color.Black)
        ) {
            Box(
                modifier = Modifier.fillMaxSize(),
                contentAlignment = Alignment.Center
            ) {
                val bitmap = imageBitmap
                if (bitmap != null) {
                    Image(
                        bitmap = bitmap.asImageBitmap(),
                        contentDescription = "Selected plant image",
                        modifier = Modifier.fillMaxSize(),
                        contentScale = ContentScale.Crop
                    )
                } else {
                    Column(
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.CameraAlt,
                            contentDescription = "No Image Selected",
                            tint = GreenPrimary.copy(alpha = 0.5f),
                            modifier = Modifier.size(64.dp)
                        )
                        Text(
                            text = "Camera will open automatically",
                            color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.6f),
                            fontSize = 14.sp
                        )
                    }
                }

                // Sci-fi Viewfinder HUD overlay
                Canvas(modifier = Modifier.fillMaxSize()) {
                    val w = size.width
                    val h = size.height
                    
                    // Corner brackets
                    val bracketLen = 30f
                    val bracketOffset = 20f
                    val color = if (isScanning) Color(0xFF00E676) else Color.White.copy(alpha = 0.5f)
                    
                    // Top-Left
                    drawLine(color, androidx.compose.ui.geometry.Offset(bracketOffset, bracketOffset), androidx.compose.ui.geometry.Offset(bracketOffset + bracketLen, bracketOffset), strokeWidth = 3f)
                    drawLine(color, androidx.compose.ui.geometry.Offset(bracketOffset, bracketOffset), androidx.compose.ui.geometry.Offset(bracketOffset, bracketOffset + bracketLen), strokeWidth = 3f)
                    // Top-Right
                    drawLine(color, androidx.compose.ui.geometry.Offset(w - bracketOffset, bracketOffset), androidx.compose.ui.geometry.Offset(w - bracketOffset - bracketLen, bracketOffset), strokeWidth = 3f)
                    drawLine(color, androidx.compose.ui.geometry.Offset(w - bracketOffset, bracketOffset), androidx.compose.ui.geometry.Offset(w - bracketOffset, bracketOffset + bracketLen), strokeWidth = 3f)
                    // Bottom-Left
                    drawLine(color, androidx.compose.ui.geometry.Offset(bracketOffset, h - bracketOffset), androidx.compose.ui.geometry.Offset(bracketOffset + bracketLen, h - bracketOffset), strokeWidth = 3f)
                    drawLine(color, androidx.compose.ui.geometry.Offset(bracketOffset, h - bracketOffset), androidx.compose.ui.geometry.Offset(bracketOffset, h - bracketOffset - bracketLen), strokeWidth = 3f)
                    // Bottom-Right
                    drawLine(color, androidx.compose.ui.geometry.Offset(w - bracketOffset, h - bracketOffset), androidx.compose.ui.geometry.Offset(w - bracketOffset - bracketLen, h - bracketOffset), strokeWidth = 3f)
                    drawLine(color, androidx.compose.ui.geometry.Offset(w - bracketOffset, h - bracketOffset), androidx.compose.ui.geometry.Offset(w - bracketOffset, h - bracketOffset - bracketLen), strokeWidth = 3f)

                    // Moving laser scanning line
                    if (isScanning) {
                        val laserY = h * scanProgress
                        drawLine(
                            color = Color(0xFF00E676),
                            start = androidx.compose.ui.geometry.Offset(bracketOffset, laserY),
                            end = androidx.compose.ui.geometry.Offset(w - bracketOffset, laserY),
                            strokeWidth = 4f
                        )
                    }
                }

                // If scanning, show glassmorphic blinking banner
                if (isScanning) {
                    Box(
                        modifier = Modifier
                            .fillMaxSize()
                            .background(Color.Black.copy(alpha = 0.3f))
                    )
                    Card(
                        shape = RoundedCornerShape(12.dp),
                        colors = CardDefaults.cardColors(containerColor = Color.Black.copy(alpha = 0.75f)),
                        border = BorderStroke(1.dp, Color(0xFF00E676).copy(alpha = 0.5f)),
                        modifier = Modifier.align(Alignment.Center)
                    ) {
                        Row(
                            modifier = Modifier.padding(horizontal = 20.dp, vertical = 12.dp),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(8.dp)
                                    .clip(androidx.compose.foundation.shape.CircleShape)
                                    .background(Color(0xFF00E676).copy(alpha = blinkAlpha))
                            )
                            Text(
                                text = "AI CROP SCANNING...",
                                color = Color(0xFF00E676),
                                fontSize = 12.sp,
                                fontWeight = FontWeight.Bold,
                                letterSpacing = 1.5.sp
                            )
                        }
                    }
                }
            }
        }

        // Choose Source Action Buttons
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Button(
                onClick = { cameraLauncher.launch() },
                modifier = Modifier.weight(1f),
                colors = ButtonDefaults.buttonColors(containerColor = GreenPrimary),
                shape = RoundedCornerShape(12.dp)
            ) {
                Icon(imageVector = Icons.Default.CameraAlt, contentDescription = "Camera")
                Spacer(modifier = Modifier.width(8.dp))
                Text("Take Photo")
            }

            Button(
                onClick = { galleryLauncher.launch(
                    androidx.activity.result.PickVisualMediaRequest(
                        ActivityResultContracts.PickVisualMedia.ImageOnly
                    )
                ) },
                modifier = Modifier.weight(1f),
                colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondaryContainer),
                shape = RoundedCornerShape(12.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.PhotoLibrary, 
                    contentDescription = "Gallery",
                    tint = MaterialTheme.colorScheme.onSecondaryContainer
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text("Gallery", color = MaterialTheme.colorScheme.onSecondaryContainer)
            }
        }

        // Manual Search Card for Pesticides and Treatments
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f))
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(
                    text = "🔍 Search Pesticides & Treatments",
                    fontWeight = FontWeight.Bold,
                    fontSize = 15.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    var searchQuery by remember { mutableStateOf("") }
                    TextField(
                        value = searchQuery,
                        onValueChange = { searchQuery = it },
                        modifier = Modifier.weight(1f),
                        placeholder = { Text("e.g. Tomato Early Blight or Neem Oil", fontSize = 13.sp) },
                        shape = RoundedCornerShape(8.dp),
                        colors = TextFieldDefaults.colors(
                            focusedIndicatorColor = Color.Transparent,
                            unfocusedIndicatorColor = Color.Transparent
                        ),
                        singleLine = true
                    )
                    Button(
                        onClick = {
                            if (searchQuery.isNotBlank()) {
                                val prompt = "Provide pesticide recommendations, treatment info, dosage, and application guidelines for: '${searchQuery}'."
                                onNavigateToChat(prompt)
                            }
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = AccentOrange),
                        shape = RoundedCornerShape(8.dp),
                        enabled = searchQuery.isNotBlank()
                    ) {
                        Text("Search", fontSize = 13.sp, color = Color.White)
                    }
                }
            }
        }

        // Re-analyze button (if image exists but user wants to run again)
        if (imageBytes != null && !isScanning && (identifyResult != null || diagnoseResult != null)) {
            Button(
                onClick = { imageBytes?.let { performFullAnalysis(it) } },
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(12.dp),
                colors = ButtonDefaults.buttonColors(containerColor = AccentOrange)
            ) {
                Text("🔄 Re-Analyze Image", color = Color.White)
            }
        }

        // Loading state indicator
        if (isScanning) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
            ) {
                Row(
                    modifier = Modifier.padding(20.dp),
                    horizontalArrangement = Arrangement.spacedBy(16.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    CircularProgressIndicator(color = GreenPrimary, modifier = Modifier.size(28.dp))
                    Text(
                        if (scanStatusText.isNotBlank()) scanStatusText 
                        else "AgroMate AI is analyzing...", 
                        fontSize = 14.sp
                    )
                }
            }
        }

        // Display results
        val identify = identifyResult
        val diagnose = diagnoseResult
        
        if (identify != null) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                border = BorderStroke(1.dp, AccentOrange.copy(alpha = 0.5f))
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Icon(imageVector = Icons.Default.Info, contentDescription = "Plant identified", tint = AccentOrange)
                            Text(text = "Plant Identified", fontWeight = FontWeight.Bold, fontSize = 18.sp)
                        }
                        
                        IconButton(
                            onClick = {
                                coroutineScope.launch {
                                    if (isSpeaking) {
                                        stopCurrentSpeech()
                                        isSpeaking = false
                                    } else {
                                        val summary = identify.ai_summary
                                        if (!summary.isNullOrBlank()) {
                                            isSpeaking = true
                                            playTTSSpeech(context, summary, Localizer.currentLanguage) {
                                                isSpeaking = false
                                            }
                                        }
                                    }
                                }
                            },
                            modifier = Modifier.size(32.dp)
                        ) {
                            Icon(
                                imageVector = if (isSpeaking) Icons.Default.Stop else Icons.Default.VolumeUp,
                                contentDescription = "Speak summary",
                                tint = if (isSpeaking) GreenPrimary else Color.Gray
                            )
                        }
                    }
                    
                    Divider()
                    
                    Text(text = "Name: ${identify.plant_name}", fontWeight = FontWeight.SemiBold, fontSize = 16.sp)
                    if (!identify.scientific_name.isNullOrBlank()) {
                        Text(text = "Scientific Name: ${identify.scientific_name}", style = MaterialTheme.typography.bodyMedium)
                    }
                    if (!identify.family.isNullOrBlank()) {
                        Text(text = "Family: ${identify.family}", style = MaterialTheme.typography.bodyMedium)
                    }
                    Text(text = "Confidence: ${(identify.confidence * 100).toInt()}%", style = MaterialTheme.typography.bodyMedium)
                    
                    if (!identify.common_names.isNullOrEmpty()) {
                        Text(text = "Common Names: ${identify.common_names.joinToString(", ")}", style = MaterialTheme.typography.bodySmall)
                    }

                    if (!identify.ai_summary.isNullOrBlank()) {
                        Spacer(modifier = Modifier.height(4.dp))
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors = CardDefaults.cardColors(containerColor = GreenPrimary.copy(alpha = 0.08f)),
                            border = BorderStroke(1.dp, GreenPrimary.copy(alpha = 0.2f))
                        ) {
                            Column(modifier = Modifier.padding(12.dp)) {
                                Text(
                                    text = "✨ AI Analysis & Cures",
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 13.sp,
                                    color = GreenPrimary
                                )
                                Spacer(modifier = Modifier.height(4.dp))
                                Text(
                                    text = identify.ai_summary,
                                    fontSize = 14.sp,
                                    lineHeight = 20.sp
                                )
                            }
                        }
                    }
                    
                    Spacer(modifier = Modifier.height(4.dp))
                    
                    Button(
                        onClick = {
                            val prompt = "Tell me more about the plant '${identify.plant_name}' (${identify.scientific_name ?: ""}). How do I cultivate it and what soil works best?"
                            onNavigateToChat(prompt)
                        },
                        modifier = Modifier.fillMaxWidth(),
                        colors = ButtonDefaults.buttonColors(containerColor = AccentOrange),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Icon(imageVector = Icons.Default.Chat, contentDescription = "Ask Chat")
                        Spacer(modifier = Modifier.width(8.dp))
                        Text("Discuss this plant with Chatbot")
                    }
                }
            }
        }

        if (diagnose != null) {
            val statusColor = if (diagnose.is_healthy) GreenPrimary else RedAlert
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                border = BorderStroke(1.dp, statusColor.copy(alpha = 0.5f))
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Icon(
                                imageVector = if (diagnose.is_healthy) Icons.Default.CheckCircle else Icons.Default.Warning,
                                contentDescription = "Scan status",
                                tint = statusColor
                            )
                            Text(text = "Diagnosis Result", fontWeight = FontWeight.Bold, fontSize = 18.sp)
                        }

                        IconButton(
                            onClick = {
                                coroutineScope.launch {
                                    if (isSpeaking) {
                                        stopCurrentSpeech()
                                        isSpeaking = false
                                    } else {
                                        val summary = diagnose.ai_summary
                                        if (!summary.isNullOrBlank()) {
                                            isSpeaking = true
                                            playTTSSpeech(context, summary, Localizer.currentLanguage) {
                                                isSpeaking = false
                                            }
                                        }
                                    }
                                }
                            },
                            modifier = Modifier.size(32.dp)
                        ) {
                            Icon(
                                imageVector = if (isSpeaking) Icons.Default.Stop else Icons.Default.VolumeUp,
                                contentDescription = "Speak summary",
                                tint = if (isSpeaking) GreenPrimary else Color.Gray
                            )
                        }
                    }
                    
                    Divider()
                    
                    Text(text = "Plant: ${diagnose.plant_name}", fontWeight = FontWeight.SemiBold, fontSize = 16.sp)
                    Text(
                        text = "Condition: ${diagnose.disease_name}", 
                        fontWeight = FontWeight.Bold, 
                        color = statusColor,
                        fontSize = 16.sp
                    )
                    Text(text = "Confidence: ${(diagnose.confidence * 100).toInt()}%", style = MaterialTheme.typography.bodyMedium)
                    Text(text = "Severity: ${diagnose.severity}", style = MaterialTheme.typography.bodyMedium)
                    
                    if (!diagnose.treatment.isNullOrBlank()) {
                        Spacer(modifier = Modifier.height(4.dp))
                        Text(text = "💊 Chemical Treatment:", fontWeight = FontWeight.SemiBold)
                        Text(text = diagnose.treatment, style = MaterialTheme.typography.bodyMedium)
                    }
                    
                    if (!diagnose.organic_treatment.isNullOrBlank()) {
                        Spacer(modifier = Modifier.height(4.dp))
                        Text(text = "🌿 Organic Treatment:", fontWeight = FontWeight.SemiBold, color = GreenPrimary)
                        Text(text = diagnose.organic_treatment, style = MaterialTheme.typography.bodyMedium)
                    }
                    
                    if (!diagnose.ai_summary.isNullOrBlank()) {
                        Spacer(modifier = Modifier.height(4.dp))
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors = CardDefaults.cardColors(containerColor = GreenPrimary.copy(alpha = 0.08f)),
                            border = BorderStroke(1.dp, GreenPrimary.copy(alpha = 0.2f))
                        ) {
                            Column(modifier = Modifier.padding(12.dp)) {
                                Text(
                                    text = "✨ AI Analysis & Cures",
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 13.sp,
                                    color = GreenPrimary
                                )
                                Spacer(modifier = Modifier.height(4.dp))
                                Text(
                                    text = diagnose.ai_summary,
                                    fontSize = 14.sp,
                                    lineHeight = 20.sp
                                )
                            }
                        }
                    }
                    
                    Spacer(modifier = Modifier.height(8.dp))
                    
                    // Action buttons row
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = {
                                val prompt = if (diagnose.is_healthy) {
                                    "My ${diagnose.plant_name} crop is healthy. What seasonal care tips should I implement?"
                                } else {
                                    "My ${diagnose.plant_name} plant has been diagnosed with '${diagnose.disease_name}' with ${diagnose.severity} severity. How do I treat it organically and prevent it from spreading?"
                                }
                                onNavigateToChat(prompt)
                            },
                            modifier = Modifier.weight(1f),
                            colors = ButtonDefaults.buttonColors(containerColor = statusColor),
                            shape = RoundedCornerShape(8.dp)
                        ) {
                            Icon(imageVector = Icons.Default.Chat, contentDescription = "Ask Chat")
                            Spacer(modifier = Modifier.width(4.dp))
                            Text("Ask AI", fontSize = 13.sp)
                        }
                        
                        if (!diagnose.is_healthy) {
                            Button(
                                onClick = {
                                    val prompt = "What pesticides and fungicides should I use for '${diagnose.disease_name}' on ${diagnose.plant_name}? Include dosage, application method, and safety precautions."
                                    onNavigateToChat(prompt)
                                },
                                modifier = Modifier.weight(1f),
                                colors = ButtonDefaults.buttonColors(containerColor = AccentOrange),
                                shape = RoundedCornerShape(8.dp)
                            ) {
                                Icon(imageVector = Icons.Default.BugReport, contentDescription = "Pesticides")
                                Spacer(modifier = Modifier.width(4.dp))
                                Text("Pesticides", fontSize = 13.sp)
                            }
                        }
                    }
                }
            }
        }
    }
}
