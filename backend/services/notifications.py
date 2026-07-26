"""AgroMate Notification Service — Push notifications for mobile app."""

import logging
from typing import Optional

logger = logging.getLogger(__name__)


class NotificationService:
    """Push notification service for the Android app."""

    def __init__(self):
        self._subscribers: dict[str, list[dict]] = {}

    async def initialize(self):
        logger.info("Notification service initialized")

    async def send_notification(
        self, user_id: str, title: str, body: str, data: dict = None
    ) -> bool:
        """Send a push notification to a user."""
        logger.info(f"Notification -> {user_id}: {title}")
        # Store for WebSocket delivery
        if user_id not in self._subscribers:
            self._subscribers[user_id] = []
        self._subscribers[user_id].append({
            "title": title,
            "body": body,
            "data": data or {},
        })
        return True

    async def send_robot_alert(
        self, robot_name: str, alert_type: str, message: str
    ):
        """Send a robot-related alert to all subscribers."""
        await self.send_notification(
            "broadcast",
            f"🤖 {robot_name} — {alert_type}",
            message,
        )

    def get_pending(self, user_id: str) -> list[dict]:
        """Get and clear pending notifications for a user."""
        notifications = self._subscribers.pop(user_id, [])
        return notifications

    @property
    def is_ready(self) -> bool:
        return True
