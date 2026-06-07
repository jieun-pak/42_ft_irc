# 4-phase implementation order

- Skeleton + server loop (socket → poll() → accept)
- Registration (PASS / NICK / USER → 001 RPL_WELCOME)
- Channels + messaging (JOIN / PRIVMSG / PART / QUIT)
- Operator commands (KICK / INVITE / TOPIC / MODE i,t,k,o,l)

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