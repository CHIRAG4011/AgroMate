"""
AgroMate API Router — Main router aggregating all API endpoints.
"""

from fastapi import APIRouter

from . import auth, plants, scans, robots, chat, analytics, websocket, voice, i18n

api_router = APIRouter(prefix="/api")

# Include sub-routers
api_router.include_router(auth.router, prefix="/auth", tags=["Authentication"])
api_router.include_router(plants.router, prefix="/plants", tags=["Plants"])
api_router.include_router(scans.router, prefix="/scans", tags=["Scans"])
api_router.include_router(robots.router, prefix="/robots", tags=["Robots"])
api_router.include_router(chat.router, prefix="/chat", tags=["Chat"])
api_router.include_router(analytics.router, prefix="/analytics", tags=["Analytics"])
api_router.include_router(voice.router, prefix="/voice", tags=["Voice"])
api_router.include_router(i18n.router, prefix="/i18n", tags=["Internationalization"])
