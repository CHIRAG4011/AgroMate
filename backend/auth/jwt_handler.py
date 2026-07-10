"""
AgroMate Authentication — JWT Handler

JWT token creation and validation for API authentication.
"""

import logging
from datetime import datetime, timedelta, timezone
from typing import Optional

from jose import JWTError, jwt

from ..config import get_settings

logger = logging.getLogger(__name__)

ALGORITHM = "HS256"
ACCESS_TOKEN_EXPIRE_MINUTES = 60 * 24  # 24 hours


def create_access_token(
    data: dict,
    expires_delta: timedelta = None,
) -> str:
    """Create a JWT access token."""
    settings = get_settings()
    to_encode = data.copy()

    if expires_delta:
        expire = datetime.now(timezone.utc) + expires_delta
    else:
        expire = datetime.now(timezone.utc) + timedelta(minutes=ACCESS_TOKEN_EXPIRE_MINUTES)

    to_encode.update({"exp": expire})
    return jwt.encode(to_encode, settings.secret_key, algorithm=ALGORITHM)


def verify_token(token: str) -> Optional[dict]:
    """Verify and decode a JWT token."""
    settings = get_settings()
    try:
        payload = jwt.decode(token, settings.secret_key, algorithms=[ALGORITHM])
        return payload
    except JWTError as e:
        logger.warning(f"Token verification failed: {e}")
        return None


def create_refresh_token(user_id: int) -> str:
    """Create a longer-lived refresh token."""
    return create_access_token(
        {"sub": str(user_id), "type": "refresh"},
        expires_delta=timedelta(days=30),
    )
