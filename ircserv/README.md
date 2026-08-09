# ft_irc — dev README (working doc)

Team working doc for the `ircserv` implementation. Detailed Phase 1 design/decisions: [plan-phase1.md](../jin-note/plan-phase1.md).

> NOTE: Subject Chapter V requires a `README.md` **at the repo root** (italic first line
> "*This project has been created as part of the 42 curriculum by \<login1\>, \<login2\>*",
> plus **Description / Instructions / Resources** sections, in English). That file doesn't exist yet — see Defense checklist below.

# General implementation plan

- [x] Phase 1 — Skeleton + server loop (socket → poll() → accept)
- [x] Phase 2 — Registration (PASS / NICK / USER → 001 RPL_WELCOME)
- [~] Phase 3 — Channels + messaging (JOIN / PRIVMSG / PART / QUIT) — JOIN implemented and working; PRIVMSG/PART/QUIT still empty
- [~] Phase 4 — Operator commands (KICK / INVITE / TOPIC / MODE i,t,k,o,l) — MODE and TOPIC implemented and wired in; KICK/INVITE not started

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
- [x] reply with real IRC numerics instead of `std::cerr`/ad-hoc strings — **2026-08-08**:
      `001 RPL_WELCOME`, `433 ERR_NICKNAMEINUSE`, `464 ERR_PASSWDMISMATCH`,
      `451 ERR_NOTREGISTERED`, `461 ERR_NEEDMOREPARAMS`, `421 ERR_UNKNOWNCOMMAND`,
      `462 ERR_ALREADYREGISTRED` (closes the "`PASS`/`USER` re-processed after
      registration" bug). New `includes/Replies.hpp` (numeric enum) +
      `Server::sendNumericReply()`/`replyTarget()` (`Server.cpp`) format every reply
      as `:ircserv <code> <target>[ <middleParam>] :<trailing>\r\n`. Also used to fix
      `sendTopic()`/`sendNamesList()` (Phase 3), which were emitting `331`/`332`/`353`/`366`
      **without** the leading `:ircserv ` — found by comparing against a teammate's
      reference implementation, not by manual testing.
- [x] gate all non-registration commands behind "is this client registered yet?" —
      **2026-08-08**, two-stage gate in `executeCommand()`: (1) **`PASS` is the
      gatekeeper** — until `PASS` succeeds, every other command (including `NICK`/`USER`)
      gets `451` immediately, so an unauthenticated connection can no longer probe
      state via e.g. `NICK` → `433`; (2) once `PASS` succeeds, only `PASS`/`NICK`/`USER`
      are allowed until registration completes, everything else gets `451`.
      `CMD_UNKNOWN` gets `421` before either gate is checked.
- [x] `Server::parse()` supports the trailing `:` param (rest-of-line-as-one-arg, 2026-08-08) —
      `USER jin 0 * :Jin Park` now yields realname `"Jin Park"`; also fixes multi-word
      `PRIVMSG #chan :hello world` — verified via `nc` (hand-traced params, no debug
      print of stored values yet; add one if you want to see it live during WHOIS/etc.)

### Phase 3 — Channels + messaging (in progress)
- [x] `Channel` class: name, members, topic, invite-only flag, password, user limit, banned-users list
- [x] `JOIN`: creates channel if missing, adds member, sends `RPL_NOTOPIC`/`RPL_TOPIC` (331/332)
      and `RPL_NAMREPLY`/`RPL_ENDOFNAMES` (353/366), broadcasts JOIN to channel. The original
      crash bug and auth-check bug are long fixed (see "Bugs found in code review, 2026-08-08"
      below); `addMember()` now returns `bool` so a rejected add can never be mistaken for
      success (2026-08-08).
- [x] first joiner becomes operator — **2026-08-08**, `handleJoin` calls `channel->addOperator(client)`
      when creating a new channel, and `Channel` has a real `_operators` list with
      `addOperator()`/`removeOperator()` (used by `MODE +o`/`-o` too). **Known bug in this,
      see "Bugs" below:** `isOperator()` doesn't actually check `_operators` — see next section.
- [ ] `PRIVMSG`: still an empty stub — to a channel (broadcast to members) and to a nick (direct)
- [ ] `PART`: still an empty stub — remove from channel, broadcast, destroy channel if now empty
- [ ] `QUIT`: still an empty stub — remove from all joined channels, broadcast, close fd, clean up
- [x] `Channel::broadcastMessage()` (called `send()` directly, bypassing `queueSend()`/`POLLOUT`,
      violating subject rule D2) — **fixed 2026-08-08, removed entirely.** `Channel` has no
      `Server*` to call `queueSend()` through, so rather than plumb one in, the broadcast logic
      moved to `Server::broadcastToChannel(channel, message, sender)` (`Server.cpp`), which
      already has `queueSend()` and just iterates `channel->getMembers()`. Both call sites
      (`handleJoin`'s JOIN broadcast, `handleMode`'s MODE broadcast) updated. `sendTopic()`/
      `sendNamesList()` were already fixed earlier the same day via `sendNumericReply()`.
- [x] `_channels` map entries (`new Channel(...)`) are never freed — **already fixed** (found
      already-implemented 2026-08-08, doc was stale): `Server::~Server()`'s cleanup loop is
      live, not commented out — deletes every `Channel*` and clears the map.
- [ ] **`Channel::MAX_MEMBERS` (hard cap of 10) doesn't seem to be checked anywhere
      reachable — found 2026-08-08.** `handleJoin` only checks `isUserLimitReached()`,
      which is the `_userLimit`/`MODE +l` value (defaults to unlimited, 0), not
      `MAX_MEMBERS`; `Channel::addMember`'s own guard uses that same `_userLimit`-based
      check too. So a channel can grow past 10 members freely unless `+l` is explicitly
      set — flagging in case that's not intentional, not fixed yet.

### Phase 4 — Operator commands (in progress)
- [x] `handleMode` (`+/-i`, `+/-t`, `+/-k`, `+/-l`, `+/-o`) is written **and wired in** —
      `CMD_MODE` is in the `CommandType` enum, `getCommandType()`, and the `executeCommand()`
      switch (doc was stale: this was previously flagged as unwired, no longer true). Every
      branch replies with a real numeric; see the earlier "which case do I need std::cerr vs
      sendNumeric" review — this handler was already fully converted before that review.
- [x] **Bug found 2026-08-08: `Channel::isOperator()` didn't check `_operators` at all —
      fixed same day.** It used to check `!_members.empty() && _members[0] == client` ("are
      you literally the first member"), while `addOperator()`/`removeOperator()` maintain a
      separate `_operators` vector that `isOperator()` never read. Effects before the fix:
      `MODE +o` granting operator to anyone other than the channel's first member did
      nothing; `MODE -o` on the first member did nothing either; once `PART` exists, whoever
      becomes the new first member would inherit operator status never granted to them. Now
      `isOperator()` searches `_operators` (mirrors `isMember()` searching `_members`), so
      `+o`/`-o` actually take effect for whoever they're applied to.
- [ ] `KICK #channel nick [:reason]` — not started
- [ ] `INVITE nick #channel` — not started
- [x] `TOPIC #channel [:newtopic]` — **2026-08-09**, `CMD_TOPIC` added to the enum,
      `getCommandType()`, and `executeCommand()`; `handleTopic()` (`ServerComandHandlers.cpp`)
      handles both query (no second param → reuses `sendTopic()`, same 331/332 as JOIN) and
      set (checks `Channel::isTopicRestricted()` — new getter, the flag was previously
      write-only via `setRestrictedTopic()` with no way to read it back — against
      `isOperator()` when `+t` is set, `482 ERR_CHANOPRIVSNEEDED` if not). On a successful
      set, confirmation is sent to **both** the setter (`queueSend`) and the rest of the
      channel (`broadcastToChannel`) — unlike `handleMode`, which only calls
      `broadcastToChannel` and so never echoes the MODE change back to whoever issued it.
      Worth aligning later if that self-echo gap in `handleMode` turns out to matter for irssi.

## Bugs / things to handle (found in manual testing, 2026-07-19)
- [x] `handleNick`: old==new nick in broadcast — **fixed 2026-08-08**. `oldNickname`
      is now captured before `setNickname()` overwrites it.
- [x] `handleNick`: broadcast on a client's first NICK — **fixed 2026-08-08**. Now
      only broadcasts when `oldNickname` was non-empty (a genuine rename), not on
      initial registration. Audience is still "every connected client" rather than
      "clients sharing a channel with you" — that half needs channels (Phase 3) and
      is still open; also still true that the sender itself is never notified of its
      own successful NICK, valid or redundant (separate gap, not fixed here).
- [x] **Re-setting to the SAME nickname you already have still broadcast a no-op
      `NICK` change to everyone else — fixed 2026-08-08.** New guard at the top of
      `handleNick` (before the "already in use" check): if the requested nickname
      equals the client's current one, treat it as a no-op — call
      `checkRegistrationComplete()` (harmless if already registered) and return,
      skipping both `isNicknameInUse()` (which excludes the client's own fd and
      would've said "not in use") and the broadcast.
- [x] `PASS` sent *after* successful registration is re-processed — **fixed
      2026-08-08**, now answers `462 ERR_ALREADYREGISTRED` and leaves auth state
      untouched (Phase 2 numerics).
- [x] commands before registration (e.g. `hello` before `PASS`) are silently
      dropped — **fixed 2026-08-08**, registration gating in `executeCommand()`
      now answers `421`/`451` (Phase 2).

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
- [ ] originally reported: "other clients can't communicate while one uses NICK" — not yet
      re-verified against the current code; D1 (one `recv()` per `POLLIN`, no drain loop)
      should already rule out this class of freeze. Retest now that the build is fixed; if
      it still reproduces, it's a new bug, not the old blocking one.

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
| `PASS` → `NICK` → `USER` → welcome | [x] real numeric `001 RPL_WELCOME` |
| Errors (wrong pass, dup nick, missing params, etc.) | [x] real numerics to the client (`464`/`433`/`461`/`462`/`451`) — **and** still logged on server stderr |
| Unknown command | [x] `421 ERR_UNKNOWNCOMMAND` |
| Commands before `PASS` succeeds, or before registration completes | [x] `451 ERR_NOTREGISTERED` (`PASS` is the gatekeeper — see Phase 2) |
| Real IRC client (irssi) full registration | [~] `001` now sent — not yet live-verified against irssi |
| `JOIN` | [x] works, sends `331`/`332`/`353`/`366` + broadcasts `JOIN` to the channel; first joiner becomes operator |
| `MODE` (`i`,`t`,`k`,`l`,`o`) | [x] works, operator-gated, broadcasts to channel (sender not self-echoed) |
| `TOPIC` | [x] works — query always allowed; set is operator-gated when `+t`; self-echoed to setter + broadcast to channel |
| `PRIVMSG` / `PART` / `QUIT` | [ ] empty stubs |

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
Expected reply on the client (after `USER`, once all three are done — order-independent):
```
:ircserv 001 jin :Welcome to the IRC server, jin!
```

Scripted one-liner (real TCP, exercises the line-splitting too since all 3 commands
arrive as one packet):
```bash
printf "PASS mypassword\r\nNICK jin\r\nUSER jin 0 * :Jin Park\r\n" | nc -q1 localhost 6667
```
(`printf` not `echo`: reliably emits the `\r\n` CRLF endings IRC requires.
`-q1`: nc exits 1s after stdin ends instead of hanging.)

## 5. Per-command test matrix (Scenario A: 2 clients, Window 2 = client 1, Window 3 = client 2)

Each command below: happy path, then unhappy/edge cases, with which window sees what.
Server started as `./ircserv 6667 mypassword` in Window 1 throughout.

### PASS
- **Happy:** `PASS mypassword` — no reply yet (IRC doesn't ACK `PASS` on its own;
  confirmation only comes via `001` once `NICK`+`USER` also land).
- **Unhappy — wrong password:** `PASS wrongpw` → Window 2: `:ircserv 464 * :Password incorrect`.
- **Unhappy — missing param:** `PASS` alone → Window 2: `:ircserv 461 * PASS :Not enough parameters`.
- **Unhappy — already registered:** after full registration, send `PASS mypassword` again →
  Window 2: `:ircserv 462 * :You may not reregister`; auth state untouched.
- **Edge — gatekeeper, any command before `PASS` succeeds:** open Window 2, send `NICK jinn`
  *before* `PASS` → Window 2: `:ircserv 451 * :You have not registered` (not `433`, even
  though `jinn` may be free — `PASS` must succeed first, checked before any other handler runs).
  Only after that does `PASS mypassword` get accepted.

### NICK
- **Happy — first NICK during registration:** `NICK jin` (before registration completes) →
  no broadcast to Window 3 (a client's very first nickname isn't a "change" from anything).
- **Happy — rename after registration:** both clients registered as `jin`/`ha`; Window 2 sends
  `NICK jinny` → Window 3 sees `:jin NICK jinny` (Window 2 itself is not echoed the change).
- **Unhappy — missing param:** `NICK` alone → Window 2: `:ircserv 461 * NICK :Not enough parameters`.
- **Unhappy — duplicate nickname:** Window 3 is `ha`; Window 2 sends `NICK ha` →
  Window 2: `:ircserv 433 * ha :Nickname is already in use`; Window 2's nickname unchanged.
- **Edge — resetting to the SAME nickname (fixed 2026-08-08):** Window 2 is already `jin`,
  sends `NICK jin` again → no reply, no broadcast to Window 3 — treated as a no-op.

### USER
- **Happy:** `USER jin 0 * :Jin Park` — no direct reply (silent, like `PASS`); only
  contributes to the eventual `001` once `PASS`+`NICK` also land. Realname with spaces
  (`:Jin Park`) must survive intact — verifies `Server::parse()`'s trailing-`:` handling.
- **Unhappy — missing params:** `USER jin 0 *` (only 3 params) → Window 2:
  `:ircserv 461 * USER :Not enough parameters`.
- **Unhappy — already registered:** send `USER` again after full registration → Window 2:
  `:ircserv 462 * :You may not reregister`; username/realname untouched (previously this
  silently overwrote them and re-sent the welcome — fixed).

### JOIN
- **Happy — first member creates the channel:** registered Window 2 sends `JOIN #general` →
  Window 2 gets `:ircserv 331 jin #general :No topic is set` then
  `:ircserv 353 jin = #general :jin ` then `:ircserv 366 jin #general :End of /NAMES list`.
  Window 3 (not in the channel) sees nothing.
- **Happy — second member joins existing channel:** Window 3 (`ha`) sends `JOIN #general` →
  Window 3 gets the same 331/332+353/366 sequence (now listing both nicks); Window 2
  (already in `#general`) sees `:ha JOIN #general`.
- **Unhappy — missing channel param:** `JOIN` alone → Window 2:
  `:ircserv 461 * JOIN :Not enough parameters`.
- **Unhappy — before registration:** unregistered client sends `JOIN #general` → covered by
  the general gate: `:ircserv 451 * :You have not registered` (not JOIN-specific anymore —
  `handleJoin`'s own auth check was removed once `executeCommand()` started gating).
- **Edge — invalid channel name (no `#` prefix):** `JOIN general` → currently **silent**,
  only logged on the server (`Error: Invalid channel name format.`) — no `476` reply to the
  client yet (documented gap, out of scope for the 7-code Phase 2 list).

### MODE
Setup for every case below: Window 2 (`jin`) creates `#general` via `JOIN` (becomes operator),
then Window 3 (`ha`) also `JOIN`s (regular member).
- **Happy — set `+t`:** Window 2 (operator) sends `MODE #general +t` → Window 3 sees
  `:jin MODE #general +t`. Window 2 itself gets **no reply** — `handleMode` only calls
  `broadcastToChannel()`, which excludes the sender (asymmetric with `TOPIC` below, which
  does echo back to the setter — see Phase 4 TODOs).
- **Happy — unset `-t`:** Window 2 sends `MODE #general -t` → Window 3 sees `:jin MODE #general -t`.
- **Unhappy — non-operator tries any MODE:** Window 3 (not operator) sends `MODE #general +t` →
  Window 3: `:ircserv 482 ha #general :You're not channel operator`.
- **Unhappy — missing mode param:** Window 2 sends `MODE #general` alone → Window 2:
  `:ircserv 461 jin MODE :Not enough parameters`.
- **Unhappy — malformed mode string (no leading `+`/`-`):** `MODE #general t` → Window 2:
  `:ircserv 501 jin :Unknown mode flag`.
- **Unhappy — unrecognized mode char:** `MODE #general +z` → Window 2:
  `:ircserv 472 jin z :is unknown mode char to me`.
- **Unhappy — channel doesn't exist:** `MODE #nope +t` → Window 2:
  `:ircserv 403 jin #nope :No such channel`.
- **Edge — issued by a client not in the channel (Scenario B, Window 4 = client 3):** a
  registered-but-not-joined client sends `MODE #general +t` →
  `:ircserv 442 <nick> #general :You're not on that channel` — checked *before* the operator
  check, so a non-member gets `442`, not `482`.
- **Edge — setting a mode that's already set (`+t` twice in a row):** no error, just re-applies
  and re-broadcasts; must not crash or double-insert into any internal list.
- **`+o`/`-o` (grant/revoke operator):** Window 2 sends `MODE #general +o ha` → `ha` is now
  operator and can itself run operator-gated commands; `-o` reverses it. Missing nick param →
  `461`; unknown nick → `401 ERR_NOSUCHNICK`; nick not in the channel → `441 ERR_USERNOTINCHANNEL`.

### TOPIC
Same setup as MODE above (Window 2 = operator, Window 3 = member).
- **Happy — query, no topic set yet:** Window 2 sends `TOPIC #general` → Window 2:
  `:ircserv 331 jin #general :No topic is set`.
- **Happy — set (as operator, `+t` off by default):** Window 2 sends
  `TOPIC #general :Hello World` → **both** Window 2 and Window 3 see
  `:jin TOPIC #general :Hello World` — unlike `MODE`, the setter is echoed too.
- **Happy — query after it's set:** Window 3 sends `TOPIC #general` → Window 3:
  `:ircserv 332 ha #general :Hello World`.
- **Happy — clear the topic:** `TOPIC #general :` (colon, empty trailing) → sets it to `""`;
  a later `TOPIC #general` query goes back to `331`, not `332`.
- **Unhappy — `+t` set, non-operator tries to change it:** Window 2 sends `MODE #general +t`,
  then Window 3 (non-op) sends `TOPIC #general :ha's topic` → Window 3:
  `:ircserv 482 ha #general :You're not channel operator`.
- **Happy — `+t` set, query still works for anyone:** with `+t` still on, Window 3 sends
  `TOPIC #general` (no new topic) → succeeds normally (`331`/`332`) — the restriction only
  applies to *setting*, checked after the query branch, never before it.
- **Happy — `+t` set, operator can still change it:** Window 2 (operator) sends
  `TOPIC #general :still allowed` → succeeds.
- **Unhappy — missing channel param:** `TOPIC` alone → `:ircserv 461 jin TOPIC :Not enough parameters`.
- **Unhappy — channel doesn't exist:** `TOPIC #nope` → `:ircserv 403 jin #nope :No such channel`.
- **Unhappy — not a member of the channel:** a client who hasn't `JOIN`ed sends `TOPIC #general`
  (query **or** set) → `:ircserv 442 <nick> #general :You're not on that channel` — checked
  before the query/set branch, so even a *query* from a non-member is rejected.

### PRIVMSG / PART / QUIT
- Not implemented yet — empty stubs (Phase 3 TODO). Sending them currently does nothing
  visible on either window and logs nothing distinctive; not a crash, just a no-op.

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
**Should now work** — `001 RPL_WELCOME` landed 2026-08-08. Not yet re-verified live
against irssi (deferred to manual testing); a working reference client is a hard
subject requirement for evaluation.

## 9. Leaks / cleanup
```bash
valgrind --leak-check=full ./ircserv 6667 pw   # connect/disconnect a few clients, Ctrl+C
ps aux | grep ircserv | grep -v grep           # after tests: no leftover server process
```

---

# Defense checklist (from subject v10.0)
- [ ] `README.md` at **repo root**: italic first line with logins, Description, Instructions, Resources (incl. how AI was used) — English
- [ ] stop tracking the compiled binary `ircserv/ircserv` (it's in git; add to `.gitignore`, `git rm --cached`)
- [ ] reference client = **irssi** (proposed): must connect with zero errors — `001` numeric landed 2026-08-08, not yet live-verified against irssi
- [ ] only allowed external functions used (see subject p.6); `fcntl` **only** as `fcntl(fd, F_SETFL, O_NONBLOCK)`
- [ ] exactly **one** `poll()` handles everything — read *and* write; no fork, no threads
- [ ] server never crashes / never quits unexpectedly (crash = grade 0)
- [ ] be ready for a small live code modification during evaluation