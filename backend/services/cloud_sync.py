"""AgroMate Cloud Sync Service — Future cloud synchronization support."""

import logging

logger = logging.getLogger(__name__)


class CloudSyncService:
    """Cloud synchronization service (placeholder for future implementation)."""

    def __init__(self):
        self._provider = "none"

    async def initialize(self):
        logger.info("Cloud sync service initialized (no provider configured)")

    async def sync_scans(self, scans: list) -> bool:
        """Sync scan data to cloud."""
        logger.debug("Cloud sync not configured — skipping scan sync")
        return False

    async def sync_settings(self, settings: dict) -> bool:
        """Sync settings to cloud."""
        return False

    async def pull_updates(self) -> dict | None:
        """Pull updates from cloud."""
        return None

    @property
    def is_ready(self) -> bool:
        return False
