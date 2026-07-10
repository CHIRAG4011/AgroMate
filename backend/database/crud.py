"""
AgroMate Database CRUD Operations

Async CRUD (Create, Read, Update, Delete) operations for all database models.
Uses SQLAlchemy async sessions for non-blocking database access.
"""

import logging
from datetime import datetime, timezone, timedelta
from typing import Optional, Sequence

from sqlalchemy import select, func, delete, update
from sqlalchemy.ext.asyncio import AsyncSession, create_async_engine, async_sessionmaker
from sqlalchemy.orm import selectinload

from .models import (
    Base, User, Robot, Scan, Diagnosis, ChatMessage,
    WeatherCache, DatasetRecord, AnalyticsRecord,
    RobotStatus, ScanType,
)

logger = logging.getLogger(__name__)

# Global engine and session factory
_engine = None
_session_factory = None


async def init_database(database_url: str):
    """Initialize the database engine and create tables."""
    global _engine, _session_factory

    _engine = create_async_engine(
        database_url,
        echo=False,
        pool_pre_ping=True,
    )
    _session_factory = async_sessionmaker(_engine, expire_on_commit=False)

    async with _engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)

    logger.info("Database initialized successfully")


async def get_session() -> AsyncSession:
    """Get an async database session."""
    if _session_factory is None:
        raise RuntimeError("Database not initialized. Call init_database() first.")
    return _session_factory()


async def close_database():
    """Close the database engine."""
    global _engine
    if _engine:
        await _engine.dispose()
        logger.info("Database connection closed")


# ============================================
# User CRUD
# ============================================

async def create_user(
    session: AsyncSession,
    username: str,
    email: str,
    password_hash: str,
    full_name: str = None,
    role: str = "user",
    language: str = "en",
) -> User:
    """Create a new user."""
    user = User(
        username=username,
        email=email,
        password_hash=password_hash,
        full_name=full_name,
        role=role,
        language=language,
    )
    session.add(user)
    await session.commit()
    await session.refresh(user)
    return user


async def get_user_by_id(session: AsyncSession, user_id: int) -> Optional[User]:
    """Get user by ID."""
    return await session.get(User, user_id)


async def get_user_by_username(session: AsyncSession, username: str) -> Optional[User]:
    """Get user by username."""
    result = await session.execute(
        select(User).where(User.username == username)
    )
    return result.scalar_one_or_none()


async def get_user_by_email(session: AsyncSession, email: str) -> Optional[User]:
    """Get user by email."""
    result = await session.execute(
        select(User).where(User.email == email)
    )
    return result.scalar_one_or_none()


async def get_all_users(session: AsyncSession) -> Sequence[User]:
    """Get all users."""
    result = await session.execute(select(User).order_by(User.created_at.desc()))
    return result.scalars().all()


# ============================================
# Robot CRUD
# ============================================

async def create_or_update_robot(
    session: AsyncSession,
    robot_id: str,
    name: str,
    mac_address: str,
    **kwargs,
) -> Robot:
    """Create a new robot or update an existing one."""
    result = await session.execute(
        select(Robot).where(Robot.robot_id == robot_id)
    )
    robot = result.scalar_one_or_none()

    if robot:
        robot.name = name
        robot.mac_address = mac_address
        for key, value in kwargs.items():
            if hasattr(robot, key):
                setattr(robot, key, value)
    else:
        robot = Robot(robot_id=robot_id, name=name, mac_address=mac_address, **kwargs)
        session.add(robot)

    await session.commit()
    await session.refresh(robot)
    return robot


async def get_robot_by_id(session: AsyncSession, robot_id: str) -> Optional[Robot]:
    """Get robot by robot_id."""
    result = await session.execute(
        select(Robot).where(Robot.robot_id == robot_id)
    )
    return result.scalar_one_or_none()


async def get_all_robots(session: AsyncSession) -> Sequence[Robot]:
    """Get all registered robots."""
    result = await session.execute(select(Robot).order_by(Robot.name))
    return result.scalars().all()


async def update_robot_status(
    session: AsyncSession,
    robot_id: str,
    status: RobotStatus,
    battery_percent: int = None,
):
    """Update robot status."""
    values = {
        "status": status.value,
        "last_seen": datetime.now(timezone.utc),
    }
    if battery_percent is not None:
        values["battery_percent"] = battery_percent

    await session.execute(
        update(Robot).where(Robot.robot_id == robot_id).values(**values)
    )
    await session.commit()


# ============================================
# Scan CRUD
# ============================================

async def create_scan(
    session: AsyncSession,
    image_path: str,
    scan_type: str = ScanType.IDENTIFICATION.value,
    user_id: int = None,
    robot_id: int = None,
    latitude: float = None,
    longitude: float = None,
    notes: str = None,
    thumbnail_path: str = None,
) -> Scan:
    """Create a new scan record."""
    scan = Scan(
        user_id=user_id,
        robot_id=robot_id,
        scan_type=scan_type,
        image_path=image_path,
        thumbnail_path=thumbnail_path,
        latitude=latitude,
        longitude=longitude,
        notes=notes,
    )
    session.add(scan)
    await session.commit()
    await session.refresh(scan)
    return scan


async def get_scan_by_id(session: AsyncSession, scan_id: int) -> Optional[Scan]:
    """Get scan with diagnoses."""
    result = await session.execute(
        select(Scan)
        .options(selectinload(Scan.diagnoses))
        .where(Scan.id == scan_id)
    )
    return result.scalar_one_or_none()


async def get_scans(
    session: AsyncSession,
    user_id: int = None,
    scan_type: str = None,
    limit: int = 50,
    offset: int = 0,
) -> Sequence[Scan]:
    """Get scans with optional filtering."""
    query = select(Scan).options(selectinload(Scan.diagnoses))

    if user_id:
        query = query.where(Scan.user_id == user_id)
    if scan_type:
        query = query.where(Scan.scan_type == scan_type)

    query = query.order_by(Scan.created_at.desc()).limit(limit).offset(offset)
    result = await session.execute(query)
    return result.scalars().all()


async def get_scan_count(session: AsyncSession, user_id: int = None) -> int:
    """Get total scan count."""
    query = select(func.count(Scan.id))
    if user_id:
        query = query.where(Scan.user_id == user_id)
    result = await session.execute(query)
    return result.scalar() or 0


# ============================================
# Diagnosis CRUD
# ============================================

async def create_diagnosis(
    session: AsyncSession,
    scan_id: int,
    **kwargs,
) -> Diagnosis:
    """Create a new diagnosis record."""
    diagnosis = Diagnosis(scan_id=scan_id, **kwargs)
    session.add(diagnosis)
    await session.commit()
    await session.refresh(diagnosis)
    return diagnosis


async def get_disease_statistics(
    session: AsyncSession,
    days: int = 30,
) -> list[dict]:
    """Get disease frequency statistics for the last N days."""
    cutoff = datetime.now(timezone.utc) - timedelta(days=days)
    result = await session.execute(
        select(
            Diagnosis.disease_name,
            func.count(Diagnosis.id).label("count"),
            func.avg(Diagnosis.confidence).label("avg_confidence"),
        )
        .where(Diagnosis.created_at >= cutoff)
        .where(Diagnosis.disease_name.isnot(None))
        .group_by(Diagnosis.disease_name)
        .order_by(func.count(Diagnosis.id).desc())
        .limit(20)
    )
    return [
        {"disease": row[0], "count": row[1], "avg_confidence": round(row[2], 3)}
        for row in result.all()
    ]


# ============================================
# Chat CRUD
# ============================================

async def create_chat_message(
    session: AsyncSession,
    role: str,
    content: str,
    user_id: int = None,
    robot_id: str = None,
    language: str = "en",
    context: dict = None,
) -> ChatMessage:
    """Create a new chat message."""
    msg = ChatMessage(
        user_id=user_id,
        robot_id=robot_id,
        role=role,
        content=content,
        language=language,
        context=context,
    )
    session.add(msg)
    await session.commit()
    await session.refresh(msg)
    return msg


async def get_chat_history(
    session: AsyncSession,
    user_id: int = None,
    robot_id: str = None,
    limit: int = 50,
) -> Sequence[ChatMessage]:
    """Get chat history."""
    query = select(ChatMessage)

    if user_id:
        query = query.where(ChatMessage.user_id == user_id)
    if robot_id:
        query = query.where(ChatMessage.robot_id == robot_id)

    query = query.order_by(ChatMessage.created_at.desc()).limit(limit)
    result = await session.execute(query)
    return list(reversed(result.scalars().all()))


# ============================================
# Weather Cache CRUD
# ============================================

async def get_cached_weather(
    session: AsyncSession,
    location: str,
    max_age_minutes: int = 30,
) -> Optional[dict]:
    """Get cached weather data if not expired."""
    cutoff = datetime.now(timezone.utc) - timedelta(minutes=max_age_minutes)
    result = await session.execute(
        select(WeatherCache)
        .where(WeatherCache.location == location)
        .where(WeatherCache.fetched_at >= cutoff)
        .order_by(WeatherCache.fetched_at.desc())
        .limit(1)
    )
    cache = result.scalar_one_or_none()
    return cache.data if cache else None


async def cache_weather(
    session: AsyncSession,
    location: str,
    latitude: float,
    longitude: float,
    data: dict,
):
    """Store weather data in cache."""
    cache = WeatherCache(
        location=location,
        latitude=latitude,
        longitude=longitude,
        data=data,
    )
    session.add(cache)
    await session.commit()


# ============================================
# Dataset CRUD
# ============================================

async def register_dataset(
    session: AsyncSession,
    source: str,
    name: str,
    **kwargs,
) -> DatasetRecord:
    """Register a dataset in the database."""
    result = await session.execute(
        select(DatasetRecord)
        .where(DatasetRecord.source == source)
        .where(DatasetRecord.name == name)
    )
    existing = result.scalar_one_or_none()

    if existing:
        for key, value in kwargs.items():
            if hasattr(existing, key):
                setattr(existing, key, value)
        await session.commit()
        return existing

    record = DatasetRecord(source=source, name=name, **kwargs)
    session.add(record)
    await session.commit()
    await session.refresh(record)
    return record


async def get_all_datasets(session: AsyncSession) -> Sequence[DatasetRecord]:
    """Get all registered datasets."""
    result = await session.execute(
        select(DatasetRecord).order_by(DatasetRecord.source, DatasetRecord.name)
    )
    return result.scalars().all()


# ============================================
# Analytics CRUD
# ============================================

async def store_analytics_batch(
    session: AsyncSession,
    events: list[dict],
):
    """Store a batch of analytics events."""
    records = [AnalyticsRecord(**event) for event in events]
    session.add_all(records)
    await session.commit()


async def get_analytics_summary(
    session: AsyncSession,
    days: int = 7,
) -> dict:
    """Get analytics summary for dashboard."""
    cutoff = datetime.now(timezone.utc) - timedelta(days=days)

    # Total scans
    scan_count_res = await session.execute(
        select(func.count(Scan.id)).where(Scan.created_at >= cutoff)
    )
    total_scans = scan_count_res.scalar() or 0

    # Total diagnoses
    diagnosis_count_res = await session.execute(
        select(func.count(Diagnosis.id)).where(Diagnosis.created_at >= cutoff)
    )
    total_diagnoses = diagnosis_count_res.scalar() or 0

    # Total chats
    chat_count_res = await session.execute(
        select(func.count(ChatMessage.id)).where(ChatMessage.created_at >= cutoff)
    )
    total_chats = chat_count_res.scalar() or 0

    # Healthy scans (where disease name is empty, starts with "healthy" or is "Healthy")
    healthy_count_res = await session.execute(
        select(func.count(Diagnosis.id))
        .where(Diagnosis.created_at >= cutoff)
        .where((Diagnosis.disease_name == "") | (Diagnosis.disease_name == "Healthy") | (Diagnosis.disease_name.ilike("%healthy%")))
    )
    healthy_scans = healthy_count_res.scalar() or 0

    # Average confidence
    avg_conf_res = await session.execute(
        select(func.avg(Diagnosis.confidence))
        .where(Diagnosis.created_at >= cutoff)
        .where(Diagnosis.confidence > 0)
    )
    avg_confidence = float(avg_conf_res.scalar() or 0.0)

    return {
        "period_days": days,
        "total_scans": total_scans,
        "total_diagnoses": total_diagnoses,
        "total_diseases": total_diagnoses,
        "total_chats": total_chats,
        "healthy_scans": healthy_scans,
        "avg_confidence": avg_confidence,
    }
