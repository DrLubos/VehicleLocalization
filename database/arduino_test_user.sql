-- Create test user for testing Arduino
INSERT INTO users (id, username, password_hash, email) 
VALUES (2, 'arduino', 'securehash', 'arduino@example.com');

-- Create test vehicle for testing Arduino
INSERT INTO vehicles (id, name, imei, color) 
VALUES (2, 'ArduinoVehicle', '862771071976313', '#0F20FF');

-- Assign test vehicle to test user
INSERT INTO user_vehicle_assignments (id, user_id, vehicle_id) 
VALUES (2, 2, 2);

-- Reset sequence values for teble users, vehicles, user_vehicle_assignments and routes
SELECT setval('users_id_seq', (SELECT MAX(id) FROM users) + 1);
SELECT setval('vehicles_id_seq', (SELECT MAX(id) FROM vehicles) + 1);
SELECT setval('user_vehicle_assignments_id_seq', (SELECT MAX(id) FROM user_vehicle_assignments) + 1);
SELECT setval('routes_id_seq', (SELECT MAX(id) FROM routes) + 1);
