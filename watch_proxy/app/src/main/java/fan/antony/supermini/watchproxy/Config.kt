package fan.antony.supermini.watchproxy

object Config {
    /** Same hub the ESP uses over WiFi. Values come from local.properties via BuildConfig. */
    val WEATHER_URL: String = BuildConfig.SUPERMINI_HUB.trimEnd('/') + "/supermini/api/weather"
    val API_KEY: String = BuildConfig.SUPERMINI_API_KEY

    const val DEVICE_NAME = "SuperMini"
    const val SVC_UUID = "6bc80001-a1b2-c3d4-e5f6-000000000001"
    const val DATA_UUID = "6bc80002-a1b2-c3d4-e5f6-000000000001"
    const val CTRL_UUID = "6bc80003-a1b2-c3d4-e5f6-000000000001"

    /** BLE write chunk size (stay under default ATT MTU). */
    const val CHUNK = 160
}
