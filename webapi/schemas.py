"""
This file contains the Pydantic models for the webapi.
"""
from datetime import datetime
from typing import Optional
from pydantic import BaseModel, Field
from api_db_helper.models import VehicleStatus

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
        snap_to_road (bool): Whether to snap the position to the nearest road.
        manual_route_start (bool): Whether to manually start the route.
    """
    name: str
    imei: str
    color: str
    position_check_freq: int
    min_distance_delta: int
    max_idle_minutes: int
    snap_to_road: bool
    manual_route_start: bool


class VehicleUpdate(BaseModel):
    """
    VehicleUpdate schema for updating a vehicle.

    Attributes:
        name (str, optional): The name of the vehicle.
        color (str, optional): The color of the vehicle in hex format.
        position_check_freq (int, optional): The position check frequency in minutes.
        min_distance_delta (int, optional): The minimum distance delta for updates.
        max_idle_minutes (int, optional): The maximum idle time in minutes.
        snap_to_road (bool, optional): Whether to snap the position to the nearest road.
        manual_route_start (bool, optional): Whether to manually start the route.
    """
    name: Optional[str] = None
    status: Optional[VehicleStatus] = None
    color: Optional[str] = None
    position_check_freq: Optional[int] = None
    min_distance_delta: Optional[int] = None
    max_idle_minutes: Optional[int] = None
    snap_to_road: Optional[bool] = None
    manual_route_start: Optional[bool] = None


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
        snap_to_road (bool): Whether to snap the position to the nearest road.
        manual_route_start (bool): Whether to manually start the route.
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
    snap_to_road: bool = Field(..., title="Snap To Road",
                               description="Enable snap position to nearest road", example=False)
    manual_route_start: bool = Field(..., title="Manual Route Start",
                                     description="Whether to manually start route", example=True)
    created_at: datetime = Field(..., title="Created At",
                            description="Creation time of vehicle", example="2025-01-01T00:00:00Z")


class RouteResponse(BaseModel):
    """
    RouteResponse schema for API response.

    Attributes:
        id (int): The ID of the route.
        assignment_id (int): The ID of the user vehicle assignment.
        start_time (str): The start time of the route.
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
    end_city: Optional[str] = Field(None, title="End City",
                          description="Ending city of the route", example="Los Angeles")
    route_geometry: Optional[str] = Field(None, title="Route Geometry",
                                          description="Encoded polyline of the route",
                                          example="LINESTRING(30 10, 10 30, 40 40)")


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
