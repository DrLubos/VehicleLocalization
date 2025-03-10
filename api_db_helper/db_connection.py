"""
This module is used to create a connection to the database.
"""
from typing import AsyncGenerator, Any
from sqlalchemy.ext.asyncio import create_async_engine, AsyncSession
from sqlalchemy.orm import sessionmaker

DATABASE_URL = "postgresql+asyncpg://api_user:heslo@db/vehicle_db"

engine = create_async_engine(DATABASE_URL, echo=True)
SessionLocal = sessionmaker(
    autoflush=False,
    bind=engine,
    class_=AsyncSession,
    expire_on_commit=False
)

async def get_db() -> AsyncGenerator[AsyncSession, Any]:
    """
    Asynchronous generator function that provides a database session.

    Yields:
        AsyncSession: An asynchronous SQLAlchemy session object.
    """
    async with SessionLocal() as session:
        yield session
