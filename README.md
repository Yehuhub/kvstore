# kvstore

## Overview

`kvstore` is a lightweight, Redis-compatible in-memory key-value database written in modern C++. It implements the Redis Serialization Protocol (RESP) and supports strings, lists, and hashes through a Redis-compatible command interface.

The server supports concurrent client connections, with synchronized access to a shared database. It also provides key expiration with a background cleanup thread and custom persistence through a versioned snapshot format, allowing data and expiration times to survive server restarts.

The project was built from the ground up to explore C++ systems programming concepts including networking, concurrency, synchronization, data structures, protocol parsing, resource management, and serialization.

## Description

**Name:** `kvstore`  
**Default Port:** `6379`

It supports:

- **Common Commands:** `PING`, `ECHO`, `FLUSHALL`
- **Key/Value:** `SET`, `GET`, `KEYS`, `TYPE`, `DEL` / `UNLINK`, `EXPIRE`, `RENAME`
- **List:** `LLEN`, `LPUSH` / `RPUSH` (multi-element), `LPOP` / `RPOP`, `LREM`, `LINDEX`, `LSET`, `LRANGE`
- **Hash:** `HSET`, `HGET`, `HEXISTS`, `HDEL`, `HKEYS`, `HVALS`, `HLEN`, `HGETALL`


## Repository Structure

```
kvstore/
├── include/
│   ├── Command.h
│   ├── CommandHandler.h
│   ├── Database.h
│   ├── RespEncoder.h
│   ├── RespParser.h
│   ├── Server.h
│   └── WrongTypeError.h
├── src/
│   ├── CommandHandler.cpp
│   ├── Database.cpp
│   ├── main.cpp
│   ├── RespEncoder.cpp
│   ├── RespParser.cpp
│   └── Server.cpp
├── tests/
│   └── integration/
│       ├── keyval.sh       # PING/ECHO/FLUSHALL + key/value ops
│       ├── list.sh         # list ops
│       ├── hash.sh         # hash ops
│       └── concurrency.sh  # concurrent client access
├── Makefile
└── README.md
```


## Installation
This project uses a simple Makefile. Ensure you have a C++17 (or later) compiler.
- `make`
- `make clean`

```bash
# from project root
make
```


## Usage
After compiling the server, you can run it and use with client.

### Running the Server

Start the server on the default port (6379) or specify a port:

```bash
./kvstore            # listens on 6379
./kvstore 6380        # listens on 6380
```


To gracefully shutdown and persist immediately, press `Ctrl+C`.


### Using the Server

Connect with the standard `redis-cli`.

```bash
# Using redis-cli:
redis-cli -p 6379

# Example session:
127.0.0.1:6379> PING
PONG

127.0.0.1:6379> SET foo "bar"
OK

127.0.0.1:6379> GET foo
"bar"
```


## Supported Commands

### Common
- **PING**: `PING` → `PONG`
- **ECHO**: `ECHO <msg>` → `<msg>`
- **FLUSHALL**: `FLUSHALL` → clear all data

### Key/Value
- **SET**: `SET <key> <value>` → store string
- **GET**: `GET <key>` → retrieve string or nil
- **KEYS**: `KEYS *` → list all keys
- **TYPE**: `TYPE <key>` → `string`/`list`/`hash`/`none`
- **DEL/UNLINK**: `DEL <key>` → delete key
- **EXPIRE**: `EXPIRE <key> <seconds>` → set TTL
- **RENAME**: `RENAME <old> <new>` → rename key

### Lists
- **LLEN**: `LLEN <key>` → length
- **LPUSH/RPUSH**: `LPUSH <key> <v1> [v2 ...]` / `RPUSH` → push multiple
- **LPOP/RPOP**: `LPOP <key>` / `RPOP <key>` → pop one
- **LREM**: `LREM <key> <count> <value>` → remove occurrences
- **LINDEX**: `LINDEX <key> <index>` → get element
- **LSET**: `LSET <key> <index> <value>` → set element
- **LRANGE**: `LRANGE <key> <start> <stop>` → get a range of elements

### Hashes
- **HSET**: `HSET <key> <field> <value>`
- **HGET**: `HGET <key> <field>`
- **HEXISTS**: `HEXISTS <key> <field>`
- **HDEL**: `HDEL <key> <field>`
- **HLEN**: `HLEN <key>` → field count
- **HKEYS**: `HKEYS <key>` → all fields
- **HVALS**: `HVALS <key>` → all values
- **HGETALL**: `HGETALL <key>` → field/value pairs


## Design & Architecture

- **Server** – listens on a TCP socket and accepts connections. Each client gets its own thread (`Worker`), tracked in a map so finished threads can be cleaned up (`reapFinishedThreads`). `Ctrl+C` triggers a signal handler that shuts things down gracefully.
- **RespParser / RespEncoder** – convert between raw RESP protocol bytes and `Command` objects / reply strings.
- **CommandHandler** – takes a parsed `Command`, looks up the right handler in a name → member-function-pointer map, and calls it against the `Database`.
- **Database** – stores everything in a single `unordered_map<string, RedisValue>`, where `RedisValue` is a `variant<string, deque<string>, unordered_map<string,string>>` (covers string/list/hash). A `shared_mutex` protects it: reads take a shared lock, writes take an exclusive lock, so multiple clients can read at the same time but writes are serialized.
- **Persistence** – `Database::save`/`Database::load` (de)serialize `m_data` and `m_expiryMap` to a simple line-based text format (magic header, entry count, then per-key type/name/expiry followed by the value). `save` writes to a temp file and renames it into place to avoid a partial/corrupt file if the process dies mid-write. `main.cpp` calls `load("persistence.kv")` before starting the server and `save("persistence.kv")` after `dbServer.run()` returns (i.e. on graceful shutdown).

## Concepts & Use Cases

This project is mainly a learning exercise covering:
- TCP sockets and a basic client/server protocol (RESP)
- Multithreading — one thread per client connection
- Thread-safe data access with `shared_mutex`
- Parsing a wire protocol and mapping it to command handlers


## Testing

Tests live in `tests/integration/` as bash scripts (`keyval.sh`, `list.sh`, `hash.sh`, `concurrency.sh`). Each script starts the server, runs a sequence of commands through `redis-cli`, and checks the output. `concurrency.sh` fires commands from multiple clients at once to check the locking holds up.

Run a script from its directory, e.g.:

```bash
cd tests/integration
./keyval.sh
```

