# ft_irc — Status Summary (2026-07-18)

You paused this project a few weeks ago. Here's what it is, what exists, what's left, and what to watch out for — based on reading everything currently in `ircserv/` plus your own notes in `jin-note/`.

---

## 1. Concept (for a junior dev / refresher)

**ft_irc** is a 42 school project: build your own IRC server in C++98 from scratch.

- Run as: `./ircserv <port> <password>`
- Real IRC clients (irssi, HexChat, or `nc`) connect to it, authenticate, and chat through it.
- You are **not** building a client — only the server that routes messages between connected users.

Hard constraints from the subject:
- **No multi-threading, no `fork()`.** All clients must be handled by a single process using **one** call to `poll()` (or `epoll`/`kqueue`) that watches every socket at once.
- **All I/O must be non-blocking.** `recv()`/`send()` must never freeze the whole server waiting on one client.
- Must correctly handle **fragmented reads** — a client's `"JOIN #general\r\n"` can arrive in pieces (`"JO"`, `"IN #ge"`, `"neral\r\n"`), so each client needs its own buffer and you only process a line once you've seen `\r\n`.

Functionally, the server must support:
- **Registration**: `PASS`, `NICK`, `USER` → then a welcome reply.
- **Channels & messaging**: `JOIN`, `PART`, `PRIVMSG`, `QUIT`.
- **Operator commands**: `KICK`, `INVITE`, `TOPIC`, `MODE`.
- **Channel modes**: `i` (invite-only), `t` (topic locked to ops), `k` (password), `o` (operator), `l` (user limit).

Your own notes (`jin-note/overview.md`, `jin-note/polling_socket.md`) are a solid beginner explainer of sockets/fd/`poll()` — worth re-reading first if `poll()` mechanics feel fuzzy again. Short version: `poll()` takes a list of `struct pollfd{fd, events, revents}`, blocks until *something* is ready, and you check each `revents` afterward instead of blocking on one client at a time.

You also have 4 reference implementations checked into `jin-reference/` (AhmedFatir, dalexhd, marineks, raveriss) for comparing design approaches — dalexhd's even has an automated test suite (`jin-reference/dalexhd/tester/`) you could study for test ideas.

---

## 2. What's done

Architecture is in place, phase 1 (skeleton) is essentially finished, phase 2 (registration) is drafted but not working yet:

- **Project skeleton**: `Makefile` (C++98, `-Wall -Wextra -Werror`), Orthodox Canonical Form on `Server` and `Client`.
- **Socket setup**: `initSocket()` / `bindSocket()` / `listenSocket()` / `acceptConnection()` — basic happy-path socket creation.
- **poll() event loop** (`Server::eventLoop()`): single `poll()` call over listening socket + all client sockets, dispatches to `acceptConnection()` or `receiveData()`.
- **Client read buffering**: `Client::appendToReadBuf()` + `extractLines()` correctly accumulates partial `recv()`s and only yields complete `\n`-terminated lines (handles the fragmentation requirement).
- **`receiveData()`** loop: distinguishes `size > 0` (data), `size == 0` (clean disconnect), `size == -1` with `EAGAIN`/`EWOULDBLOCK` (no more data) vs. real error — and cleans up (`close`, `deleteClient`, `removeClientFromPoll`) on disconnect/error.
- **SIGINT handling**: `signal_handler()` sets a global flag (shutdown logic itself is still a TODO).
- **Command dispatch skeleton**: `Server::parse()` splits a raw line into command + params; `getCommandType()` + `executeCommand()` switch to handler functions for `PASS/NICK/USER/JOIN/PART/PRIVMSG/QUIT`.
- **Command handlers drafted** in `srcs/ServerComandHandlers.cpp`: `handlePass`, `handleNick`, `handleUser` have real logic (password check, nickname validation/uniqueness, registration completion + welcome message). `handleJoin/handlePart/handlePrivmsg/handleQuit` are empty stubs (comments only).
- 22 commits of incremental history, most recent: "handling PASS, NICK, USER".

---

## 3. To do

**Immediate — the build is currently broken.** Running `make re` today fails with 7 compiler errors:

1. `srcs/ServerComandHandlers.cpp` is **not listed in the Makefile's `SRCS`**, so none of that PASS/NICK/USER logic is even being compiled right now.
2. `Message::getParams()` is **called** throughout `ServerComandHandlers.cpp` but **never declared** in `Client.hpp` (only `getCommand()` exists, and `params` is a public member with no getter). Won't compile once you add the file to the Makefile.
3. `Client::isNicknameInUse()` (in `Client.cpp`) reaches into `Server::_clients` directly — but `_clients` is a **private, non-static** member of `Server`. This can't compile as written; `Client` has no business owning nickname-uniqueness logic against another class's private state anyway.
4. Constructor member-init-list order warnings (`-Werror` fails the build): `_isUserReceived` initialized before `_nickname` but declared after it in the class — reorder the init list to match declaration order.
5. `Client::getNickname()` return type mismatch between header (`std::string`) and `.cpp` (`const std::string`) — non-blocking but caught by `-Werror`.

Once it builds, functional work remaining (per your own `jin-note/overview.md` roadmap):

- **Phase 2 (finish registration)**: send real numeric replies (`001 RPL_WELCOME`, `433 ERR_NICKNAMEINUSE`, `464 ERR_PASSWDMISMATCH`, `451 ERR_NOTREGISTERED`) instead of just `std::cerr` logging or ad-hoc strings — the subject/an IRC client expects these exact formats. Gate all non-registration commands behind "is this client registered yet?".
- **Phase 3 — channels + messaging**: `Channel` class doesn't exist yet (only forward-declared in `Server.hpp`). Need `JOIN` (create-if-missing, first joiner = operator, `RPL_NAMREPLY`/`RPL_ENDOFNAMES`), `PRIVMSG` (to channel and to nick), `PART`, `QUIT` (leave all channels + broadcast).
- **Phase 4 — operator commands**: `KICK`, `INVITE`, `TOPIC`, `MODE` (`i/t/k/o/l`) — none started.
- **`Server::parse()` doesn't handle the IRC trailing-parameter syntax** (`:` prefix marking "rest of line is one param, may contain spaces") — needed for anything like `PRIVMSG #chan :hello world`, which is core to the project. Right now every param is just space-split, so multi-word message text would be split into separate params incorrectly.
- **`fcntl(..., O_NONBLOCK)` on sockets** — the subject requires non-blocking I/O explicitly; skimmed the code and didn't see this set anywhere. Worth double-checking `initSocket()`/`acceptConnection()`.
- **`SO_REUSEADDR`** — your own notes flagged this as needed (`bind()` fails for ~60s after Ctrl+C without it); doesn't appear to be set in `bindSocket()` yet.
- Graceful shutdown on SIGINT (currently only sets a flag; nothing checks it).

---

## 4. Things to consider

- **You have two open design questions in your own notes** (`jin-note/overview.md`, bottom): how to route PRIVMSG to the right recipients, and the full list of IRC commands you need to cover. Worth revisiting once the build is green again.
- **Ownership boundary between `Client` and `Server` is blurry** — `Client::isNicknameInUse()` reaching into `Server`'s private client map (bug #3 above) is a symptom of this. Cleaner: keep nickname-uniqueness checks in `Server` (which already owns `_clients`), and have `Client` stay a dumb state holder. This will keep recurring if not addressed structurally.
- **`ServerComandHandlers.cpp` being absent from the Makefile** suggests the last session ended mid-edit — you may have been about to wire it in when you stopped. That's the fastest path back in: add it to `SRCS`, then fix the resulting compile errors one at a time (they're listed above in the order the compiler will hit them).
- You have 4 full reference implementations sitting in `jin-reference/` — useful for unblocking specific pieces (e.g. dalexhd's `Mode.hpp`/`commands/mode/*.hpp` split for channel modes, or raveriss's `IrcNumericReplies.hpp` for the exact numeric-reply string formats) without having to look them up from the RFC each time.
- No tests exist yet in `ircserv/` itself. Given the "does this build and respond correctly to `nc`" nature of this project, a quick smoke-test script (send a fixed sequence of commands via `nc`, check output) would catch regressions like the current broken build much earlier than "try to compile weeks later."
