"""
AgroMate Plant Knowledge Base

Searchable agricultural knowledge database used by the chatbot
for context-aware responses about plants, diseases, and farming.
"""

import logging


from ..database.seed import COMMON_PLANTS, COMMON_DISEASES, NUTRIENT_DEFICIENCIES, COMMON_PESTS

logger = logging.getLogger(__name__)


class PlantKnowledgeBase:
    """
    In-memory searchable knowledge base for agricultural information.
    
    Indexes plant species, diseases, treatments, and nutrient info
    for fast retrieval during chatbot conversations.
    """

    def __init__(self):
        self._entries: list[dict] = []
        self._initialized = False

    def initialize(self):
        """Build the knowledge base from seed data."""
        self._entries.clear()

        # Index plants
        for plant in COMMON_PLANTS:
            self._entries.append({
                "topic": plant["name"],
                "type": "plant",
                "keywords": [
                    plant["name"].lower(),
                    plant["scientific"].lower(),
                    plant["category"].lower(),
                ],
                "info": f"{plant['name']} ({plant['scientific']}) is a {plant['category'].lower()} crop.",
            })

        # Index diseases
        for disease in COMMON_DISEASES:
            self._entries.append({
                "topic": f"{disease['plant']} - {disease['disease']}",
                "type": "disease",
                "keywords": [
                    disease["plant"].lower(),
                    disease["disease"].lower(),
                    disease.get("pathogen", "").lower(),
                ],
                "info": f"{disease['disease']} in {disease['plant']}: {disease['symptoms']}",
                "treatment": disease.get("treatment", ""),
                "organic": disease.get("organic_treatment", ""),
                "pathogen": disease.get("pathogen", ""),
            })

        # Index nutrient deficiencies
        for nutrient in NUTRIENT_DEFICIENCIES:
            self._entries.append({
                "topic": f"Nutrient: {nutrient['nutrient']}",
                "type": "nutrient",
                "keywords": [
                    nutrient["nutrient"].lower(),
                    *nutrient["symptoms"].lower().split(),
                ],
                "info": f"{nutrient['nutrient']} deficiency: {nutrient['symptoms']}",
                "treatment": nutrient.get("treatment", ""),
                "organic": nutrient.get("organic_option", ""),
            })

        # Index agricultural pests
        for pest in COMMON_PESTS:
            self._entries.append({
                "topic": f"Pest: {pest['pest']}",
                "type": "pest",
                "keywords": [
                    pest["pest"].lower(),
                    *pest.get("crops_affected", "").lower().split(", "),
                    *pest.get("symptoms", "").lower().split(),
                ],
                "info": f"{pest['pest']} (affects {pest.get('crops_affected', 'crops')}): {pest['symptoms']}",
                "treatment": pest.get("treatment", ""),
                "organic": pest.get("organic_treatment", ""),
                "prevention": pest.get("prevention", ""),
            })

        self._initialized = True
        logger.info(f"Knowledge base initialized with {len(self._entries)} entries")

    def search(self, query: str, max_results: int = 3) -> list[dict]:
        """
        Search the knowledge base for relevant entries.

        Uses simple keyword matching with scoring.
        """
        if not self._initialized:
            self.initialize()

        # Clean query by removing punctuation
        cleaned_query = query.lower()
        for char in [".", ",", "?", "!", "-", "_", "(", ")"]:
            cleaned_query = cleaned_query.replace(char, " ")
        
        query_words = set(cleaned_query.split())
        scored = []

        # Common helper words to ignore for substring matching
        stop_words = {"can", "me", "to", "in", "an", "on", "us", "is", "at", "by", "for", "he", "it", "of", "or", "so", "the", "you", "your", "my"}

        for entry in self._entries:
            score = 0
            for keyword in entry["keywords"]:
                keyword_words = set(keyword.split())
                for word in query_words:
                    if word in stop_words:
                        if word in keyword_words or word == keyword:
                            score += 3
                        continue
                    
                    if word == keyword:
                        score += 5
                    elif word in keyword_words:
                        score += 3
                    elif len(word) >= 4 and (word in keyword or keyword in word):
                        score += 1

            if score > 0:
                scored.append((score, entry))

        # Sort by score descending
        scored.sort(key=lambda x: x[0], reverse=True)

        return [entry for _, entry in scored[:max_results]]

    def get_plant_info(self, plant_name: str) -> dict | None:
        """Get detailed information about a specific plant."""
        for entry in self._entries:
            if entry["type"] == "plant" and plant_name.lower() in entry["topic"].lower():
                return entry
        return None

    def get_disease_info(self, disease_name: str, plant_name: str = None) -> dict | None:
        """Get information about a specific disease."""
        for entry in self._entries:
            if entry["type"] == "disease":
                if disease_name.lower() in entry["topic"].lower():
                    if plant_name is None or plant_name.lower() in entry["topic"].lower():
                        return entry
        return None

    @property
    def entry_count(self) -> int:
        return len(self._entries)
