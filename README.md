*This project has been created as part of the 42 curriculum by hmote and jiepark*

# ft_irc

An IRC server written in C++98. It uses a single `poll()` event loop with non-blocking sockets and no threads or forks, while supporting the core IRC workflow expected by real clients.

## Description

The server implements the essential IRC features needed for registration, channel management, messaging, and operator control:

- Authentication with `PASS`, `NICK`, and `USER`
- Channel workflow with `JOIN`, `PART`, `QUIT`, and topic handling
- Private and channel messaging with `PRIVMSG`
- Keepalive support with `PING` and `PONG`
- Operator commands with `KICK`, `INVITE`, and `MODE`
- Channel modes `+i`, `+t`, `+k`, `+l`, and `+o`

The project is designed to work with standard IRC clients such as `irssi` and simple terminal tools such as `nc`.

## Instructions

### Build

```bash
cd ircserv
make
make re
make clean
```

### Run

```bash
./ircserv <port> <password>
```

- `<port>`: listening port, for example `6667`
- `<password>`: server password required during registration

Example:

```bash
./ircserv 6667 mypassword
```

### Connect

```bash
nc localhost 6667
irssi --connect=localhost
```

### Register

```text
PASS mypassword
NICK yournickname
USER username 0 * :Real Name
```

### Basic Commands

- `JOIN #channelname`
- `PRIVMSG #channelname :hello world`
- `PRIVMSG nickname :hello`
- `MODE #channelname +i`
- `KICK #channelname nickname`
- `TOPIC #channelname :new topic`
- `PART #channelname`
- `QUIT`

## Challenges

Some of the main difficulties in this project were handling a non-blocking `poll()` loop cleanly, keeping the registration flow correct when `PASS`, `NICK`, and `USER` arrive in any order, and making sure the server stayed compatible with real IRC clients such as `irssi`.

## Resources

- RFC 1459: Internet Relay Chat Protocol
- `irssi`: IRC client used for real-client testing
