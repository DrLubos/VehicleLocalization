FROM python:3.13

WORKDIR /app

RUN pip install fastapi uvicorn psycopg2-binary geoalchemy2 sqlalchemy asyncpg shapely

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8001"]
