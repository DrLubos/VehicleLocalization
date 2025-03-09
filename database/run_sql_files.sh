#!/bin/bash

docker exec -it db psql -U api_user -d vehicle_db -f /scripts/structure.sql
docker exec -it db psql -U api_user -d vehicle_db -f /scripts/test_data.sql
docker exec -it db psql -U api_user -d vehicle_db -f /scripts/arduino_test_user.sql
