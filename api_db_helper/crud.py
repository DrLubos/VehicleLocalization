"""
This module contains functions that perform CRUD operations on the database.
"""
import datetime

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select

from api_db_helper.models import Vehicle, UserVehicleAssignment, Route, Position


async def get_vehicle_by_token(session: AsyncSession, token: str) -> Vehicle | None:
    """
    Retrieve a vehicle by its token.

    This asynchronous function queries the database for a vehicle that matches the provided token.

    Args:
        session (AsyncSession): The SQLAlchemy asynchronous session to use for the query.
        token (str): The token associated with the vehicle to retrieve.

    Returns:
        Vehicle | None: The vehicle object if found, otherwise None.
    """
    result = await session.execute(
        select(Vehicle).where(Vehicle.token == token)
    )
    return result.scalar_one_or_none()


async def get_active_assignment(session: AsyncSession,
                                vehicle_id: int) -> UserVehicleAssignment | None:
    """
    Retrieve the active assignment for a given vehicle.

    This function queries the database to find the most recent active assignment
    for a specified vehicle. An assignment is considered active if its `end_date`
    is either `None` or a future date.

    Args:
        session (AsyncSession): The SQLAlchemy asynchronous session to use for the query.
        vehicle_id (int): The ID of the vehicle for which to retrieve the active assignment.

    Returns:
        UserVehicleAssignment | None: The most recent active assignment for the vehicle,
        or `None` if no active assignment is found.
    """
    result = await session.execute(
        select(UserVehicleAssignment).where(
            (UserVehicleAssignment.vehicle_id == vehicle_id) &
            (
                (UserVehicleAssignment.end_date is None) |
                (UserVehicleAssignment.end_date > datetime.datetime.utcnow())
            )
        ).order_by(UserVehicleAssignment.id.desc())
    )
    return result.scalar_one_or_none()


async def get_latest_route(session: AsyncSession, assignment_id: int) -> Route | None:
    """
    Fetches the latest route for a given assignment.

    This asynchronous function queries the database to retrieve the most recent 
    route associated with the specified assignment ID. It orders the routes by 
    their start time in descending order and limits the result to one entry.

    Args:
        session (AsyncSession): The SQLAlchemy asynchronous session used to perform database query.
        assignment_id (int): The ID of the assignment for which the latest route is to be fetched.

    Returns:
        Route | None: The latest Route object if found, otherwise None.
    """
    result = await session.execute(
        select(Route)
        .where(Route.assignment_id == assignment_id)
        .order_by(Route.start_time.desc())
        .limit(1)
    )
    return result.scalar_one_or_none()


async def get_latest_position(session: AsyncSession, route_id: int) -> Position | None:
    """
    Fetches the latest position for a given route.

    Args:
        session (AsyncSession): The SQLAlchemy asynchronous session to use for the query.
        route_id (int): The ID of the route for which to fetch the latest position.

    Returns:
        Position | None: The latest Position for the given route, or None if no position is found.
    """
    result = await session.execute(
        select(Position)
        .where(Position.route_id == route_id)
        .order_by(Position.timestamp.desc())
        .limit(1)
    )
    return result.scalar_one_or_none()
