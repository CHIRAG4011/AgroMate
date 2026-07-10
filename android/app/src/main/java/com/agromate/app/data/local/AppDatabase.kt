package com.agromate.app.data.local

import androidx.room.Dao
import androidx.room.Database
import androidx.room.Entity
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.PrimaryKey
import androidx.room.Query
import androidx.room.RoomDatabase
import kotlinx.coroutines.flow.Flow

@Entity(tableName = "scans")
data class LocalScan(
    @PrimaryKey val id: String,
    val plantName: String,
    val condition: String,
    val confidence: Int,
    val timestamp: String,
    val isHealthy: Boolean
)

@Entity(tableName = "robots")
data class LocalRobot(
    @PrimaryKey val id: String,
    val name: String,
    val macAddress: String,
    val batteryPercent: Int,
    val isConnected: Boolean
)

@Entity(tableName = "chat_messages")
data class LocalChatMessage(
    @PrimaryKey(autoGenerate = true) val id: Int = 0,
    val text: String,
    val isUser: Boolean,
    val timestamp: String,
    val hasDetailed: Boolean = false,
    val responseType: String = "summary"
)

@Dao
interface AgroMateDao {
    @Query("SELECT * FROM scans ORDER BY timestamp DESC")
    fun getAllScans(): Flow<List<LocalScan>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertScan(scan: LocalScan)

    @Query("SELECT * FROM robots")
    fun getAllRobots(): Flow<List<LocalRobot>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertRobot(robot: LocalRobot)

    @Query("SELECT * FROM chat_messages ORDER BY id ASC")
    fun getAllChatMessages(): Flow<List<LocalChatMessage>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertChatMessage(message: LocalChatMessage)

    @Query("DELETE FROM chat_messages")
    suspend fun clearChatMessages()
}

@Database(entities = [LocalScan::class, LocalRobot::class, LocalChatMessage::class], version = 2, exportSchema = false)
abstract class AppDatabase : RoomDatabase() {
    abstract fun dao(): AgroMateDao

    companion object {
        @Volatile
        private var INSTANCE: AppDatabase? = null

        fun getDatabase(context: android.content.Context): AppDatabase {
            return INSTANCE ?: synchronized(this) {
                val instance = androidx.room.Room.databaseBuilder(
                    context.applicationContext,
                    AppDatabase::class.java,
                    "agromate.db"
                ).fallbackToDestructiveMigration().build()
                INSTANCE = instance
                instance
            }
        }
    }
}
