plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "fan.antony.supermini.watchproxy"
    compileSdk = 35

    defaultConfig {
        applicationId = "fan.antony.supermini.watchproxy"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
        // OPPO Watch often wants 32-bit
        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a")
        }

        val localProps = java.util.Properties()
        val localFile = rootProject.file("local.properties")
        if (localFile.exists()) {
            localFile.inputStream().use { localProps.load(it) }
        }
        val hub = localProps.getProperty("supermini.hub", "http://127.0.0.1")
        val apiKey = localProps.getProperty("supermini.apiKey", "")
        buildConfigField("String", "SUPERMINI_HUB", "\"$hub\"")
        buildConfigField("String", "SUPERMINI_API_KEY", "\"$apiKey\"")
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        viewBinding = true
        buildConfig = true
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
}
