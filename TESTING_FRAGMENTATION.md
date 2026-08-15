# IRC Server — Message Fragmentation & Argument Handling Test Guide

Testing that the server correctly handles:
- Partial/fragmented messages
- `\r\n` vs `\n` line endings
- Trailing parameters (colon syntax)
- Ctrl+D (EOF) handling
---

## Summary Checklist

[x] manually checked by JIN (working + no leak)
- [X] Test 1: Normal registration works
- [X] Test 2: Partial commands buffered (Ctrl+D)
- [X] Test 3: Multiple commands in one paste processed in order
- [X] Test 4: Trailing `:` captures rest of line as single parameter
- [ ] Test 5: Both `\r\n` and `\n` line endings work
- [ ] Test 6: Slow character-by-character input handled
- [ ] Test 7: Empty lines ignored gracefully
- [ ] Test 8: Very long parameters work
- [ ] Test 9: Case-insensitive commands work
- [ ] Test 10: Missing `\r` doesn't break parsing
- [ ] Test 11: Multiple concurrent clients work without interference

**If all tests pass:** ✅ Message fragmentation and argument handling are correct per subject requirements.

---

---

## Setup

**Terminal 1: Start the server**
```bash
cd ircserv
./ircserv 6667 testpass
```

Server is now listening. You should see no output (expected per subject).

---

## Test 1: Normal Registration (baseline)

**Terminal 2:**
```bash
nc localhost 6667
```

Now type (each line followed by ENTER):
```
PASS testpass
NICK alice
USER alice 0 * :Alice Smith
```

**Expected output in Terminal 2:**
```
:ircserv 001 alice :Welcome to the IRC server, alice!
```

**Server logs (Terminal 1):** Should show `Processing line from client` for each command.

✅ **Baseline works** — proceed to fragmentation tests.

---

## Test 2: Partial Command (mid-line Ctrl+D)

**Terminal 2:**
```bash
nc localhost 6667
```

Type the first line normally:
```
PASS testpass
[ENTER]
```

You should see welcome or similar response. Now type (DON'T PRESS ENTER):
```
NICK bob
[NOW PRESS Ctrl+D]
```

**Expected behavior:**
- Server doesn't respond to "NICK bob" (incomplete, no newline)
- nc connection closes
- Server logs show the partial line was buffered but not processed
- Server stays running, ready for next client

**Terminal 1 logs:** Should show the PASS processed, but no "Processing line" for "NICK bob".

✅ **Partial commands are buffered, not processed**

---

## Test 3: Multiple Commands in One Paste

**Terminal 2:**
```bash
nc localhost 6667
```

Copy and paste this entire block at once:
```
PASS testpass
NICK charlie
USER charlie 0 * :Charlie Brown
JOIN #general
PRIVMSG #general :hello everyone
```

[ENTER after the last line]

**Expected output:**
```
:ircserv 001 charlie :Welcome to the IRC server, charlie!
:charlie JOIN #general
:ircserv 331 charlie #general :No topic is set
:ircserv 353 charlie = #general :@charlie
:ircserv 366 charlie #general :End of /NAMES list
```

**What this tests:**
- Server receives 5 commands in one chunk
- Buffers them by newline
- Processes each line in order
- Handles back-to-back commands without blocking other clients

✅ **Multiple commands processed correctly**

---

## Test 4: Trailing Parameter (colon = rest of line)

**Terminal 2:**
```bash
nc localhost 6667
```

Type:
```
PASS testpass
NICK dave
USER dave 0 * :Dave Johnson With Spaces
```

[ENTER]

**Expected:**
```
:ircserv 001 dave :Welcome to the IRC server, dave!
```

**What this verifies:**
- Parameter after `:` is treated as ONE argument
- `USER dave 0 * :Dave Johnson With Spaces` → realname = "Dave Johnson With Spaces"
- NOT split on spaces when after `:`

Now test with PRIVMSG:
```
JOIN #test
PRIVMSG #test :this is a message with many spaces
```

**Server logs should show:** Trailing message "this is a message with many spaces" stored as single param.

✅ **Trailing parameters work (colon syntax)**

---

## Test 5: \r\n vs \n (line ending variations)

**Terminal 2:**
```bash
nc localhost 6667
```

Type normally (your terminal sends \n):
```
PASS testpass
NICK eve
USER eve 0 * :Eve
```

[ENTER]

**Expected:** Welcome response received.

Now open a second nc connection with explicit \r\n:

**Terminal 3:**
```bash
printf "PASS testpass\r\nNICK frank\r\nUSER frank 0 * :Frank\r\n" | nc localhost 6667
```

**Expected output in Terminal 3:**
```
:ircserv 001 frank :Welcome to the IRC server, frank!
```

**What this tests:**
- Server handles proper IRC format (\r\n)
- Server also handles just \n (from nc terminal)
- Both line endings work

✅ **Both \r\n and \n line endings handled**

---

## Test 6: Very Slow Typing (character by character)

This simulates a very slow/laggy connection.

**Terminal 2:**
```bash
(
  printf "P"; sleep 0.3
  printf "A"; sleep 0.3
  printf "S"; sleep 0.3
  printf "S"; sleep 0.3
  printf " testpass"; sleep 0.3
  printf "\n"
) | nc localhost 6667
```

**Expected behavior:**
- Server buffers each character as it arrives
- Once `\n` is received, processes the complete line
- Responds with confirmation or error

**What this tests:**
- Read buffer accumulates partial data
- Server doesn't try to process incomplete lines
- Only processes on newline boundary

✅ **Slow/fragmented input handled**

---

## Test 7: Empty Lines & Whitespace

**Terminal 2:**
```bash
nc localhost 6667
```

Type:
```
PASS testpass
[ENTER]
[ENTER]
[ENTER]
NICK gina
```

[ENTER]

**Expected:**
- Empty lines are ignored (no response/error)
- NICK command processes normally

**Server logs:** Should show PASS processed, empty lines skipped, NICK processed.

✅ **Empty lines handled gracefully**

---

## Test 8: Very Long Parameter

**Terminal 2:**
```bash
nc localhost 6667
```

Type:
```
PASS testpass
NICK henry
USER henry 0 * :Henry With A Very Long Realname That Has Many Words And Spaces And Should All Be Captured As One Parameter Because Of The Colon Prefix
```

[ENTER]

**Expected:**
```
:ircserv 001 henry :Welcome to the IRC server, henry!
```

**Server logs:** Entire long realname captured as single parameter.

✅ **Long trailing parameters work**

---

## Test 9: Case-Insensitive Commands

**Terminal 2:**
```bash
nc localhost 6667
```

Type (mix of cases):
```
pass testpass
NICK iris
user iris 0 * :Iris
```

[ENTER]

**Expected:**
```
:ircserv 001 iris :Welcome to the IRC server, iris!
```

**What this tests:**
- `pass` (lowercase) works
- `NICK` (uppercase) works
- `user` (lowercase) works
- All case variants accepted

✅ **Case-insensitive command parsing**

---

## Test 10: Invalid Line Endings (should still work)

**Terminal 2:**
```bash
nc localhost 6667
```

Type:
```
PASS testpass
NICK jack
USER jack 0 * :Jack
```

[ENTER]

Even though nc may send just `\n` (no `\r`), server should handle it.

**Expected:**
```
:ircserv 001 jack :Welcome to the IRC server, jack!
```

✅ **Server strips \r if present, works without it**

---

## Test 11: Multiple Clients Simultaneously

**Terminal 2:**
```bash
nc localhost 6667
PASS testpass
NICK user1
USER user1 0 * :User One
JOIN #shared
```

[Let this connection stay open]

**Terminal 3 (new connection while Terminal 2 is active):**
```bash
nc localhost 6667
PASS testpass
NICK user2
USER user2 0 * :User Two
JOIN #shared
PRIVMSG #shared :hello from user2
```

[ENTER]

**Expected in Terminal 2:**
```
:user2 JOIN #shared
:user2 PRIVMSG #shared :hello from user2
```

**What this tests:**
- Server handles multiple concurrent connections
- Messages from one client visible to another
- No blocking or interference

✅ **Concurrent client handling works**

---

## Troubleshooting

**Server not responding to nc:**
- Verify server is running: `ps aux | grep ircserv`
- Check port: `netstat -tuln | grep 6667` (or your port)
- Try: `nc -v localhost 6667` to see connection details

**Command processed but no response:**
- Might be a registration gate issue
- Ensure you sent PASS first before other commands
- Check server logs for error messages

**Partial command not buffered (seems to block):**
- This is actually correct behavior
- Server waits for `\n` before processing
- Use Ctrl+D to close connection and move on

---

**All tests created 2026-08-15 — ready for evaluation**
