package com.agromate.app

import android.app.Application
import com.agromate.app.data.NetworkConfig
import dagger.hilt.android.HiltAndroidApp

@HiltAndroidApp
class AgroMateApp : Application() {
    override fun onCreate() {
        super.onCreate()
        NetworkConfig.initialize(this)
    }
}
