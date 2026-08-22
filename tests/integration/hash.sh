#!/bin/bash

set -e

# run server
echo ========starting server========
cd ../..
./kvstore &
SERVER_PID=$!

sleep 1
#========run test========

# set multiple fields on a fresh hash in one call
echo ------hset multiple fields on myhash------
redis-cli -p 6379 HSET myhash name gopher age 3
redis-cli -p 6379 HGETALL myhash

# check the length of the hash
echo ------hlen on myhash------
redis-cli -p 6379 HLEN myhash

# fetch an existing field and a missing field
echo ------hget on myhash------
redis-cli -p 6379 HGET myhash name
redis-cli -p 6379 HGET myhash nosuchfield

# check field existence, both present and absent
echo ------hexists on myhash------
redis-cli -p 6379 HEXISTS myhash name
redis-cli -p 6379 HEXISTS myhash nosuchfield

# list out all fields and all values
echo ------hkeys/hvals on myhash------
redis-cli -p 6379 HKEYS myhash
redis-cli -p 6379 HVALS myhash

# overwrite an existing field via hset and confirm it updates in place
echo ------hset overwriting an existing field------
redis-cli -p 6379 HSET myhash age 4
redis-cli -p 6379 HGET myhash age
redis-cli -p 6379 HLEN myhash

# delete one field and confirm the rest survive
echo ------hdel a single field on myhash------
redis-cli -p 6379 HDEL myhash age
redis-cli -p 6379 HGETALL myhash
redis-cli -p 6379 HLEN myhash

# delete the remaining field(s) and see the hash emptied out
echo ------hdel down to empty, then against a missing key------
redis-cli -p 6379 HDEL myhash name
redis-cli -p 6379 HLEN myhash
redis-cli -p 6379 HGET myhash name
redis-cli -p 6379 HDEL myhash name

# type check should report hash, and del should remove the key
echo ------type/del on a hash------
redis-cli -p 6379 HSET otherhash f1 v1
redis-cli -p 6379 TYPE otherhash
redis-cli -p 6379 DEL otherhash
redis-cli -p 6379 HLEN otherhash



# gracefully shutdown server
echo ========shutting down server========
kill -SIGINT $SERVER_PID
wait $SERVER_PID 2>/dev/null || true
