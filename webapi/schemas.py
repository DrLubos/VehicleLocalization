"""
This file contains the Pydantic models for the webapi.
"""
from pydantic import BaseModel

class LoginRequest(BaseModel):
    """
    LoginRequest schema for user login.

    Attributes:
        username (str): The username of the user.
        password (str): The password of the user.
    """
    username: str
    password: str
