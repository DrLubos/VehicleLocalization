"""
This module contains API endpoints for interaction with the database from web clients.
"""
import datetime
import logging
import json
from typing import Callable, Awaitable
import bcrypt
import jwt
from fastapi import FastAPI, HTTPException, Depends, Request, Response
from fastapi.middleware.cors import CORSMiddleware
from starlette.middleware.base import BaseHTTPMiddleware

from schemas import LoginRequest
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from api_db_helper.models import User
from api_db_helper.db_connection import get_db


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    filename="/app/logs/access.log",
    filemode="a"
)

class LoggingMiddleware(BaseHTTPMiddleware):
    """
    Middleware for logging HTTP requests.

    This middleware logs the HTTP method, URL, request body, and client IP address
    for each incoming request.

    Methods:
        dispatch(request: Request, call_next): Asynchronously processes the incoming request,
        logs relevant information, and forwards the request to the next middleware or endpoint.

    Attributes:
        None
    """
    async def dispatch(self, request: Request,
                       call_next: Callable[[Request], Awaitable[Response]]) -> Response:
        body_bytes = await request.body()
        try:
            body_str = body_bytes.decode('utf-8')
        except UnicodeDecodeError:
            body = "Error reading body"
        else:
            try:
                body = json.loads(body_str)
            except json.JSONDecodeError:
                body = body_str

        client_ip = request.client.host if request.client else "unknown"
        logging.info("Request: %s %s - Body: %s - IP: %s",
                     request.method, request.url, body, client_ip)

        response = await call_next(request)
        return response


SECRET_KEY = "supersecret"
ALGORITHM = "HS256"

DOCS_ENABLED = True
app = FastAPI(
    title="Vehicle Location API",
    docs_url="/docs" if DOCS_ENABLED else None,
    redoc_url="/redoc" if DOCS_ENABLED else None,
    openapi_url="/openapi.json" if DOCS_ENABLED else None,
)
app.add_middleware(LoggingMiddleware)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.post("/login", status_code=200)
async def login_user(login_data: LoginRequest, db: AsyncSession = Depends(get_db)):
    """
    Handles user login by verifying credentials and generating a JWT token.

    Args:
        login_data (LoginRequest): The login request data containing username and password.
        db (AsyncSession, optional): The database session dependency.

    Returns:
        dict: A dictionary containing the JWT token if login is successful.

    Raises:
        HTTPException: If the user is not found or the credentials are invalid.
    """
    stmt = select(User).filter(User.username == login_data.username)
    result = await db.execute(stmt)
    user_in_db = result.scalars().first()
    if not user_in_db:
        raise HTTPException(status_code=401, detail="Invalid credentials")
    if not bcrypt.checkpw(login_data.password.encode('utf-8'),
                          user_in_db.password_hash.encode('utf-8')):
        raise HTTPException(status_code=401, detail="Invalid credentials")

    expire = datetime.datetime.utcnow() + datetime.timedelta(minutes=60)
    payload = {
        "sub": user_in_db.username,
        "exp": expire
    }
    token = jwt.encode(payload, SECRET_KEY, algorithm=ALGORITHM)

    return {"token": token}
