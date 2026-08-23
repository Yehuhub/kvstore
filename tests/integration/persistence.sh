#!/bin/bash

set -e

cd ../..
rm -f persistence.kv

# make sure no leftover server from a previous (failed) run is still
# squatting on the port, which would silently corrupt this run's data
pkill -f "\./kvstore" 2>/dev/null || true
sleep 0.5

cleanup() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill -SIGINT "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# run server
echo "========starting server========"
./kvstore &
SERVER_PID=$!

sleep 1

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "server failed to start (port already in use?)" >&2
    exit 1
fi
#========run test========

# seed one of each type, plus a key with a TTL that will still be
# alive at restart and one that will have already expired by then
echo ------seeding data------
redis-cli -p 6379 SET pkey "persisted value"
redis-cli -p 6379 RPUSH plist a b c
redis-cli -p 6379 HSET phash f1 v1
redis-cli -p 6379 SET long_ttl foo
redis-cli -p 6379 EXPIRE long_ttl 100
redis-cli -p 6379 SET short_ttl foo
redis-cli -p 6379 EXPIRE short_ttl 1
redis-cli -p 6379 KEYS

# let short_ttl expire before we shut down
sleep 2

# gracefully shutdown server, which should save persistence.kv
echo "========shutting down server (should save)========"
kill -SIGINT $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

echo ------checking persistence.kv was written------
ls -l persistence.kv

# restart the server, which should load persistence.kv
echo "========restarting server (should load)========"
./kvstore &
SERVER_PID=$!

sleep 1

echo ------checking data survived restart------
redis-cli -p 6379 KEYS
redis-cli -p 6379 GET pkey
redis-cli -p 6379 LRANGE plist 0 -1
redis-cli -p 6379 HGETALL phash

# already-expired key should not have been persisted
echo ------checking expired key was dropped------
redis-cli -p 6379 GET short_ttl

# TTL key that was still alive should survive with its expiry intact
echo ------checking live-TTL key survived and still expires------
redis-cli -p 6379 GET long_ttl

# gracefully shutdown server
echo ========shutting down server========
kill -SIGINT $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

rm -f persistence.kv
