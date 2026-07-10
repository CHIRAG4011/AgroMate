package com.agromate.app.ui.screens

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import com.agromate.app.data.Localizer
import com.agromate.app.ui.theme.GreenPrimary
import com.agromate.app.ui.theme.GreenSecondary
import com.agromate.app.ui.theme.RedAlert

@Composable
fun AnalyticsScreen(modifier: Modifier = Modifier, onBack: (() -> Unit)? = null) {
    val animProgress = remember { Animatable(0f) }
    LaunchedEffect(Unit) {
        animProgress.animateTo(
            targetValue = 1f,
            animationSpec = tween(durationMillis = 1400, easing = FastOutSlowInEasing)
        )
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            if (onBack != null) {
                IconButton(onClick = onBack) {
                    Icon(imageVector = Icons.Default.ArrowBack, contentDescription = "Back")
                }
            }
            Column {
                Text(text = Localizer.t("nav_analytics"), style = MaterialTheme.typography.titleLarge)
                Text(text = "Overview of scanning and diagnostics activity.", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f))
            }
        }

        // Stats Summary Cards
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Card(
                modifier = Modifier
                    .weight(1f)
                    .padding(vertical = 4.dp),
                shape = RoundedCornerShape(16.dp),
                border = BorderStroke(1.dp, GreenPrimary.copy(alpha = 0.15f)),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f))
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(text = "Total Scans", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f))
                    Text(text = "142", fontWeight = FontWeight.Bold, fontSize = 28.sp, color = GreenPrimary)
                }
            }

            Card(
                modifier = Modifier
                    .weight(1f)
                    .padding(vertical = 4.dp),
                shape = RoundedCornerShape(16.dp),
                border = BorderStroke(1.dp, GreenSecondary.copy(alpha = 0.15f)),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f))
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(text = "Healthy Crops", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f))
                    Text(text = "78%", fontWeight = FontWeight.Bold, fontSize = 28.sp, color = GreenSecondary)
                }
            }
        }

        // Disease Distribution Chart Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(20.dp),
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f)),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f))
        ) {
            Column(modifier = Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
                Text(text = "Diagnosed Disease Distribution", fontWeight = FontWeight.Bold, fontSize = 16.sp)
                
                // Horizontal bar chart items
                Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    DiseaseBar(name = "Late Blight", percentage = 35f, color = RedAlert, progress = animProgress.value)
                    DiseaseBar(name = "Common Rust", percentage = 25f, color = Color(0xFFFF9F1C), progress = animProgress.value)
                    DiseaseBar(name = "Leaf Spot", percentage = 20f, color = Color(0xFF00E5FF), progress = animProgress.value)
                    DiseaseBar(name = "Healthy / Clear", percentage = 20f, color = GreenSecondary, progress = animProgress.value)
                }
            }
        }

        // Scanning trends weekly card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(20.dp),
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f)),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f))
        ) {
            Column(modifier = Modifier.padding(20.dp)) {
                Text(text = "Weekly Scan Volume", fontWeight = FontWeight.Bold, fontSize = 16.sp)
                Spacer(modifier = Modifier.height(16.dp))
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(110.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.Bottom
                ) {
                    listOf(15, 25, 10, 42, 30, 20, 50).forEachIndexed { index, height ->
                        val day = when (index) {
                            0 -> "M"
                            1 -> "T"
                            2 -> "W"
                            3 -> "T"
                            4 -> "F"
                            5 -> "S"
                            6 -> "S"
                            else -> ""
                        }
                        Column(
                            horizontalAlignment = Alignment.CenterHorizontally,
                            verticalArrangement = Arrangement.spacedBy(4.dp)
                        ) {
                            val barHeight = height * 1.6 * animProgress.value
                            Box(
                                modifier = Modifier
                                    .width(16.dp)
                                    .height(barHeight.dp)
                                    .clip(RoundedCornerShape(topStart = 4.dp, topEnd = 4.dp))
                                    .background(GreenPrimary)
                            )
                            Text(text = day, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f))
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun DiseaseBar(name: String, percentage: Float, color: Color, progress: Float) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text(text = name, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.8f))
            Text(text = "${(percentage * progress).toInt()}%", fontWeight = FontWeight.Bold)
        }
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(8.dp)
                .clip(RoundedCornerShape(4.dp))
                .background(MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f))
        ) {
            Box(
                modifier = Modifier
                    .fillMaxHeight()
                    .fillMaxWidth((percentage / 100f) * progress)
                    .background(color)
            )
        }
    }
}
