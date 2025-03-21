"""
This file contains the Pydantic models for the webapi.
"""
from datetime import datetime
from typing import Optional
from pydantic import BaseModel, Field
from api_db_helper.models import VehicleStatus


class RegistrationRequest(BaseModel):
    """
    RegistrationRequest schema for user registration.

    Attributes:
        username (str): Username for registration. Example: "user".
        email (str): Email address for registration. Example: "name@domain.com".
        password (str): Password for registration. Example: "password".
    """
    username: str = Field(..., title="Username",
                          description="Username for registration", example="user")
    email: str = Field(..., title="Email",
                       description="Email for registration", example="name@domain.com")
    password: str = Field(..., title="Password",
                          description="Password for registration", example="password")


class LoginRequest(BaseModel):
    """
    LoginRequest schema for user login.

    Attributes:
        username (str): The username of the user.
        password (str): The password of the user.
    """
    username: str
    password: str


class VehicleCreate(BaseModel):
    """
    VehicleCreate schema for creating a new vehicle.

    Attributes:
        name (str): The name of the vehicle.
        imei (str): The IMEI number of the vehicle.
        color (str): The color of the vehicle in hex format.
        position_check_freq (int): The position check frequency in minutes.
        min_distance_delta (int): The minimum distance delta for updates.
        max_idle_minutes (int): The maximum idle time in minutes.
        manual_route_start_enabled (bool): Whether to manually start the route.
    """
    name: str
    imei: str
    color: str
    position_check_freq: int
    min_distance_delta: int
    max_idle_minutes: int
    manual_route_start_enabled: bool


class VehicleUpdate(BaseModel):
    """
    VehicleUpdate schema for updating a vehicle.

    Attributes:
        name (str, optional): The name of the vehicle.
        color (str, optional): The color of the vehicle in hex format.
        position_check_freq (int, optional): The position check frequency in minutes.
        min_distance_delta (int, optional): The minimum distance delta for updates.
        max_idle_minutes (int, optional): The maximum idle time in minutes.
        manual_route_start_enabled (bool, optional): Whether to manually start the route.
    """
    name: Optional[str] = None
    status: Optional[VehicleStatus] = None
    color: Optional[str] = None
    position_check_freq: Optional[int] = None
    min_distance_delta: Optional[int] = None
    max_idle_minutes: Optional[int] = None
    manual_route_start_enabled: Optional[bool] = None


class VehicleResponse(BaseModel):
    """
    VehicleResponse schema for API response.

    Attributes:
        id (int): The ID of the vehicle.
        name (str): The name of the vehicle.
        token (str): The token of the vehicle.
        imei (str): The IMEI number of the vehicle.
        status (str): The status of the vehicle.
        color (str): The color of the vehicle in hex format.
        position_check_freq (int): The position check frequency in seconds.
        min_distance_delta (int): The minimum distance delta for updates.
        max_idle_minutes (int): The maximum idle time in minutes.
        manual_route_start_enabled (bool): Whether to manually start the route.
        created_at (str): The creation timestamp of the vehicle.
    """
    id: int = Field(..., title="ID", description="The ID of the vehicle", example=1)
    name: str = Field(..., title="Name", description="The name of the vehicle", example="Car")
    token: Optional[str] = Field(None, title="Token",
                       description="The token of the vehicle", example="abcdef1234567890")
    imei: str = Field(..., title="IMEI",
                      description="The IMEI number of the vehicle", example="123456789012345")
    status: VehicleStatus = Field(..., title="Status",
                                  description="The status of the vehicle", example="registered")
    color: str = Field(..., title="Color",
                       description="The color of the vehicle in hex format", example="#FF0000")
    position_check_freq: int = Field(..., title="Position Check Frequency",
                                     description="Position check frequency in seconds", example=15)
    min_distance_delta: int = Field(..., title="Min Distance Delta",
                                    description="The minimum distance delta for updates", example=3)
    max_idle_minutes: int = Field(..., title="Max Idle Minutes",
                                  description="The maximum idle time in minutes", example=15)
    manual_route_start_enabled: bool = Field(..., title="Manual Route Start",
                                     description="Whether to manually start route", example=True)
    created_at: datetime = Field(..., title="Created At",
                            description="Creation time of vehicle", example="2025-01-01T00:00:00Z")


class LastPosition(BaseModel):
    """
    LastPosition schema for vehicle localization.

    Attributes:
        city (Optional[str]): The city where the vehicle is located. Example: "New York".
        latitude (float): The latitude coordinate of the vehicle's location. Example: 48.1486.
        longitude (float): The longitude coordinate of the vehicle's location. Example: 17.1077.
        location_time (datetime): The timestamp of when the vehicle was at the given location.
            Example: "2023-03-14T18:41:00Z".
        speed (float): The speed of the vehicle at the given location. Example: 50.0.
    """
    city: Optional[str] = Field(None, title="City", example="New York")
    latitude: float = Field(..., title="Latitude", example=48.1486)
    longitude: float = Field(..., title="Longitude", example=17.1077)
    location_time: datetime = Field(..., title="Location Time", example="2023-03-14T18:41:00Z")
    speed: float = Field(..., title="Speed", example=50.0)


class VehiclePositionResponse(BaseModel):
    """
    VehiclePositionResponse

    Attributes:
        vehicle (VehicleResponse): The vehicle information.
        last_position (LastPosition, optional): Most recent position of the vehicle.
    """
    vehicle: VehicleResponse = Field(..., title="Vehicle",
                                     description="The vehicle information",
                                     example={
                                         "id": 1,
                                         "name": "Car",
                                         "token": "abcdef1234567890",
                                         "imei": "123456789012345",
                                         "status": "registered",
                                         "color": "#FF0000",
                                         "position_check_freq": 15,
                                         "min_distance_delta": 3,
                                         "max_idle_minutes": 15,
                                         "manual_route_start_enabled": True,
                                         "created_at": "2025-01-01T00:00:00Z"
                                     })
    last_position: Optional[LastPosition] = Field(None, title="Last Position",
                                                  description="Most recent position of the vehicle",
                                                  example={
                                                      "city": "New York",
                                                      "latitude": 48.1486,
                                                      "longitude": 17.1077,
                                                      "location_time": "2023-03-14T18:41:00Z",
                                                      "speed": 50.0
                                                  })


class RouteResponse(BaseModel):
    """
    RouteResponse schema for representing route information.

    Attributes:
        id (int): The ID of the route.
        assignment_id (int): The ID of the user vehicle assignment.
        start_time (datetime): The start time of the route.
        end_time (Optional[datetime]): The end time of the route.
        total_distance (int): Total distance of the route.
        start_city (Optional[str]): Starting city of the route.
        start_coords (str): Starting coordinates of the route.
        end_city (Optional[str]): Ending city of the route.
        end_coords (str): Ending coordinates of the route.
        route_geometry (Optional[dict]): GeoJSON representation of the route.
    """
    id: int = Field(..., title="ID", description="The ID of the route", example=1)
    assignment_id: int = Field(..., title="Assignment ID",
                               description="The ID of the user vehicle assignment", example=1)
    start_time: datetime = Field(..., title="Start Time",description="The start time of the route",
                                 example="2025-01-01T00:00:00Z")
    end_time: Optional[datetime] = Field(None, title="End Time",
                                         description="The end time of the route",
                                         example="2025-01-01T00:00:00Z")
    total_distance: int = Field(..., title="Total Distance",
                                  description="Total distance of the route", example=100)
    start_city: Optional[str] = Field(None, title="Start City",
                            description="Starting city of the route", example="New York")
    start_coords: str = Field(..., title="Start Coordinates",
                            description="Starting coordinates of route", example="40.7128 74.0060")
    end_city: Optional[str] = Field(None, title="End City",
                          description="Ending city of the route", example="Los Angeles")
    end_coords: str = Field(..., title="End Coordinates",
                          description="Ending coordinates of the route", example="34.0522 118.2437")
    route_geometry: Optional[dict] = Field(None, title="Route Geometry",
                                            description="GeoJSON representation of the route",
                                            example={
                                                "type": "LineString",
                                                "coordinates": [
                                                    [30, 10],
                                                    [10, 30],
                                                    [40, 40]
                                                ]
                                            })


class PositionResponse(BaseModel):
    """
    PositionResponse schema for API response.

    Attributes:
        latitude (float): The latitude of the position.
        longitude (float): The longitude of the position.
        timestamp (str): The timestamp of the position.
    """
    id: int = Field(..., title="ID", description="The ID of the position", example=1)
    route_id: int = Field(..., title="Route ID", description="The ID of the route", example=1)
    timestamp: datetime = Field(..., title="Timestamp",
                                description="The timestamp of the position",
                                example="2025-01-01T00:00:00Z")
    latitude: float = Field(..., title="Latitude", description="The latitude of the position",
                            example=37.7749)
    longitude: float = Field(..., title="Longitude", description="The longitude of the position",
                             example=-122.4194)
    speed: float = Field(..., title="Speed", description="The speed of the vehicle", example=60.0)
