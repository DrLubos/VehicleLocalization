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

from schemas import LoginRequest, VehicleCreate, VehicleUpdate, VehicleResponse, RouteResponse,\
    PositionResponse
from sqlalchemy import or_, case, delete
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from api_db_helper.api_logging import LoggingMiddleware
from api_db_helper.models import User, Vehicle, UserVehicleAssignment, Route, Position,\
    VehicleStatus
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


def extract_lat_lon(wkt: str) -> tuple[float, float]:
    """
    Extracts latitude and longitude from a WKT (Well-Known Text) POINT string.

    Args:
        wkt (str): A WKT POINT string in the format "POINT(lon lat)".

    Returns:
        tuple[float, float]: A tuple containing the latitude and longitude as floats.
    """
    coords = wkt.lstrip("POINT(").rstrip(")").split()
    lon, lat = float(coords[0]), float(coords[1])
    return lat, lon


async def get_current_user(token: str = Depends(oauth2_scheme),
                           db: AsyncSession = Depends(get_db)) -> User:
    """
    Retrieve the current user based on the provided JWT token.

    Args:
        token (str): The JWT token provided by the user.
        db (AsyncSession): The database session dependency.

    Returns:
        User: The authenticated user object.

    Raises:
        HTTPException: If token is invalid, the user is not found, or the credentials are invalid.
    """
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
async def login_user(login_data: LoginRequest, db: AsyncSession = Depends(get_db)) -> dict:
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


@app.get("/all-vehicles", status_code=200, response_model=list[VehicleResponse])
async def get_all_vehicles(current_user: User = Depends(get_current_user),
                       db: AsyncSession = Depends(get_db)) -> list[Vehicle]:
    """
    Retrieve all vehicles assigned to the current user, ordered by their status and creation date.

    The vehicles are ordered by their status in the following order:
    1. REGISTERED
    2. ACTIVE
    3. INACTIVE
    4. DELETED
    5. Any other status

    Args:
        current_user (User): The current authenticated user, obtained from the dependency injection.
        db (AsyncSession): The database session, obtained from the dependency injection.

    Returns:
        list[Vehicle]: List of vehicles assigned to the user, ordered by status and creation date.
    """
    subq = select(UserVehicleAssignment.vehicle_id).filter(
        UserVehicleAssignment.user_id == current_user.id
    )

    order_expr = case(
        (Vehicle.status == VehicleStatus.REGISTERED, 1),
        (Vehicle.status == VehicleStatus.ACTIVE, 2),
        (Vehicle.status == VehicleStatus.INACTIVE, 3),
        (Vehicle.status == VehicleStatus.DELETED, 4),
        else_=5
    )

    stmt = (
        select(Vehicle)
        .filter(Vehicle.id.in_(subq))
        .order_by(order_expr, Vehicle.created_at)
    )

    result = await db.execute(stmt)
    return result.scalars().all()


@app.get("/vehicles", status_code=200, response_model=list[VehicleResponse])
async def get_vehicles(current_user: User = Depends(get_current_user),
                       db: AsyncSession = Depends(get_db)) -> list[Vehicle]:
    """
    Retrieve a list of vehicles assigned to the current user.

    Args:
        current_user (User): The current authenticated user, obtained via dependency injection.
        db (AsyncSession): The database session, obtained via dependency injection.

    Returns:
        list[Vehicle]: A list of vehicles assigned to the current user.
    """
    now = datetime.datetime.utcnow()
    assignments_result = await db.execute(
        select(UserVehicleAssignment).filter(
            UserVehicleAssignment.user_id == current_user.id,
            UserVehicleAssignment.start_date <= now,
            or_(
                UserVehicleAssignment.end_date.is_(None),
                UserVehicleAssignment.end_date >= now
            )
        )
    )
    assignments = assignments_result.scalars().all()

    vehicle_ids = [assignment.vehicle_id for assignment in assignments]
    if not vehicle_ids:
        return []
    vehicles_result = await db.execute(
        select(Vehicle).filter(Vehicle.id.in_(vehicle_ids))
    )
    return vehicles_result.scalars().all()


@app.post("/vehicles", status_code=201, response_model=VehicleResponse)
async def create_vehicle(vehicle_data: VehicleCreate,
                current_user: User = Depends(get_current_user),
                db: AsyncSession = Depends(get_db)) -> Vehicle:
    """
    Create a new vehicle and assign it to the current user.

    Args:
        vehicle_data (VehicleCreate): The data required to create a new vehicle.
        current_user (User): The current authenticated user, provided by dependency injection.
        db (AsyncSession): The database session, provided by dependency injection.

    Returns:
        Vehicle: The newly created vehicle.
    """
    new_vehicle = Vehicle(
        name=vehicle_data.name,
        imei=vehicle_data.imei,
        color=vehicle_data.color,
        position_check_freq=vehicle_data.position_check_freq,
        min_distance_delta=vehicle_data.min_distance_delta,
        max_idle_minutes=vehicle_data.max_idle_minutes,
        snap_to_road=vehicle_data.snap_to_road,
        manual_route_start=vehicle_data.manual_route_start,
    )
    db.add(new_vehicle)

    await db.flush()

    new_user_vehicle_assignment = UserVehicleAssignment(
        user_id=current_user.id,
        vehicle_id=new_vehicle
    )
    db.add(new_user_vehicle_assignment)

    await db.commit()
    await db.refresh(new_vehicle)
    return new_vehicle


@app.put("/vehicles/{vehicle_id}", status_code=200, response_model=VehicleResponse)
async def update_vehicle(vehicle_id: int,
                         vehicle_data: VehicleUpdate,
                         current_user: User = Depends(get_current_user),
                         db: AsyncSession = Depends(get_db)) -> Vehicle:
    """
    Update vehicle information.

    This endpoint allows an authenticated user to update the details of a vehicle they are 
    assigned to.

    Args:
        vehicle_id (int): The ID of the vehicle to update.
        vehicle_data (VehicleUpdate): The new data for the vehicle.
        current_user (User, optional): The current authenticated user. Defaults to the user 
        returned by `get_current_user`.
        db (AsyncSession, optional): The database session. Defaults to the session returned by 
        `get_db`.

    Returns:
        Vehicle: The updated vehicle object.

    Raises:
        HTTPException: If no active assignment is found for the vehicle or if the vehicle does 
        not exist.
    """
    now = datetime.datetime.utcnow()
    assignment_stmt = select(UserVehicleAssignment).filter(
        UserVehicleAssignment.vehicle_id == vehicle_id,
        UserVehicleAssignment.user_id == current_user.id,
        UserVehicleAssignment.start_date <= now,
        or_(
            UserVehicleAssignment.end_date.is_(None),
            UserVehicleAssignment.end_date >= now
        )
    )
    assignment_result = await db.execute(assignment_stmt)
    assignment = assignment_result.scalars().first()
    if not assignment:
        raise HTTPException(status_code=404, detail="Active assignment not found for this vehicle")

    result = await db.execute(select(Vehicle).filter(Vehicle.id == vehicle_id))
    vehicle = result.scalars().first()
    if not vehicle:
        raise HTTPException(status_code=404, detail="Vehicle not found")

    if vehicle_data.name is not None:
        vehicle.name = vehicle_data.name
    if vehicle_data.status is not None:
        vehicle.status = vehicle_data.status
    if vehicle_data.color is not None:
        vehicle.color = vehicle_data.color
    if vehicle_data.position_check_freq is not None:
        vehicle.position_check_freq = vehicle_data.position_check_freq
    if vehicle_data.min_distance_delta is not None:
        vehicle.min_distance_delta = vehicle_data.min_distance_delta
    if vehicle_data.max_idle_minutes is not None:
        vehicle.max_idle_minutes = vehicle_data.max_idle_minutes
    if vehicle_data.snap_to_road is not None:
        vehicle.snap_to_road = vehicle_data.snap_to_road
    if vehicle_data.manual_route_start is not None:
        vehicle.manual_route_start = vehicle_data.manual_route_start

    await db.commit()
    await db.refresh(vehicle)
    return vehicle


@app.delete("/vehicles/{vehicle_id}", status_code=200)
async def unassign_vehicle(vehicle_id: int,
                         current_user: User = Depends(get_current_user),
                         db: AsyncSession = Depends(get_db)) -> dict:
    """
    Unassigns a vehicle from the current user and marks the vehicle as deleted.

    Args:
        vehicle_id (int): The ID of the vehicle to be unassigned.
        current_user (User): The current authenticated user, provided by dependency injection.
        db (AsyncSession, optional): The database session. Defaults to Depends(get_db).

    Returns:
        dict: A dictionary containing the success status and a message.

    Raises:
        HTTPException: If no active assignment is found for the vehicle.
        HTTPException: If the vehicle is not found.
    """
    now = datetime.datetime.utcnow()
    stmt = await db.execute(
        select(UserVehicleAssignment).filter(
            UserVehicleAssignment.user_id == current_user.id,
            UserVehicleAssignment.vehicle_id == vehicle_id,
            UserVehicleAssignment.start_date <= now,
            or_(
                UserVehicleAssignment.end_date.is_(None),
                UserVehicleAssignment.end_date >= now
            )
        )
    )
    result = await db.execute(stmt)
    assignment = result.scalars().first()
    if not assignment:
        raise HTTPException(status_code=404, detail="Active assignment not found for this vehicle")
    assignment.end_date = now

    stmt_vehicle = select(Vehicle).filter(Vehicle.id == vehicle_id)
    result_vehicle = await db.execute(stmt_vehicle)
    vehicle = result_vehicle.scalars().first()
    if not vehicle:
        raise HTTPException(status_code=404, detail="Vehicle not found")
    vehicle.status = VehicleStatus.DELETED

    await db.commit()
    return {"success": True, "message": "Vehicle" + vehicle.name + "unassigned successfully."}


@app.delete("/vehicles/{vehicle_id}/force-delete", status_code=200)
async def force_delete_vehicle_data(vehicle_id: int,
                         current_user: User = Depends(get_current_user),
                         db: AsyncSession = Depends(get_db)) -> dict:
    """
    Force delete vehicle data for the current user.

    Deletes vehicle assignment for current user, if no other assignments exist, deletes the vehicle.

    Args:
        vehicle_id (int): The ID of the vehicle to be deleted.
        current_user (User): The current authenticated user, provided by dependency injection.
        db (AsyncSession, optional): The database session. Defaults to Depends(get_db).

    Returns:
        dict: A dictionary containing the success status and a message.

    Raises:
        HTTPException: If no assignment is found for the vehicle.
    """
    stmt_assign = await db.execute(
        select(UserVehicleAssignment).filter(
            UserVehicleAssignment.user_id == current_user.id,
            UserVehicleAssignment.vehicle_id == vehicle_id,
        )
    )
    result_assign = await db.execute(stmt_assign)
    assignments = result_assign.scalars().all()
    if not assignments:
        raise HTTPException(status_code=404, detail="No assignment found for this vehicle")

    await db.execute(
        delete(UserVehicleAssignment).where(
            UserVehicleAssignment.vehicle_id == vehicle_id,
            UserVehicleAssignment.user_id == current_user.id
        )
    )
    await db.commit()

    stmt_other_assign = select(UserVehicleAssignment).filter(
        UserVehicleAssignment.vehicle_id == vehicle_id
    )
    result_other_assign = await db.execute(stmt_other_assign)
    other_assignments = result_other_assign.scalars().all()

    if not other_assignments:
        await db.execute(delete(Vehicle).where(Vehicle.id == vehicle_id))
        await db.commit()

    return {"success": True, "message": "Force delete performed for routes, " +
            "positions, and assignments for current user."}


@app.put("/vehicles/{vehicle_id}/togle-status", status_code=200)
async def toggle_vehicle_status(vehicle_id: int,
                                current_user: User = Depends(get_current_user),
                                db: AsyncSession = Depends(get_db)) -> dict:
    """
    Toggle the status of a vehicle between ACTIVE and INACTIVE.

    Args:
        vehicle_id (int): The ID of the vehicle to toggle the status for.
        current_user (User): The current authenticated user, provided by dependency injection.
        db (AsyncSession): The database session, provided by dependency injection.

    Returns:
        dict: Dictionary containing success status and a message indicating new status of vehicle.

    Raises:
        HTTPException: If no active assignment is found for the vehicle and user, or if the vehicle
                         is not found, or if the vehicle status is not toggleable.
    """
    now = datetime.datetime.utcnow()
    stmt_assignment = select(UserVehicleAssignment).filter(
        UserVehicleAssignment.vehicle_id == vehicle_id,
        UserVehicleAssignment.user_id == current_user.id,
        UserVehicleAssignment.start_date <= now,
        or_(
            UserVehicleAssignment.end_date.is_(None),
            UserVehicleAssignment.end_date >= now
        )
    )
    result_assignment = await db.execute(stmt_assignment)
    assignment = result_assignment.scalars().first()
    if not assignment:
        raise HTTPException(status_code=404, detail="Active assignment not found for this vehicle")

    stmt_vehicle = select(Vehicle).filter(Vehicle.id == vehicle_id)
    result_vehicle = await db.execute(stmt_vehicle)
    vehicle = result_vehicle.scalars().first()
    if not vehicle:
        raise HTTPException(status_code=404, detail="Vehicle not found")

    if vehicle.status == VehicleStatus.ACTIVE:
        vehicle.status = VehicleStatus.INACTIVE
    elif vehicle.status == VehicleStatus.INACTIVE:
        vehicle.status = VehicleStatus.ACTIVE
    else:
        raise HTTPException(status_code=400, detail="Vehicle status is not toggleable")

    await db.commit()
    await db.refresh(vehicle)
    return {
        "success": True,
        "message": f"Changed vehicle status to {vehicle.status.value} for vehicle: {vehicle.name}",
    }


@app.get("/vehicles/{vehicle_id}/routes", status_code=200, response_model=RouteResponse)
async def get_vehicle_routes(vehicle_id: int,
                             current_user: User = Depends(get_current_user),
                             db: AsyncSession = Depends(get_db),
                             number_of_routes: int = 5) -> list[Route]:
    """
    Retrieve a list of routes for a specific vehicle associated with the current user.

    Args:
        vehicle_id (int): The ID of the vehicle to retrieve routes for.
        current_user (User): The current authenticated user, provided by dependency injection.
        db (AsyncSession, optional): The database session. Defaults to Depends(get_db).
        number_of_routes (int, optional): The maximum number of routes to retrieve. Defaults to 5.

    Returns:
        list[Route]: A list of Route objects associated with the specified vehicle and user.
    """
    stmt = (
       select(Route)
       .join(UserVehicleAssignment, Route.assignment_id == UserVehicleAssignment.id)
       .filter(
          UserVehicleAssignment.vehicle_id == vehicle_id,
          UserVehicleAssignment.user_id == current_user.id,
          Route.start_time >= UserVehicleAssignment.start_date,
          or_(
             UserVehicleAssignment.end_date.is_(None),
             Route.start_time <= UserVehicleAssignment.end_date
          )
       )
       .order_by(Route.start_time.desc())
       .limit(number_of_routes)
    )
    result_routes = await db.execute(stmt)
    return result_routes.scalars().all()


@app.get("/routes/{route_id}/positions", status_code=200, response_model=PositionResponse)
async def get_route_positions(route_id: int,
                           current_user: User = Depends(get_current_user),
                           db: AsyncSession = Depends(get_db)) -> list[Position]:
    """
    Endpoint to get positions for a specific route.

    Args:
        route_id (int): The ID of the route to retrieve positions for.
        current_user (User): The current authenticated user, provided by dependency injection.
        db (AsyncSession, optional): The database session. Defaults to Depends(get_db).

    Returns:
        list[Position]: A list of positions associated with the specified route.

    Raises:
        HTTPException: If the route is not found (404).
        HTTPException: If the user is not authorized to view the route (403).
    """
    result = await db.execute(select(Route).filter(Route.id == route_id))
    route = result.scalars().first()
    if not route:
        raise HTTPException(status_code=404, detail="Route not found")

    stmt_assignment = select(UserVehicleAssignment).filter(
        UserVehicleAssignment.id == route.assignment_id,
        UserVehicleAssignment.user_id == current_user.id,
        UserVehicleAssignment.start_date <= route.start_time,
        or_(
            UserVehicleAssignment.end_date.is_(None),
            UserVehicleAssignment.end_date >= route.start_time
        )
    )
    result_assignment = await db.execute(stmt_assignment)
    assignment = result_assignment.scalars().first()
    if not assignment:
        raise HTTPException(status_code=403, detail="Not authorized to view this route")

    result_positions = await db.execute(select(Position).filter(Position.route_id == route_id))
    positions = result_positions.scalars().all()
    response_positions = []
    for pos in positions:
        lat, lon = extract_lat_lon(pos.location)
        response_positions.append({
            "id": pos.id,
            "route_id": pos.route_id,
            "timestamp": pos.timestamp,
            "latitude": lat,
            "longitude": lon,
            "speed": pos.speed,
        })
    return response_positions


@app.delete("/routes/{route_id}", status_code=200)
async def delete_route(route_id: int,
                       current_user: User = Depends(get_current_user),
                       db: AsyncSession = Depends(get_db)) -> dict:
    """
    Delete a route by its ID.

    Args:
        route_id (int): The ID of the route to delete.
        current_user (User): The current authenticated user, provided by dependency injection.
        db (AsyncSession, optional): The database session. Defaults to Depends(get_db).

    Returns:
        dict: A dictionary containing the success status and a message.

    Raises:
        HTTPException: If the route is not found (404).
        HTTPException: If the user is not authorized to delete the route (403).
    """
    result = await db.execute(select(Route).filter(Route.id == route_id))
    route = result.scalars().first()
    if not route:
        raise HTTPException(status_code=404, detail="Route not found")

    stmt_assignment = select(UserVehicleAssignment).filter(
        UserVehicleAssignment.id == route.assignment_id,
        UserVehicleAssignment.user_id == current_user.id,
        UserVehicleAssignment.start_date <= route.start_time,
        or_(
            UserVehicleAssignment.end_date.is_(None),
            UserVehicleAssignment.end_date >= route.start_time
        )
    )
    result_assignment = await db.execute(stmt_assignment)
    assignment = result_assignment.scalars().first()
    if not assignment:
        raise HTTPException(status_code=403, detail="Not authorized to delete this route")

    await db.execute(delete(Route).where(Route.id == route_id))
    await db.commit()
    return {"success": True, "message": "Route deleted successfully."}
