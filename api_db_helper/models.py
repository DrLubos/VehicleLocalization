"""
This module contains the SQLAlchemy models for the database tables.
"""
import datetime

from sqlalchemy import Column, Integer, String, DateTime, Boolean, Float, ForeignKey
from sqlalchemy.orm import declarative_base
from geoalchemy2 import Geometry


Base = declarative_base()


class Vehicle(Base):
    """
    Represents a vehicle in the database.

    Attributes:
        id (int): Primary key.
        name (str): Vehicle name.
        token (str): Unique token.
        imei (str): IMEI number.
        status (str): Vehicle status, default "registered".
        color (str): Vehicle color in hex, default "#FF0000".
        position_check_freq (int): Position check frequency, default 15.
        min_distance_delta (int): Min distance delta for updates, default 3.
        max_idle_minutes (int): Max idle time in minutes, default 15.
        snap_to_road (bool): Snap position to nearest road, default False.
        manual_route_start (bool): Manual route start, default True.
        created_at (datetime): Record creation timestamp, default current UTC time.
    """
    __tablename__ = "vehicles"
    id = Column(Integer, primary_key=True)
    name = Column(String(255), nullable=False)
    token = Column(String(255), unique=True, nullable=False)
    imei = Column(String(255), nullable=False)
    status = Column(String(20), nullable=False, default="registered")
    color = Column(String(7), nullable=False, default="#FF0000")
    position_check_freq = Column(Integer, nullable=False, default=15)
    min_distance_delta = Column(Integer, nullable=False, default=3)
    max_idle_minutes = Column(Integer, nullable=False, default=15)
    snap_to_road = Column(Boolean, nullable=False, default=False)
    manual_route_start = Column(Boolean, nullable=False, default=True)
    created_at = Column(DateTime, nullable=False,
                        default=datetime.datetime.now(datetime.timezone.utc))


class UserVehicleAssignment(Base):
    """
    Represents an assignment of a vehicle to a user.

    Attributes:
        id (int): The primary key of the assignment.
        user_id (int): The ID of the user to whom the vehicle is assigned.
        vehicle_id (int): The ID of the vehicle assigned to the user.
        start_date (datetime): The start date of the assignment.
        end_date (datetime, optional): The end date of the assignment or None.
    """
    __tablename__ = "user_vehicle_assignments"
    id = Column(Integer, primary_key=True)
    user_id = Column(Integer, ForeignKey("users.id", ondelete="CASCADE"))
    vehicle_id = Column(Integer, ForeignKey("vehicles.id", ondelete="CASCADE"))
    start_date = Column(DateTime, nullable=False,
                        default=datetime.datetime.now(datetime.timezone.utc))
    end_date = Column(DateTime, nullable=True)


class Route(Base):
    """
    Represents a route taken by a vehicle.

    Attributes:
        id (int): Primary key.
        assignment_id (int): Foreign key referencing user_vehicle_assignments.
        start_time (datetime): Start time, defaults to current UTC.
        end_time (datetime, optional): End time, can be null.
        total_distance (int): Total distance, defaults to 0.
        start_city (str): Starting city.
        end_city (str): Ending city.
        route_geom (Geometry): Geometric representation as LINESTRING with SRID 4326.
    """
    __tablename__ = "routes"
    id = Column(Integer, primary_key=True)
    assignment_id = Column(Integer, ForeignKey("user_vehicle_assignments.id",
                                               ondelete="CASCADE"), nullable=False)
    start_time = Column(DateTime, nullable=False,
                        default=datetime.datetime.now(datetime.timezone.utc))
    end_time = Column(DateTime, nullable=True)
    total_distance = Column(Integer, nullable=False, default=0)
    start_city = Column(String(100), nullable=False)
    end_city = Column(String(100), nullable=False)
    route_geom = Column(Geometry(geometry_type="LINESTRING", srid=4326), nullable=False)


class Position(Base):
    """
    Represents a Position in the database.

    Attributes:
        position_id (int): Primary key.
        route_id (int): Foreign key referencing the route.
        timestamp (datetime): When the position was recorded. Defaults to current UTC time.
        location (Geometry): Geographical location as POINT with SRID 4326.
        speed (float): Speed in km/h at the recorded position.
    """
    __tablename__ = "positions"
    position_id = Column(Integer, primary_key=True)
    route_id = Column(Integer, ForeignKey("routes.id", ondelete="CASCADE"), nullable=False)
    timestamp = Column(DateTime, nullable=False,
                       default=datetime.datetime.now(datetime.timezone.utc))
    location = Column(Geometry(geometry_type="POINT", srid=4326), nullable=False)
    speed = Column(Float, nullable=False)
