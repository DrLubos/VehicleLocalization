"""
This module contains API endpoints for interaction with the database from web clients.
"""
import datetime
import logging
import bcrypt
import jwt
from fastapi import FastAPI, HTTPException, Depends
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import OAuth2PasswordBearer

from schemas import LoginRequest
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from api_db_helper.api_logging import LoggingMiddleware
from api_db_helper.models import User
from api_db_helper.db_connection import get_db


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    filename="/app/logs/access.log",
    filemode="a"
)


SECRET_KEY = "supersecret"
ALGORITHM = "HS256"

DOCS_ENABLED = True
app = FastAPI(
    title="Vehicle Location WebAPI",
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


oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/login")

async def get_current_user(token: str = Depends(oauth2_scheme), db: AsyncSession = Depends(get_db)):
    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
        username: str = payload.get("sub")
        if username is None:
            raise HTTPException(status_code=401, detail="Invalid credentials")
    except jwt.PyJWTError as e:
        raise HTTPException(status_code=401, detail="Invalid credentials") from e
    stmt = select(User).filter(User.username == username)
    result = await db.execute(stmt)
    user = result.scalars().first()
    if user is None:
        raise HTTPException(status_code=401, detail="User not found")
    return user


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
