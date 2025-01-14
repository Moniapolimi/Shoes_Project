package com.example.myapplication

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import java.util.*
import android.widget.Button

class MainActivity : AppCompatActivity() {

    private lateinit var bluetoothAdapter: BluetoothAdapter
    private var bluetoothGatt: BluetoothGatt? = null

    private lateinit var tvStatus: TextView
    private lateinit var sensorDataViews: List<TextView>
    private lateinit var imuDataViews: Map<String, TextView>

    private val deviceMacAddress = "CB:0C:88:A0:94:4B" // MAC address del microcontrollore
    private val serviceUuid = UUID.fromString("0000180A-0000-1000-8000-00805f9b34fb")
    private val characteristicUuid = UUID.fromString("00002A58-0000-1000-8000-00805f9b34fb")

    private var partialData = ByteArray(20) // Buffer per dati parziali
    private var partialIndex = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        tvStatus = findViewById(R.id.tvStatus)
        val btnConnect = findViewById<Button>(R.id.btnConnect)
        sensorDataViews = listOf(
            findViewById(R.id.sensor1), findViewById(R.id.sensor2), findViewById(R.id.sensor3),
            findViewById(R.id.sensor4), findViewById(R.id.sensor5), findViewById(R.id.sensor6),
            findViewById(R.id.sensor7), findViewById(R.id.sensor8)
        )

        imuDataViews = mapOf(
            "accelX" to findViewById(R.id.accelX),
            "accelY" to findViewById(R.id.accelY),
            "accelZ" to findViewById(R.id.accelZ),
            "gyroX" to findViewById(R.id.gyroX),
            "gyroY" to findViewById(R.id.gyroY),
            "gyroZ" to findViewById(R.id.gyroZ)
        )

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            checkAndRequestPermissions()
        } else {
            initializeBluetooth()
        }

        btnConnect.setOnClickListener {
            if (bluetoothGatt == null) {
                startScan()
                btnConnect.text = "Disconnetti"
            } else {
                disconnectDevice()
                btnConnect.text = "Connetti"
            }
        }
    }

    private fun disconnectDevice() {
        if (ActivityCompat.checkSelfPermission(
                this,
                Manifest.permission.BLUETOOTH_CONNECT
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            return
        }
        bluetoothGatt?.disconnect()
        bluetoothGatt = null
        runOnUiThread {
            tvStatus.text = "Stato: Disconnesso"
        }
    }

    private fun checkAndRequestPermissions() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED ||
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED
        ) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT,
                ),
                101
            )
        } else {
            initializeBluetooth()
        }
    }

    private fun initializeBluetooth() {
        val bluetoothManager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter
        startScan()
    }

    private fun startScan() {
        val scanner = bluetoothAdapter.bluetoothLeScanner
        tvStatus.text = "Scanning..."
        if (ActivityCompat.checkSelfPermission(
                this,
                Manifest.permission.BLUETOOTH_SCAN
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            return
        }
        scanner.startScan(object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                result?.device?.let { device ->
                    if (device.address == deviceMacAddress) {
                        if (ActivityCompat.checkSelfPermission(
                                this@MainActivity,
                                Manifest.permission.BLUETOOTH_SCAN
                            ) != PackageManager.PERMISSION_GRANTED
                        ) {
                            // TODO: Consider calling
                            //    ActivityCompat#requestPermissions
                            // here to request the missing permissions, and then overriding
                            //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
                            //                                          int[] grantResults)
                            // to handle the case where the user grants the permission. See the documentation
                            // for ActivityCompat#requestPermissions for more details.
                            return
                        }
                        scanner.stopScan(this)
                        connectToDevice(device.address)
                    }
                }
            }
        })
    }

    private fun connectToDevice(deviceAddress: String) {
        val device = bluetoothAdapter.getRemoteDevice(deviceAddress)
        if (ActivityCompat.checkSelfPermission(
                this,
                Manifest.permission.BLUETOOTH_CONNECT
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            return
        }
        bluetoothGatt = device.connectGatt(this, false, object : BluetoothGattCallback() {
            override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
                if (newState == BluetoothGatt.STATE_CONNECTED) {
                    runOnUiThread { tvStatus.text = "Connected" }
                    if (ActivityCompat.checkSelfPermission(
                            this@MainActivity,
                            Manifest.permission.BLUETOOTH_CONNECT
                        ) != PackageManager.PERMISSION_GRANTED
                    ) {
                        // TODO: Consider calling
                        //    ActivityCompat#requestPermissions
                        // here to request the missing permissions, and then overriding
                        //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
                        //                                          int[] grantResults)
                        // to handle the case where the user grants the permission. See the documentation
                        // for ActivityCompat#requestPermissions for more details.
                        return
                    }
                    gatt.discoverServices()
                }
            }

            override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
                val service = gatt.getService(serviceUuid)
                val characteristic = service?.getCharacteristic(characteristicUuid)
                characteristic?.let { startNotifications(gatt, it) }
            }

            private fun startNotifications(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
                if (ActivityCompat.checkSelfPermission(
                        this@MainActivity,
                        Manifest.permission.BLUETOOTH_CONNECT
                    ) != PackageManager.PERMISSION_GRANTED
                ) {
                    return
                }
                gatt.setCharacteristicNotification(characteristic, true)
                val descriptor = characteristic.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
                descriptor?.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                gatt.writeDescriptor(descriptor)
            }

            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
                processIncomingData(characteristic.value)
            }
        })
    }

    private fun processIncomingData(data: ByteArray) {
        for (byte in data) {
            partialData[partialIndex++] = byte
            if (partialIndex >= partialData.size) {
                parseCompleteData(partialData)
                partialIndex = 0
            }
        }
    }

    private fun parseCompleteData(data: ByteArray) {
        val header = (data[0].toInt() and 0xC0) shr 6
        when (header) {
            0b11 -> {
                val fsrValues = (0 until 8).map { i ->
                    (data[i * 2].toInt() and 0xFF) or ((data[i * 2 + 1].toInt() and 0x3F) shl 8)
                }
                updateFSRData(fsrValues)
            }
            0b10 -> {
                val imuValues = listOf(
                    ((data[2].toInt() shl 8) or data[3].toInt()),
                    ((data[4].toInt() shl 8) or data[5].toInt()),
                    ((data[6].toInt() shl 8) or data[7].toInt()),
                    ((data[8].toInt() shl 8) or data[9].toInt()),
                    ((data[10].toInt() shl 8) or data[11].toInt()),
                    ((data[12].toInt() shl 8) or data[13].toInt())
                )
                updateIMUData(imuValues)
            }
            else -> Log.e("BLE", "Header non riconosciuto: $header")
        }
    }

    private fun updateFSRData(values: List<Int>) {
        runOnUiThread {
            values.forEachIndexed { index, value ->
                if (index < sensorDataViews.size) {
                    sensorDataViews[index].text = "FSR${index + 1}: $value"
                }
            }
        }
    }

    private fun updateIMUData(values: List<Int>) {
        runOnUiThread {
            imuDataViews["accelX"]?.text = "Accel X: ${values[0]}"
            imuDataViews["accelY"]?.text = "Accel Y: ${values[1]}"
            imuDataViews["accelZ"]?.text = "Accel Z: ${values[2]}"
            imuDataViews["gyroX"]?.text = "Gyro X: ${values[3]}"
            imuDataViews["gyroY"]?.text = "Gyro Y: ${values[4]}"
            imuDataViews["gyroZ"]?.text = "Gyro Z: ${values[5]}"
        }
    }
}
