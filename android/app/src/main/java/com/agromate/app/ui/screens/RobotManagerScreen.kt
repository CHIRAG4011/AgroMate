package com.agromate.app.ui.screens

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.BluetoothSearching
import androidx.compose.material.icons.filled.Camera
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.SignalCellularAlt
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.material.icons.filled.ArrowBack
import com.agromate.app.ui.theme.AccentOrange
import com.agromate.app.ui.theme.GreenPrimary
import com.agromate.app.data.NetworkConfig
import com.agromate.app.data.remote.RobotSyncRequest
import kotlinx.coroutines.launch

data class RobotDevice(val id: String, val name: String, val macAddress: String, val rssi: Int, val isConnected: Boolean = false)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RobotManagerScreen(modifier: Modifier = Modifier, onBack: (() -> Unit)? = null) {
    var isScanning by remember { mutableStateOf(false) }
    var devices by remember { mutableStateOf(listOf(
        RobotDevice("1", "AgroMate-Robot-1", "A6:AD:01:23:45:67", -65, true),
        RobotDevice("2", "AgroMate-Robot-2", "A6:AD:01:89:AB:CD", -82),
        RobotDevice("3", "ESP32-Camera-Bot", "34:85:18:9B:4C:E2", -90)
    )) }
 
    val coroutineScope = rememberCoroutineScope()

    var showConfigDialog by remember { mutableStateOf(false) }
    var selectedDeviceForConfig by remember { mutableStateOf<RobotDevice?>(null) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Scanning Header
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
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
                    Text(text = "BLE Robot Discovery", fontWeight = FontWeight.Bold, fontSize = 20.sp)
                    Text(text = "Connect and configure AgroMate client nodes", style = MaterialTheme.typography.bodySmall)
                }
            }
            IconButton(
                onClick = { 
                    isScanning = !isScanning 
                    // Simulate scan additions
                    if (isScanning) {
                        devices = devices.toMutableList().apply {
                            if (none { it.id == "4" }) {
                                add(RobotDevice("4", "AgroMate-Robot-Beta", "B6:C3:FF:11:22:33", -78))
                            }
                        }
                    }
                },
                colors = IconButtonDefaults.iconButtonColors(
                    containerColor = if (isScanning) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant
                )
            ) {
                Icon(
                    imageVector = if (isScanning) Icons.Default.BluetoothSearching else Icons.Default.Refresh,
                    contentDescription = "Scan status",
                    tint = if (isScanning) GreenPrimary else MaterialTheme.colorScheme.onSurface
                )
            }
        }

        if (isScanning) {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth().height(6.dp).clip(RoundedCornerShape(3.dp)), color = GreenPrimary)
        }

        // Robot List
        LazyColumn(
            verticalArrangement = Arrangement.spacedBy(12.dp),
            modifier = Modifier.weight(1f)
        ) {
            items(devices) { device ->
                val cardBorder = if (device.isConnected) {
                    BorderStroke(1.5.dp, GreenPrimary)
                } else {
                    BorderStroke(1.dp, MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f))
                }
                
                val cardBackground = if (device.isConnected) {
                    GreenPrimary.copy(alpha = 0.08f)
                } else {
                    MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)
                }

                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(18.dp),
                    border = cardBorder,
                    colors = CardDefaults.cardColors(containerColor = cardBackground)
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(16.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(48.dp)
                                    .clip(CircleShape)
                                    .background(
                                        if (device.isConnected) GreenPrimary else MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f)
                                    ),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Bluetooth,
                                    contentDescription = "BLE Icon",
                                    tint = if (device.isConnected) MaterialTheme.colorScheme.background else MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            }
                            Column {
                                Text(text = device.name, fontWeight = FontWeight.Bold, fontSize = 16.sp)
                                Text(text = device.macAddress, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f))
                                Row(
                                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.SignalCellularAlt,
                                        contentDescription = "RSSI",
                                        tint = if (device.rssi > -70) GreenPrimary else AccentOrange,
                                        modifier = Modifier.size(14.dp)
                                    )
                                    Text(text = "${device.rssi} dBm", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f))
                                }
                            }
                        }

                        // Connection Actions
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            if (device.isConnected) {
                                IconButton(onClick = {
                                    selectedDeviceForConfig = device
                                    showConfigDialog = true
                                }) {
                                    Icon(imageVector = Icons.Default.Settings, contentDescription = "Config")
                                }
                                Button(
                                    onClick = {
                                        devices = devices.map { if (it.id == device.id) it.copy(isConnected = false) else it }
                                        coroutineScope.launch {
                                            try {
                                                val apiService = NetworkConfig.getApiService()
                                                apiService.syncRobot(
                                                    RobotSyncRequest(
                                                        robot_id = device.macAddress,
                                                        name = device.name,
                                                        mac_address = device.macAddress,
                                                        connected = false,
                                                        battery = 15,
                                                        signal_strength = "Weak",
                                                        scans_today = 0
                                                    )
                                                )
                                            } catch (e: Exception) {
                                                e.printStackTrace()
                                            }
                                        }
                                    },
                                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
                                    shape = RoundedCornerShape(8.dp)
                                ) {
                                    Text(text = "Disconnect", fontSize = 12.sp, color = Color.White)
                                }
                            } else {
                                Button(
                                    onClick = {
                                        devices = devices.map { if (it.id == device.id) it.copy(isConnected = true) else it.copy(isConnected = false) }
                                        coroutineScope.launch {
                                            try {
                                                val apiService = NetworkConfig.getApiService()
                                                apiService.syncRobot(
                                                    RobotSyncRequest(
                                                        robot_id = device.macAddress,
                                                        name = device.name,
                                                        mac_address = device.macAddress,
                                                        connected = true,
                                                        battery = if (device.id == "1") 89 else 15,
                                                        signal_strength = if (device.rssi > -70) "Excellent" else "Poor",
                                                        scans_today = if (device.id == "1") 124 else 0
                                                    )
                                                )
                                            } catch (e: Exception) {
                                                e.printStackTrace()
                                            }
                                        }
                                    },
                                    colors = ButtonDefaults.buttonColors(containerColor = GreenPrimary),
                                    shape = RoundedCornerShape(8.dp)
                                ) {
                                    Text(text = "Connect", fontSize = 12.sp, color = MaterialTheme.colorScheme.background)
                                }
                            }
                        }
                    }
                }
            }
        }

        // Configuration Dialog
        if (showConfigDialog && selectedDeviceForConfig != null) {
            AlertDialog(
                onDismissRequest = { showConfigDialog = false },
                title = { Text(text = "Configure ${selectedDeviceForConfig!!.name}") },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
                        Text(text = "Settings will sync with ESP32 device.")
                        
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(text = "Stream Low-Res Preview")
                            Switch(checked = true, onCheckedChange = {})
                        }

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(text = "Mouth-sync Face LED Anim")
                            Switch(checked = true, onCheckedChange = {})
                        }

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(text = "Enable Head Servos")
                            Switch(checked = false, onCheckedChange = {})
                        }
                    }
                },
                confirmButton = {
                    TextButton(onClick = { showConfigDialog = false }) {
                        Text(text = "Apply", color = GreenPrimary)
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showConfigDialog = false }) {
                        Text(text = "Cancel")
                    }
                }
            )
        }
    }
}
