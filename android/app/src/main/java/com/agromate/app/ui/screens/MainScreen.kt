package com.agromate.app

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Analytics
import androidx.compose.material.icons.filled.Chat
import androidx.compose.material.icons.filled.Dashboard
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.PrecisionManufacturing
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.PhotoCamera
import androidx.compose.material.icons.filled.MenuBook
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.agromate.app.ui.screens.AnalyticsScreen
import com.agromate.app.ui.screens.DashboardScreen
import com.agromate.app.ui.screens.AIChatScreen
import com.agromate.app.ui.screens.RobotManagerScreen
import com.agromate.app.ui.screens.ScanHistoryScreen
import com.agromate.app.ui.screens.ReportsScreen
import com.agromate.app.ui.screens.SettingsScreen
import com.agromate.app.ui.screens.ScanScreen
import com.agromate.app.ui.screens.KnowledgeBaseScreen
import com.agromate.app.ui.screens.WeatherScreen
import androidx.compose.runtime.collectAsState
import androidx.compose.ui.platform.LocalContext
import com.agromate.app.data.local.AppDatabase
import com.agromate.app.data.local.LocalChatMessage

sealed class Screen(val route: String, val title: String, val icon: @Composable () -> Unit) {
    object Dashboard : Screen("dashboard", "Dashboard", { Icon(Icons.Default.Dashboard, "Dashboard") })
    object Scan : Screen("scan", "Scan", { Icon(Icons.Default.PhotoCamera, "Scan") })
    object RobotManager : Screen("robot_manager", "Robots", { Icon(Icons.Default.PrecisionManufacturing, "Robots") })
    object AIChat : Screen("ai_chat", "AI Chat", { Icon(Icons.Default.Chat, "AI Chat") })
    object ScanHistory : Screen("scan_history", "History", { Icon(Icons.Default.History, "History") })
    object Reports : Screen("reports", "Reports", { Icon(Icons.Default.Description, "Reports") })
    object Analytics : Screen("analytics", "Analytics", { Icon(Icons.Default.Analytics, "Analytics") })
    object Knowledge : Screen("knowledge", "Knowledge", { Icon(Icons.Default.MenuBook, "Knowledge") })
    object Weather : Screen("weather", "Weather", { Icon(Icons.Default.Cloud, "Weather") })
    object Settings : Screen("settings", "Settings", { Icon(Icons.Default.Settings, "Settings") })
}

@Composable
fun MainScreen() {
    val items = listOf(
        Screen.Dashboard,
        Screen.Scan,
        Screen.AIChat,
        Screen.Settings
    )

    val context = LocalContext.current
    val database = remember { AppDatabase.getDatabase(context) }
    val dao = remember { database.dao() }
    val localMessages by dao.getAllChatMessages().collectAsState(initial = emptyList())

    val chatMessages = remember(localMessages) {
        localMessages.map { 
            ChatMessage(
                text = it.text, 
                isUser = it.isUser, 
                timestamp = it.timestamp, 
                hasDetailed = it.hasDetailed, 
                responseType = it.responseType
            ) 
        }
    }

    var currentScreen by remember { mutableStateOf<Screen>(Screen.Dashboard) }
    var chatPreloadMessage by remember { mutableStateOf<String?>(null) }
    val coroutineScope = rememberCoroutineScope()

    // Fetch chat history from SQLite backend on startup and cache in Room
    LaunchedEffect(Unit) {
        coroutineScope.launch {
            try {
                val apiService = NetworkConfig.getApiService()
                val response = apiService.getChatHistory()
                if (response.status == "success") {
                    val loaded = response.messages.map { dbMsg ->
                        val timeStr = try {
                            val isoFormat = java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", java.util.Locale.getDefault())
                            val date = isoFormat.parse(dbMsg.created_at) ?: throw NullPointerException()
                            java.text.SimpleDateFormat("hh:mm a", java.util.Locale.getDefault()).format(date)
                        } catch (e: Exception) {
                            "10:00 AM"
                        }
                        ChatMessage(dbMsg.content, dbMsg.role == "user", timeStr)
                    }
                    if (loaded.isNotEmpty()) {
                        dao.clearChatMessages()
                        for (msg in loaded) {
                            dao.insertChatMessage(
                                LocalChatMessage(
                                    text = msg.text,
                                    isUser = msg.isUser,
                                    timestamp = msg.timestamp,
                                    hasDetailed = msg.hasDetailed,
                                    responseType = msg.responseType
                                )
                            )
                        }
                    }
                }
            } catch (e: Exception) {
                // Network failed or offline, will fallback automatically to Room observed messages
            }
        }
    }

    // Insert default welcome message if local message store is empty
    LaunchedEffect(localMessages) {
        if (localMessages.isEmpty()) {
            dao.insertChatMessage(
                LocalChatMessage(
                    text = "Hello! I am AgroMate, your agricultural AI assistant. Ask me anything about crop cultivation, plant diseases, or soil suitability.",
                    isUser = false,
                    timestamp = "10:00 AM"
                )
            )
        }
    }

    val localizerLanguage = Localizer.currentLanguage
    LaunchedEffect(localizerLanguage) {
        coroutineScope.launch {
            try {
                Localizer.fetchTranslations(localizerLanguage)
            } catch (e: Exception) {
                // Handle offline or error
            }
        }
    }

    Scaffold(
        bottomBar = {
            NavigationBar(
                containerColor = MaterialTheme.colorScheme.surface.copy(alpha = 0.9f),
                tonalElevation = 8.dp,
                modifier = Modifier
                    .padding(horizontal = 16.dp, vertical = 12.dp)
                    .clip(RoundedCornerShape(24.dp))
                    .border(
                        BorderStroke(1.dp, MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f)),
                        RoundedCornerShape(24.dp)
                    )
            ) {
                items.forEach { item ->
                    val isSelected = currentScreen.route == item.route
                    NavigationBarItem(
                        icon = item.icon,
                        label = {
                            val labelText = when (item.route) {
                                "dashboard" -> Localizer.t("nav_dashboard")
                                "scan" -> Localizer.t("nav_scan")
                                "ai_chat" -> Localizer.t("nav_chatbot")
                                "settings" -> Localizer.t("nav_settings")
                                else -> item.title
                            }
                            Text(labelText)
                        },
                        selected = isSelected,
                        onClick = { currentScreen = item },
                        colors = NavigationBarItemDefaults.colors(
                            selectedIconColor = GreenPrimary,
                            selectedTextColor = GreenPrimary,
                            unselectedIconColor = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                            unselectedTextColor = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                            indicatorColor = GreenPrimary.copy(alpha = 0.12f)
                        )
                    )
                }
            }
        }
    ) { innerPadding ->
        Modifier.padding(innerPadding).let {
            when (currentScreen) {
                Screen.Dashboard -> DashboardScreen(
                    modifier = Modifier.padding(innerPadding),
                    onNavigate = { screen -> currentScreen = screen }
                )
                Screen.Scan -> ScanScreen(
                    modifier = Modifier.padding(innerPadding),
                    onNavigateToChat = { message ->
                        chatPreloadMessage = message
                        currentScreen = Screen.AIChat
                    }
                )
                Screen.AIChat -> AIChatScreen(
                    modifier = Modifier.padding(innerPadding),
                    messages = chatMessages,
                    onMessagesChange = { chatMessages = it },
                    chatPreloadMessage = chatPreloadMessage,
                    onPreloadHandled = { chatPreloadMessage = null }
                )
                Screen.RobotManager -> RobotManagerScreen(
                    modifier = Modifier.padding(innerPadding),
                    onBack = { currentScreen = Screen.Dashboard }
                )
                Screen.ScanHistory -> ScanHistoryScreen(
                    modifier = Modifier.padding(innerPadding),
                    onBack = { currentScreen = Screen.Dashboard }
                )
                Screen.Reports -> ReportsScreen(
                    modifier = Modifier.padding(innerPadding),
                    onBack = { currentScreen = Screen.Dashboard }
                )
                Screen.Analytics -> AnalyticsScreen(
                    modifier = Modifier.padding(innerPadding),
                    onBack = { currentScreen = Screen.Dashboard }
                )
                Screen.Knowledge -> KnowledgeBaseScreen(
                    modifier = Modifier.padding(innerPadding),
                    onBack = { currentScreen = Screen.Dashboard }
                )
                Screen.Weather -> WeatherScreen(
                    modifier = Modifier.padding(innerPadding),
                    onBack = { currentScreen = Screen.Dashboard }
                )
                Screen.Settings -> SettingsScreen(modifier = Modifier.padding(innerPadding))
            }
        }
    }
}
