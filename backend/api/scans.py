"""AgroMate Scans API — Scan history management."""

import logging
from fastapi import APIRouter, HTTPException

from ..database.crud import get_session, get_scans, get_scan_by_id, get_scan_count

logger = logging.getLogger(__name__)
router = APIRouter()


@router.get("/")
async def list_scans(limit: int = 50, offset: int = 0, scan_type: str = None):
    """Get scan history with optional filtering."""
    async with await get_session() as session:
        scans = await get_scans(session, scan_type=scan_type, limit=limit, offset=offset)
        total = await get_scan_count(session)
        return {
            "status": "success",
            "total": total,
            "scans": [
                {
                    "id": s.id,
                    "scan_type": s.scan_type,
                    "image_path": s.image_path,
                    "created_at": s.created_at.isoformat(),
                    "diagnoses": [
                        {
                            "plant_name": d.plant_name,
                            "disease_name": d.disease_name,
                            "confidence": d.confidence,
                            "severity": d.severity,
                        }
                        for d in s.diagnoses
                    ],
                }
                for s in scans
            ],
        }


@router.get("/{scan_id}")
async def get_scan(scan_id: int):
    """Get scan details with diagnoses."""
    async with await get_session() as session:
        scan = await get_scan_by_id(session, scan_id)
        if not scan:
            raise HTTPException(status_code=404, detail="Scan not found")
        return {
            "status": "success",
            "scan": {
                "id": scan.id,
                "scan_type": scan.scan_type,
                "image_path": scan.image_path,
                "latitude": scan.latitude,
                "longitude": scan.longitude,
                "notes": scan.notes,
                "created_at": scan.created_at.isoformat(),
                "diagnoses": [
                    {
                        "id": d.id,
                        "plant_name": d.plant_name,
                        "disease_name": d.disease_name,
                        "confidence": d.confidence,
                        "severity": d.severity,
                        "treatment": d.treatment,
                        "organic_treatment": d.organic_treatment,
                        "processing_time_ms": d.processing_time_ms,
                    }
                    for d in scan.diagnoses
                ],
            },
        }
