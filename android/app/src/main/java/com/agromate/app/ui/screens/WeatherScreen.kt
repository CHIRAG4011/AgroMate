package com.agromate.app.ui.screens

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Cloud
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.LocationOn
import androidx.compose.material.icons.filled.Thermostat
import androidx.compose.material.icons.filled.WaterDrop
import androidx.compose.material.icons.filled.WindPower
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.platform.LocalContext
import android.content.Context
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.Dispatchers
import com.agromate.app.data.Localizer
import com.agromate.app.data.NetworkConfig
import com.agromate.app.ui.theme.AccentOrange
import com.agromate.app.ui.theme.GreenPrimary
import com.agromate.app.ui.theme.GreenSecondary

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun WeatherScreen(modifier: Modifier = Modifier, onBack: (() -> Unit)? = null) {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    val prefs = remember { context.getSharedPreferences("agromate_prefs", Context.MODE_PRIVATE) }

    var locationName by remember { mutableStateOf("New Delhi") }
    var temperature by remember { mutableStateOf("28.5°C") }
    var description by remember { mutableStateOf("Clear Sky") }
    var humidity by remember { mutableStateOf("64%") }
    var windSpeed by remember { mutableStateOf("5 km/h") }
    
    var advisories by remember { mutableStateOf<List<String>>(emptyList()) }
    var forecasts by remember { mutableStateOf<List<Map<String, Any>>>(emptyList()) }

    var searchInput by remember { mutableStateOf("") }
    var isSearchingCity by remember { mutableStateOf(false) }

    val apiService = remember { NetworkConfig.getApiService() }

    suspend fun fetchWeather(api: com.agromate.app.data.remote.ApiService, lat: Double, lon: Double, city: String) {
        try {
            val response = api.getWeather(lat, lon, city)
            
            val weather = response["weather"] as? Map<*, *>
            if (weather != null) {
                val tempVal = weather["temperature"]
                val descVal = weather["description"]
                val humVal = weather["humidity"]
                val windVal = weather["wind_speed"]
                
                if (tempVal != null) {
                    val doubleTemp = (tempVal as? Double ?: (tempVal as? Int)?.toDouble() ?: 28.5)
                    temperature = "${String.format("%.1f", doubleTemp)}°C"
                }
                if (descVal != null) {
                    description = descVal.toString()
                }
                if (humVal != null) {
                    val doubleHum = (humVal as? Double ?: (humVal as? Int)?.toDouble() ?: 64.0)
                    humidity = "${doubleHum.toInt()}%"
                }
                if (windVal != null) {
                    val doubleWind = (windVal as? Double ?: (windVal as? Int)?.toDouble() ?: 5.0)
                    windSpeed = "${String.format("%.1f", doubleWind)} km/h"
                }
                val locVal = weather["location"]
                if (locVal != null) {
                    locationName = locVal.toString()
                }
            }

            // Extract forecast
            val forecastList = response["forecast"] as? List<*>
            if (forecastList != null) {
                val parsedForecasts = mutableListOf<Map<String, Any>>()
                for (item in forecastList) {
                    val m = item as? Map<*, *>
                    if (m != null) {
                        val parsedItem = mutableMapOf<String, Any>()
                        m.forEach { (k, v) ->
                            if (k != null && v != null) {
                                parsedItem[k.toString()] = v
                            }
                        }
                        parsedForecasts.add(parsedItem)
                    }
                }
                forecasts = parsedForecasts
            }

            // Extract advisory
            val advisoryMap = response["advisory"] as? Map<*, *>
            if (advisoryMap != null) {
                val parsedAdvisories = mutableListOf<String>()
                val recs = advisoryMap["recommendations"] as? List<*>
                if (recs != null) {
                    recs.forEach { r ->
                        if (r != null) parsedAdvisories.add(r.toString())
                    }
                }
                if (parsedAdvisories.isEmpty()) {
                    val msg = advisoryMap["message"]
                    if (msg != null) parsedAdvisories.add(msg.toString())
                }
                advisories = parsedAdvisories
            }
        } catch (e: Exception) {
            e.printStackTrace()
            // Provide default fallback recommendations if endpoint throws error
            advisories = listOf(
                "Optimal conditions for sowing Kharif crops.",
                "Maintain adequate irrigation for vegetable nurseries.",
                "Spray organic neem solution under foliage to check pests."
            )
        }
    }

    LaunchedEffect(Unit) {
        val lat = prefs.getFloat("weather_latitude", 28.6139f).toDouble()
        val lon = prefs.getFloat("weather_longitude", 77.2090f).toDouble()
        val city = prefs.getString("weather_selected_city", "New Delhi") ?: "New Delhi"
        locationName = city
        fetchWeather(apiService, lat, lon, city)
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Page Header
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
                Text(text = "Weather & Advisories", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                Text(
                    text = "Agricultural weather alerts and crop recommendations",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f)
                )
            }
        }

        // City Search Bar
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            OutlinedTextField(
                value = searchInput,
                onValueChange = { searchInput = it },
                placeholder = { Text("Search City...", fontSize = 13.sp) },
                modifier = Modifier.weight(1f),
                shape = RoundedCornerShape(12.dp),
                singleLine = true,
                trailingIcon = {
                    if (isSearchingCity) {
                        CircularProgressIndicator(modifier = Modifier.size(18.dp), strokeWidth = 2.dp)
                    }
                }
            )
            Button(
                onClick = {
                    if (searchInput.isNotBlank()) {
                        coroutineScope.launch {
                            isSearchingCity = true
                            try {
                                val url = "https://geocoding-api.open-meteo.com/v1/search?name=${java.net.URLEncoder.encode(searchInput, "UTF-8")}&count=1&language=en&format=json"
                                val client = okhttp3.OkHttpClient()
                                val request = okhttp3.Request.Builder().url(url).build()
                                val response = withContext(Dispatchers.IO) { client.newCall(request).execute() }
                                if (response.isSuccessful) {
                                    val bodyStr = response.body?.string()
                                    if (!bodyStr.isNullOrBlank()) {
                                        val json = org.json.JSONObject(bodyStr)
                                        val results = json.optJSONArray("results")
                                        if (results != null && results.length() > 0) {
                                            val first = results.getJSONObject(0)
                                            val newLat = first.getDouble("latitude")
                                            val newLon = first.getDouble("longitude")
                                            val cityName = first.getString("name")
                                            val admin1 = first.optString("admin1", "")
                                            val country = first.optString("country", "")
                                            val formattedName = if (admin1.isNotBlank()) "$cityName, $admin1 ($country)" else "$cityName ($country)"

                                            // Save to preferences for persistence
                                            prefs.edit()
                                                .putFloat("weather_latitude", newLat.toFloat())
                                                .putFloat("weather_longitude", newLon.toFloat())
                                                .putString("weather_selected_city", formattedName)
                                                .apply()

                                            // Update status and fetch weather
                                            locationName = formattedName
                                            fetchWeather(apiService, newLat, newLon, formattedName)
                                            searchInput = ""
                                        }
                                    }
                                }
                            } catch (e: Exception) {
                                e.printStackTrace()
                            } finally {
                                isSearchingCity = false
                            }
                        }
                    }
                },
                shape = RoundedCornerShape(12.dp),
                colors = ButtonDefaults.buttonColors(containerColor = GreenPrimary),
                modifier = Modifier.height(56.dp)
            ) {
                Text("Search", color = MaterialTheme.colorScheme.background)
            }
        }

        // Current weather card
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(20.dp),
            border = BorderStroke(1.dp, GreenPrimary.copy(alpha = 0.15f)),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f))
        ) {
            Column(
                modifier = Modifier.padding(20.dp),
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(imageVector = Icons.Default.LocationOn, contentDescription = "Location", tint = GreenPrimary)
                        Text(text = locationName, fontWeight = FontWeight.Bold, fontSize = 16.sp)
                    }
                    Text(
                        text = "Live Update",
                        fontSize = 11.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = GreenPrimary,
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(GreenPrimary.copy(alpha = 0.12f))
                            .padding(horizontal = 8.dp, vertical = 4.dp)
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(text = temperature, fontWeight = FontWeight.ExtraBold, fontSize = 48.sp, color = Color.White)
                        Text(text = description, fontWeight = FontWeight.Medium, fontSize = 14.sp, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f))
                    }
                    Icon(
                        imageVector = Icons.Default.Cloud,
                        contentDescription = "Weather Icon",
                        tint = GreenSecondary,
                        modifier = Modifier.size(64.dp)
                    )
                }

                Divider(color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(16.dp)
                ) {
                    Row(
                        modifier = Modifier.weight(1f),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(imageVector = Icons.Default.WaterDrop, contentDescription = "Humidity", tint = Color(0xFF2196F3))
                        Column {
                            Text(text = "Humidity", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.5f))
                            Text(text = humidity, fontWeight = FontWeight.Bold, fontSize = 14.sp)
                        }
                    }

                    Row(
                        modifier = Modifier.weight(1f),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(imageVector = Icons.Default.WindPower, contentDescription = "Wind", tint = AccentOrange)
                        Column {
                            Text(text = "Wind Speed", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.5f))
                            Text(text = windSpeed, fontWeight = FontWeight.Bold, fontSize = 14.sp)
                        }
                    }
                }
            }
        }

        // Agricultural Advisories
        Text(text = "🌾 Agricultural Advisory", fontWeight = FontWeight.Bold, fontSize = 18.sp)
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            border = BorderStroke(1.dp, AccentOrange.copy(alpha = 0.15f)),
            colors = CardDefaults.cardColors(containerColor = AccentOrange.copy(alpha = 0.06f))
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                if (advisories.isEmpty()) {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(imageVector = Icons.Default.Info, contentDescription = "Info", tint = AccentOrange)
                        Text(text = "No alerts or actions required for today.", fontSize = 13.sp)
                    }
                } else {
                    advisories.forEach { advisory ->
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            verticalAlignment = Alignment.Top
                        ) {
                            Text(text = "•", color = AccentOrange, fontWeight = FontWeight.Bold, fontSize = 16.sp)
                            Text(text = advisory, fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurface)
                        }
                    }
                }
            }
        }


    }
}
