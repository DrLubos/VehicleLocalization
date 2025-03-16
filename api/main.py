"""
Main module of the Vehicle Location API for communicating with GPS tracking device.
"""
import datetime
import math
import secrets
import logging

from typing import Tuple
from fastapi import FastAPI, Depends, HTTPException
from sqlalchemy import select, update
from sqlalchemy.ext.asyncio import AsyncSession
from geoalchemy2 import WKTElement

from schemas import PositionRequest, RouteCreationRequest, TokenRequest,\
    TokenResponse, TokenVerifyRequest
from api_db_helper.db_connection import get_db
from api_db_helper.api_logging import LoggingMiddleware
from api_db_helper.models import Vehicle, Route, Position, VehicleStatus
from api_db_helper.crud import get_vehicle_by_token, get_active_assignment_by_vehicle,\
    get_latest_route, get_latest_position

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    filename="/app/logs/access.log",
    filemode="a"
)


DOCS_ENABLED = True
app = FastAPI(
    title="Vehicle Location API",
    docs_url="/docs" if DOCS_ENABLED else None,
    redoc_url="/redoc" if DOCS_ENABLED else None,
    openapi_url="/openapi.json" if DOCS_ENABLED else None,
)
app.add_middleware(LoggingMiddleware)


def calculate_distance(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    """
    Calculate the distance between two points on the Earth's surface using the Haversine formula.

    Args:
        lat1 (float): Latitude of the first point in decimal degrees.
        lon1 (float): Longitude of the first point in decimal degrees.
        lat2 (float): Latitude of the second point in decimal degrees.
        lon2 (float): Longitude of the second point in decimal degrees.

    Returns:
        float: Distance between the two points in meters.
    """
    earth_radius = 6378000
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    delta_phi = math.radians(lat2 - lat1)
    delta_lambda = math.radians(lon2 - lon1)
    a = math.sin(delta_phi/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin(delta_lambda/2)**2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))
    return earth_radius * c


def knots_to_kmh(speed_knots: float) -> float:
    """
    Convert speed from knots to kilometers per hour (km/h).

    Args:
        speed_knots (float): Speed in knots.

    Returns:
        float: Speed in kilometers per hour, rounded to two decimal places.
    """
    return round(speed_knots * 1.852, 2)


async def get_city_by_coords(lat: float, lon: float) -> str:
    """
    Asynchronously retrieves the city name based on the provided latitude and longitude coordinates.

    Args:
        lat (float): The latitude of the location.
        lon (float): The longitude of the location.

    Returns:
        str: The name of the city corresponding to the given coordinates.
    """
    round(lat, 8)
    round(lon, 8)
    #TODO: Implement city name retrieval based on coordinates
    return "CityName"


def extract_coords(point_geom: WKTElement) -> Tuple[float, float]:
    """
    Extracts latitude and longitude coordinates from a WKTElement.

    Args:
        point_geom (WKTElement): A WKTElement containing the point geometry in WKT format.

    Returns:
        Tuple[float, float]: A tuple containing the latitude and longitude as floats.
                             Returns (0.0, 0.0) if extraction fails.
    """
    try:
        wkt_str = str(point_geom)
        parts = wkt_str.strip().split("(")[1].split(")")[0].split()
        lon_val = float(parts[0])
        lat_val = float(parts[1])
        return lat_val, lon_val
    except (IndexError, ValueError):
        return (0.0, 0.0)


async def update_route_geom(session: AsyncSession, route_id: int, lon: float, lat: float) -> None:
    """
    Update the geometry of a route in the database.

    This function updates the `route_geom` field of a route in the `routes` table.
    If the `route_geom` is NULL, it initializes it with a new line containing the given point.
    Otherwise, it adds the given point to the existing geometry.

    Args:
        session (AsyncSession): The SQLAlchemy asynchronous session to use for database operation.
        route_id (int): The ID of the route to update.
        lon (float): The longitude of the point to add to the route geometry.
        lat (float): The latitude of the point to add to the route geometry.

    Returns:
        None
    """
    sql = """
    UPDATE routes
    SET route_geom = CASE 
        WHEN route_geom IS NULL THEN ST_MakeLine(ST_SetSRID(ST_MakePoint(:lon, :lat), 4326))
        ELSE ST_AddPoint(route_geom, ST_SetSRID(ST_MakePoint(:lon, :lat), 4326))
    END
    WHERE id = :route_id;
    """
    await session.execute(sql, {"lon": lon, "lat": lat, "route_id": route_id})


@app.post("/location", status_code=200)
async def post_location(data: PositionRequest,
                        session: AsyncSession = Depends(get_db)) -> dict:
    """
    Handles the posting of a vehicle's location.

    Args:
        data (PositionRequest): Request data containing the vehicle's informations.
        session (AsyncSession, optional): Database session dependency. Default Depends(get_db).


    Raises:
        HTTPException: If the vehicle is not found (404).
        HTTPException: If posting location is not allowed for the vehicle (403).
        HTTPException: If the provided coordinates are invalid (400).
        HTTPException: If the vehicle is not assigned to any user (404).
        HTTPException: If there is a database error (500).

    Returns:
        dict: A dictionary containing success status, a message, and additional distance traveled.
    """
    now = datetime.datetime.utcnow()
    vehicle = await get_vehicle_by_token(session, data.token)
    if vehicle is None:
        raise HTTPException(status_code=404, detail="Vehicle not found")
    if vehicle.status not in (VehicleStatus.ACTIVE, VehicleStatus.REGISTERED):
        raise HTTPException(status_code=403, detail="Post location is not allowed for this vehicle")
    if not -90 <= data.lat <= 90 or not -180 <= data.lon <= 180:
        raise HTTPException(status_code=400, detail="Invalid coordinates")
    lat = round(data.lat, 7)
    lon = round(data.lon, 7)
    # Check if vehicle is assigned to only one user
    assignment = await get_active_assignment_by_vehicle(session, vehicle.id)
    if assignment is None:
        raise HTTPException(status_code=404, detail="Vehicle is not assigned")
    # Check if exists route for this assignment
    route = await get_latest_route(session, assignment.id)
    create_new_route = False

    if route is None:
        create_new_route = True
    else:
        latest_pos = await get_latest_position(session, route.id)
        if latest_pos is not None:
            diff = (now - latest_pos.timestamp).total_seconds()
            if diff > vehicle.max_idle_minutes * 60:
                city = await get_city_by_coords(latest_pos.location.y, latest_pos.location.x)
                route.end_time = latest_pos.timestamp
                route.end_city = city
                create_new_route = True
        else:
            create_new_route = False

    if create_new_route:
        start_city = await get_city_by_coords(lat, lon)
        new_route = Route(
            assignment_id=assignment.id,
            start_time=now,
            start_city=start_city,
            end_city=None,
            route_geom=WKTElement(f"POINT({lon} {lat})", srid=4326)
        )
        session.add(new_route)
        await session.flush()
        route_id = new_route.id
    else:
        route_id = route.id

    last_position = await get_latest_position(session, route_id)
    additional_distance = 0
    if last_position is not None:
        last_lat, last_lon = extract_coords(last_position.location)
        additional_distance = calculate_distance(last_lat, last_lon, lat, lon)
        if not create_new_route:
            route.total_distance += int(additional_distance)
            route.route_geom = update_route_geom(session, route_id, lon, lat)

    point = WKTElement(f"POINT({lon} {lat})", srid=4326)
    new_position = Position(
        route_id=route_id,
        timestamp=now,
        location=point,
        speed=knots_to_kmh(data.speed)
    )
    session.add(new_position)

    try:
        await session.commit()
    except Exception as e:
        await session.rollback()
        raise HTTPException(status_code=500, detail="Database error: " + str(e)) from e

    return {"success": True, "message": "Location is saved",
            "additional_distance": additional_distance}


@app.post("/route", status_code=200)
async def post_route(data: RouteCreationRequest,
                     session: AsyncSession = Depends(get_db)) -> dict:
    """
    Create a new route for a vehicle based on the provided data.

    Args:
        data (RouteCreationRequest): Data required to create a new route, including vehicle token.
        session (AsyncSession, optional): Database session dependency. Default Depends(get_db).

    Returns:
        dict: A dictionary containing success status, message, and ID of the newly created route.

    Raises:
        HTTPException: If the vehicle is not found (404).
        HTTPException: If the vehicle's status is not 'active' or 'registered' (403).
        HTTPException: If no active assignment is found for the vehicle (404).
        HTTPException: If there is a database error during the commit (500).
    """
    now = datetime.datetime.utcnow()
    vehicle = await get_vehicle_by_token(session, data.token)
    if vehicle is None:
        raise HTTPException(status_code=404, detail="Vehicle not found")
    if vehicle.status not in (VehicleStatus.ACTIVE, VehicleStatus.REGISTERED):
        raise HTTPException(status_code=403, detail="Post route is not allowed for this vehicle")
    assignment = await get_active_assignment_by_vehicle(session, vehicle.id)
    if assignment is None:
        raise HTTPException(status_code=404, detail="No active assignment found for this vehicle")
    new_route = Route(
        assignment_id=assignment.id,
        start_time=now,
        end_time=None,
        total_distance=0,
        start_city=None,
        end_city=None,
        route_geom=None
    )
    session.add(new_route)
    await session.flush()
    try:
        await session.commit()
    except Exception as e:
        await session.rollback()
        raise HTTPException(status_code=500, detail="Database error: " + str(e)) from e
    return {"success": True, "message": "New route is created", "route_id": new_route.id}


@app.post("/request_token", response_model=TokenResponse, status_code=200)
async def post_token(data: TokenRequest, session: AsyncSession = Depends(get_db)) -> dict:
    """
    Endpoint to request a new token for a vehicle.
    Args:
        data (TokenRequest): The request payload containing the vehicle's IMEI.
        session (AsyncSession): The database session dependency.
    Returns:
        dict: A dictionary containing the new token and vehicle configuration details.
    Raises:
        HTTPException: If the vehicle is not found (404) or if there is a database error (500).
    Response Model:
        TokenResponse: The response model containing new token and vehicle configuration details.
    """
    stmt = select(Vehicle).where(
        Vehicle.imei == data.imei,
        Vehicle.status.in_([VehicleStatus.ACTIVE.value, VehicleStatus.REGISTERED.value])
    )
    result = await session.execute(stmt)
    vehicle = result.scalar_one_or_none()
    if vehicle is None:
        raise HTTPException(status_code=404, detail="Vehicle not found")

    new_token = secrets.token_hex(16)
    stmt = update(Vehicle).where(Vehicle.id == vehicle.id).values(token=new_token)
    await session.execute(stmt)
    try:
        await session.commit()
    except Exception as e:
        await session.rollback()
        raise HTTPException(status_code=500, detail="Database error: " + str(e)) from e

    return TokenResponse(
        token=new_token,
        position_check_freq=vehicle.position_check_freq,
        min_distance_delta=vehicle.min_distance_delta,
        max_idle_minutes=vehicle.max_idle_minutes
    )


@app.post("/verify_token", status_code=200)
async def post_verify_token(data: TokenVerifyRequest,
                            session: AsyncSession = Depends(get_db)) -> dict:
    """
    Endpoint to verify a vehicle token.

    This endpoint receives a token verification request and checks if the provided
    IMEI and token match a vehicle in the database. If the vehicle is found, it 
    returns a success message. Otherwise, it raises a 404 HTTP exception.

    Args:
        data (TokenVerifyRequest): The request body containing the IMEI and token.
        session (AsyncSession): The database session dependency.

    Returns:
        dict: A dictionary containing the success status and a message.

    Raises:
        HTTPException: If the vehicle is not found, a 404 error is raised.
    """
    stmt = select(Vehicle).where(
        Vehicle.imei == data.imei,
        Vehicle.token == data.token
    )
    result = await session.execute(stmt)
    vehicle = result.scalar_one_or_none()
    if vehicle is None:
        raise HTTPException(status_code=404, detail="Vehicle not found")
    return {"success": True, "message": "Token is verified"}
