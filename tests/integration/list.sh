#!/bin/bash

set -e

# run server
echo ========starting server========
cd ../..
./kvstore &
SERVER_PID=$!

sleep 1
#========run test========

# push some values onto a fresh list from both ends
echo ------lpush/rpush onto mylist------
redis-cli -p 6379 RPUSH mylist a b c
redis-cli -p 6379 LPUSH mylist z
redis-cli -p 6379 LRANGE mylist 0 -1

# check the length of the list
echo ------llen on mylist------
redis-cli -p 6379 LLEN mylist

# check an index in range and one out of range
echo ------lindex on mylist------
redis-cli -p 6379 LINDEX mylist 0
redis-cli -p 6379 LINDEX mylist 100

# overwrite the value at an index
echo ------lset on mylist------
redis-cli -p 6379 LSET mylist 0 zz
redis-cli -p 6379 LRANGE mylist 0 -1

# remove a value from the list
echo ------lrem on mylist------
redis-cli -p 6379 RPUSH mylist a
redis-cli -p 6379 LREM mylist 1 a
redis-cli -p 6379 LRANGE mylist 0 -1

# pop from both ends and see the list shrink
echo ------lpop/rpop on mylist------
redis-cli -p 6379 LPOP mylist
redis-cli -p 6379 RPOP mylist
redis-cli -p 6379 LRANGE mylist 0 -1
redis-cli -p 6379 LLEN mylist

# pop the list empty, then check behavior against a missing key
echo ------popping mylist down to empty------
redis-cli -p 6379 LPOP mylist
redis-cli -p 6379 LLEN mylist
redis-cli -p 6379 LPOP mylist

# type check should still report list even before it's emptied, and del should remove it
echo ------type/del on a list------
redis-cli -p 6379 RPUSH otherlist 1 2 3
redis-cli -p 6379 TYPE otherlist
redis-cli -p 6379 DEL otherlist
redis-cli -p 6379 LLEN otherlist



# gracefully shutdown server
echo ========shutting down server========
kill -SIGINT $SERVER_PID
wait $SERVER_PID 2>/dev/null || true
