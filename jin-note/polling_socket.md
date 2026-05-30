# Polling & Socket — Beginner Note

---

## 1. What is a Socket?

A socket is a **two-way communication endpoint** between two programs over a network.

Think of it like a phone line:
- Server opens a line and waits (`socket` → `bind` → `listen`)
- Client calls in (`connect`)
- Server picks up (`accept`)
- Both sides can talk (`send` / `recv`)

A socket is represented as an **fd (file descriptor)** — just an integer the OS gives you.

```
_sockfd = socket(...);   // OS returns e.g. 3
```

`-1` means no socket open yet. Any positive number is a valid socket.

---

## 2. What is an fd?

fd = **file descriptor**. In Unix, everything is a file — including sockets.

| fd | what it is |
|----|------------|
| 0  | stdin      |
| 1  | stdout     |
| 2  | stderr     |
| 3+ | your sockets, files, etc. |

When you open a socket, the OS gives you the next available number (e.g. 3, 4, 5...).  
When you `close(fd)`, that number is freed.

---

## 3. The Problem: One Client at a Time

Without polling, `accept()` **blocks** — it freezes the program until one client connects.  
Then you handle that one client. Then wait for the next. You can never serve two clients at once.

```cpp
while (true)
{
    accept();      // blocks here — can't do anything else
    handle();      // only one client at a time
}
```

This doesn't work for IRC — you need to handle many clients simultaneously.

---

## 4. What is poll()?

`poll()` watches **multiple fds at once** and wakes up when **any** of them has activity.

Instead of blocking on one fd, you give poll a list and it tells you which ones are ready.

```cpp
poll(&fds[0], fds.size(), -1);
//                         ^^ -1 = wait forever until something happens
```

After poll returns, you check each fd to see what happened.

---

## 5. struct pollfd
see `man poll` (`<sys/poll.h>`)

Each entry in the list is a `struct pollfd`:

```cpp
struct pollfd {
    int   fd;       // which fd to watch
    short events;   // what you want to watch for  (you set this)
    short revents;  // what actually happened       (OS fills this in)
};
```

`POLLIN` means "data is ready to read" (new connection or client sent a message).

`events` and `revents` are **independent** — they don't have to match:

| situation | events | revents |
|---|---|---|
| client sent data | POLLIN | POLLIN |
| nothing happened | POLLIN | 0 |
| client disconnected | POLLIN | POLLHUP |
| socket error | POLLIN | POLLERR |

- `events` = your standing request — "tell me when data arrives." You set it once, it never changes.
- `revents` = what the OS actually observed this iteration. Changes every loop.

That's why you zero `revents = 0` before each `poll()` call — leftover bits from the previous iteration would give false positives.

---

## 6. How fds[] is Organized in run()

```
fds[0]  →  _sockfd         (server's listening socket — new connections)
fds[1]  →  first client fd
fds[2]  →  second client fd
fds[3]  →  third client fd
...
```

`fds[0]` is always the server. Clients start at `fds[1]` and grow as more connect.

---

## 7. What Happens in run() Step by Step

```cpp
void Server::run()
{
    initSocket();    // create _sockfd
    bindSocket();    // attach _sockfd to the port
    listenSocket();  // start accepting connections on _sockfd

    // 1. Add the server socket to the watch list
    std::vector<struct pollfd> fds;
    struct pollfd serverPfd;
    serverPfd.fd      = _sockfd;
    serverPfd.events  = POLLIN;   // watch for new connections
    serverPfd.revents = 0;
    fds.push_back(serverPfd);     // fds[0] = server

    while (true)
    {
        // 2. Wait until any fd has activity
        int ready = poll(&fds[0], fds.size(), -1);
        if (ready < 0)
            break;  // poll failed

        // 3. fds[0] active = new client wants to connect
        if (fds[0].revents & POLLIN)
        {
            acceptConnection();           // creates Client, pushes to _clients

            struct pollfd clientPfd;
            clientPfd.fd      = _clients.back().getFd();  // the new client's fd
            clientPfd.events  = POLLIN;
            clientPfd.revents = 0;
            fds.push_back(clientPfd);     // fds[1], fds[2], ... as clients join
        }

        // 4. fds[1+] active = existing client sent data
        for (size_t i = 1; i < fds.size(); i++)
        {
            if (fds[i].revents & POLLIN)
            {
                // TODO: recv data and parse IRC commands from fds[i].fd
            }
        }
    }
}
```

---

## 8. client_fd vs _sockfd

| | `_sockfd` | `clientFd` |
|---|---|---|
| What | server's listening socket | one connected client's socket |
| Created by | `socket()` in `initSocket()` | `accept()` in `acceptConnection()` |
| Lives in | `Server._sockfd` | `Client._fd` via `_clients` vector |
| Purpose | wait for new connections | send/recv with that specific client |

One `_sockfd` exists for the whole server lifetime.  
A new `clientFd` is created for **every client** that connects.

---

## 9. Summary Flow

```
socket()       →  create _sockfd
bind()         →  attach to port
listen()       →  mark as server
poll()         →  wait for activity
  fds[0] ready →  accept() new client, add to fds[]
  fds[i] ready →  recv() IRC message from client i
```
