# plan-phase1.md — Skeleton + Server Loop (detailed)

Scope: everything the server needs **before** IRC semantics matter — sockets, the
poll loop, non-blocking I/O, connect/disconnect lifecycle, shutdown. Phase 2+
(registration numerics, channels, operators) builds on top of this and is only
referenced where a Phase 1 decision affects it.

---

## 0. The subject's laws for this phase (v10.0)

These are graded as hard fails, so they shape every decision below:

1. **One `poll()` (or equivalent) for everything** — listen, read, **and write**.
   The subject warns explicitly: attempting `read/recv` or `write/send` on any fd
   without going through `poll()` → **grade 0**.
2. **All I/O non-blocking.** `fcntl` is allowed **only** as
   `fcntl(fd, F_SETFL, O_NONBLOCK);` — any other flag is forbidden.
3. **No fork, no threads.** Multiple clients, one process, no hanging.
4. **Never crash, never quit unexpectedly** — even out-of-memory → otherwise grade 0.
5. Allowed functions (p.6): `socket, close, setsockopt, getsockname, getprotobyname,
   gethostbyname, getaddrinfo, freeaddrinfo, bind, connect, listen, accept, htons,
   htonl, ntohs, ntohl, inet_addr, inet_ntoa, inet_ntop, send, recv, signal,
   sigaction, sig*, lseek, fstat, fcntl, poll` (or equivalent) + everything in C++98.

---

## 1. What's done so far

| Piece | Where | Notes |
|---|---|---|
| Arg parsing (`<port> <password>`) | `srcs/main.cpp` | port range checked, empty password rejected |
| `socket()` → `bind()` → `listen()` | `srcs/Server.cpp` (`initSocket`/`bindSocket`/`listenSocket`) | binds `INADDR_ANY:<port>` |
| `accept()` + per-client object | `Server::acceptConnection()` | `new Client(fd)` stored in `std::map<int, Client*> _clients` |
| Single poll loop | `Server::eventLoop()` | `_pfds[0]` = listen fd, `_pfds[1..n]` = clients, all on `POLLIN` |
| Read + line buffering | `Server::receiveData()` + `Client::appendToReadBuf()`/`extractLines()` | survives fragmented input; splits on `\n`, strips `\r` |
| Disconnect cleanup | `receiveData()` on `recv()==0` / error | `close`, delete Client, remove from `_pfds` |
| Command parse + dispatch | `Server::parse()`, `getCommandType()`, `executeCommand()` | PASS/NICK/USER work (plain-text replies); JOIN/PART/PRIVMSG/QUIT are stubs |
| SIGINT handler | `srcs/signal.cpp` | sets `server_shutdown = true` — **but nothing reads it yet** |

Verified working end-to-end: build is clean (`make re`, zero warnings) and a live
`nc` test (`PASS`/`NICK`/`USER`) returns the welcome message; multi-line input in
one packet is split correctly.

---

## 2. Concepts to know (junior level, tied to this codebase)

Longer beginner notes live in [../jin-note/](../jin-note/) (`overview.md`,
`polling_socket.md`). Short version of what phase 1 already uses:

- **Socket = fd.** `socket()` returns an int; everything after (bind/listen/accept/
  recv/send/close) takes that int. `_sockfd` is the *listening* socket (one, lives
  forever); `accept()` mints a *new* fd per connected client.
- **The kernel does the TCP handshake.** After `listen()`, completed connections
  queue up inside the kernel; `POLLIN` on the *listening* fd means "a connection is
  waiting — call `accept()`", not "data to read".
- **`poll()` is the whole trick.** One call sleeps on N fds at once; `events` is
  what you ask for (standing request), `revents` is what actually happened this
  round. That's how one thread serves many clients without ever blocking on one.
- **Non-blocking + `EAGAIN`.** With `O_NONBLOCK`, `recv`/`send`/`accept` return
  `-1` with `errno == EAGAIN/EWOULDBLOCK` instead of freezing when there's nothing
  to do. That's not an error — it means "try again after poll says ready".
- **TCP is a byte stream, not messages.** One `recv()` may deliver half a command
  or three commands glued together. Hence per-client `_readBuf` + only acting on
  complete `\r\n`-terminated lines (this is the subject's `nc -C` / Ctrl+D test).
- **`send()` can be partial too.** It may write fewer bytes than asked (full kernel
  buffer, slow client). A correct server keeps the unsent remainder in a per-client
  out-buffer and retries when `poll()` reports `POLLOUT`. We don't do this yet —
  see Decision D2.
- **Signals interrupt `poll()`.** Ctrl+C makes `poll()` return `-1` with
  `errno == EINTR`. That's the natural moment to check the shutdown flag. The flag
  itself should be `volatile sig_atomic_t` (the only type guaranteed safe to write
  from a signal handler).

---

## 3. Things to decide before implementing

### D1 — `recv()` strategy: loop-until-EAGAIN vs one recv per POLLIN
Current code loops `recv()` until `EAGAIN`. Alternative: **one `recv()` per
`POLLIN` event** — `poll()` is level-triggered, so if bytes remain, the next
iteration fires again immediately.
**Recommendation: one recv per event.** Simpler, no starvation of other clients by
one chatty client, and it's the cleanest story for the "everything goes through
poll()" rule during defense. → decide together, then simplify `receiveData()`.

### D2 — `send()` strategy: direct send vs out-buffer + POLLOUT  *(the big one)*
Today handlers call `send()` directly. Problems: (a) subject's grade-0 warning about
send without poll; (b) partial sends are silently dropped; (c) a slow client could
block-ish/lose data.
**Recommendation: per-client `_writeBuf`.** Handlers only append to it; the poll
loop enables `POLLOUT` for that fd, sends what it can when writable, disables
`POLLOUT` when drained. One helper (`Server::queueSend(fd, msg)`) keeps handler code
as simple as `send()` was. Decide now — Phase 2 numerics multiply the send sites,
retrofitting later is painful.

### D3 — Where to set `O_NONBLOCK`
Two places, both needed: the listening fd (in `initSocket()`, so `accept()` can't
block) and every client fd (right after `accept()`). Only the subject-approved
form: `fcntl(fd, F_SETFL, O_NONBLOCK);`.

### D4 — Error policy after startup: never `exit()`
`acceptConnection()` currently calls `exit(1)` if `accept()` fails — one transient
failure (e.g. client gone before accept, fd limit hit) kills the whole server =
grade 0. Startup errors (bad bind) may exit; **after `eventLoop()` starts, nothing
may exit** — log, clean up the one client involved, continue. Same for `recv`/`send`
errors. Also handle `POLLHUP`/`POLLERR`/`POLLNVAL` per fd (treat as disconnect).

### D5 — Removing fds while iterating `_pfds`
`receiveData()` erases from `_pfds` while `eventLoop()`'s `for (i)` walks the same
vector → indices shift, one entry gets skipped that round. Options: iterate
backwards; or collect dead fds during the sweep and erase after; or `i--` after
removal. **Recommendation: collect-then-erase after the loop** — most explicit,
no index arithmetic to defend.

### D6 — Clean shutdown path
`server_shutdown` flag: make it `volatile sig_atomic_t`, declare it `extern` in
`signal.hpp`, check it in the `while` condition of `eventLoop()`, and treat
`poll() == -1 && errno == EINTR` as "loop around and re-check flag", not fatal.
On exit: close every client fd + listen fd, free every `Client*` (destructor
already does most of this — verify order).

### D7 — `SO_REUSEADDR`
`setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, ...)` before `bind()`. Without it,
restarting the server within ~60s of a Ctrl+C fails on `bind()` (TIME_WAIT). It's
in the allowed-function list. No reason not to.

### D8 — `Server` copy semantics
The OCF copy ctor/`operator=` shallow-copy `std::map<int, Client*>` — two Servers
would `delete` the same `Client*` → double free (crash = grade 0). A Server is not
meaningfully copyable. **Recommendation: declare copy ctor + `operator=` private
and unimplemented** (the C++98 idiom for "non-copyable"), keep the OCF shape.

### D9 — Reference client
Subject requires one; the evaluator uses it. irssi is on campus machines and is the
common pick. Whatever we choose dictates which numerics matter first in Phase 2
(irssi minimally needs `001`, and behaves better with `PING`/`PONG`). Decide and
write it in the root README.

---

## 4. TODOs (ordered, with "done when")

1. **`O_NONBLOCK` on all fds** (D3)
   — done when: `recv` on an idle client returns `-1/EAGAIN` instead of blocking (add temporary log to verify once).
2. **`SO_REUSEADDR`** (D7)
   — done when: Ctrl+C server → immediately restart on same port → bind succeeds.
3. **Never-exit error policy + POLLHUP/POLLERR** (D4)
   — done when: killing a client mid-connect / spamming connects never terminates the server; a client closing its terminal shows clean disconnect handling.
4. **Safe `_pfds` removal** (D5)
   — done when: 3 clients connected, middle one disconnects, remaining two still receive events that same loop iteration.
5. **Clean SIGINT shutdown** (D6)
   — done when: Ctrl+C on server → loop exits, all fds closed, valgrind reports no leaks, exit code 0.
6. **Out-buffer + POLLOUT send path** (D2)
   — done when: handlers use `queueSend()`; a message queued to a client is delivered; (stretch) partial-send path exercised with a large payload.
7. **Single-recv-per-event simplification** (D1)
   — done when: `receiveData()` has no `while(true)`; two clients typing simultaneously both get served.
8. **Non-copyable Server** (D8)
   — done when: copy attempt fails to compile; build stays green.

After these, Phase 1 is defensible on every subject bullet, and Phase 2
(numerics, registration gating, `:` trailing-param parsing) starts on solid ground.

---

## 5. Known Phase 2 items this phase deliberately leaves open

- Numeric replies (`001/433/451/461/464/421`) — blocked on D2's `queueSend()`.
- `parse()` trailing `:` parameter (`USER jin 0 * :Jin Park` currently stores
  realname `":Jin"`, drops `"Park"`).
- Registration gating (`ERR_NOTREGISTERED` for pre-registration commands).
- `handleNick` step 5 bug to revisit: it reads `getNickname()` *after* setting the
  new nick, so `oldNickname == newNickname` in the change-notification — restructure
  when numerics land.
