"""
This module contains the LoggingMiddleware class, which is a middleware for logging HTTP requests.
"""
import logging
import json
from typing import Callable, Awaitable
from fastapi import Request, Response
from starlette.middleware.base import BaseHTTPMiddleware


class LoggingMiddleware(BaseHTTPMiddleware):
    """
    Middleware for logging HTTP requests.

    This middleware logs the HTTP method, URL, request body, and client IP address
    for each incoming request.

    Methods:
        dispatch(request: Request, call_next): Asynchronously processes the incoming request,
        logs relevant information, and forwards the request to the next middleware or endpoint.

    Attributes:
        None
    """
    async def dispatch(self, request: Request,
                       call_next: Callable[[Request], Awaitable[Response]]) -> Response:
        body_bytes = await request.body()
        try:
            body_str = body_bytes.decode('utf-8')
        except UnicodeDecodeError:
            body = "Error reading body"
        else:
            try:
                body = json.loads(body_str)
            except json.JSONDecodeError:
                body = body_str

        logging.info("Request: %s %s - Body: %s",request.method, request.url, body)

        response = await call_next(request)
        return response
