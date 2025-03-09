-- Table that contains the users of the system
CREATE TABLE users (
    id SERIAL PRIMARY KEY,-- Unique identifier for the user
    username VARCHAR(50) UNIQUE NOT NULL,-- Username of the user
    password_hash TEXT NOT NULL,-- Hash of the user's password
    email VARCHAR(150) UNIQUE NOT NULL,-- Email of the user
    created_at TIMESTAMP(0) DEFAULT CURRENT_TIMESTAMP,-- Date when the user was created
    last_login TIMESTAMP(0) DEFAULT NULL-- Date when the user last logged in
);

-- Table that contains the vehicles
CREATE TABLE vehicles (
    id SERIAL PRIMARY KEY,-- Unique identifier for the vehicle
    name VARCHAR(255) NOT NULL,-- Name of the vehicle
    token CHAR(32) UNIQUE,-- Token used to authenticate the vehicle
    imei VARCHAR(255) NOT NULL,-- IMEI of the vehicle
    status VARCHAR(20) DEFAULT 'registered',-- Status of the vehicle
    color CHAR(7) NOT NULL DEFAULT '#FF0000',-- Hexadecimal color of the vehicle
    position_check_freq SMALLINT NOT NULL DEFAULT 15,-- Frequency in seconds to check the position of the vehicle
    min_distance_delta SMALLINT NOT NULL DEFAULT 3,-- Minimum distance in meters to consider a new position
    max_idle_minutes SMALLINT NOT NULL DEFAULT 15,-- Maximum time in minutes to consider the vehicle as idle
    snap_to_road BOOLEAN NOT NULL DEFAULT FALSE,-- Flag that indicates if the positions should be snapped to the road
    manual_route_start BOOLEAN NOT NULL DEFAULT TRUE,-- Flag that indicates if the routes can be started manually
    created_at TIMESTAMP(0) DEFAULT CURRENT_TIMESTAMP-- Date when the vehicle was created
);

-- Table that contains the user-vehicle assignments
CREATE TABLE user_vehicle_assignments (
    id SERIAL PRIMARY KEY,-- Unique identifier for the assignment
    user_id INT REFERENCES users(id) ON DELETE CASCADE,-- User that is assigned to the vehicle
    vehicle_id INT REFERENCES vehicles(id) ON DELETE CASCADE,-- Vehicle that is assigned to the user
    start_date TIMESTAMP(0) DEFAULT CURRENT_TIMESTAMP,-- Date when the assignment started
    end_date TIMESTAMP(0) DEFAULT NULL,-- Date when the assignment ended
    UNIQUE(user_id, vehicle_id, start_date)-- Unique constraint to avoid duplicate assignments
);

-- Table that contains the routes
CREATE TABLE routes (
    id SERIAL PRIMARY KEY,-- Unique identifier for the route
    assignment_id INT NOT NULL REFERENCES user_vehicle_assignments(id) ON DELETE CASCADE,-- Assignment that the route belongs to
    start_time TIMESTAMP(0) NOT NULL DEFAULT CURRENT_TIMESTAMP,-- Date when the route started
    end_time TIMESTAMP(0) DEFAULT NULL,-- Date when the route ended
    total_distance INT NOT NULL DEFAULT 0,-- Total distance of the route in meters
    start_city VARCHAR(100),-- City where the route started
    end_city VARCHAR(100),-- City where the route ended
    route_geom geometry(LineString, 4326)-- Geometry of the route
);

-- Table that contains the positions for each route
CREATE TABLE positions (
    position_id SERIAL PRIMARY KEY,-- Unique identifier for the position
    route_id INT NOT NULL REFERENCES routes(id) ON DELETE CASCADE,-- Route that the position belongs to
    timestamp TIMESTAMP(0) NOT NULL DEFAULT CURRENT_TIMESTAMP,-- Date when the position was recorded
    location GEOMETRY(Point, 4326) NOT NULL,-- Location of the position
    speed NUMERIC(5,2) NOT NULL-- Speed of the vehicle in km/h
);
