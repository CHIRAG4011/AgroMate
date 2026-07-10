package com.agromate.app.ui.screens

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.agromate.app.data.Localizer
import com.agromate.app.ui.theme.GreenPrimary
import com.agromate.app.ui.theme.GreenSecondary

data class HandbookItem(val title: String, val desc: String, val details: String)

val KNOWLEDGE_HANDBOOK = mapOf(
    "crops" to listOf(
        HandbookItem(
            title = "Rice Cultivation (Kharif)",
            desc = "Optimal temperature: 22-32°C. Requires standing water (clayey loam soil). Sow nursery in June, transplant in July. Yield cycle: 120-150 days.",
            details = "NPK ratio: 120:60:40 kg/ha. Keep field flooded with 2-5cm water during tillering and flowering stage. Susceptible to blast disease and stem borers."
        ),
        HandbookItem(
            title = "Wheat Farming (Rabi)",
            desc = "Optimal temperature: 15-25°C. Requires loam soil. Sow in November, harvest in March/April. Yield cycle: 110-140 days.",
            details = "Requires 4-6 irrigations at critical growth periods (Crown Root Initiation stage is most critical). Susceptible to rust diseases."
        ),
        HandbookItem(
            title = "Tomato Management",
            desc = "Optimal temperature: 20-28°C. Sandy loam soil, pH 6.0-7.0. Can be sown year-round. Yield cycle: 80-110 days.",
            details = "Water at base to keep foliage dry, preventing fungal blights. Stake plants to support fruit loads. Feed with potassium-rich organic mulch during fruit set."
        )
    ),
    "diseases" to listOf(
        HandbookItem(
            title = "Early Blight (Alternaria solani)",
            desc = "Fungal disease targeting tomatoes and potatoes. Symptoms: Concentric dark target-like spots starting on older lower leaves.",
            details = "Organic Remedy: Spray 0.5% neem oil solution or baking soda mix (3g per liter). Prune lower leaves to enhance airflow and isolate infected plants."
        ),
        HandbookItem(
            title = "Common Rust (Puccinia sorghi)",
            desc = "Fungal pathogen attacking corn leaves. Symptoms: Elongated golden-brown pustules on both upper and lower leaf surfaces.",
            details = "Organic Remedy: Apply sulfur-based organic dust or copper sulfate solution. Plant rust-resistant seed hybrids."
        ),
        HandbookItem(
            title = "Bacterial Spot (Xanthomonas)",
            desc = "Bacterial infection causing lesions on peppers and tomatoes. Symptoms: Small water-soaked spots on foliage turning dark brown.",
            details = "Organic Remedy: Spray copper fungicide early in the season. Avoid overhead sprinkler irrigation. Remove crop residues after harvest."
        )
    ),
    "organic" to listOf(
        HandbookItem(
            title = "Neem Oil Bio-Pesticide",
            desc = "Natural repellent active against aphids, spider mites, whiteflies, and fungal powdery mildew.",
            details = "Preparation: Mix 5ml pure cold-pressed neem oil with 2ml organic liquid soap inside 1 liter of warm water. Spray thoroughly under leaf surfaces every 7 days."
        ),
        HandbookItem(
            title = "Garlic-Chili Insecticide Spray",
            desc = "Strong contact insecticide targeting caterpillars, leaf miners, and chewing beetles.",
            details = "Preparation: Grind 2 heads of garlic and 5 hot green chilies into a paste. Steep in 1 liter water for 24 hours. Strain liquid, dilute 1:5 with water, and spray."
        ),
        HandbookItem(
            title = "Buttermilk Fungal Spray",
            desc = "Sour buttermilk spray creates an acidic film that prevents powdery mildew and leaf blights.",
            details = "Preparation: Dilute sour buttermilk or curd with water in 1:10 ratio. Spray on crop foliage during dry mornings."
        )
    )
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun KnowledgeBaseScreen(modifier: Modifier = Modifier, onBack: (() -> Unit)? = null) {
    var activeCategory by remember { mutableStateOf("crops") }
    var searchQuery by remember { mutableStateOf("") }

    Column(
        modifier = modifier
            .fillMaxSize()
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
                Text(text = "Knowledge Base", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                Text(
                    text = "Offline farm guides, disease catalog & bio-remedies",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f)
                )
            }
        }

        // Search Field
        OutlinedTextField(
            value = searchQuery,
            onValueChange = { searchQuery = it },
            modifier = Modifier.fillMaxWidth(),
            placeholder = { Text("Search guides, diseases, or remedies...") },
            leadingIcon = { Icon(imageVector = Icons.Default.Search, contentDescription = "Search icon") },
            shape = RoundedCornerShape(12.dp),
            colors = TextFieldDefaults.outlinedTextFieldColors(
                focusedBorderColor = GreenPrimary,
                unfocusedBorderColor = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
            ),
            singleLine = true
        )

        // Categories selector tabs
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            val categories = listOf(
                "crops" to "Crop Guides",
                "diseases" to "Disease Catalog",
                "organic" to "Bio-Pesticides"
            )
            categories.forEach { (key, label) ->
                val isActive = activeCategory == key
                Card(
                    modifier = Modifier
                        .weight(1f)
                        .clickable { activeCategory = key },
                    shape = RoundedCornerShape(12.dp),
                    border = BorderStroke(1.dp, if (isActive) GreenPrimary else Color.Transparent),
                    colors = CardDefaults.cardColors(
                        containerColor = if (isActive) GreenPrimary.copy(alpha = 0.12f) else MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)
                    )
                ) {
                    Box(
                        modifier = Modifier.fillMaxWidth().padding(vertical = 12.dp),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = label,
                            fontWeight = FontWeight.SemiBold,
                            fontSize = 11.sp,
                            color = if (isActive) GreenPrimary else MaterialTheme.colorScheme.onSurface
                        )
                    }
                }
            }
        }

        // Cards List
        val currentList = KNOWLEDGE_HANDBOOK[activeCategory] ?: emptyList()
        val filteredList = currentList.filter {
            it.title.contains(searchQuery, ignoreCase = true) ||
            it.desc.contains(searchQuery, ignoreCase = true) ||
            it.details.contains(searchQuery, ignoreCase = true)
        }

        if (filteredList.isEmpty()) {
            Box(
                modifier = Modifier.fillMaxWidth().weight(1f),
                contentAlignment = Alignment.Center
            ) {
                Text(text = "No matching guides found", color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.5f))
            }
        } else {
            LazyVerticalGrid(
                columns = GridCells.Fixed(1),
                verticalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.weight(1f)
            ) {
                items(filteredList) { item ->
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(16.dp),
                        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f)),
                        border = BorderStroke(1.dp, MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f))
                    ) {
                        Column(
                            modifier = Modifier.padding(16.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Text(
                                text = item.title,
                                fontWeight = FontWeight.Bold,
                                fontSize = 15.sp,
                                color = GreenSecondary
                            )
                            Text(
                                text = item.desc,
                                fontSize = 13.sp,
                                color = MaterialTheme.colorScheme.onSurface
                            )
                            Divider(color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.08f))
                            Text(
                                text = item.details,
                                fontSize = 12.sp,
                                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f)
                            )
                        }
                    }
                }
            }
        }
    }
}
