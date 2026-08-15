*This project has been created as part of the 42 curriculum by jiepark*

# ft_irc — Internet Relay Chat Server Implementation

A minimal RFC 1459-compliant IRC server written in C++98, implementing core IRC protocol features with focus on socket programming, non-blocking I/O, and proper client-server communication patterns.

## Description

This project implements an Internet Relay Chat (IRC) server from scratch, handling multiple concurrent clients through a single `poll()` event loop (no fork, no threads). The server supports essential IRC features:

**Authentication:** PASS / NICK / USER with proper registration state machine
**Channels:** JOIN / PART / QUIT with member tracking and operator privileges  
**Messaging:** PRIVMSG for both channel broadcasts and direct messages  
**Operators:** KICK (remove members), INVITE (with invite-only channel support), MODE (channel modes: `+i` invite-only, `+t` topic-restricted, `+k` password, `+l` member limit, `+o` operator), TOPIC  
**Keepalive:** PING / PONG for connection verification

**Verified against:** irssi 1.2.3 IRC client (full registration and multi-client broadcast scenarios, live-tested 2026-08-15)

## Instructions

### Build
```bash
cd ircserv
make                # Clean build with -Wall -Wextra -Werror -std=c++98
make re             # Rebuild from scratch
make clean          # Remove object files and binary
```

### Run
```bash
./ircserv <port> <password>
```
- `<port>`: Port number to listen on (e.g., 6667 for standard IRC)
- `<password>`: Server password required by all clients during registration (PASS command)

**Example:**
```bash
./ircserv 6667 mypassword
```

Then connect with an IRC client:
```bash
nc localhost 6667          # netcat (basic testing)
irssi --connect=localhost  # irssi (full-featured IRC client)
```

### Connect & Test
Once the server is running, register with three commands:
```
PASS mypassword
NICK yournickname
USER username 0 * :Real Name
```

Then test features:
- `JOIN #channelname` — join a channel
- `PRIVMSG #channelname :hello world` — send a message to the channel
- `PRIVMSG nickname :hello` — send a direct message to a user
- `MODE #channelname +i` — set the channel invite-only (requires operator)
- `INVITE nickname #channelname` — invite someone to an invite-only channel
- `KICK #channelname nickname` — remove a member from a channel (operator-only)
- `PART #channelname` — leave a channel
- `QUIT` — disconnect

For detailed feature status and test plans, see [ircserv/README.md](ircserv/README.md).

## Resources

### RFC Documents
- **RFC 1459:** Internet Relay Chat Protocol (the specification this implementation targets)

### Reference Materials
- **irssi:** Lightweight IRC client used for live protocol verification

### Implementation Notes
- **Source code organization:**
  - `ircserv/includes/` — header files (Server, Channel, Client, Message parsing, numeric reply codes)
  - `ircserv/srcs/` — implementation files (main loop, command handlers, I/O management)
  - `ircserv/srcs/handlers/` — specialized command handlers (JOIN, TOPIC/MODE, KICK/INVITE)

- **Design decisions:**
  - Single `poll()` loop handles all I/O (read + write via POLLIN/POLLOUT flags)
  - Per-client read/write buffers to handle fragmented network packets
  - Channel operator tracking enables MODE +o/-o and KICK privileges
  - Invite list (`Channel._invitedUsers`) enables +i (invite-only) channel enforcement

### How AI Was Used

Claude Haiku 4.5 was used throughout this project for:
- **Debugging:** Analysis of network protocol behavior, testing output verification
- **Implementation:** Writing command handlers, numeric reply formatting, client state management
- **Code review:** Identifying edge cases (malformed replies, incorrect reply targets, invite list integration)
- **Documentation:** Updating implementation notes and tracking known issues

## Notes

### Mandatory Tier Requirements (Completed)
- [x] **Tier 0:** No crashes or memory leaks in core functionality
- [x] **Tier 1:** All mandatory phase features (Phases 1-4) + root README (this file)
- [x] **Tier 2:** Defense checklist compliance (only allowed functions, single poll loop)
- [x] **Tier 3:** Real bugs fixed (malformed error replies, registration state handling)

### Optional Tier Items
- [ ] **Tier 4:** Code cleanup (remove unused `AddMemberResult` enum, rename handler files for clarity)
- [ ] **Tier 5:** Enhanced features (case-insensitive commands, multi-channel JOIN in one command)

See [ircserv/README.md](ircserv/README.md) for comprehensive testing procedures and known limitations.
