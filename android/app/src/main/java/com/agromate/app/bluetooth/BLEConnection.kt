package com.agromate.app.bluetooth

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.content.Context
import java.util.UUID

@SuppressLint("MissingPermission")
class BLEConnection(
    private val context: Context,
    private val device: BluetoothDevice,
    private val onConnectionStateChanged: (Boolean) -> Unit,
    private val onMessageReceived: (BLEMessage) -> Unit
) {
    private var bluetoothGatt: BluetoothGatt? = null
    
    private val serviceUuid = UUID.fromString("12345678-1234-5678-1234-56789abcde01")
    private val cmdTxUuid = UUID.fromString("12345678-1234-5678-1234-56789abcdf01")
    private val cmdRxUuid = UUID.fromString("12345678-1234-5678-1234-56789abcdf02")
    private val cccdUuid = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                onConnectionStateChanged(true)
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                onConnectionStateChanged(false)
                bluetoothGatt = null
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                // Subscribe to notifications from cmdRx characteristic
                val service = gatt.getService(serviceUuid)
                val characteristic = service?.getCharacteristic(cmdRxUuid)
                if (characteristic != null) {
                    gatt.setCharacteristicNotification(characteristic, true)
                    val descriptor = characteristic.getDescriptor(cccdUuid)
                    if (descriptor != null) {
                        descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        gatt.writeDescriptor(descriptor)
                    }
                }
            }
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            if (characteristic.uuid == cmdRxUuid) {
                BLEMessage.decode(value)?.let { msg ->
                    onMessageReceived(msg)
                }
            }
        }
    }

    fun connect() {
        bluetoothGatt = device.connectGatt(context, false, gattCallback)
    }

    fun sendMessage(msg: BLEMessage): Boolean {
        val gatt = bluetoothGatt ?: return false
        val service = gatt.getService(serviceUuid)
        val characteristic = service?.getCharacteristic(cmdTxUuid) ?: return false
        
        characteristic.value = msg.encode()
        return gatt.writeCharacteristic(characteristic)
    }

    fun disconnect() {
        bluetoothGatt?.disconnect()
    }
}
