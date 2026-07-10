"""AgroMate Plant API Service — External API integrations for Plant.id and Pl@ntNet."""

import logging
from ..config import get_settings

logger = logging.getLogger(__name__)


class PlantAPIService:
    """Wrapper for external plant identification APIs."""

    def __init__(self):
        self.settings = get_settings()

    async def initialize(self):
        apis = []
        if self.settings.plant_id_api_key:
            apis.append("Plant.id")
        if self.settings.plantnet_api_key:
            apis.append("Pl@ntNet")
        if self.settings.trefle_api_key:
            apis.append("Trefle")
        logger.info(f"Plant API service initialized. Available APIs: {apis or ['None configured']}")

    async def get_plant_details(self, scientific_name: str) -> dict | None:
        """Get detailed plant information from Trefle API."""
        if not self.settings.trefle_api_key:
            return None
        try:
            import httpx
            async with httpx.AsyncClient(timeout=15) as client:
                response = await client.get(
                    f"https://trefle.io/api/v1/plants/search",
                    params={"token": self.settings.trefle_api_key, "q": scientific_name},
                )
                if response.status_code == 200:
                    data = response.json()
                    results = data.get("data", [])
                    if results:
                        return results[0]
        except Exception as e:
            logger.error(f"Trefle API error: {e}")
        return None

    @property
    def is_ready(self) -> bool:
        return bool(
            self.settings.plant_id_api_key
            or self.settings.plantnet_api_key
            or self.settings.trefle_api_key
        )
