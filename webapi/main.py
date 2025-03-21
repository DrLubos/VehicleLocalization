"""
This module contains API endpoints for interaction with the database from web clients.
"""
import datetime
import logging
import json
import bcrypt
import jwt
from fastapi import FastAPI, HTTPException, Depends
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import OAuth2PasswordBearer

from schemas import LoginRequest, VehicleCreate, VehicleUpdate, VehicleResponse, RouteResponse,\
    PositionResponse, VehiclePositionResponse, RegistrationRequest
from sqlalchemy import or_, case, delete, func
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from api_db_helper.api_logging import LoggingMiddleware
from api_db_helper.models import User, Vehicle, UserVehicleAssignment, Route, Position,\
    VehicleStatus, extract_lat_lon_from_wkt
from api_db_helper.db_connection import get_db
from api_db_helper.crud import get_active_assignments_by_user, get_assignment_for_route_and_user,\
    get_active_assignment_by_user_and_vehicle


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


def clamp(value: int, min_val: int = 0, max_val: int = 255) -> int:
    """
    Clamps an integer value between a minimum and maximum value.

    Args:
        value (int): The integer value to be clamped.
        min_val (int, optional): The minimum value to clamp to. Defaults to 0.
        max_val (int, optional): The maximum value to clamp to. Defaults to 255.

    Returns:
        int: The clamped value.
    """
    return max(min_val, min(value, max_val))


def trim_field(field: str, max_length: int = 255) -> str:
    """
    Trims the input field by removing leading and trailing whitespace and ensures it does not exceed
    the specified maximum length.
    Args:
        field (str): The input string to be trimmed.
        max_length (int, optional): Maximum allowed length of the trimmed string. Defaults to 255.
    Returns:
        str: The trimmed string, truncated to the maximum length if necessary.
    Raises:
        ValueError: If the trimmed field is empty.
    """
    field = field.strip()
    if len(field) == 0:
        raise ValueError("Field cannot be empty")
    return field if len(field) <= max_length else field[:max_length]


@app.post("/register", status_code=201)
async def register_user(registration_data: RegistrationRequest,
                        db: AsyncSession = Depends(get_db)) -> dict:
    """
    Registers a new user in the system.

    This endpoint handles user registration by validating the provided username 
    and email for uniqueness, hashing the password, and storing the new user 
    in the database.

    Args:
        registration_data (RegistrationRequest): The registration details provided 
            by the user, including username, email, and password.
        db (AsyncSession): The database session dependency used to interact 
            with the database.

    Returns:
        dict: A dictionary containing the username and ID of the newly registered user.

    Raises:
        HTTPException: If the username already exists in the database (status code 400).
        HTTPException: If the email already exists in the database (status code 400).
    """
    stmt = select(User).filter(User.username == registration_data.username)
    result = await db.execute(stmt)
    existing_user = result.scalars().first()
    if existing_user:
        raise HTTPException(status_code=400, detail="Username already exists")
    stmt = select(User).filter(User.email == registration_data.email)
    result = await db.execute(stmt)
    existing_user = result.scalars().first()
    if existing_user:
        raise HTTPException(status_code=400, detail="Email already exists")
    hashed_pw = bcrypt.hashpw(
        registration_data.password.encode('utf-8'), bcrypt.gensalt()
        ).decode('utf-8')
    new_user = User(
        username=registration_data.username,
        email=registration_data.email,
        password_hash=hashed_pw)
    db.add(new_user)
    await db.commit()
    await db.refresh(new_user)
    return {"username": new_user.username, "id": new_user.id}


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
    return {"token": token, "username": user_in_db.username}


@app.get("/vehicles/last-position", status_code=200, response_model=list[VehiclePositionResponse])
async def get_last_location_for_all_vehicles(current_user: User = Depends(get_current_user),
                                             db: AsyncSession = Depends(get_db)) -> list[dict]:
    """
    Endpoint to get the last known location for all vehicles assigned to the current user.

    Args:
        current_user (User): The current authenticated user, injected by dependency.
        db (AsyncSession): The database session, injected by dependency.

    Returns:
        list[dict]: List of dictionaries containing vehicle information and their last positions.

    Response Model:
        list[VehiclePositionResponse]: A list of VehiclePositionResponse objects.

    Raises:
        HTTPException: If there is an error in retrieving data from the database.

    Notes:
        - This endpoint retrieves active assignments for the current user.
        - For each assignment, it fetches the associated vehicle and its last known position.
        - Last known position includes latitude, longitude, timestamp, speed, and optionally city.
        - Vehicle information includes various attributes such as id, name, imei, status, etc.
    """
    assignments = await get_active_assignments_by_user(db, current_user.id)
    responses = []
    for assignment in assignments:
        vehicle_result = await db.execute(
            select(Vehicle).filter(Vehicle.id == assignment.vehicle_id)
        )
        vehicle = vehicle_result.scalars().first()
        if not vehicle:
            continue

        last_position = None
        route_result = await db.execute(
            select(Route)
            .filter(Route.assignment_id == assignment.id)
            .order_by(Route.start_time.desc())
            .limit(1)
        )
        route = route_result.scalars().first()
        if route:
            position_result = await db.execute(
                select(Position.id,
                       Position.route_id,
                       Position.timestamp,
                       func.ST_AsText(Position.location).label("location"),
                       Position.speed)
                .filter(Position.route_id == route.id)
                .order_by(Position.timestamp.desc())
                .limit(1)
            )
            position = position_result.first()
            if position:
                lat, lon = extract_lat_lon_from_wkt(position.location)
                last_position = {
                    "latitude": lat,
                    "longitude": lon,
                    "location_time": position.timestamp,
                    "speed": position.speed,
                }
            if route.end_city:
                last_position["city"] = route.end_city
        response_dict = {
            "vehicle" : {
                "id": vehicle.id,
                "name": vehicle.name,
                "imei": vehicle.imei,
                "status": vehicle.status.value,
                "color": vehicle.color,
                "position_check_freq": vehicle.position_check_freq,
                "min_distance_delta": vehicle.min_distance_delta,
                "max_idle_minutes": vehicle.max_idle_minutes,
                "manual_route_start_enabled": vehicle.manual_route_start_enabled,
                "created_at": vehicle.created_at,
            },
            "last_position": last_position,
        }
        responses.append(response_dict)
    logging.info(responses)
    return responses


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
    assignments = await get_active_assignments_by_user(db, current_user.id)
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
        name = trim_field(vehicle_data.name),
        imei = trim_field(vehicle_data.imei),
        color = vehicle_data.color,
        position_check_freq = clamp(vehicle_data.position_check_freq, min_val = 1),
        min_distance_delta = clamp(vehicle_data.min_distance_delta),
        max_idle_minutes = clamp(vehicle_data.max_idle_minutes, min_val = 1),
        manual_route_start_enabled = vehicle_data.manual_route_start_enabled,
    )
    db.add(new_vehicle)

    await db.flush()

    new_user_vehicle_assignment = UserVehicleAssignment(
        user_id=current_user.id,
        vehicle_id=new_vehicle.id,
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
    assignment = await get_active_assignment_by_user_and_vehicle(db, current_user.id, vehicle_id)
    if not assignment:
        raise HTTPException(status_code=404, detail="Active assignment not found for this vehicle")

    result = await db.execute(select(Vehicle).filter(Vehicle.id == vehicle_id))
    vehicle = result.scalars().first()
    if not vehicle:
        raise HTTPException(status_code=404, detail="Vehicle not found")

    if vehicle_data.name is not None:
        vehicle.name = trim_field(vehicle_data.name)
    if vehicle_data.status is not None:
        vehicle.status = vehicle_data.status
    if vehicle_data.color is not None:
        vehicle.color = vehicle_data.color
    if vehicle_data.position_check_freq is not None:
        vehicle.position_check_freq = clamp(vehicle_data.position_check_freq, min_val = 1)
    if vehicle_data.min_distance_delta is not None:
        vehicle.min_distance_delta = clamp(vehicle_data.min_distance_delta)
    if vehicle_data.max_idle_minutes is not None:
        vehicle.max_idle_minutes = clamp(vehicle_data.max_idle_minutes, min_val = 1)
    if vehicle_data.manual_route_start_enabled is not None:
        vehicle.manual_route_start_enabled = vehicle_data.manual_route_start_enabled

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
    assignment = await get_active_assignment_by_user_and_vehicle(db, current_user.id, vehicle_id)
    if not assignment:
        raise HTTPException(status_code=404, detail="Active assignment not found for this vehicle")
    assignment.end_date = datetime.datetime.utcnow()

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
    assignment = await get_active_assignment_by_user_and_vehicle(db, current_user.id, vehicle_id)
    if not assignment:
        raise HTTPException(status_code=404, detail="Active assignment not found for this vehicle")

    stmt_vehicle = select(Vehicle).filter(Vehicle.id == vehicle_id)
    result_vehicle = await db.execute(stmt_vehicle)
    vehicle = result_vehicle.scalars().first()
    if not vehicle:
        raise HTTPException(status_code=404, detail="Vehicle not found")

    if vehicle.status == VehicleStatus.ACTIVE or vehicle.status == VehicleStatus.REGISTERED:
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
        "new_status": vehicle.status.value
    }


@app.get("/vehicles/{vehicle_id}/routes", status_code=200, response_model=list[RouteResponse])
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
       select(Route.id,
              Route.assignment_id,
              Route.start_time,
              Route.end_time,
              Route.total_distance,
              Route.start_city,
              Route.end_city,
              func.ST_AsGeoJSON(Route.route_geom).label("route_geometry"))
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
    route_list = result_routes.all()
    respone = []
    for route in route_list:
        route_geojson = json.loads(route.route_geometry) if route.route_geometry else None
        route_dict = {
            "id": route.id,
            "assignment_id": route.assignment_id,
            "start_time": route.start_time,
            "end_time": route.end_time,
            "total_distance": route.total_distance,
            "start_city": route.start_city,
            "start_coords": "No location data",
            "end_city": route.end_city,
            "end_coords": "No location data",
            "route_geometry": route_geojson,
        }
        stmt_positions = (
            select(Position.id,
                    Position.route_id,
                    func.ST_AsText(Position.location).label("location"))
            .filter(Position.route_id == route.id)
            .order_by(Position.timestamp)
        )
        result_positions = await db.execute(stmt_positions)
        positions = result_positions.all()
        if positions:
            first_lat, first_lon = extract_lat_lon_from_wkt(positions[0].location)
            last_lat, last_lon = extract_lat_lon_from_wkt(positions[-1].location)
            route_dict["start_coords"] = f"{first_lat} {first_lon}"
            route_dict["end_coords"] = f"{last_lat} {last_lon}"
        respone.append(route_dict)
    return respone


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

    result_assignment = await get_assignment_for_route_and_user(db, route, current_user.id)
    assignment = result_assignment.scalars().first()
    if not assignment:
        raise HTTPException(status_code=403, detail="Not authorized to view this route")

    result_positions = await db.execute(
        select(
            Position.id,
            Position.route_id,
            Position.timestamp,
            func.ST_AsText(Position.location).label("location"),
            Position.speed)
        .filter(Position.route_id == route_id))
    positions = result_positions.all()
    response_positions = []
    for pos in positions:
        lat, lon = extract_lat_lon_from_wkt(pos.location)
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

    result_assignment = await get_assignment_for_route_and_user(db, route, current_user.id)
    assignment = result_assignment.scalars().first()
    if not assignment:
        raise HTTPException(status_code=403, detail="Not authorized to delete this route")

    await db.execute(delete(Route).where(Route.id == route_id))
    await db.commit()
    return {"success": True, "message": "Route deleted successfully."}
