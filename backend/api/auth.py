"""AgroMate Auth API — Login, registration, and token management."""

import logging
from fastapi import APIRouter, HTTPException, status
from pydantic import BaseModel, EmailStr

from ..auth.jwt_handler import create_access_token, create_refresh_token
from ..auth.password import hash_password, verify_password
from ..database.crud import get_session, create_user, get_user_by_username, get_user_by_email

logger = logging.getLogger(__name__)
router = APIRouter()


class LoginRequest(BaseModel):
    username: str
    password: str

class RegisterRequest(BaseModel):
    username: str
    email: str
    password: str
    full_name: str = ""
    language: str = "en"

class TokenResponse(BaseModel):
    access_token: str
    refresh_token: str
    token_type: str = "bearer"
    user: dict


@router.post("/login", response_model=TokenResponse)
async def login(request: LoginRequest):
    """Authenticate user and return JWT tokens."""
    async with await get_session() as session:
        user = await get_user_by_username(session, request.username)
        if not user or not verify_password(request.password, user.password_hash):
            raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid credentials")

        access_token = create_access_token({"sub": str(user.id), "username": user.username, "role": user.role})
        refresh_token = create_refresh_token(user.id)

        return TokenResponse(
            access_token=access_token,
            refresh_token=refresh_token,
            user={"id": user.id, "username": user.username, "email": user.email, "role": user.role},
        )


@router.post("/register", response_model=TokenResponse, status_code=status.HTTP_201_CREATED)
async def register(request: RegisterRequest):
    """Register a new user."""
    async with await get_session() as session:
        existing = await get_user_by_username(session, request.username)
        if existing:
            raise HTTPException(status_code=400, detail="Username already taken")

        existing_email = await get_user_by_email(session, request.email)
        if existing_email:
            raise HTTPException(status_code=400, detail="Email already registered")

        user = await create_user(
            session,
            username=request.username,
            email=request.email,
            password_hash=hash_password(request.password),
            full_name=request.full_name,
            language=request.language,
        )

        access_token = create_access_token({"sub": str(user.id), "username": user.username, "role": user.role})
        refresh_token = create_refresh_token(user.id)

        return TokenResponse(
            access_token=access_token,
            refresh_token=refresh_token,
            user={"id": user.id, "username": user.username, "email": user.email, "role": user.role},
        )
