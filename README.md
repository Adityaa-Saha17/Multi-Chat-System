# CN LAB-7: Multi-Client Chat Application (TCP, C)

A multi-client chat system implemented in C using TCP sockets and POSIX threads as part of Computer Networks Lab-7.

---

## Group Details

| Name         | Roll No   |
|--------------|-----------|
| Adityaa Saha | 23CS01002 |
| Debtanu Das  | 23CS01016 |

---

## Objective

Design and implement a Client-Server Chat Application using TCP sockets in C that:

- Handles multiple clients simultaneously
- Supports client join and disconnect
- Enables communication between clients via server
- Allows broadcasting messages
- Includes additional functionalities

---

## Features

### Core Features
- Multi-client support using pthreads
- Clients can join and leave anytime
- Communication between clients via server
- Broadcast messaging

### Additional Features
- Unique username enforcement
- Private messaging (`/msg`)
- List active users (`/list`)
- Graceful exit (`/quit`)
- Timestamped messages
- Thread-safe client handling using mutex

---

## Project Structure
.
├── server.c
├── client.c
├── Makefile
├── README.md
└── screenshots/
---

## Compilation

make
To clean: make clean
To compile server: make server
To compile client: make client

---

## Execution

### Start Server

./server

### Start Client

./client
Or: ./client <ip> <port>

---

## Usage

|        Command        |      Description     |
|-----------------------|----------------------|
| /msg <user> <message> | Send private message |
| /list                 | Show active users    |
| /quit                 | Exit chat            |
| <text>                | Broadcast message    |

---

## Working

### Server
- Uses thread-per-client model
- Maintains list of connected clients
- Uses mutex for synchronization
- Handles broadcast and private messaging

### Client
- Two threads:
  - One for sending messages
  - One for receiving messages
- Handles input and server communication concurrently

## Additional Functionalities

1. **Unique Username Enforcement** — The server rejects duplicate usernames and re-prompts the client until a unique one is entered.
2. **Single-Word Username Validation** — Usernames with spaces or tabs are rejected with a warning.
3. **Private Messaging (`/msg`)** — Messages are delivered only to the target user, labelled `[PM from ...]`. The sender gets a `[PM to ...]` echo for confirmation. If the target is not online, the sender is notified.
4. **User Listing (`/list`)** — Returns a live list of all currently connected usernames.
5. **Timestamped Messages** — Every message and server log entry is prefixed with `[HH:MM:SS]`.
6. **Graceful Shutdown** — `Ctrl+C` on the server broadcasts a shutdown notice to all clients. `Ctrl+C` on the client sends `/quit` before closing.
7. **Dynamic Slot Management** — Supports up to 32 simultaneous clients; slots are recycled automatically on disconnect.
8. **Thread-per-Client with Mutex Protection** — Each client runs in its own `pthread`; a global mutex protects the shared client registry from race conditions.
9. **Server-Side Logging** — Join/leave events and broadcasts are logged to the server console with timestamps.
10. **Message Echo-back** — Broadcast messages are echoed back to the sender so they appear in their own chat view.
11. **`SO_REUSEADDR`** — The server can be restarted immediately after shutdown without waiting for the OS `TIME_WAIT` to expire.