"""
AgroMate BLE Robot Discovery

Scans for nearby AgroMate robots using BLE advertisement data,
manages pairing, and tracks discovered devices.
"""

import asyncio
import logging
import platform
from typing import Optional, Callable
from dataclasses import dataclass

try:
    from bleak import BleakScanner
    from bleak.backends.device import BLEDevice
    from bleak.backends.scanner import AdvertisementData

    BLEAK_AVAILABLE = True
except ImportError:
    BLEAK_AVAILABLE = False

from .protocol import BLE_SERVICE_UUIDS
from .connection import ConnectionManager, RobotConnection

logger = logging.getLogger(__name__)

# AgroMate manufacturer-specific data prefix
AGROMATE_MANUFACTURER_ID = 0xA6AD
AGROMATE_DEVICE_PREFIX = "AgroMate"


@dataclass
class DiscoveredDevice:
    """Represents a BLE device found during scanning."""

    address: str
    name: str
    rssi: int
    is_agromate: bool = False
    service_uuids: list[str] = None

    def __post_init__(self):
        if self.service_uuids is None:
            self.service_uuids = []


class RobotDiscovery:
    """
    BLE scanner for discovering nearby AgroMate robots.

    Continuously scans for BLE devices advertising the AgroMate service UUID,
    and registers them with the ConnectionManager for pairing.
    """

    def __init__(
        self,
        connection_manager: ConnectionManager,
        scan_timeout: float = 10.0,
        scan_interval: float = 30.0,
    ):
        self.connection_manager = connection_manager
        self.scan_timeout = scan_timeout
        self.scan_interval = scan_interval
        self._scanner: Optional[BleakScanner] = None
        self._scanning = False
        self._scan_task: Optional[asyncio.Task] = None
        self._discovered: dict[str, DiscoveredDevice] = {}
        self._on_discovered_callbacks: list[Callable] = []

        if not BLEAK_AVAILABLE:
            logger.warning(
                "Bleak not installed — BLE discovery unavailable. "
                "Install with: pip install bleak"
            )

    @property
    def is_scanning(self) -> bool:
        """Whether the scanner is currently active."""
        return self._scanning

    @property
    def discovered_devices(self) -> dict[str, DiscoveredDevice]:
        """Get all discovered devices from last scan."""
        return self._discovered.copy()

    def on_robot_discovered(self, callback: Callable):
        """Register a callback for when a new robot is discovered."""
        self._on_discovered_callbacks.append(callback)

    async def scan_once(self) -> list[DiscoveredDevice]:
        """
        Perform a single BLE scan and return discovered AgroMate devices.
        """
        if not BLEAK_AVAILABLE:
            logger.warning("Bleak not available, returning empty scan")
            return []

        logger.info(f"Starting BLE scan (timeout={self.scan_timeout}s)...")
        discovered = []

        try:
            devices = await BleakScanner.discover(
                timeout=self.scan_timeout,
                return_adv=True,
            )

            for device, adv_data in devices.values():
                disc = self._process_device(device, adv_data)
                if disc and disc.is_agromate:
                    discovered.append(disc)
                    self._discovered[disc.address] = disc

                    # Register with connection manager
                    robot = self.connection_manager.register_robot(
                        robot_id=disc.address,
                        name=disc.name,
                        mac_address=disc.address,
                        rssi=disc.rssi,
                        ble_device=device,
                    )

                    if robot:
                        for callback in self._on_discovered_callbacks:
                            try:
                                result = callback(robot)
                                if asyncio.iscoroutine(result):
                                    await result
                            except Exception as e:
                                logger.error(f"Discovery callback error: {e}")

            logger.info(
                f"Scan complete: {len(discovered)} AgroMate robot(s) found "
                f"out of {len(devices)} total devices"
            )

        except Exception as e:
            logger.error(f"BLE scan error: {e}")

        return discovered

    def _process_device(
        self, device: "BLEDevice", adv_data: "AdvertisementData"
    ) -> Optional[DiscoveredDevice]:
        """Process a discovered BLE device and check if it's an AgroMate robot."""
        name = device.name or adv_data.local_name or "Unknown"
        service_uuids = adv_data.service_uuids or []

        is_agromate = False

        # Check 1: Device name starts with AgroMate prefix
        if name.startswith(AGROMATE_DEVICE_PREFIX):
            is_agromate = True

        # Check 2: Service UUID matches
        for uuid in service_uuids:
            if uuid.lower() in [
                v.lower() for v in BLE_SERVICE_UUIDS.values()
            ]:
                is_agromate = True
                break

        # Check 3: Manufacturer data contains AgroMate ID
        if adv_data.manufacturer_data:
            for mfr_id in adv_data.manufacturer_data:
                if mfr_id == AGROMATE_MANUFACTURER_ID:
                    is_agromate = True
                    break

        return DiscoveredDevice(
            address=device.address,
            name=name,
            rssi=adv_data.rssi or -100,
            is_agromate=is_agromate,
            service_uuids=service_uuids,
        )

    async def start_continuous_scan(self):
        """Start background continuous scanning."""
        if self._scanning:
            logger.warning("Scanner already running")
            return

        self._scanning = True
        self._scan_task = asyncio.create_task(self._scan_loop())
        logger.info("Continuous BLE scanning started")

    async def stop_continuous_scan(self):
        """Stop background scanning."""
        self._scanning = False
        if self._scan_task:
            self._scan_task.cancel()
            try:
                await self._scan_task
            except asyncio.CancelledError:
                pass
        logger.info("Continuous BLE scanning stopped")

    async def _scan_loop(self):
        """Background scanning loop."""
        import sys
        from .connection import ConnectionState
        while self._scanning:
            try:
                if sys.platform == "win32":
                    has_active = any(
                        r.state == ConnectionState.ACTIVE
                        for r in self.connection_manager._robots.values()
                    )
                    if has_active:
                        await asyncio.sleep(self.scan_interval)
                        continue

                await self.scan_once()
                await asyncio.sleep(self.scan_interval)
            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"Scan loop error: {e}")
                await asyncio.sleep(5)

    def get_bluetooth_info(self) -> dict:
        """Get information about the Bluetooth adapter."""
        return {
            "platform": platform.system(),
            "bleak_available": BLEAK_AVAILABLE,
            "scanning": self._scanning,
            "discovered_count": len(self._discovered),
            "agromate_count": sum(
                1 for d in self._discovered.values() if d.is_agromate
            ),
        }
