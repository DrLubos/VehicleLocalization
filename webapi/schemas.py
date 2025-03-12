"""
This file contains the Pydantic models for the webapi.
"""
from pydantic import BaseModel
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
    name: str = None
    status: VehicleStatus = None
    color: str = None
    position_check_freq: int = None
    min_distance_delta: int = None
    max_idle_minutes: int = None
    snap_to_road: bool = None
    manual_route_start: bool = None
