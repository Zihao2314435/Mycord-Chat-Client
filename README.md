# Mycord Chat Client

A command-line chat client implemented in C for Linux/Unix environments, using TCP/IPv4 sockets and POSIX threads for concurrent message handling.

## Overview

Mycord is a networked chat client that connects to a compatible server and supports sending and receiving messages concurrently. It uses a structured message protocol to communicate with the server while handling user input, incoming messages, and connection events.

The client also supports command-line configuration, username validation, timestamped messages, terminal mention highlighting, and graceful shutdown handling.

## Features

- TCP/IPv4 socket communication
- Concurrent message sending and receiving using POSIX threads
- Structured message protocol with message types, timestamps, usernames, and message content
- Command-line options for server IP address, domain name, port, and quiet mode
- Domain name resolution for server connections
- Timestamped incoming chat messages
- Username mention highlighting with terminal colors and audible alerts
- Input validation for usernames and outgoing messages
- Handling of partial socket reads and writes
- Error handling for connection failures and interrupted system calls
- Signal handling for SIGINT and SIGTERM
- Logout message transmission and resource cleanup

## Technologies

- C
- Linux/Unix
- TCP/IP and IPv4
- POSIX Sockets
- POSIX Threads (`pthreads`)
- Unix Signals
- Standard I/O and File I/O
- GCC

## Implementation

### Network Communication

The client establishes a TCP connection to a Mycord server using POSIX sockets. It sends login, chat, and logout messages and processes messages received from the server.

### Concurrent Message Handling

A dedicated receiving thread handles incoming messages while the main thread reads user input and sends outgoing messages. This allows incoming chat messages to be displayed while the user continues interacting with the client.

### Message Protocol

The client uses a structured message format containing a message type, timestamp, username, and message body. Network byte order conversions are used for protocol fields such as message types and timestamps.

### Input Validation and Notifications

The client validates usernames and outgoing messages, rejects empty or oversized messages, and supports highlighting mentions of the current username with terminal colors and audible notifications.

## Usage

The client supports the following command-line options:

- `--help` or `-h` - Display usage information
- `--port PORT` - Specify the server port (default: `8080`)
- `--ip IP` - Specify the server's IPv4 address (default: `127.0.0.1`)
- `--domain DOMAIN` - Connect using a domain name
- `--quiet` - Disable mention highlighting and alerts

Example:

```bash
./client --ip 127.0.0.1 --port 8080
```

A compatible Mycord server must be running and reachable at the specified address and port.

## Build Environment

The project uses Linux/Unix networking headers and POSIX APIs and is intended to be compiled in a compatible environment.

Example compilation command:

```bash
gcc -Wall -Wextra -pthread client.c -o client
```

## Academic Context

This project was completed as part of Penn State's CMPSC 311 coursework.
