#!/bin/bash

set -e

# run server
echo ========starting server========
cd ../..
./kvstore &
SERVER_PID=$!

sleep 1
#========run test========

#set two keys and get all keys back
echo ------setting two keys------
redis-cli -p 6379 SET munani 123
redis-cli -p 6379 SET muni 123
redis-cli -p 6379 KEYS


# rename muni to muna 
echo ------renaming muni to muna------
redis-cli -p 6379 RENAME muni muna
redis-cli -p 6379 KEYS
redis-cli -p 6379 GET muna

# expire muna see if expiry worked
echo ------testing expire on muna------
redis-cli -p 6379 EXPIRE muna 5
sleep 6
redis-cli -p 6379 GET muna
redis-cli -p 6379 KEYS

# test delete on munani
echo ------testing delete------
redis-cli -p 6379 DEL munani
redis-cli -p 6379 GET munani



# gracefully shutdown server
echo ========shutting down server========
kill -SIGINT $SERVER_PID
wait $SERVER_PID 2>/dev/null || true