"""
AgroMate Database Migrations

Simple migration system using SQLAlchemy create_all for initial setup
and manual migration scripts for schema updates.
"""

import logging
from sqlalchemy.ext.asyncio import AsyncEngine

from .models import Base

logger = logging.getLogger(__name__)


async def run_migrations(engine: AsyncEngine):
    """
    Run database migrations.

    For initial development, this simply creates all tables.
    For production, replace with Alembic migrations.
    """
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    logger.info("Database migrations complete")


async def seed_default_data(session):
    """Seed the database with default data for first-time setup."""
    from .crud import get_user_by_username, create_user
    from ..auth.password import hash_password

    # Create default admin user if not exists
    admin = await get_user_by_username(session, "admin")
    if not admin:
        await create_user(
            session,
            username="admin",
            email="admin@agromate.local",
            password_hash=hash_password("admin123"),
            full_name="AgroMate Admin",
            role="admin",
        )
        logger.info("Created default admin user (username: admin)")
