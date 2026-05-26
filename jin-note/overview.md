# Project
## definition
to create our own IRC server.
can use an actual IRC client to connect to our server and test it.
create a C++ program that...
- listens for incoming connections on a specific port, 
- understands IRC text commands
- routes messages between users

## in practice
the program should run:
```cpp
./ircserv <port> <password>
```
port: the port your server listens to (usually 6667)
pw: the global pw users must provide to connect

## requirements
[] must use non-blocking I/O multiplexing
   - [] do not use multi-threading or fork() to handle multiple users
   - [] should use one of these system calls (`poll/epoll/kqueue`)
      - `poll()`: instead of checking every user connection constantly, it alerts you: "A sent you msg", "B disconnected", etc.

[] support `core actions` using standard IRC text commands
   - [] Authenticaton
   - [] identity(Nickname, username)
   - [] channels
   - [] messaging

[] support channel operator commands for admin
   - [] `KICK`:	Boot a user out of the channel.
   - [] `INVITE`:	Allow a specific user into a channel.
   - [] `TOPIC`:	View or change the channel's description.
   - [] `MODE`:	Change the rules of the channel (see below).

[] support channel mods
   - []`i`: Make the channel invite-only.
   - []`t`: Only Operators can change the TOPIC.
   - []`k`: Set a channel password.
   - []`o`: Promote a regular user to Operator.
   - []`l`: Set a maximum limit of users in the channel.

[] handle data fragmentation issue of `read()`
   - If a user sends the command `"JOIN #general\n"`, it might arrive at your server in chunks: `"JO"`, `"IN #ge"`,`"neral\n"`
   - handle this (the nc -C test)
   - store incoming text in a buffer for each user and only process it once you detect the end-of-line character (\n or \r\n).



# IRC (Internet Relay Chat)
## definition
protocoal for real-time text messaging btw internet-connected computers in 1988.

grandfather of Slack or Discord

## characteristics
- text-based and real-time chat system
- has two parts
   - client: the app a user opens on their computer with UI. We are NOT building it.
   - server: central hub that all clients connect to. it receives a message from one client and distributes it to the correct recipients. We should build it.

## port
- `binding to port 0`
   - OS will bind it to any available port within the range of `Dynamic or private ports 49152 to 655`
   - useful when the port number is not important.


## I/O operations
- `fcntl()` can set `O_NONBLOCK` flag on the server socket FD.
   - non-blocking mode: operations such as `read()` and `write()` on the socket will return immediately.

## socket
### active socket (IRC client)
- an active socket in IRF = client-side connection to IRC server
- IRC client can connect to IRC server by TCP/IP connection
- once connected, IRC client socket handles `user input`, `sending msgs to the server`, and `processes server responses`

### passive socket (IRC server)
- server-side listening socket that accepts multiple connections from IRC clients on a specific port

## TCP connection opening
### sequence
- `client`
   - 1. create a socket
   - 2. connect to the server
> c -> s: SYN
- `server`
   - 1. create a socket
   - 2. bind IP and port
   - 3. listen n-connections
   - 4. accept connections
> s -> c: SYN-ACK

> c -> s: ACK

> connection establisted !





## functions
### socket related
#### socket()
- system call used to create a new socket of a specific type(e.g. stream, datagram)
- returns a file descriptor which will be used to refer to that socket in subsequent system calls

#### setsockopt()
- to set options on a socket
- configure socket-level options to control the behavior

```cpp
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
```
#### bind()
- takes 3 params(socket fd `sockfd`, pointer to a struct sockaddr `addr`, size of the address structure `addrlen`)

#### listen() system call
- makes socket passive

### others
#### fcntl()
- performs specific operations on specific FD
```cpp
int fcntl(int fd, int cmd, ... /* arg */ );
```

# Implementation
## initial plan (updated: 23/05/2026)

### file structure
```
ircserv/
├── Makefile
├── includes/
│   ├── Server.hpp
│   ├── Client.hpp
│   └── Channel.hpp
└── srcs/
    ├── main.cpp
    ├── Server.cpp          ← socket setup, poll loop, client accept
    ├── Client.cpp          ← per-client state, read buffer
    ├── Channel.cpp         ← members, modes, operator list
    └── commands/
        ├── Pass.cpp        ← PASS (registration)
        ├── Nick.cpp        ← NICK
        ├── User.cpp        ← USER
        ├── Join.cpp        ← JOIN
        ├── Part.cpp        ← PART
        ├── Privmsg.cpp     ← PRIVMSG / NOTICE
        ├── Quit.cpp        ← QUIT
        ├── Kick.cpp        ← KICK (operator)
        ├── Invite.cpp      ← INVITE (operator)
        ├── Topic.cpp       ← TOPIC (operator)
        └── Mode.cpp        ← MODE i/t/k/o/l (operator)
```

### key design decisions (from reference study)
- use `poll()` with `std::vector<pollfd>` — simplest and most portable (3/4 refs use this)
- one poll() call covers all fds: listening socket + all client sockets (required by subject)
- separate command files (marineks style) → easiest to extend and debug
- command dispatch via `std::map<std::string, void(*)(...)>` or method dispatch on Server
- each Client owns a `std::string _readBuf` → append on recv(), process on `\r\n`

### class responsibilities
| Class | owns | key fields |
|-------|------|------------|
| `Server` | all Clients, all Channels | `int _listenFd`, `std::vector<pollfd> _pfds`, `std::map<int,Client*> _clients`, `std::map<std::string,Channel*> _channels`, `std::string _password` |
| `Client` | its own state | `int _fd`, `std::string _nick`, `_user`, `_readBuf`, `bool _registered` |
| `Channel` | member list, mode flags | `std::string _name`, `std::set<Client*> _members`, `std::set<Client*> _ops`, `bool _inviteOnly`, `bool _topicLocked`, `std::string _key`, `int _userLimit` |

### implementation order (step-by-step)
#### phase 1 — skeleton + server loop
1. `Makefile`: `NAME=ircserv`, `c++ -Wall -Wextra -Werror -std=c++98`
2. `main.cpp`: parse `<port>` and `<password>` args, handle SIGINT/SIGQUIT with static Server pointer
3. `Server`: create TCP socket → `setsockopt(SO_REUSEADDR)` → `bind()` → `listen()` → `fcntl(O_NONBLOCK)`
4. `Server::run()`: main `poll()` loop
   - if `_pfds[0]` (listen fd) is POLLIN → `accept()` new client, add to `_pfds` and `_clients`
   - for each client fd with POLLIN → `recv()` into `Client::_readBuf`, extract `\r\n`-delimited lines, dispatch command
   - on 0-byte recv or POLLERR → disconnect client

#### phase 2 — registration (PASS / NICK / USER)
5. Parse raw IRC line into `command` + `params` (split on spaces, handle `:` prefix for trailing)
6. `PASS`: verify password, set `_passOk = true`
7. `NICK`: check uniqueness, set `_nick`
8. `USER`: set `_user`, `_realname`
9. After all three succeed → `_registered = true` → send `001 RPL_WELCOME`

#### phase 3 — channels + messaging
10. `JOIN #channel [key]`: create or join Channel, enforce `+i` / `+k` / `+l` modes, send `JOIN` + `353 RPL_NAMREPLY` + `366`
11. `PRIVMSG #channel :text` → broadcast to all Channel members except sender
12. `PRIVMSG nick :text` → find Client by nick, send directly
13. `PART`, `QUIT`: remove from channels, close fd, clean up

#### phase 4 — operator commands
14. `KICK #channel nick [:reason]`
15. `INVITE nick #channel`
16. `TOPIC #channel [:newtopic]`
17. `MODE #channel +/-flag [param]`
    - `+i/-i`: invite-only toggle
    - `+t/-t`: topic-locked toggle
    - `+k/-k <key>`: channel password
    - `+o/-o <nick>`: grant/revoke operator
    - `+l/-l <limit>`: user limit

### critical nc test (partial data)
```bash
nc -C 127.0.0.1 6667
# type: "JOI"  → ctrl+D  (sends without newline)
# type: "N #ge" → ctrl+D
# type: "neral\r\n" → must work!
```
- Buffer per client, only dispatch when `\r\n` is found
- Never call `recv()` outside of poll() readiness check

# Questions
- Q1. how to connect to correct recipients?
- Q2. all IRC text commands to cover?
- Q3. how IRC is working in detail?