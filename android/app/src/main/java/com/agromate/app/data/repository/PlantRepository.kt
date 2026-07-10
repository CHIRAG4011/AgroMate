package com.agromate.app.data.repository

import com.agromate.app.data.local.AgroMateDao
import com.agromate.app.data.local.LocalScan
import com.agromate.app.data.remote.ApiService
import kotlinx.coroutines.flow.Flow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class PlantRepository @Inject constructor(
    private val dao: AgroMateDao,
    private val api: ApiService
) {
    val allScans: Flow<List<LocalScan>> = dao.getAllScans()

    suspend fun diagnoseCrop(robotId: String, imageBytes: ByteArray) {
        try {
            val response = api.submitScan(robotId, imageBytes)
            val scan = LocalScan(
                id = response.id,
                plantName = response.plantName,
                condition = response.condition,
                confidence = (response.confidence * 100).toInt(),
                timestamp = System.currentTimeMillis().toString(),
                isHealthy = response.condition.contains("Healthy", ignoreCase = true)
            )
            dao.insertScan(scan)
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
