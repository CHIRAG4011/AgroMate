"""
AgroMate Database Models

SQLAlchemy ORM models for all persistent data including robots,
users, scans, diagnoses, chat history, and analytics.
"""

from datetime import datetime, timezone
from typing import Optional

from sqlalchemy import (
    Column,
    Integer,
    String,
    Float,
    Boolean,
    DateTime,
    Text,
    ForeignKey,
    JSON,
    Index,
    Enum as SAEnum,
)
from sqlalchemy.orm import DeclarativeBase, relationship, Mapped, mapped_column
from sqlalchemy.sql import func
import enum


class Base(DeclarativeBase):
    """Base class for all database models."""
    pass


class UserRole(str, enum.Enum):
    """User role enumeration."""
    ADMIN = "admin"
    USER = "user"
    VIEWER = "viewer"


class RobotStatus(str, enum.Enum):
    """Robot connection status."""
    ONLINE = "online"
    OFFLINE = "offline"
    SLEEPING = "sleeping"
    ERROR = "error"


class ScanType(str, enum.Enum):
    """Type of plant scan."""
    IDENTIFICATION = "identification"
    DISEASE = "disease"
    PEST = "pest"
    NUTRIENT = "nutrient"
    WEED = "weed"
    GROWTH = "growth"


# ============================================
# User Model
# ============================================

class User(Base):
    __tablename__ = "users"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    username: Mapped[str] = mapped_column(String(50), unique=True, nullable=False, index=True)
    email: Mapped[str] = mapped_column(String(255), unique=True, nullable=False, index=True)
    password_hash: Mapped[str] = mapped_column(String(255), nullable=False)
    full_name: Mapped[Optional[str]] = mapped_column(String(100))
    role: Mapped[str] = mapped_column(String(20), default=UserRole.USER.value)
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    language: Mapped[str] = mapped_column(String(10), default="en")
    created_at: Mapped[datetime] = mapped_column(
        DateTime, default=lambda: datetime.now(timezone.utc)
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime,
        default=lambda: datetime.now(timezone.utc),
        onupdate=lambda: datetime.now(timezone.utc),
    )

    # Relationships
    scans = relationship("Scan", back_populates="user", cascade="all, delete-orphan")
    chat_messages = relationship("ChatMessage", back_populates="user", cascade="all, delete-orphan")

    def __repr__(self):
        return f"<User(id={self.id}, username='{self.username}')>"


# ============================================
# Robot Model
# ============================================

class Robot(Base):
    __tablename__ = "robots"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    robot_id: Mapped[str] = mapped_column(String(50), unique=True, nullable=False, index=True)
    name: Mapped[str] = mapped_column(String(100), nullable=False)
    mac_address: Mapped[str] = mapped_column(String(17), unique=True, nullable=False)
    status: Mapped[str] = mapped_column(String(20), default=RobotStatus.OFFLINE.value)
    firmware_version: Mapped[Optional[str]] = mapped_column(String(20))
    battery_percent: Mapped[int] = mapped_column(Integer, default=0)
    last_seen: Mapped[Optional[datetime]] = mapped_column(DateTime)
    config: Mapped[Optional[dict]] = mapped_column(JSON, default=dict)
    created_at: Mapped[datetime] = mapped_column(
        DateTime, default=lambda: datetime.now(timezone.utc)
    )

    # Relationships
    scans = relationship("Scan", back_populates="robot", cascade="all, delete-orphan")

    def __repr__(self):
        return f"<Robot(id={self.id}, name='{self.name}', mac='{self.mac_address}')>"


# ============================================
# Scan Model
# ============================================

class Scan(Base):
    __tablename__ = "scans"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    user_id: Mapped[Optional[int]] = mapped_column(Integer, ForeignKey("users.id"), index=True)
    robot_id: Mapped[Optional[int]] = mapped_column(Integer, ForeignKey("robots.id"), index=True)
    scan_type: Mapped[str] = mapped_column(String(20), default=ScanType.IDENTIFICATION.value)
    image_path: Mapped[str] = mapped_column(String(500), nullable=False)
    thumbnail_path: Mapped[Optional[str]] = mapped_column(String(500))
    latitude: Mapped[Optional[float]] = mapped_column(Float)
    longitude: Mapped[Optional[float]] = mapped_column(Float)
    notes: Mapped[Optional[str]] = mapped_column(Text)
    created_at: Mapped[datetime] = mapped_column(
        DateTime, default=lambda: datetime.now(timezone.utc), index=True
    )

    # Relationships
    user = relationship("User", back_populates="scans")
    robot = relationship("Robot", back_populates="scans")
    diagnoses = relationship("Diagnosis", back_populates="scan", cascade="all, delete-orphan")

    def __repr__(self):
        return f"<Scan(id={self.id}, type='{self.scan_type}')>"


# ============================================
# Diagnosis Model
# ============================================

class Diagnosis(Base):
    __tablename__ = "diagnoses"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    scan_id: Mapped[int] = mapped_column(Integer, ForeignKey("scans.id"), nullable=False, index=True)
    plant_name: Mapped[Optional[str]] = mapped_column(String(200))
    plant_scientific_name: Mapped[Optional[str]] = mapped_column(String(200))
    disease_name: Mapped[Optional[str]] = mapped_column(String(200))
    pest_name: Mapped[Optional[str]] = mapped_column(String(200))
    confidence: Mapped[float] = mapped_column(Float, default=0.0)
    severity: Mapped[Optional[str]] = mapped_column(String(20))  # mild, moderate, severe
    affected_area: Mapped[Optional[float]] = mapped_column(Float)  # Percentage
    treatment: Mapped[Optional[str]] = mapped_column(Text)
    organic_treatment: Mapped[Optional[str]] = mapped_column(Text)
    fertilizer_recommendation: Mapped[Optional[str]] = mapped_column(Text)
    water_recommendation: Mapped[Optional[str]] = mapped_column(Text)
    additional_info: Mapped[Optional[dict]] = mapped_column(JSON)
    model_used: Mapped[Optional[str]] = mapped_column(String(100))
    processing_time_ms: Mapped[Optional[float]] = mapped_column(Float)
    created_at: Mapped[datetime] = mapped_column(
        DateTime, default=lambda: datetime.now(timezone.utc)
    )

    # Relationships
    scan = relationship("Scan", back_populates="diagnoses")

    def __repr__(self):
        return f"<Diagnosis(id={self.id}, plant='{self.plant_name}', disease='{self.disease_name}')>"


# ============================================
# Chat Message Model
# ============================================

class ChatMessage(Base):
    __tablename__ = "chat_messages"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    user_id: Mapped[Optional[int]] = mapped_column(Integer, ForeignKey("users.id"), index=True)
    robot_id: Mapped[Optional[str]] = mapped_column(String(50))
    role: Mapped[str] = mapped_column(String(20), nullable=False)  # user, assistant, system
    content: Mapped[str] = mapped_column(Text, nullable=False)
    language: Mapped[str] = mapped_column(String(10), default="en")
    context: Mapped[Optional[dict]] = mapped_column(JSON)
    created_at: Mapped[datetime] = mapped_column(
        DateTime, default=lambda: datetime.now(timezone.utc), index=True
    )

    # Relationships
    user = relationship("User", back_populates="chat_messages")

    def __repr__(self):
        return f"<ChatMessage(id={self.id}, role='{self.role}')>"


# ============================================
# Weather Cache Model
# ============================================

class WeatherCache(Base):
    __tablename__ = "weather_cache"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    location: Mapped[str] = mapped_column(String(200), nullable=False, index=True)
    latitude: Mapped[float] = mapped_column(Float, nullable=False)
    longitude: Mapped[float] = mapped_column(Float, nullable=False)
    data: Mapped[dict] = mapped_column(JSON, nullable=False)
    fetched_at: Mapped[datetime] = mapped_column(
        DateTime, default=lambda: datetime.now(timezone.utc)
    )

    def __repr__(self):
        return f"<WeatherCache(location='{self.location}')>"


# ============================================
# Dataset Registry Model
# ============================================

class DatasetRecord(Base):
    __tablename__ = "datasets"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    source: Mapped[str] = mapped_column(String(100), nullable=False, index=True)
    name: Mapped[str] = mapped_column(String(200), nullable=False)
    version: Mapped[Optional[str]] = mapped_column(String(50))
    description: Mapped[Optional[str]] = mapped_column(Text)
    local_path: Mapped[Optional[str]] = mapped_column(String(500))
    size_bytes: Mapped[Optional[int]] = mapped_column(Integer)
    num_samples: Mapped[Optional[int]] = mapped_column(Integer)
    license: Mapped[Optional[str]] = mapped_column(String(100))
    url: Mapped[Optional[str]] = mapped_column(String(500))
    is_downloaded: Mapped[bool] = mapped_column(Boolean, default=False)
    last_updated: Mapped[Optional[datetime]] = mapped_column(DateTime)
    metadata_extra: Mapped[Optional[dict]] = mapped_column(JSON)
    created_at: Mapped[datetime] = mapped_column(
        DateTime, default=lambda: datetime.now(timezone.utc)
    )

    def __repr__(self):
        return f"<DatasetRecord(source='{self.source}', name='{self.name}')>"


# ============================================
# Analytics Event Model
# ============================================

class AnalyticsRecord(Base):
    __tablename__ = "analytics"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    event_type: Mapped[str] = mapped_column(String(50), nullable=False, index=True)
    robot_id: Mapped[Optional[str]] = mapped_column(String(50))
    user_id: Mapped[Optional[int]] = mapped_column(Integer)
    data: Mapped[Optional[dict]] = mapped_column(JSON)
    duration_ms: Mapped[Optional[float]] = mapped_column(Float)
    timestamp: Mapped[datetime] = mapped_column(
        DateTime, default=lambda: datetime.now(timezone.utc), index=True
    )

    __table_args__ = (
        Index("ix_analytics_type_time", "event_type", "timestamp"),
    )

    def __repr__(self):
        return f"<AnalyticsRecord(type='{self.event_type}')>"
