import asyncio
import logging
import sys
from bleak import BleakScanner, BleakClient

# Configure logging
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)

SERVICE_UUID = "12345678-1234-5678-1234-56789abcde01"

async def main():
    logger.info("🔍 Scanning for AgroMate-Robot BLE advertisement...")
    
    # Scan for 10 seconds
    devices = await BleakScanner.discover(timeout=10.0)
    
    target_device = None
    for d in devices:
        name = d.name or "Unknown"
        logger.info(f"Found: Address={d.address}, Name={name}")
        if "AgroMate" in name:
            target_device = d
            
    if not target_device:
        logger.error("❌ AgroMate-Robot NOT found in scan. Make sure the robot is powered on and advertising.")
        return
        
    logger.info(f"✅ Found AgroMate-Robot: {target_device.name} ({target_device.address})")
    logger.info(f"🔌 Attempting connection to {target_device.address}...")
    
    try:
        async with BleakClient(target_device.address, timeout=15.0) as client:
            logger.info(f"🎉 Connected successfully: {client.is_connected}")
            logger.info("Reading GATT services...")
            for service in client.services:
                logger.info(f"Service: {service.uuid}")
                for char in service.characteristics:
                    logger.info(f"  Characteristic: {char.uuid} ({','.join(char.properties)})")
            logger.info("Disconnected clean.")
    except Exception as e:
        logger.error(f"❌ Connection failed: {e}", exc_info=True)

if __name__ == "__main__":
    asyncio.run(main())
