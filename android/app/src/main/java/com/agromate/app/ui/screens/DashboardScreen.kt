package com.agromate.app.ui.screens

import androidx.compose.animation.core.*
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Analytics
import androidx.compose.material.icons.filled.Cloud
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.Grass
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.PrecisionManufacturing
import androidx.compose.material.icons.filled.Thermostat
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material.icons.filled.MenuBook
import androidx.compose.material.icons.filled.Chat
import androidx.compose.material.icons.filled.PhotoCamera
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.text.style.TextAlign
import com.agromate.app.Screen
import com.agromate.app.data.Localizer
import com.agromate.app.data.NetworkConfig
import com.agromate.app.ui.theme.AccentOrange
import com.agromate.app.ui.theme.RedAlert
import com.agromate.app.ui.theme.GreenPrimary
import com.agromate.app.ui.theme.GreenSecondary

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DashboardScreen(
    modifier: Modifier = Modifier,
    onNavigate: (Screen) -> Unit
) {
    var scansCount by remember { mutableStateOf("—") }
    var robotsCount by remember { mutableStateOf("—") }
    var diseasesCount by remember { mutableStateOf("—") }
    var chatsCount by remember { mutableStateOf("—") }
 
    LaunchedEffect(Unit) {
        try {
            val apiService = NetworkConfig.getApiService()
            val analyticsResponse = apiService.getAnalytics(30)
            scansCount = analyticsResponse.database.total_scans.toString()
            diseasesCount = analyticsResponse.database.total_diagnoses.toString()
            chatsCount = analyticsResponse.database.total_chats.toString()
        } catch (e: Exception) {
            e.printStackTrace()
            scansCount = "0"
            diseasesCount = "0"
            chatsCount = "0"
        }
 
        try {
            val apiService = NetworkConfig.getApiService()
            val robots = apiService.getRobots()
            robotsCount = robots.robots.size.toString()
        } catch (e: Exception) {
            e.printStackTrace()
            robotsCount = "0"
        }
    }

    // Infinite transitions for modern animations
    val infiniteTransition = rememberInfiniteTransition(label = "DashboardAnimation")
    
    // Shifting color shift for premium gradient
    val colorShift by infiniteTransition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 3000, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "colorShift"
    )

    // Pulse animation for badges / alerts
    val pulseAlpha by infiniteTransition.animateFloat(
        initialValue = 0.4f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 900, easing = EaseInOutSine),
            repeatMode = RepeatMode.Reverse
        ),
        label = "pulseAlpha"
    )

    // Scan line animation for the drone video HUD overlay
    val scanProgress by infiniteTransition.animateFloat(
        initialValue = 0.05f,
        targetValue = 0.95f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 2000, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "scanProgress"
    )

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Premium Animated Header Card
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(140.dp)
                .clip(RoundedCornerShape(24.dp))
                .background(
                    Brush.horizontalGradient(
                        colors = listOf(
                            GreenPrimary.copy(alpha = 1f - (colorShift * 0.15f)),
                            GreenSecondary.copy(alpha = 0.8f + (colorShift * 0.2f))
                        )
                    )
                )
                .padding(24.dp),
            contentAlignment = Alignment.CenterStart
        ) {
            Column {
                Text(
                    text = if (Localizer.currentLanguage == "hi") "नमस्ते, किसान भाई!" else if (Localizer.currentLanguage == "gu") "નમસ્તે, ખેડૂત મિત્ર!" else "Welcome, Farmer!",
                    color = Color.White,
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold
                )
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = Localizer.t("welcome"),
                    color = Color.White.copy(alpha = 0.85f),
                    fontSize = 14.sp
                )
            }
        }

        // Live Statistics Widgets (2x2 Grid matching Web UI)
        Column(
            verticalArrangement = Arrangement.spacedBy(16.dp),
            modifier = Modifier.fillMaxWidth()
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Card 1: Total Scans
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .height(100.dp)
                        .clickable { onNavigate(Screen.ScanHistory) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, GreenPrimary.copy(alpha = 0.15f))
                ) {
                    Box(modifier = Modifier.fillMaxSize()) {
                        Box(modifier = Modifier.fillMaxHeight().width(5.dp).background(GreenPrimary).align(Alignment.CenterStart))
                        Column(
                            modifier = Modifier.fillMaxSize().padding(12.dp),
                            verticalArrangement = Arrangement.SpaceBetween
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(32.dp)
                                    .clip(RoundedCornerShape(8.dp))
                                    .background(GreenPrimary.copy(alpha = 0.12f)),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Analytics,
                                    contentDescription = "Scans",
                                    tint = GreenPrimary,
                                    modifier = Modifier.size(18.dp)
                                )
                            }
                            Column {
                                Text(
                                    text = Localizer.t("dashboard_total_scans").ifBlank { "Total Scans" },
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                                    fontSize = 11.sp
                                )
                                Text(
                                    text = scansCount,
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 20.sp,
                                    color = GreenPrimary
                                )
                            }
                        }
                    }
                }

                // Card 2: Connected Robots
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .height(100.dp)
                        .clickable { onNavigate(Screen.RobotManager) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, GreenSecondary.copy(alpha = 0.15f))
                ) {
                    Box(modifier = Modifier.fillMaxSize()) {
                        Box(modifier = Modifier.fillMaxHeight().width(5.dp).background(GreenSecondary).align(Alignment.CenterStart))
                        Column(
                            modifier = Modifier.fillMaxSize().padding(12.dp),
                            verticalArrangement = Arrangement.SpaceBetween
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(32.dp)
                                    .clip(RoundedCornerShape(8.dp))
                                    .background(GreenSecondary.copy(alpha = 0.12f)),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    imageVector = Icons.Default.PrecisionManufacturing,
                                    contentDescription = "Robots",
                                    tint = GreenSecondary,
                                    modifier = Modifier.size(18.dp)
                                )
                            }
                            Column {
                                Text(
                                    text = Localizer.t("dashboard_connected_robots").ifBlank { "Connected Robots" },
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                                    fontSize = 11.sp
                                )
                                Text(
                                    text = robotsCount,
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 20.sp,
                                    color = GreenSecondary
                                )
                            }
                        }
                    }
                }
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Card 3: Diseases Detected
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .height(100.dp)
                        .clickable { onNavigate(Screen.Reports) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, RedAlert.copy(alpha = 0.15f))
                ) {
                    Box(modifier = Modifier.fillMaxSize()) {
                        Box(modifier = Modifier.fillMaxHeight().width(5.dp).background(RedAlert).align(Alignment.CenterStart))
                        Column(
                            modifier = Modifier.fillMaxSize().padding(12.dp),
                            verticalArrangement = Arrangement.SpaceBetween
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(32.dp)
                                    .clip(RoundedCornerShape(8.dp))
                                    .background(RedAlert.copy(alpha = 0.12f)),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Warning,
                                    contentDescription = "Diseases",
                                    tint = RedAlert,
                                    modifier = Modifier.size(18.dp)
                                )
                            }
                            Column {
                                Text(
                                    text = Localizer.t("dashboard_diseases_detected").ifBlank { "Diseases Detected" },
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                                    fontSize = 11.sp
                                )
                                Text(
                                    text = diseasesCount,
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 20.sp,
                                    color = RedAlert
                                )
                            }
                        }
                    }
                }

                // Card 4: AI Conversations
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .height(100.dp)
                        .clickable { onNavigate(Screen.AIChat) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, AccentOrange.copy(alpha = 0.15f))
                ) {
                    Box(modifier = Modifier.fillMaxSize()) {
                        Box(modifier = Modifier.fillMaxHeight().width(5.dp).background(AccentOrange).align(Alignment.CenterStart))
                        Column(
                            modifier = Modifier.fillMaxSize().padding(12.dp),
                            verticalArrangement = Arrangement.SpaceBetween
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(32.dp)
                                    .clip(RoundedCornerShape(8.dp))
                                    .background(AccentOrange.copy(alpha = 0.12f)),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Chat,
                                    contentDescription = "Chats",
                                    tint = AccentOrange,
                                    modifier = Modifier.size(18.dp)
                                )
                            }
                            Column {
                                Text(
                                    text = Localizer.t("dashboard_conversations").ifBlank { "AI Conversations" },
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                                    fontSize = 11.sp
                                )
                                Text(
                                    text = chatsCount,
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 20.sp,
                                    color = AccentOrange
                                )
                            }
                        }
                    }
                }
            }
        }

        // Animated Threat Alert Warning Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.errorContainer.copy(alpha = 0.12f + (pulseAlpha * 0.08f))
            ),
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.error.copy(alpha = 0.2f + (pulseAlpha * 0.1f)))
        ) {
            Row(
                modifier = Modifier.padding(16.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Warning,
                    contentDescription = "Alert",
                    tint = MaterialTheme.colorScheme.error.copy(alpha = pulseAlpha)
                )
                Column {
                    Text(
                        text = Localizer.t("alert_title") + " / Warning",
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.error
                    )
                    Text(
                        text = "Nearby farms reported rust leaf disease. Keep scanning fields regularly.",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onErrorContainer
                    )
                }
            }
        }

        // Live Telemetry Video Stream Card (ExoPlayer Loop)
        Text(
            text = Localizer.t("live_farm_telemetry"),
            fontWeight = FontWeight.Bold,
            fontSize = 18.sp,
            modifier = Modifier.padding(top = 8.dp)
        )
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .height(200.dp),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = Color.Black),
            border = BorderStroke(1.dp, Color(0xFF00E676).copy(alpha = 0.3f))
        ) {
            Box(modifier = Modifier.fillMaxSize()) {
                AndroidView(
                    factory = { ctx ->
                        android.widget.VideoView(ctx).apply {
                            layoutParams = android.view.ViewGroup.LayoutParams(
                                android.view.ViewGroup.LayoutParams.MATCH_PARENT,
                                android.view.ViewGroup.LayoutParams.MATCH_PARENT
                            )
                            setVideoPath("https://assets.mixkit.co/videos/preview/mixkit-agriculture-drone-flying-over-a-green-field-42411-large.mp4")
                            setOnPreparedListener { mp ->
                                mp.isLooping = true
                                mp.setVolume(0f, 0f) // Keep muted for dashboard view
                                start()
                            }
                            setOnErrorListener { mp, what, extra ->
                                // Fallback to simpler codec profile video on error
                                setVideoPath("https://www.w3schools.com/html/mov_bbb.mp4")
                                start()
                                true
                            }
                        }
                    },
                    modifier = Modifier.fillMaxSize()
                )

                // Drone HUD telemetry overlay
                Canvas(modifier = Modifier.fillMaxSize()) {
                    val w = size.width
                    val h = size.height
                    val centerPt = center
                    
                    // Crosshairs in the center
                    val chSize = 30f
                    drawLine(Color(0x8000FF00), centerPt - androidx.compose.ui.geometry.Offset(chSize, 0f), centerPt + androidx.compose.ui.geometry.Offset(chSize, 0f), strokeWidth = 2f)
                    drawLine(Color(0x8000FF00), centerPt - androidx.compose.ui.geometry.Offset(0f, chSize), centerPt + androidx.compose.ui.geometry.Offset(0f, chSize), strokeWidth = 2f)
                    drawCircle(Color(0x8000FF00), radius = 10f, center = centerPt, style = androidx.compose.ui.graphics.drawscope.Stroke(width = 2f))
                    
                    // Corner brackets
                    val bracketLen = 25f
                    val bracketOffset = 15f
                    // Top-Left
                    drawLine(Color(0xFF00E676), androidx.compose.ui.geometry.Offset(bracketOffset, bracketOffset), androidx.compose.ui.geometry.Offset(bracketOffset + bracketLen, bracketOffset), strokeWidth = 3f)
                    drawLine(Color(0xFF00E676), androidx.compose.ui.geometry.Offset(bracketOffset, bracketOffset), androidx.compose.ui.geometry.Offset(bracketOffset, bracketOffset + bracketLen), strokeWidth = 3f)
                    // Top-Right
                    drawLine(Color(0xFF00E676), androidx.compose.ui.geometry.Offset(w - bracketOffset, bracketOffset), androidx.compose.ui.geometry.Offset(w - bracketOffset - bracketLen, bracketOffset), strokeWidth = 3f)
                    drawLine(Color(0xFF00E676), androidx.compose.ui.geometry.Offset(w - bracketOffset, bracketOffset), androidx.compose.ui.geometry.Offset(w - bracketOffset, bracketOffset + bracketLen), strokeWidth = 3f)
                    // Bottom-Left
                    drawLine(Color(0xFF00E676), androidx.compose.ui.geometry.Offset(bracketOffset, h - bracketOffset), androidx.compose.ui.geometry.Offset(bracketOffset + bracketLen, h - bracketOffset), strokeWidth = 3f)
                    drawLine(Color(0xFF00E676), androidx.compose.ui.geometry.Offset(bracketOffset, h - bracketOffset), androidx.compose.ui.geometry.Offset(bracketOffset, h - bracketOffset - bracketLen), strokeWidth = 3f)
                    // Bottom-Right
                    drawLine(Color(0xFF00E676), androidx.compose.ui.geometry.Offset(w - bracketOffset, h - bracketOffset), androidx.compose.ui.geometry.Offset(w - bracketOffset - bracketLen, h - bracketOffset), strokeWidth = 3f)
                    drawLine(Color(0xFF00E676), androidx.compose.ui.geometry.Offset(w - bracketOffset, h - bracketOffset), androidx.compose.ui.geometry.Offset(w - bracketOffset, h - bracketOffset - bracketLen), strokeWidth = 3f)

                    // Laser scan line animating vertically
                    val laserY = h * scanProgress
                    drawLine(
                        color = Color(0xAA00FF00),
                        start = androidx.compose.ui.geometry.Offset(bracketOffset, laserY),
                        end = androidx.compose.ui.geometry.Offset(w - bracketOffset, laserY),
                        strokeWidth = 3f
                    )
                }

                // HUD Text Details
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .align(Alignment.TopStart)
                        .padding(12.dp),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Column {
                        Text(text = "CAM: AGROMATE_DRONE_03", color = Color(0xFF00E676), fontSize = 10.sp, fontWeight = FontWeight.Bold, style = androidx.compose.ui.text.TextStyle(shadow = androidx.compose.ui.graphics.Shadow(Color.Black, blurRadius = 4f)))
                        Text(text = "ALT: 15.4 M  SPEED: 4.8 M/S", color = Color(0xFF00E676), fontSize = 9.sp, style = androidx.compose.ui.text.TextStyle(shadow = androidx.compose.ui.graphics.Shadow(Color.Black, blurRadius = 4f)))
                    }
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        Box(
                            modifier = Modifier
                                .size(6.dp)
                                .clip(androidx.compose.foundation.shape.CircleShape)
                                .background(if (pulseAlpha > 0.6f) Color.Red else Color.Transparent)
                        )
                        Text(text = "LIVE", color = Color.White, fontSize = 9.sp, fontWeight = FontWeight.Bold, style = androidx.compose.ui.text.TextStyle(shadow = androidx.compose.ui.graphics.Shadow(Color.Black, blurRadius = 4f)))
                    }
                }

                Column(
                    modifier = Modifier
                        .align(Alignment.BottomStart)
                        .padding(12.dp)
                ) {
                    Text(text = "GPS: 28.6139° N, 77.2090° E", color = Color(0xFF00E676), fontSize = 9.sp, style = androidx.compose.ui.text.TextStyle(shadow = androidx.compose.ui.graphics.Shadow(Color.Black, blurRadius = 4f)))
                    Text(text = "SIGNAL: EXCELLENT (-58 dBm)", color = Color(0xFF00E676), fontSize = 9.sp, style = androidx.compose.ui.text.TextStyle(shadow = androidx.compose.ui.graphics.Shadow(Color.Black, blurRadius = 4f)))
                }
            }
        }

        // Quick Actions (Matching Web UI)
        Text(
            text = Localizer.t("quick_actions"),
            fontWeight = FontWeight.Bold,
            fontSize = 18.sp,
            modifier = Modifier.padding(top = 8.dp)
        )

        Column(
            verticalArrangement = Arrangement.spacedBy(16.dp),
            modifier = Modifier.fillMaxWidth()
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Scan & Diagnose
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .height(115.dp)
                        .clickable { onNavigate(Screen.Scan) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.1f))
                ) {
                    Column(
                        modifier = Modifier.fillMaxSize().padding(12.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center
                    ) {
                        Box(
                            modifier = Modifier
                                .size(48.dp)
                                .clip(RoundedCornerShape(12.dp))
                                .background(GreenPrimary.copy(alpha = 0.12f)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.PhotoCamera,
                                contentDescription = "Scan & Diagnose",
                                tint = GreenPrimary,
                                modifier = Modifier.size(24.dp)
                            )
                        }
                        Spacer(modifier = Modifier.height(6.dp))
                        Text(
                            text = Localizer.t("nav_scan_diagnose").ifBlank { "Scan & Diagnose" },
                            fontWeight = FontWeight.SemiBold,
                            fontSize = 12.sp,
                            textAlign = TextAlign.Center,
                            maxLines = 2,
                            lineHeight = 15.sp
                        )
                    }
                }

                // AI Chat
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .height(115.dp)
                        .clickable { onNavigate(Screen.AIChat) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.1f))
                ) {
                    Column(
                        modifier = Modifier.fillMaxSize().padding(12.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center
                    ) {
                        Box(
                            modifier = Modifier
                                .size(48.dp)
                                .clip(RoundedCornerShape(12.dp))
                                .background(AccentOrange.copy(alpha = 0.12f)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.Chat,
                                contentDescription = "AI Chat",
                                tint = AccentOrange,
                                modifier = Modifier.size(24.dp)
                            )
                        }
                        Spacer(modifier = Modifier.height(6.dp))
                        Text(
                            text = Localizer.t("nav_chatbot").ifBlank { "AI Chat" },
                            fontWeight = FontWeight.SemiBold,
                            fontSize = 12.sp,
                            textAlign = TextAlign.Center,
                            maxLines = 2,
                            lineHeight = 15.sp
                        )
                    }
                }
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Weather
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .height(115.dp)
                        .clickable { onNavigate(Screen.Weather) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.1f))
                ) {
                    Column(
                        modifier = Modifier.fillMaxSize().padding(12.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center
                    ) {
                        Box(
                            modifier = Modifier
                                .size(48.dp)
                                .clip(RoundedCornerShape(12.dp))
                                .background(GreenSecondary.copy(alpha = 0.12f)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.Cloud,
                                contentDescription = "Weather",
                                tint = GreenSecondary,
                                modifier = Modifier.size(24.dp)
                            )
                        }
                        Spacer(modifier = Modifier.height(6.dp))
                        Text(
                            text = Localizer.t("nav_weather").ifBlank { "Weather" },
                            fontWeight = FontWeight.SemiBold,
                            fontSize = 12.sp,
                            textAlign = TextAlign.Center,
                            maxLines = 2,
                            lineHeight = 15.sp
                        )
                    }
                }

                // Knowledge Base
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .height(115.dp)
                        .clickable { onNavigate(Screen.Knowledge) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.1f))
                ) {
                    Column(
                        modifier = Modifier.fillMaxSize().padding(12.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center
                    ) {
                        Box(
                            modifier = Modifier
                                .size(48.dp)
                                .clip(RoundedCornerShape(12.dp))
                                .background(Color(0xFF81C784).copy(alpha = 0.12f)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.MenuBook,
                                contentDescription = "Knowledge Base",
                                tint = Color(0xFF81C784),
                                modifier = Modifier.size(24.dp)
                            )
                        }
                        Spacer(modifier = Modifier.height(6.dp))
                        Text(
                            text = "Knowledge Base",
                            fontWeight = FontWeight.SemiBold,
                            fontSize = 12.sp,
                            textAlign = TextAlign.Center,
                            maxLines = 2,
                            lineHeight = 15.sp
                        )
                    }
                }
            }
        }

        // Control Center
        Text(
            text = "Robot & Data Control",
            fontWeight = FontWeight.Bold,
            fontSize = 18.sp,
            modifier = Modifier.padding(top = 8.dp)
        )

        Column(
            verticalArrangement = Arrangement.spacedBy(16.dp),
            modifier = Modifier.fillMaxWidth()
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Robot Manager
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .clickable { onNavigate(Screen.RobotManager) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.1f))
                ) {
                    Column(
                        modifier = Modifier.padding(16.dp).fillMaxWidth(),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .size(48.dp)
                                .clip(RoundedCornerShape(12.dp))
                                .background(GreenPrimary.copy(alpha = 0.12f)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.PrecisionManufacturing,
                                contentDescription = "Robots",
                                tint = GreenPrimary,
                                modifier = Modifier.size(24.dp)
                            )
                        }
                        Text(Localizer.t("nav_robots").ifBlank { "Robot Manager" }, fontWeight = FontWeight.SemiBold, fontSize = 13.sp)
                    }
                }

                // Reports
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .clickable { onNavigate(Screen.Reports) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.1f))
                ) {
                    Column(
                        modifier = Modifier.padding(16.dp).fillMaxWidth(),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .size(48.dp)
                                .clip(RoundedCornerShape(12.dp))
                                .background(AccentOrange.copy(alpha = 0.12f)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.Description,
                                contentDescription = "Reports",
                                tint = AccentOrange,
                                modifier = Modifier.size(24.dp)
                            )
                        }
                        Text(Localizer.t("nav_analytics") + " / Reports", fontWeight = FontWeight.SemiBold, fontSize = 13.sp)
                    }
                }
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Scan History
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .clickable { onNavigate(Screen.ScanHistory) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.1f))
                ) {
                    Column(
                        modifier = Modifier.padding(16.dp).fillMaxWidth(),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .size(48.dp)
                                .clip(RoundedCornerShape(12.dp))
                                .background(Color(0xFF2196F3).copy(alpha = 0.12f)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.History,
                                contentDescription = "History",
                                tint = Color(0xFF2196F3),
                                modifier = Modifier.size(24.dp)
                            )
                        }
                        Text(Localizer.t("nav_history").ifBlank { "Scan History" }, fontWeight = FontWeight.SemiBold, fontSize = 13.sp)
                    }
                }

                // Analytics
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .clickable { onNavigate(Screen.Analytics) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.1f))
                ) {
                    Column(
                        modifier = Modifier.padding(16.dp).fillMaxWidth(),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .size(48.dp)
                                .clip(RoundedCornerShape(12.dp))
                                .background(Color(0xFF9C27B0).copy(alpha = 0.12f)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.Analytics,
                                contentDescription = "Analytics",
                                tint = Color(0xFF9C27B0),
                                modifier = Modifier.size(24.dp)
                            )
                        }
                        Text(Localizer.t("nav_analytics").ifBlank { "Weekly Analytics" }, fontWeight = FontWeight.SemiBold, fontSize = 13.sp)
                    }
                }
            }
        }

        // Connected Robot Telemetry Status Box
        Text(
            text = Localizer.t("active_robot"),
            fontWeight = FontWeight.Bold,
            fontSize = 18.sp,
            modifier = Modifier.padding(top = 8.dp)
        )

        val targetProgress = remember { mutableStateOf(0f) }
        LaunchedEffect(Unit) {
            targetProgress.value = 0.85f
        }
        val progressAnim by animateFloatAsState(
            targetValue = targetProgress.value,
            animationSpec = tween(durationMillis = 1500, easing = FastOutSlowInEasing),
            label = "RobotBatteryProgress"
        )

        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
            border = BorderStroke(1.dp, GreenPrimary.copy(alpha = 0.15f))
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(12.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            imageVector = Icons.Default.Grass,
                            contentDescription = "Robot icon",
                            tint = GreenPrimary
                        )
                        Column {
                            Text(text = "AgroMate-Robot-1", fontWeight = FontWeight.Bold)
                            Text(text = "Connected (BLE)", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f))
                        }
                    }
                    Badge(
                        containerColor = GreenPrimary.copy(alpha = pulseAlpha)
                    ) {
                        Text(
                            text = "Active",
                            modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp),
                            color = Color.White
                        )
                    }
                }
                Spacer(modifier = Modifier.height(16.dp))
                LinearProgressIndicator(
                    progress = progressAnim,
                    modifier = Modifier.fillMaxWidth().height(8.dp).clip(RoundedCornerShape(4.dp)),
                    color = GreenPrimary,
                    trackColor = GreenPrimary.copy(alpha = 0.15f)
                )
                Spacer(modifier = Modifier.height(8.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text(text = "${Localizer.t("battery_level")}: 85%", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f))
                    Text(text = "Signal: Excellent (-62dBm)", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f))
                }
            }
        }
    }
}
