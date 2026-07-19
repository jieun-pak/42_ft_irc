# ft_irc — dev README (working doc)

Team working doc for the `ircserv` implementation. Detailed Phase 1 design/decisions: [plan-phase1.md](../jin-note/plan-phase1.md).

> NOTE: Subject Chapter V requires a `README.md` **at the repo root** (italic first line
> "*This project has been created as part of the 42 curriculum by \<login1\>, \<login2\>*",
> plus **Description / Instructions / Resources** sections, in English). That file doesn't exist yet — see Defense checklist below.

# General implementation plan

- [x] Phase 1 — Skeleton + server loop (socket → poll() → accept)
- [~] Phase 2 — Registration (PASS / NICK / USER → 001 RPL_WELCOME)
- [ ] Phase 3 — Channels + messaging (JOIN / PRIVMSG / PART / QUIT)
- [ ] Phase 4 — Operator commands (KICK / INVITE / TOPIC / MODE i,t,k,o,l)

> `[x]` done · `[~]` in progress · `[ ]` not started

## TODOs

### Phase 1 — Skeleton + server loop (details & rationale: [plan-phase1.md](../jin-note/plan-phase1.md))
- [x] `socket()` / `bind()` / `listen()` / `accept()`
- [x] single `poll()` event loop (one thread, no fork)
- [x] per-client read buffer + line extraction (handles `\r\n` fragmentation from `nc -C`)
- [x] SIGINT handler sets a shutdown flag
- [x] one `recv()` per `POLLIN` event (D1) — no drain loop, poll re-notifies for leftovers
- [x] `fcntl(fd, F_SETFL, O_NONBLOCK)` on listen fd + every client fd (D3)
- [x] route `send()` through the poll loop (D2): per-client `_writeBuf` + `POLLOUT`; handlers use `queueSend()`, the only `send()` lives in `sendData()`
- [x] fix: `accept()` failure no longer `exit(1)` — logs, returns -1, server keeps running (D4 slice)
- [x] `setsockopt(SO_REUSEADDR)` on the listening socket (D7 — fixes `bind()` failing ~60s after restart)
- [x] handle `POLLHUP` / `POLLERR` / `POLLNVAL` in `eventLoop()` (D4 — hangup-only events disconnect the client)
- [x] safe fd removal (D5): disconnects go through `disconnectClient()` → `_fdsToRemove`, `_pfds` erased only after the scan loop
- [x] clean shutdown (D6): `g_serverShutdown` is `volatile sig_atomic_t`, checked in `eventLoop()`; `EINTR` handled; SIGINT+SIGQUIT; destructor closes all fds
- [x] `Server` non-copyable (D8): copy ctor/`operator=` private and unimplemented — copying would double-delete `Client*`
- [x] reference client decided (D9): **irssi** (proposed — teammate to confirm)

### Phase 2 — Registration (PASS / NICK / USER)
- [x] `handlePass` / `handleNick` / `handleUser` drafted with real logic
- [x] make it compile
- [ ] reply with real IRC numerics instead of `std::cerr`/ad-hoc strings:
      `001 RPL_WELCOME`, `433 ERR_NICKNAMEINUSE`, `464 ERR_PASSWDMISMATCH`,
      `451 ERR_NOTREGISTERED`, `461 ERR_NEEDMOREPARAMS`, `421 ERR_UNKNOWNCOMMAND`,
      `462 ERR_ALREADYREGISTRED` (found in testing: `PASS` after registration is currently re-processed)
- [ ] gate all non-registration commands behind "is this client registered yet?"
- [ ] `Server::parse()` must support the trailing `:` param (rest-of-line-as-one-arg) —
      right now `USER jin 0 * :Jin Park` stores realname as `":Jin"` and drops `"Park"`

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

## Bugs / things to handle (found in manual testing, 2026-07-19)
- [ ] `handleNick`: broadcast shows old nick == new nick (`:JIN NICK JIN`) —
      it reads `getNickname()` *after* `setNickname()`, so the "old" name is
      already the new one. Fix: capture `oldNickname` *before* setting.
- [ ] `handleNick`: broadcasts to **every** connected client, and even on a
      client's *first* NICK. Correct IRC: first NICK during registration is not
      broadcast at all; a real nick *change* goes only to the client itself +
      clients sharing a channel (audience fix needs channels — Phase 3).
      Phase 2 fix: only broadcast when `oldNickname` was non-empty.
- [ ] `PASS` sent *after* successful registration is re-processed (wrong pw just
      logs an error) — must answer `462 ERR_ALREADYREGISTRED` and leave auth
      state untouched (also listed in Phase 2 numerics).
- [ ] commands before registration (e.g. `hello` before `PASS`) are silently
      dropped — needs registration gating + `421`/`451` replies (Phase 2).

---

# How to Test

## What works today (state of `main` + local fixes)

| Behavior | Status |
|---|---|
| `make` / `make re` clean build (C++98, `-Wall -Wextra -Werror`) | [x] |
| Accepts one or many `nc` clients through one `poll()` | [x] |
| Buffers partial input, splits on `\r\n` / `\n` | [x] |
| `PASS` → `NICK` → `USER` → welcome text | [x] plain text, **not** numeric `001` yet |
| Errors (wrong pass, dup nick) | [~] printed on **server** stderr only — client gets nothing |
| Unknown command | [~] silently ignored (no `421` yet) |
| Real IRC client (irssi) full registration | [ ] irssi waits for numeric `001` — not sent yet |
| `JOIN` / `PRIVMSG` / `PART` / `QUIT` | [ ] empty stubs |

## 1. Build checks
```bash
make            # must compile with -Wall -Wextra -Werror -std=c++98, no errors
make            # run again: must do nothing (subject: no unnecessary relinking)
make re         # full rebuild from scratch
```

## 2. Start the server
```bash
./ircserv 6667 mypassword
```
Also test bad args — the server must refuse and not crash:
```bash
./ircserv                 # usage message
./ircserv 99999 pw        # invalid port
./ircserv 6667 ""         # empty password
```
> `SO_REUSEADDR` is set (D7), so restarting immediately after Ctrl+C must succeed —
> if you ever see "Error binding socket", that's a regression.

## 3. Basic connection + multi-client
```bash
# Terminal 2
nc localhost 6667
# Terminal 3 (same time — server must serve both without blocking)
nc localhost 6667
```
Type anything; server terminal must log `Received data ...` / `Processing line ...`
per connected client, each with its own fd.

## 4. Registration — happy path
Manually (Terminal 2):
```
PASS mypassword
NICK jin
USER jin 0 * :Jin Park
```
Expected reply on the client: `Welcome to the IRC server, jin!`

Scripted one-liner (real TCP, exercises the line-splitting too since all 3 commands
arrive as one packet):
```bash
printf "PASS mypassword\r\nNICK jin\r\nUSER jin 0 * :Jin Park\r\n" | nc -q1 localhost 6667
```
(`printf` not `echo`: reliably emits the `\r\n` CRLF endings IRC requires.
`-q1`: nc exits 1s after stdin ends instead of hanging.)

## 5. Registration — error paths
```bash
# wrong password → currently: error on server stderr, client sees nothing (numeric 464 is TODO)
printf "PASS wrongpw\r\nNICK jin\r\nUSER jin 0 * :Jin\r\n" | nc -q1 localhost 6667

# duplicate nick → open two clients, send same NICK; second must be rejected (433 is TODO)
# missing params → "PASS", "NICK", "USER x" alone must not crash the server
```

## 6. Fragmentation test (subject IV.3 — the `ctrl+D` test)
```bash
nc -C 127.0.0.1 6667
```
Type `com`, press **Ctrl+D** (flushes without newline), type `man`, **Ctrl+D**,
type `d`, press **Enter**. The server must aggregate the pieces and process one
single `command` line only when the line ending arrives. Works because each
`Client` owns `_readBuf` and lines are only extracted on `\n`.

## 7. Disconnect handling
- **Ctrl+C a client**: server must log `Client disconnected`, remove its fd, keep serving others.
- Reconnect afterwards: must get a fresh fd, everything still works.
- Kill/restart clients repeatedly: server must never crash or leak fds.

## 8. Reference client (irssi)
```bash
irssi
/connect 127.0.0.1 6667 mypassword jin
```
**Current limitation:** irssi will connect but hang at "waiting for server" —
it requires the numeric `001` welcome (Phase 2 TODO). Re-test after numerics land;
a working reference client is a hard subject requirement for evaluation.

## 9. Leaks / cleanup
```bash
valgrind --leak-check=full ./ircserv 6667 pw   # connect/disconnect a few clients, Ctrl+C
ps aux | grep ircserv | grep -v grep           # after tests: no leftover server process
```

---

# Defense checklist (from subject v10.0)
- [ ] `README.md` at **repo root**: italic first line with logins, Description, Instructions, Resources (incl. how AI was used) — English
- [ ] stop tracking the compiled binary `ircserv/ircserv` (it's in git; add to `.gitignore`, `git rm --cached`)
- [ ] reference client = **irssi** (proposed): must connect with zero errors — blocked on Phase 2 numerics (`001`)
- [ ] only allowed external functions used (see subject p.6); `fcntl` **only** as `fcntl(fd, F_SETFL, O_NONBLOCK)`
- [ ] exactly **one** `poll()` handles everything — read *and* write; no fork, no threads
- [ ] server never crashes / never quits unexpectedly (crash = grade 0)
- [ ] be ready for a small live code modification during evaluation
