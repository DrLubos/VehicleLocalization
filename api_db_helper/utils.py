"""This module contains utility functions for interacting with the OpenWeatherMap API."""
import os
import logging
import requests
from dotenv import load_dotenv


load_dotenv()


async def get_city_by_coords(lat: float, lon: float) -> str:
    """
    Fetches the city name and country based on the provided latitude and longitude coordinates
    using the OpenWeatherMap API.

    Args:
        lat (float): Latitude of the location.
        lon (float): Longitude of the location.

    Returns:
        str: A string containing the city name and country in the format "City, Country",
             or just the city or country if one is missing. Returns an empty string if
             the city cannot be determined or if an error occurs.

    Notes:
        - Requires an OpenWeatherMap API key to be set in the environment variable
          "OPENWEATHERMAP_API_KEY".
        - Logs an error if the API key is missing or if the request fails.
        - Handles API response errors and exceptions gracefully.
    """
    key = os.getenv("OPENWEATHERMAP_API_KEY")
    if not key:
        logging.error("Missing OpenWeather API key")
        return ""
    url = f"http://api.openweathermap.org/geo/1.0/reverse?lat={lat}&lon={lon}&limit=1&appid={key}"
    try:
        response = requests.get(url, timeout=5)
        if response.status_code == 200:
            data = response.json()
            print(f"Response data: {data}")
            if data:
                city_name = data[0].get("name", "")
                country = data[0].get("country", "")
                if city_name and country:
                    return f"{city_name}, {country}"
                elif city_name:
                    return city_name
                elif country:
                    return country
                return ""
        else:
            logging.error("Failed to fetch city data: %s %s", response.status_code, response.text)
    except requests.RequestException as e:
        print(f"Request failed: {e}")
        logging.error("Request failed: %s", e)
    return ""
