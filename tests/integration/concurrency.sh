#!/bin/bash

set -e

# run server
echo ========starting server========
cd ../..
./kvstore &
SERVER_PID=$!

sleep 1
#========run test========

# many clients pushing onto the same list concurrently.
# each redis-cli call is its own TCP connection, so this exercises the
# accept loop spawning multiple worker threads at once, and the Database
# mutex serializing concurrent rpush calls against the same deque.
echo ------concurrent rpush from many clients onto one list------
NUM_WORKERS=10
NUM_PUSHES=20

WORKER_PIDS=()
for w in $(seq 1 $NUM_WORKERS); do
    (
        for i in $(seq 1 $NUM_PUSHES); do
            redis-cli -p 6379 RPUSH concurrentlist "w${w}-i${i}" > /dev/null
        done
    ) &
    WORKER_PIDS+=($!)
done
wait "${WORKER_PIDS[@]}"

EXPECTED=$((NUM_WORKERS * NUM_PUSHES))
echo "expected llen: $EXPECTED"
redis-cli -p 6379 LLEN concurrentlist

echo "unique values pushed (should also be $EXPECTED, catches lost/duplicated writes):"
redis-cli -p 6379 LRANGE concurrentlist 0 -1 | sort -u | wc -l


# many clients hset-ing distinct fields onto the same hash concurrently.
# each field is unique, so a correct implementation ends with hlen == NUM_FIELDS.
# a race that drops updates (e.g. lost update on the underlying unordered_map)
# would show up as a smaller count.
echo ------concurrent hset from many clients onto one hash------
NUM_FIELDS=15

WORKER_PIDS=()
for f in $(seq 1 $NUM_FIELDS); do
    redis-cli -p 6379 HSET concurrenthash "field${f}" "value${f}" > /dev/null &
    WORKER_PIDS+=($!)
done
wait "${WORKER_PIDS[@]}"

echo "expected hlen: $NUM_FIELDS"
redis-cli -p 6379 HLEN concurrenthash


# many clients each setting/getting their own distinct key concurrently.
# checks that simultaneous connections are all actually served (not
# serialized/dropped) rather than checking data-structure correctness.
echo ------many simultaneous clients on distinct keys------
NUM_CLIENTS=20

WORKER_PIDS=()
for c in $(seq 1 $NUM_CLIENTS); do
    redis-cli -p 6379 SET "key${c}" "val${c}" > /dev/null &
    WORKER_PIDS+=($!)
done
wait "${WORKER_PIDS[@]}"

echo "expected key count: $NUM_CLIENTS"
redis-cli -p 6379 KEYS | grep -c '^key' || true

for c in $(seq 1 $NUM_CLIENTS); do
    val=$(redis-cli -p 6379 GET "key${c}")
    if [ "$val" != "val${c}" ]; then
        echo "MISMATCH: key${c} = $val (expected val${c})"
    fi
done
echo "no MISMATCH lines above means every key survived concurrent writes intact"


# clean up so a graceful shutdown has to join a pile of just-finished
# worker threads rather than an already-idle server
redis-cli -p 6379 FLUSHALL


# gracefully shutdown server
echo ========shutting down server========
kill -SIGINT $SERVER_PID
wait $SERVER_PID 2>/dev/null || true
