# ft_irc — dev README (working doc)

Team working doc for the `ircserv` implementation. Detailed Phase 1 design/decisions: [plan-phase1.md](../jin-note/plan-phase1.md).

> NOTE: Subject Chapter V requires a `README.md` **at the repo root** (italic first line
> "*This project has been created as part of the 42 curriculum by \<login1\>, \<login2\>*",
> plus **Description / Instructions / Resources** sections, in English). That file doesn't exist yet — see Defense checklist below.

# General implementation plan

- [x] Phase 1 — Skeleton + server loop (socket → poll() → accept)
- [~] Phase 2 — Registration (PASS / NICK / USER → 001 RPL_WELCOME)
- [~] Phase 3 — Channels + messaging (JOIN / PRIVMSG / PART / QUIT) — JOIN implemented but has a crash bug; PRIVMSG/PART/QUIT still empty
- [~] Phase 4 — Operator commands (KICK / INVITE / TOPIC / MODE i,t,k,o,l) — MODE drafted but not wired into the dispatcher (dead code); no operator concept yet; KICK/INVITE/TOPIC not started

> Build is currently BROKEN: `std::stoi` (C++11) used in `ServerComandHandlers.cpp:256` — does not compile under `-std=c++98`. See Bugs section below.

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
- [x] when (PASS->NICK->USER) or (PASS->USER->NICK) is completed, display welcome message
- [ ] reply with real IRC numerics instead of `std::cerr`/ad-hoc strings:
      `001 RPL_WELCOME`, `433 ERR_NICKNAMEINUSE`, `464 ERR_PASSWDMISMATCH`,
      `451 ERR_NOTREGISTERED`, `461 ERR_NEEDMOREPARAMS`, `421 ERR_UNKNOWNCOMMAND`,
      `462 ERR_ALREADYREGISTRED` (found in testing: `PASS` after registration is currently re-processed)
- [ ] gate all non-registration commands behind "is this client registered yet?"
- [x] `Server::parse()` supports the trailing `:` param (rest-of-line-as-one-arg, 2026-08-08) —
      `USER jin 0 * :Jin Park` now yields realname `"Jin Park"`; also fixes multi-word
      `PRIVMSG #chan :hello world` — verified via `nc` (hand-traced params, no debug
      print of stored values yet; add one if you want to see it live during WHOIS/etc.)

### Phase 3 — Channels + messaging (in progress)
- [x] `Channel` class: name, members, topic, invite-only flag, password, user limit, banned-users list
- [~] `JOIN`: creates channel if missing, adds member, sends `RPL_NOTOPIC`/`RPL_TOPIC` (331/332)
      and `RPL_NAMREPLY`/`RPL_ENDOFNAMES` (353/366), broadcasts JOIN to channel —
      **has a crash bug and an auth-check bug, see Bugs section**
- [ ] first joiner does NOT become operator — `Channel` has no operator/op list at all yet
      (needed for KICK/INVITE/TOPIC/MODE+o gating in Phase 4)
- [ ] `PRIVMSG`: still an empty stub — to a channel (broadcast to members) and to a nick (direct)
- [ ] `PART`: still an empty stub — remove from channel, broadcast, destroy channel if now empty
- [ ] `QUIT`: still an empty stub — remove from all joined channels, broadcast, close fd, clean up
- [ ] `Channel::broadcastMessage()` and `sendTopic()`/`sendNamesList()` call `send()` directly,
      bypassing `queueSend()`/`POLLOUT` — violates the same subject rule D2 fixed in Phase 1;
      needs converting (Channel has no access to `Server::queueSend`, needs a design decision)
- [ ] `_channels` map entries (`new Channel(...)`) are never freed — the cleanup loop in
      `Server::~Server()` is still commented out; now an active leak, not just a placeholder

### Phase 4 — Operator commands (drafted, not functional)
- [ ] `MODE` handler is written (`handleMode`, `+/-i`, `+/-k`, `+/-l`, `+/-b`) but **`CMD_MODE`
      is missing from the `CommandType` enum, `getCommandType()`, and the `executeCommand()`
      switch** — a client sending `MODE` currently falls through to `CMD_UNKNOWN` and does
      nothing. Must wire it in before any of the below can even be reached.
- [ ] `MODE +/-t` (topic lock to ops) and `MODE +/-o` (grant/revoke operator) are not implemented
      at all (`+/-o` is impossible anyway until `Channel` has an operator list — see Phase 3)
- [ ] `KICK #channel nick [:reason]` — not started
- [ ] `INVITE nick #channel` — not started
- [ ] `TOPIC #channel [:newtopic]` as its own command — not started (JOIN currently only
      *shows* the topic on join; there's no way to *view or change* it afterward)

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

## Bugs found in code review, 2026-08-08 (JOIN / MODE)
- [x] **BUILD BROKEN:** `std::stoi` (C++11) in `handleMode`'s `+l` parsing — fixed,
      replaced with `std::atoi`.
- [x] **Segfault root cause, fixed:** `handleJoin`'s `Channel *channel = NULL;` is now
      assigned in both branches (`else { channel = it->second; }` added for the
      "channel already exists" case) — this is the normal case for a second person
      joining an existing channel, so it wasn't just your repro's edge case.
- [x] **Auth-check bug, fixed:** replaced the `&&`-based partial check with
      `!client->isRegistered()` (the guarded, authoritative flag added for the
      registration-order fix below) — now correctly requires PASS + NICK + USER,
      not just two of three. Also **moved the check before channel lookup/creation**,
      so a rejected client can no longer leave a phantom empty channel in `_channels`
      (a bug I found while fixing this — every failed early JOIN attempt was silently
      creating a permanent empty `Channel` object that nothing ever cleaned up).
- [ ] "other clients can't communicate while one uses NICK" (below) — not yet re-verified
      against the current code; D1 (one `recv()` per `POLLIN`, no drain loop) should already
      rule out this class of freeze. Retest now that the build is fixed; if it still reproduces,
      it's a new bug, not the old blocking one.

## Bug found in manual testing, 2026-08-08 (registration order-dependency)
- [x] **Welcome message never fired if `NICK` arrived after `USER` — fixed.** The
      "registration complete?" check moved out of `handleUser()` alone into a shared
      `Server::checkRegistrationComplete()`, now called after `PASS`, `NICK`, and `USER`
      each, guarded by a new `Client::isRegistered()` flag so it fires exactly once
      regardless of order. `PASS → NICK → USER` still works, and `PASS → USER → NICK`
      (the order that used to silently fail — nickname was still empty when the old
      check ran only inside `handleUser()`) now completes correctly too. The guard also
      fixes the earlier double-welcome-on-repeat-`USER`/`PASS` bug, and `Client::isRegistered()`
      is now the shared source of truth used by `handleJoin`'s auth check above.

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

### Window layout (used from here on)
- **Scenario A (default):** Window 1 = server, Window 2 = client 1, Window 3 = client 2.
  Enough for authentication, single-target commands (KICK/INVITE one user), and any test
  that only needs "did the *other* client see this" — see "Answering the question" above
  for why 2 clients is the practical minimum once channels are involved.
- **Scenario B (only if needed):** adds Window 4 = client 3. Reach for this specifically to
  verify a broadcast reaches **every** member, not just the one you happened to test with
  (real risk once `PRIVMSG`/`JOIN`/`KICK` iterate a member list) — not needed for routine testing.

For this section (starting the server itself) no clients are involved yet, so only Window 1 matters.

### Happy path
```bash
./ircserv 6667 mypassword
```
- **Window 1:** process starts and blocks in `poll()` — **currently prints nothing at all**
  on success (no "listening on port X" banner). That's not a subject violation, but it means
  "did it actually start?" is only verifiable by a client managing to connect — worth keeping
  in mind while testing so silence isn't mistaken for a hang.
- **Window 2/3:** not opened yet — nothing to check at this step.

**Immediate shutdown + restart (still happy path):** Ctrl+C in Window 1 right after starting.
- **Window 1:** prints `Server shutting down` (D6) and exits — NOT `poll error`.
- Rerun `./ircserv 6667 mypassword` immediately: must bind successfully right away — no
  `Error binding socket` wait (D7 / `SO_REUSEADDR`). If you see that message here, it's a regression.

### Unhappy path (bad args — must refuse and NOT crash, per subject Ch. II)
```bash
./ircserv                 # Window 1: "Usage: ./ircserv <port> <password>", exit 1, no listen
./ircserv 6667             # Window 1: same usage message (argc != 3), exit 1
./ircserv abc mypassword  # Window 1: "Invalid port number", exit 1 — atoi("abc") -> 0, caught by port <= 0 check
./ircserv 99999 pw        # Window 1: "Invalid port number", exit 1 (out of 1-65535 range)
./ircserv 0 pw            # Window 1: "Invalid port number", exit 1 (0 rejected by port <= 0, no special "any free port" handling — expected, subject doesn't ask for that)
./ircserv 6667 ""         # Window 1: "Password cannot be empty", exit 1
```
In every case: process must exit cleanly with no listening socket left behind (verify with
`ps aux | grep ircserv | grep -v grep` — should show nothing after each of these).

### Edge cases
- **Port already in use:** start `./ircserv 6667 pw` in Window 1 and leave it running.
  In a *second* server window (not a client — a second server instance), run
  `./ircserv 6667 pw` again.
  - **Window 1:** unaffected, keeps running.
  - **New window:** `bind()` fails with `EADDRINUSE` → prints `Error binding socket`, exit 1.
    This is the one case where seeing that message is *correct*, not a regression.
- **Privileged port (< 1024), e.g. `./ircserv 80 pw`:** on most setups (including unprivileged
  42 accounts) `bind()` fails with `EACCES` → same `Error binding socket` message. Not a bug —
  just don't mistake it for the `SO_REUSEADDR` regression above; pick a port ≥ 1024 for testing.
- **Extra/missing whitespace or non-ASCII in password:** e.g. `./ircserv 6667 "pw with spaces"`
  (quoted, so it's one argument) — should start normally; the password is only ever compared
  as a whole string by `PASS`, no parsing of its contents happens at startup.

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