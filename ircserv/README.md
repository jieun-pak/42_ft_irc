# General implementation plan

- [x] Phase 1 — Skeleton + server loop (socket → poll() → accept)
- [~] Phase 2 — Registration (PASS / NICK / USER → 001 RPL_WELCOME)
- [ ] Phase 3 — Channels + messaging (JOIN / PRIVMSG / PART / QUIT)
- [ ] Phase 4 — Operator commands (KICK / INVITE / TOPIC / MODE i,t,k,o,l)

> `[x]` done 
`[~]` in progress / currently broken `[ ]` not started

## TODOs

### Phase 1 — Skeleton + server loop
- [x] `socket()` / `bind()` / `listen()` / `accept()`
- [x] single `poll()` event loop (one thread, no fork)
- [x] per-client read buffer + line extraction (handles `\r\n` fragmentation from `nc -C`)
- [x] SIGINT handler sets a shutdown flag
- [ ] actually check the shutdown flag in `eventLoop()` and break + clean up
- [ ] confirm `fcntl(fd, F_SETFL, O_NONBLOCK)` is set on sockets (subject requires non-blocking I/O)
- [ ] `setsockopt(SO_REUSEADDR)` on the listening socket (avoids `bind()` failing for ~60s after restart)

### Phase 2 — Registration (PASS / NICK / USER)
- [x] `handlePass` / `handleNick` / `handleUser` drafted with real logic
- [x] make it compile
- [ ] reply with real IRC numerics instead of `std::cerr`/ad-hoc strings:
      `001 RPL_WELCOME`, `433 ERR_NICKNAMEINUSE`, `464 ERR_PASSWDMISMATCH`,
      `451 ERR_NOTREGISTERED`, `461 ERR_NEEDMOREPARAMS`
- [ ] gate all non-registration commands behind "is this client registered yet?"
- [ ] `Server::parse()` must support the trailing `:` param (rest-of-line-as-one-arg) —
      right now every param is space-split, which breaks multi-word `PRIVMSG` text

### Phase 3 — Channels + messaging (not started)
- [ ] `Channel` class: name, members, ops, topic, key, user limit, mode flags
- [ ] `JOIN`: create channel if missing, first joiner becomes op, send `RPL_NAMREPLY`/`RPL_ENDOFNAMES`
- [ ] `PRIVMSG`: to a channel (broadcast to members) and to a nick (direct)
- [ ] `PART`: remove from channel, broadcast, destroy channel if now empty
- [ ] `QUIT`: remove from all joined channels, broadcast, close fd, clean up

### Phase 4 — Operator commands (not started)
- [ ] `KICK #channel nick [:reason]`
- [ ] `INVITE nick #channel`
- [ ] `TOPIC #channel [:newtopic]` (view vs set, gated by `+t`)
- [ ] `MODE`: `+/-i` invite-only, `+/-t` topic lock, `+/-k <key>` password,
      `+/-o <nick>` operator, `+/-l <limit>` user cap

---

# How to Test

### Build
```bash
make
```

### Terminal 1 — run the server
```bash
./ircserv 6667 mypassword
```

### Terminal 2 — connect as a client (nc)
```bash
nc localhost 6667
```
Then type IRC commands manually:
```
PASS mypassword
NICK jin
USER jin 0 * :Jin Park
```

### Terminal 3 — second client (to test multi-client)
```bash
nc localhost 6667
PASS mypassword
NICK bob
USER bob 0 * :Bob
```

### Notes
- `nc` (netcat) sends raw text — each line is one IRC message
- Use `Ctrl+C` to disconnect a client
- Use `Ctrl+C` in Terminal 1 to stop the server


## Bugs
- when a client is usint NICK command, other clients can not communicat with the server.
- seg fault in this situation:
```
% ./ircserv 6667 1234
New client connected: 127.0.0.1
With Port: 48666
Received data from client 4: JOIN #general

Processing line from client 4: JOIN #general
Error: Client must be authenticated before joining a channel.
Received data from client 4: NICK habib

Processing line from client 4: NICK habib
Received data from client 4: PASS 1234

Processing line from client 4: PASS 1234
Received data from client 4: JOIN #general

Processing line from client 4: JOIN #general
zsh: segmentation fault (core dumped)  ./ircserv 6667 1234
```