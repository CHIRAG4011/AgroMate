package com.agromate.app.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.runtime.*
import com.agromate.app.data.NetworkConfig
import com.agromate.app.data.remote.DBScanItem
import com.agromate.app.ui.theme.GreenPrimary
import com.agromate.app.ui.theme.RedAlert

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ReportsScreen(modifier: Modifier = Modifier, onBack: (() -> Unit)? = null) {
    val context = androidx.compose.ui.platform.LocalContext.current
    var reports by remember { mutableStateOf<List<DBScanItem>>(emptyList()) }
    var isLoading by remember { mutableStateOf(true) }
    var resolvedReportIds by remember { mutableStateOf(setOf<Int>()) }

    LaunchedEffect(Unit) {
        try {
            val apiService = NetworkConfig.getApiService()
            val response = apiService.getScans(scanType = "disease")
            if (response.status == "success") {
                reports = response.scans.filter { s ->
                    val d = s.diagnoses.firstOrNull()
                    d != null && !d.disease_name.isNullOrBlank() && d.disease_name.lowercase() != "healthy"
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        } finally {
            isLoading = false
        }
    }

    val activeReports = reports.filter { it.id !in resolvedReportIds }
    val resolvedReports = reports.filter { it.id in resolvedReportIds }

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
                Text(text = "Agronomic Health Reports", style = MaterialTheme.typography.titleLarge)
                Text(text = "Review active treatment and detailed diagnosis logs.", style = MaterialTheme.typography.bodySmall)
            }
        }

        // Active Reports Section
        if (isLoading) {
            Box(modifier = Modifier.fillMaxWidth().height(120.dp), contentAlignment = Alignment.Center) {
                CircularProgressIndicator(color = GreenPrimary)
            }
        } else if (activeReports.isEmpty()) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f))
            ) {
                Column(
                    modifier = Modifier.padding(24.dp).fillMaxWidth(),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text(text = "🌿 All Crops Healthy", fontWeight = FontWeight.Bold, fontSize = 18.sp, color = GreenPrimary)
                    Text(text = "No active agronomic disease threats detected in the system.", fontSize = 13.sp, color = Color.Gray)
                }
            }
        } else {
            activeReports.forEach { report ->
                val diagnosis = report.diagnoses.firstOrNull() ?: return@forEach
                val diseaseName = diagnosis.disease_name ?: "Unknown Disease"
                val plantName = diagnosis.plant_name ?: "Crop"
                val severity = diagnosis.severity ?: "unknown"

                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Column(modifier = Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(text = "Report #${report.id} - $diseaseName", fontWeight = FontWeight.Bold, fontSize = 17.sp)
                            Badge(containerColor = RedAlert) {
                                Text(text = "Action Needed", modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp), color = Color.White)
                            }
                        }
                        
                        Divider()
                        
                        Text(text = "Target Host: $plantName", fontWeight = FontWeight.SemiBold)
                        Text(
                            text = "Severity: ${severity.replaceFirstChar { it.uppercase() }}. Confidence: ${String.format("%.1f", diagnosis.confidence * 100)}%",
                            style = MaterialTheme.typography.bodyMedium
                        )
                        
                        if (!diagnosis.treatment.isNullOrBlank() || !diagnosis.organic_treatment.isNullOrBlank()) {
                            Text(text = "Recommended Treatments:", fontWeight = FontWeight.SemiBold, color = GreenPrimary)
                            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                if (!diagnosis.organic_treatment.isNullOrBlank()) {
                                    Text(text = "• Organic: ${diagnosis.organic_treatment}", fontSize = 13.sp)
                                }
                                if (!diagnosis.treatment.isNullOrBlank()) {
                                    Text(text = "• Chemical: ${diagnosis.treatment}", fontSize = 13.sp)
                                }
                            }
                        }

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Button(
                                onClick = { resolvedReportIds = resolvedReportIds + report.id },
                                colors = ButtonDefaults.buttonColors(containerColor = GreenPrimary),
                                shape = RoundedCornerShape(8.dp),
                                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp),
                                modifier = Modifier.height(32.dp)
                            ) {
                                Text(text = "Mark Resolved", color = Color.White, fontSize = 11.sp)
                            }

                            Row(verticalAlignment = Alignment.CenterVertically) {
                                IconButton(onClick = {
                                    val shareIntent = android.content.Intent().apply {
                                        action = android.content.Intent.ACTION_SEND
                                        putExtra(
                                            android.content.Intent.EXTRA_TEXT,
                                            "AgroMate Health Report #${report.id}\nTarget Host: $plantName\nCondition: $diseaseName\nSeverity: $severity\nOrganic Treatment: ${diagnosis.organic_treatment ?: "None"}"
                                        )
                                        type = "text/plain"
                                    }
                                    context.startActivity(android.content.Intent.createChooser(shareIntent, "Share Report via"))
                                }) {
                                    Icon(imageVector = Icons.Default.Share, contentDescription = "Share")
                                }

                                IconButton(onClick = {
                                    try {
                                        val dir = context.getExternalFilesDir(android.os.Environment.DIRECTORY_DOWNLOADS)
                                        val file = java.io.File(dir, "AgroMate_Report_${report.id}.txt")
                                        file.writeText("AgroMate Health Report #${report.id}\nTarget Host: $plantName\nCondition: $diseaseName\nSeverity: $severity\nConfidence: ${String.format("%.1f", diagnosis.confidence * 100)}%\nOrganic Treatment: ${diagnosis.organic_treatment ?: "None"}\nChemical Treatment: ${diagnosis.treatment ?: "None"}")
                                        android.widget.Toast.makeText(context, "Saved to Downloads: ${file.name}", android.widget.Toast.LENGTH_LONG).show()
                                    } catch (e: Exception) {
                                        android.widget.Toast.makeText(context, "Download failed: ${e.message}", android.widget.Toast.LENGTH_SHORT).show()
                                    }
                                }) {
                                    Icon(imageVector = Icons.Default.Download, contentDescription = "Download")
                                }
                            }
                        }
                    }
                }
            }
        }

        // Treatment Log Summary
        Text(text = "Resolved Issues Log", fontWeight = FontWeight.Bold, fontSize = 16.sp)
        
        if (resolvedReports.isEmpty()) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(12.dp)
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(text = "Maize - Common Rust", fontWeight = FontWeight.Bold)
                        Text(text = "Treatment completed on 2026-06-18", style = MaterialTheme.typography.bodySmall)
                    }
                    Text(text = "Resolved", color = GreenPrimary, fontWeight = FontWeight.Bold)
                }
            }
        } else {
            resolvedReports.forEach { report ->
                val diagnosis = report.diagnoses.firstOrNull() ?: return@forEach
                val diseaseName = diagnosis.disease_name ?: "Unknown Disease"
                val plantName = diagnosis.plant_name ?: "Crop"

                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(bottom = 8.dp),
                    shape = RoundedCornerShape(12.dp)
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column {
                            Text(text = "$plantName - $diseaseName", fontWeight = FontWeight.Bold)
                            Text(text = "Resolved (ID: #${report.id})", style = MaterialTheme.typography.bodySmall)
                        }
                        Button(
                            onClick = { resolvedReportIds = resolvedReportIds - report.id },
                            colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondaryContainer),
                            shape = RoundedCornerShape(8.dp),
                            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp),
                            modifier = Modifier.height(32.dp)
                        ) {
                            Text(text = "Reopen", color = MaterialTheme.colorScheme.onSecondaryContainer, fontSize = 11.sp)
                        }
                    }
                }
            }
        }
    }
}
