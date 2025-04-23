#!/bin/bash

if [[ "$1" == "-s" ]]; then
    docker exec -it db psql -U api_user -d vehicle_db -f /scripts/structure.sql
fi

if [[ "$1" == "-t" ]]; then
    docker exec -it db psql -U api_user -d vehicle_db -f /scripts/test_data.sql
fi

if [[ "$1" == "--all" ]]; then
    docker exec -it db psql -U api_user -d vehicle_db -f /scripts/structure.sql
    docker exec -it db psql -U api_user -d vehicle_db -f /scripts/test_data.sql
fi
