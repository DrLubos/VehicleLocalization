"""
This module contains the Pydantic schemas for the API.
"""
from pydantic import BaseModel, Field


class PositionRequest(BaseModel):
    """
    PositionRequest schema for vehicle localization API.

    Attributes:
        token (str): Received token from device.
            - title: Token
            - description: Received token from device
            - example: "abcdef1234567890"
        lat (float): Received latitude.
            - title: Latitude
            - description: Received latitude
            - example: 48.1234567
        lon (float): Received longitude.
            - title: Longitude
            - description: Received longitude
            - example: -17.1234567
        speed (float): Received speed.
            - title: Speed
            - description: Received speed
            - example: 50.0
    """
    token: str = Field(..., title="Token",
                       description="Received token from device", example="abcdef1234567890")
    lat: float = Field(..., title="Latitude",
                       description="Received latitude", example=48.1234567)
    lon: float = Field(..., title="Longitude",
                       description="Received longitude", example=-17.1234567)
    speed: float = Field(..., title="Speed",
                         description="Received speed", example=50.0)


class RouteCreationRequest(BaseModel):
    """
    RouteCreationRequest schema for creating a new route.

    Attributes:
        token (str): Received token from device.
            - title: Token
            - description: Received token from device
            - example: "abcdef1234567890"
    """
    token: str = Field(..., title="Token",
                       description="Received token from device", example="abcdef1234567890")


class TokenRequest(BaseModel):
    """
    TokenRequest schema for handling token requests.

    Attributes:
        imei (str): IMEI received from the device.
            - title: "IMEI"
            - description: "Received IMEI from device"
            - example: "123456789012345"
    """
    imei: str = Field(..., title="IMEI",
                      description="Received IMEI from device", example="123456789012345")


class TokenResponse(BaseModel):
    """
    TokenResponse schema for API response.

    Attributes:
        status (str): The status of the token response, default is "success".
        token (str): The token string.
        position_check_freq (int): Frequency of position checks in seconds.
        min_distance_delta (int): Minimum distance delta in meters.
        manual_start (bool): Manual start flag.
    """
    status: str = Field(default="success")
    token: str
    position_check_freq: int
    min_distance_delta: int
    manual_start: bool


class TokenVerifyRequest(BaseModel):
    """
    TokenVerifyRequest schema for verifying token requests.

    Attributes:
        imei (str): IMEI received from the device.
            - title: "IMEI"
            - description: "Received IMEI from device"
            - example: "123456789012345"
        token (str): Token received from the device.
            - title: "Token"
            - description: "Received token from device"
            - example: "abcdef1234567890"
    """
    imei: str = Field(..., title="IMEI",
                      description="Received IMEI from device", example="123456789012345")
    token: str = Field(..., title="Token",
                       description="Received token from device", example="abcdef1234567890")
