package com.agromate.app

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.core.content.ContextCompat
import com.agromate.app.ui.theme.AgroMateTheme
import dagger.hilt.android.AndroidEntryPoint

@AndroidEntryPoint
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            AgroMateTheme {
                // Request permissions on startup
                val permissions = mutableListOf(
                    Manifest.permission.INTERNET,
                    Manifest.permission.ACCESS_NETWORK_STATE,
                    Manifest.permission.CAMERA,
                    Manifest.permission.RECORD_AUDIO
                )
                
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    permissions.add(Manifest.permission.BLUETOOTH_SCAN)
                    permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
                } else {
                    permissions.add(Manifest.permission.BLUETOOTH)
                    permissions.add(Manifest.permission.BLUETOOTH_ADMIN)
                    permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
                }

                val launcher = rememberLauncherForActivityResult(
                    contract = ActivityResultContracts.RequestMultiplePermissions()
                ) { results ->
                    val allGranted = results.values.all { it }
                    if (!allGranted) {
                        Toast.makeText(
                            this,
                            "AgroMate requires Bluetooth & Network permissions to scan for robots.",
                            Toast.LENGTH_LONG
                        ).show()
                    }
                }

                LaunchedEffect(Unit) {
                    val neededPermissions = permissions.filter {
                        ContextCompat.checkSelfPermission(this@MainActivity, it) != PackageManager.PERMISSION_GRANTED
                    }
                    if (neededPermissions.isNotEmpty()) {
                        launcher.launch(neededPermissions.toTypedArray())
                    }
                }

                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    // Main app entry navigation / screen
                    MainScreen()
                }
            }
        }
    }
}
