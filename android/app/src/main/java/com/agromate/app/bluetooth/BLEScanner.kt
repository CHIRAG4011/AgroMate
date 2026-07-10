package com.agromate.app.bluetooth

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.Handler
import android.os.Looper
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class BLEScanner @Inject constructor(
    @ApplicationContext private val context: Context
) {
    private val bluetoothAdapter: BluetoothAdapter? = BluetoothAdapter.getDefaultAdapter()
    private val scanner = bluetoothAdapter?.bluetoothLeScanner
    private var isScanning = false
    private val handler = Handler(Looper.getMainLooper())

    @SuppressLint("MissingPermission")
    fun startScan(onDeviceFound: (BluetoothDevice, Int) -> Unit, onScanFinished: () -> Unit) {
        if (bluetoothAdapter == null || scanner == null || isScanning) return

        val scanCallback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                result.device?.let { device ->
                    val name = result.scanRecord?.deviceName ?: device.name ?: ""
                    if (name.contains("AgroMate", ignoreCase = true)) {
                        onDeviceFound(device, result.rssi)
                    }
                }
            }
        }

        isScanning = true
        scanner.startScan(scanCallback)

        // Stop scan after 10 seconds
        handler.postDelayed({
            if (isScanning) {
                scanner.stopScan(scanCallback)
                isScanning = false
                onScanFinished()
            }
        }, 10000)
    }
}
