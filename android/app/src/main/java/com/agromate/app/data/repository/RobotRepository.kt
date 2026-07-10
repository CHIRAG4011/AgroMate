package com.agromate.app.data.repository

import com.agromate.app.data.local.AgroMateDao
import com.agromate.app.data.local.LocalRobot
import com.agromate.app.data.remote.ApiService
import kotlinx.coroutines.flow.Flow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class RobotRepository @Inject constructor(
    private val dao: AgroMateDao,
    private val api: ApiService
) {
    val allRobots: Flow<List<LocalRobot>> = dao.getAllRobots()

    suspend fun refreshRobots() {
        try {
            val remoteResponse = api.getRobots()
            remoteResponse.robots.forEach { remote ->
                val local = LocalRobot(
                    id = remote.id,
                    name = remote.name,
                    macAddress = "A6:AD:00:00:00:00", // Placeholder MAC
                    batteryPercent = remote.battery,
                    isConnected = remote.isConnected
                )
                dao.insertRobot(local)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
