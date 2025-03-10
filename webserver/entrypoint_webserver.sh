#!/bin/sh

cleanup() {
    echo "Container is shutting down, stopping Apache and HTTP server..."
    kill "$HTTP_SERVER_PID" 2>/dev/null
    apachectl -k stop
    exit 0
}

trap cleanup TERM INT

echo "Starting Apache..."
apachectl -D FOREGROUND &

HTTP_SERVER_PID=$!

wait "$HTTP_SERVER_PID"
