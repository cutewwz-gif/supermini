package fan.antony.supermini.watchproxy

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.util.Calendar
import java.util.UUID
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

/**
 * Foreground-only bridge: eSIM HTTP → BLE write to ESP32.
 * Do not rely on background — watches will kill this.
 */
class MainActivity : AppCompatActivity() {
    private lateinit var status: TextView
    private lateinit var btn: Button
    private val io = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    private val http = OkHttpClient.Builder()
        .connectTimeout(15, TimeUnit.SECONDS)
        .readTimeout(20, TimeUnit.SECONDS)
        .build()

    private var gatt: BluetoothGatt? = null
    private var scanning = false

    private val svcUuid = UUID.fromString(Config.SVC_UUID)
    private val dataUuid = UUID.fromString(Config.DATA_UUID)
    private val ctrlUuid = UUID.fromString(Config.CTRL_UUID)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        status = findViewById(R.id.status)
        btn = findViewById(R.id.btnSync)
        btn.setOnClickListener { startSync() }
        ensurePerms()
    }

    override fun onDestroy() {
        stopScan()
        closeGatt()
        super.onDestroy()
    }

    private fun setStatus(msg: String) {
        main.post { status.text = msg }
    }

    private fun ensurePerms() {
        val need = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= 31) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN)
                != PackageManager.PERMISSION_GRANTED
            ) need += Manifest.permission.BLUETOOTH_SCAN
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED
            ) need += Manifest.permission.BLUETOOTH_CONNECT
        }
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
            != PackageManager.PERMISSION_GRANTED
        ) need += Manifest.permission.ACCESS_FINE_LOCATION
        if (need.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, need.toTypedArray(), 42)
        }
    }

    private fun startSync() {
        btn.isEnabled = false
        setStatus("1/4 Fetch weather (eSIM)…")
        io.execute {
            val body = fetchWeather()
            if (body == null) {
                setStatus("HTTP failed — check eSIM / URL")
                main.post { btn.isEnabled = true }
                return@execute
            }
            setStatus("2/4 Scanning SuperMini…")
            main.post { scanAndPush(body) }
        }
    }

    private fun fetchWeather(): String? {
        return try {
            val req = Request.Builder()
                .url(Config.WEATHER_URL)
                .header("X-SuperMini-Key", Config.API_KEY)
                .get()
                .build()
            http.newCall(req).execute().use { resp ->
                if (!resp.isSuccessful) return null
                val raw = resp.body?.string() ?: return null
                // Stamp watch local time so desk can show update:HH:MM without NTP
                return try {
                    val o = JSONObject(raw)
                    val cal = Calendar.getInstance()
                    o.put("syncH", cal.get(Calendar.HOUR_OF_DAY))
                    o.put("syncM", cal.get(Calendar.MINUTE))
                    o.toString()
                } catch (_: Exception) {
                    raw
                }
            }
        } catch (e: Exception) {
            null
        }
    }

    @SuppressLint("MissingPermission")
    private fun scanAndPush(json: String) {
        val mgr = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = mgr.adapter
        if (adapter == null || !adapter.isEnabled) {
            setStatus("Bluetooth off")
            btn.isEnabled = true
            return
        }
        closeGatt()
        stopScan()
        scanning = true
        val scanner = adapter.bluetoothLeScanner
        val cb = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val name = result.device.name ?: return
                if (!name.equals(Config.DEVICE_NAME, ignoreCase = true)) return
                stopScan()
                setStatus("3/4 Connecting ${result.device.address}…")
                connectAndWrite(result.device, json)
            }

            override fun onScanFailed(errorCode: Int) {
                setStatus("Scan failed $errorCode")
                btn.isEnabled = true
            }
        }
        scanner.startScan(cb)
        // timeout
        main.postDelayed({
            if (scanning) {
                stopScan()
                setStatus("No SuperMini found — wake desk / exit standby")
                btn.isEnabled = true
            }
        }, 12000)
        // stash callback? need keep reference - use field
        scanCb = cb
    }

    private var scanCb: ScanCallback? = null

    @SuppressLint("MissingPermission")
    private fun stopScan() {
        if (!scanning) return
        scanning = false
        try {
            val mgr = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
            scanCb?.let { mgr.adapter?.bluetoothLeScanner?.stopScan(it) }
        } catch (_: Exception) {
        }
        scanCb = null
    }

    @SuppressLint("MissingPermission")
    private fun closeGatt() {
        try {
            gatt?.close()
        } catch (_: Exception) {
        }
        gatt = null
    }

    @SuppressLint("MissingPermission")
    private fun connectAndWrite(device: BluetoothDevice, json: String) {
        gatt = device.connectGatt(this, false, object : BluetoothGattCallback() {
            override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    setStatus("3/4 Discovering…")
                    g.discoverServices()
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    setStatus("Disconnected")
                    main.post { btn.isEnabled = true }
                }
            }

            override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    setStatus("Service discover fail")
                    main.post { btn.isEnabled = true }
                    return
                }
                io.execute { pushJson(g, json) }
            }
        }, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    private fun pushJson(g: BluetoothGatt, json: String) {
        val svc = g.getService(svcUuid)
        if (svc == null) {
            setStatus("Service missing — flash new firmware")
            main.post { btn.isEnabled = true }
            return
        }
        val data = svc.getCharacteristic(dataUuid)
        val ctrl = svc.getCharacteristic(ctrlUuid)
        if (data == null || ctrl == null) {
            setStatus("Chars missing")
            main.post { btn.isEnabled = true }
            return
        }

        setStatus("4/4 Writing ${json.length} bytes…")
        if (!writeAscii(g, ctrl, "CLEAR")) {
            setStatus("CLEAR failed")
            main.post { btn.isEnabled = true }
            return
        }
        Thread.sleep(80)
        var off = 0
        val bytes = json.toByteArray(Charsets.UTF_8)
        while (off < bytes.size) {
            val end = minOf(off + Config.CHUNK, bytes.size)
            val slice = bytes.copyOfRange(off, end)
            if (!writeBytes(g, data, slice)) {
                setStatus("DATA write fail @$off")
                main.post { btn.isEnabled = true }
                return
            }
            off = end
            Thread.sleep(40)
        }
        Thread.sleep(80)
        if (!writeAscii(g, ctrl, "COMMIT")) {
            setStatus("COMMIT failed")
            main.post { btn.isEnabled = true }
            return
        }
        setStatus("Done — check desk Clock")
        main.post { btn.isEnabled = true }
        main.postDelayed({ closeGatt() }, 500)
    }

    @SuppressLint("MissingPermission")
    private fun writeAscii(g: BluetoothGatt, c: BluetoothGattCharacteristic, s: String): Boolean {
        return writeBytes(g, c, s.toByteArray(Charsets.UTF_8))
    }

    @SuppressLint("MissingPermission")
    private fun writeBytes(g: BluetoothGatt, c: BluetoothGattCharacteristic, bytes: ByteArray): Boolean {
        c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        if (Build.VERSION.SDK_INT >= 33) {
            val r = g.writeCharacteristic(c, bytes, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE)
            return r == BluetoothGatt.GATT_SUCCESS
        }
        @Suppress("DEPRECATION")
        c.value = bytes
        @Suppress("DEPRECATION")
        return g.writeCharacteristic(c)
    }
}
