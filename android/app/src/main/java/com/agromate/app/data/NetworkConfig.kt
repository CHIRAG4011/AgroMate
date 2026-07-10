package com.agromate.app.data

import android.content.Context
import com.agromate.app.data.remote.ApiService
import okhttp3.OkHttpClient
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import java.util.concurrent.TimeUnit

object NetworkConfig {
    var ipAddress: String = "10.0.2.2"
    var port: String = "8000"

    val baseUrl: String
        get() = "http://$ipAddress:$port/"

    val wsUrl: String
        get() = "ws://$ipAddress:$port"

    private val okHttpClient = OkHttpClient.Builder()
        .connectTimeout(60, TimeUnit.SECONDS)
        .readTimeout(60, TimeUnit.SECONDS)
        .build()

    fun initialize(context: Context) {
        val prefs = context.getSharedPreferences("agromate_prefs", Context.MODE_PRIVATE)
        ipAddress = prefs.getString("server_ip", "10.0.2.2") ?: "10.0.2.2"
        port = prefs.getString("server_port", "8000") ?: "8000"
    }

    fun save(context: Context, ip: String, prt: String) {
        ipAddress = ip
        port = prt
        context.getSharedPreferences("agromate_prefs", Context.MODE_PRIVATE).edit()
            .putString("server_ip", ip)
            .putString("server_port", prt)
            .apply()
    }

    fun getApiService(): ApiService {
        return Retrofit.Builder()
            .baseUrl(baseUrl)
            .client(okHttpClient)
            .addConverterFactory(GsonConverterFactory.create())
            .build()
            .create(ApiService::class.java)
    }
}
