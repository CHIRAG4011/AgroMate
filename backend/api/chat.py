"""AgroMate Chat API — V2 AI chatbot conversation endpoints with two-tier responses."""

import logging
from fastapi import APIRouter
from pydantic import BaseModel
from typing import Optional

from ..chatbot.engine import ChatbotEngine

logger = logging.getLogger(__name__)
router = APIRouter()

# Shared chatbot instance
_chatbot = ChatbotEngine()


class ChatRequest(BaseModel):
    message: str
    session_id: str = "default"
    language: str = "en"
    scan_context: Optional[dict] = None
    response_type: str = "summary"  # "summary" or "detailed"


@router.post("/")
async def send_message(request: ChatRequest):
    """
    Send a message to the AI chatbot.
    
    Returns a short summary by default. Set response_type="detailed" for
    comprehensive analysis, or use the /details endpoint.
    """
    # 1. Save user message to database
    from ..database.crud import get_session, create_chat_message
    try:
        async with await get_session() as session:
            await create_chat_message(
                session=session,
                role="user",
                content=request.message,
                language=request.language,
                context=request.scan_context,
            )
    except Exception as e:
        logger.error(f"Failed to save user message to DB: {e}")

    # 2. Get AI Response
    if not _chatbot.is_ready:
        await _chatbot.initialize()

    response = await _chatbot.chat(
        message=request.message,
        session_id=request.session_id,
        language=request.language,
        scan_context=request.scan_context,
        response_type=request.response_type,
    )

    # 3. Save assistant message to database
    try:
        async with await get_session() as session:
            await create_chat_message(
                session=session,
                role="assistant",
                content=response.text,
                language=request.language,
            )
    except Exception as e:
        logger.error(f"Failed to save assistant response to DB: {e}")

    return {
        "status": "success",
        "response": response.to_dict(),
    }


class DetailedRequest(BaseModel):
    session_id: str = "default"
    language: str = "en"


@router.post("/details")
async def get_detailed_response(request: DetailedRequest):
    """
    Get a detailed explanation for the last scan/query in the session.
    
    Call this when the user presses "More Details" or requests more
    information about the previous AI response.
    """
    if not _chatbot.is_ready:
        await _chatbot.initialize()

    response = await _chatbot.get_detailed_response(
        session_id=request.session_id,
        language=request.language,
    )

    # Save to database
    from ..database.crud import get_session, create_chat_message
    try:
        async with await get_session() as session:
            await create_chat_message(
                session=session,
                role="assistant",
                content=response.text,
                language=request.language,
            )
    except Exception as e:
        logger.error(f"Failed to save detailed response to DB: {e}")

    return {
        "status": "success",
        "response": response.to_dict(),
    }


class ContextRequest(BaseModel):
    session_id: str = "default"
    scan_context: dict


@router.post("/context")
async def set_session_context(request: ContextRequest):
    """
    Set scan/analysis context for a chat session.
    
    Call this after a plant scan to enable "More Details" follow-ups
    without re-scanning.
    """
    if not _chatbot.is_ready:
        await _chatbot.initialize()

    _chatbot.set_session_context(request.session_id, request.scan_context)

    return {
        "status": "success",
        "message": "Session context updated",
    }


@router.get("/history")
async def get_chat_history(session_id: str = "default", limit: int = 50):
    """Get chat history for a session."""
    from ..database.crud import get_session, get_chat_history as db_get_history
    async with await get_session() as session:
        messages = await db_get_history(session, limit=limit)
        return {
            "status": "success",
            "messages": [
                {"role": m.role, "content": m.content, "created_at": m.created_at.isoformat()}
                for m in messages
            ],
        }


@router.delete("/history")
async def clear_chat_history(session_id: str = "default"):
    """Clear chat history for a session."""
    _chatbot.clear_history(session_id)
    return {"status": "success", "message": "History cleared"}
