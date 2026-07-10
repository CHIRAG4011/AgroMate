"""
AgroMate Analytics Utility

Collects and aggregates usage analytics for dashboards,
reporting, and system monitoring.
"""

import time
import logging
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Optional
from datetime import datetime, timezone

logger = logging.getLogger(__name__)


@dataclass
class AnalyticsEvent:
    """Represents a single analytics event."""

    event_type: str
    timestamp: float = field(default_factory=time.time)
    robot_id: Optional[str] = None
    user_id: Optional[str] = None
    data: dict = field(default_factory=dict)
    duration_ms: Optional[float] = None


class AnalyticsCollector:
    """
    Collects and aggregates analytics events in memory.

    Events are periodically flushed to the database for persistence.
    """

    def __init__(self, flush_interval: int = 60, max_buffer_size: int = 1000):
        self.flush_interval = flush_interval
        self.max_buffer_size = max_buffer_size
        self._buffer: list[AnalyticsEvent] = []
        self._counters: dict[str, int] = defaultdict(int)
        self._timers: dict[str, list[float]] = defaultdict(list)
        self._start_times: dict[str, float] = {}

    def track(
        self,
        event_type: str,
        robot_id: str = None,
        user_id: str = None,
        data: dict = None,
    ):
        """Record an analytics event."""
        event = AnalyticsEvent(
            event_type=event_type,
            robot_id=robot_id,
            user_id=user_id,
            data=data or {},
        )
        self._buffer.append(event)
        self._counters[event_type] += 1

        if len(self._buffer) >= self.max_buffer_size:
            self.flush()

    def start_timer(self, name: str):
        """Start a named timer for measuring operation duration."""
        self._start_times[name] = time.time()

    def stop_timer(self, name: str) -> Optional[float]:
        """Stop a named timer and record the duration."""
        start = self._start_times.pop(name, None)
        if start is None:
            return None

        duration_ms = (time.time() - start) * 1000
        self._timers[name].append(duration_ms)
        return duration_ms

    def get_summary(self) -> dict:
        """Get analytics summary for dashboard display."""
        timer_stats = {}
        for name, durations in self._timers.items():
            if durations:
                timer_stats[name] = {
                    "count": len(durations),
                    "avg_ms": sum(durations) / len(durations),
                    "min_ms": min(durations),
                    "max_ms": max(durations),
                }

        return {
            "total_events": sum(self._counters.values()),
            "event_counts": dict(self._counters),
            "performance": timer_stats,
            "buffer_size": len(self._buffer),
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }

    def flush(self) -> list[AnalyticsEvent]:
        """Flush buffered events (for database persistence)."""
        events = self._buffer.copy()
        self._buffer.clear()
        logger.debug(f"Flushed {len(events)} analytics events")
        return events

    def reset(self):
        """Reset all analytics data."""
        self._buffer.clear()
        self._counters.clear()
        self._timers.clear()
        self._start_times.clear()


# Global analytics instance
analytics = AnalyticsCollector()
