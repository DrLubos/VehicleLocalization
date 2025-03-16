#!/bin/bash

docker exec -it db psql -U api_user -d vehicle_db -f /scripts/structure.sql

if [[ "$1" == "-t" ]]; then
    docker exec -it db psql -U api_user -d vehicle_db -f /scripts/test_data.sql
fi

if [[ "$1" == "--all" ]]; then
    docker exec -it db psql -U api_user -d vehicle_db -f /scripts/test_data.sql
    docker exec -it db psql -U api_user -d vehicle_db -f /scripts/arduino_test_user.sql
fi
