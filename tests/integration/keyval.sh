#!/bin/bash

set -e

# run server
echo ========starting server========
cd ../..
./kvstore &
SERVER_PID=$!

sleep 1
#========run test========

# sanity check the connection
echo ------testing ping and echo------
redis-cli -p 6379 PING
redis-cli -p 6379 ECHO hello

#set two keys and get all keys back
echo ------setting two keys------
redis-cli -p 6379 SET delete_me 123
redis-cli -p 6379 SET rename_me 123
redis-cli -p 6379 KEYS

# check type on an existing key and a missing one
echo ------testing type------
redis-cli -p 6379 TYPE delete_me
redis-cli -p 6379 TYPE ghost

# get on a key that was never set
echo ------testing get on missing key------
redis-cli -p 6379 GET ghost

# rename rename_me to renamed_key
echo ------renaming rename_me to renamed_key------
redis-cli -p 6379 RENAME rename_me renamed_key
redis-cli -p 6379 KEYS
redis-cli -p 6379 GET renamed_key

# expire renamed_key see if expiry worked
echo ------testing expire on renamed_key------
redis-cli -p 6379 EXPIRE renamed_key 5
sleep 6
redis-cli -p 6379 GET renamed_key
redis-cli -p 6379 KEYS

# test delete on delete_me
echo ------testing delete------
redis-cli -p 6379 DEL delete_me
redis-cli -p 6379 GET delete_me

# delete on a key that doesn't exist
echo ------testing delete on nonexistent key------
redis-cli -p 6379 DEL ghost

# flushall should wipe everything
echo ------testing flushall------
redis-cli -p 6379 SET leftover 123
redis-cli -p 6379 FLUSHALL
redis-cli -p 6379 KEYS

# gracefully shutdown server
echo ========shutting down server========
kill -SIGINT $SERVER_PID
wait $SERVER_PID 2>/dev/null || true