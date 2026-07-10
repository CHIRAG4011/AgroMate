package com.agromate.app.ui.screens

import android.content.Context
import android.widget.Toast
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.agromate.app.data.NetworkConfig
import com.agromate.app.data.Localizer
import com.agromate.app.ui.theme.GreenPrimary

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val prefs = remember { context.getSharedPreferences("agromate_prefs", Context.MODE_PRIVATE) }
    
    var serverIp by remember { mutableStateOf(NetworkConfig.ipAddress) }
    var serverPort by remember { mutableStateOf(NetworkConfig.port) }
    var isPushEnabled by remember { mutableStateOf(prefs.getBoolean("enable_push", true)) }
    var isAutoSyncEnabled by remember { mutableStateOf(prefs.getBoolean("auto_sync", true)) }
    
    // Voice preferences
    var voiceEngine by remember { mutableStateOf(prefs.getString("voice_engine", "edge-tts") ?: "edge-tts") }
    var voiceGender by remember { mutableStateOf(prefs.getString("voice_gender", "female") ?: "female") }
    var voiceAutoplay by remember { mutableStateOf(prefs.getBoolean("voice_autoplay", true)) }
    
    var langExpanded by remember { mutableStateOf(false) }
    var engineExpanded by remember { mutableStateOf(false) }
    var genderExpanded by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Column {
            Text(text = Localizer.t("nav_settings"), style = MaterialTheme.typography.titleLarge)
            Text(text = "Manage server connection and application preferences.", style = MaterialTheme.typography.bodySmall)
        }

        // Language Select Dropdown Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp)
        ) {
            Column(modifier = Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(text = "🌐 Language Settings", fontWeight = FontWeight.Bold, fontSize = 16.sp)
                Text(text = "Choose interface & text-to-speech language", fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                
                Box(modifier = Modifier.fillMaxWidth()) {
                    val currentLangItem = Localizer.supportedLanguages.find { it.code == Localizer.currentLanguage }
                    val buttonLabel = currentLangItem?.let { "${it.native} (${it.name})" } ?: "Select Language"
                    
                    OutlinedButton(
                        onClick = { langExpanded = true },
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text(text = buttonLabel, color = GreenPrimary)
                    }

                    DropdownMenu(
                        expanded = langExpanded,
                        onDismissRequest = { langExpanded = false },
                        modifier = Modifier.fillMaxWidth(0.9f)
                    ) {
                        Localizer.supportedLanguages.forEach { lang ->
                            DropdownMenuItem(
                                text = { Text("${lang.native} (${lang.name})") },
                                onClick = {
                                    Localizer.currentLanguage = lang.code
                                    langExpanded = false
                                    Toast.makeText(context, "Language changed to ${lang.name}", Toast.LENGTH_SHORT).show()
                                }
                            )
                        }
                    }
                }
            }
        }

        // Voice and Audio Settings Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp)
        ) {
            Column(modifier = Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(text = "🔊 Voice Settings", fontWeight = FontWeight.Bold, fontSize = 16.sp)

                // Voice Engine Selection
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(text = "Voice Engine", fontWeight = FontWeight.Medium)
                        Text(text = "Edge-TTS or Gemini Multimodal AI", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                    Box {
                        OutlinedButton(
                            onClick = { engineExpanded = true },
                            shape = RoundedCornerShape(8.dp)
                        ) {
                            Text(text = if (voiceEngine == "gemini" || voiceEngine == "elevenlabs") "Gemini AI" else "Edge-TTS", color = GreenPrimary)
                        }
                        DropdownMenu(
                            expanded = engineExpanded,
                            onDismissRequest = { engineExpanded = false }
                        ) {
                            DropdownMenuItem(
                                text = { Text("Edge-TTS") },
                                onClick = {
                                    voiceEngine = "edge-tts"
                                    prefs.edit().putString("voice_engine", "edge-tts").apply()
                                    engineExpanded = false
                                }
                            )
                            DropdownMenuItem(
                                text = { Text("Gemini Multimodal AI") },
                                onClick = {
                                    voiceEngine = "gemini"
                                    prefs.edit().putString("voice_engine", "gemini").apply()
                                    engineExpanded = false
                                }
                            )
                        }
                    }
                }

                // Voice Gender Selection
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(text = "Voice Gender", fontWeight = FontWeight.Medium)
                        Text(text = "Select speech gender preference", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                    Box {
                        OutlinedButton(
                            onClick = { genderExpanded = true },
                            shape = RoundedCornerShape(8.dp)
                        ) {
                            Text(text = if (voiceGender == "male") "Male" else "Female", color = GreenPrimary)
                        }
                        DropdownMenu(
                            expanded = genderExpanded,
                            onDismissRequest = { genderExpanded = false }
                        ) {
                            DropdownMenuItem(
                                text = { Text("Female") },
                                onClick = {
                                    voiceGender = "female"
                                    prefs.edit().putString("voice_gender", "female").apply()
                                    genderExpanded = false
                                }
                            )
                            DropdownMenuItem(
                                text = { Text("Male") },
                                onClick = {
                                    voiceGender = "male"
                                    prefs.edit().putString("voice_gender", "male").apply()
                                    genderExpanded = false
                                }
                            )
                        }
                    }
                }

                // Voice Autoplay Switch
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(text = "Voice Auto-Play", fontWeight = FontWeight.Medium)
                        Text(text = "Automatically read scan results aloud", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                    Switch(
                        checked = voiceAutoplay,
                        onCheckedChange = {
                            voiceAutoplay = it
                            prefs.edit().putBoolean("voice_autoplay", it).apply()
                        }
                    )
                }
            }
        }

        // Server Connection Config Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp)
        ) {
            Column(modifier = Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
                Text(text = "Laptop AI Server Config", fontWeight = FontWeight.Bold, fontSize = 16.sp)
                
                OutlinedTextField(
                    value = serverIp,
                    onValueChange = { serverIp = it },
                    label = { Text(Localizer.t("server_ip")) },
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = serverPort,
                    onValueChange = { serverPort = it },
                    label = { Text(Localizer.t("server_port")) },
                    modifier = Modifier.fillMaxWidth()
                )

                Button(
                    onClick = {
                        NetworkConfig.save(context, serverIp, serverPort)
                        Toast.makeText(context, "Settings saved successfully", Toast.LENGTH_SHORT).show()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = GreenPrimary),
                    modifier = Modifier.align(Alignment.End)
                ) {
                    Text(text = Localizer.t("save_settings"))
                }
            }
        }

        // Preferences Card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp)
        ) {
            Column(modifier = Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(text = Localizer.t("app_preferences"), fontWeight = FontWeight.Bold, fontSize = 16.sp)
                
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(text = Localizer.t("enable_push"))
                    Switch(
                        checked = isPushEnabled,
                        onCheckedChange = {
                            isPushEnabled = it
                            prefs.edit().putBoolean("enable_push", it).apply()
                        }
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(text = Localizer.t("auto_sync"))
                    Switch(
                        checked = isAutoSyncEnabled,
                        onCheckedChange = {
                            isAutoSyncEnabled = it
                            prefs.edit().putBoolean("auto_sync", it).apply()
                        }
                    )
                }
            }
        }

        // Version card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp)
        ) {
            Column(modifier = Modifier.padding(16.dp).fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
                Text(text = "AgroMate App v2.0.0", fontWeight = FontWeight.Medium, fontSize = 14.sp)
                Text(text = "© 2026 AgroMate Team", style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}
