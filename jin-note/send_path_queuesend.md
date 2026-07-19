# Send Path: queueSend() / POLLOUT / sendData() — Note

Why the server never calls `send()` directly from command handlers, and how the
current implementation works. Companion note to `polling_socket.md` (read side).

---

## 1. The subject rule that forces this design

Subject p.8, red warning box (paraphrased, but the key words are verbatim):

> if you attempt to read/recv or **write/send** in any file descriptor
> **without using poll()** (or equivalent), your grade will be **0**.

It names *write/send* explicitly — not only reads. Since mandatory features
(welcome reply, numerics, PRIVMSG forwarding) all require sending, every
`send()` must be sanctioned by poll. Poll's "you may write now" signal is
`POLLOUT`. So the send side must mirror the recv side.

Second, independent reason: our sockets are non-blocking (`O_NONBLOCK`, also
subject-mandated). On a non-blocking socket `send()` may:
- write only *part* of the data (kernel send-buffer nearly full), or
- fail with `-1` / `EAGAIN` (buffer completely full).

Direct `send()` calls that ignore this silently lose bytes. A per-client
out-buffer keeps the unsent remainder and retries — nothing is dropped.

## 2. send() in handlers (old) vs current implementation

| | old: `send()` in handler | current: `queueSend()` |
|---|---|---|
| poll sanction | none — send fired immediately | every `send()` runs only after `POLLOUT` |
| partial send | remainder silently lost | remainder stays in `_writeBuf`, retried |
| `EAGAIN` | message silently dropped | kept in buffer, retried next poll round |
| grade-0 risk | depends on evaluator's reading | none — matches the strict reading |
| handler code | `send(fd, msg.c_str(), msg.size(), 0)` | `queueSend(fd, msg)` (simpler) |

The change to the handlers was two lines and did not alter any handler logic —
the handler's meaning stays "deliver this string to this client"; only the
delivery mechanism moved behind one function.

**Team rule going forward: handlers never call `send()`. All replies go through
`queueSend()`.** The only `send()` in the whole codebase is inside `sendData()`.

## 3. The architecture (mirror of the read side)

| | read side | write side |
|---|---|---|
| per-client buffer | `Client::_readBuf` | `Client::_writeBuf` |
| poll event | `POLLIN` ("data waiting") | `POLLOUT` ("socket writable") |
| the one syscall | `recv()` in `receiveData()` | `send()` in `sendData()` |
| handlers touch | complete lines from `extractLines()` | `queueSend()` appends |

Flow of one reply:

```
handler decides to reply
  -> queueSend(fd, msg)         appends msg to that client's _writeBuf
                                arms POLLOUT on that client's pollfd
  -> poll()                     wakes when the socket can accept bytes
  -> sendData(fd)               one send(); removes sent bytes from _writeBuf;
                                if buffer now empty -> disarm POLLOUT
```

**Invariant: `POLLOUT` is armed if and only if `_writeBuf` is non-empty.**
A socket is almost always writable, so leaving `POLLOUT` armed with nothing to
send would make `poll()` return instantly forever — a silent 100%-CPU busy loop.
`queueSend()` arms it; `sendData()` disarms it when the buffer drains.

## 4. Q&A

**Q1. Does the current change use write/send "with poll"? I.e. once poll() says
the socket is writable, then we send data?**

Yes — that is exactly the model, stated precisely:

1. A handler never sends; it only *queues* (appends to `_writeBuf`) and arms
   `POLLOUT` for that fd.
2. The next `poll()` call watches that fd for writability. When the kernel's
   send-buffer for that socket can accept bytes, `poll()` sets `POLLOUT` in
   `revents`.
3. Only then does `eventLoop()` call `sendData(fd)`, which performs one
   `send()` of whatever is buffered.
4. If not everything fit (partial send / `EAGAIN`), the leftover stays in
   `_writeBuf`, `POLLOUT` stays armed, and poll brings us back — same
   level-triggered logic as the read side.

So every single `send()` in the program is preceded, in the same loop
iteration, by poll reporting `POLLOUT` for that exact fd. That is the "write
through poll" guarantee the subject demands.

**Q2. Explain queueSend().**

```cpp
void Server::queueSend(int clientFd, const std::string &msg)
{
    Client* client = getClient(clientFd);
    if (!client)
        return;                       // client already gone — drop quietly
    client->appendToWriteBuf(msg);    // 1. store the bytes (no I/O happens!)
    setPollOut(clientFd, true);       // 2. tell poll: watch this fd for writability
}
```

- It performs **no I/O**. Nothing touches the network here.
- Step 1 puts the message at the end of that client's private outbox
  (`_writeBuf` — a `std::string` used as a byte queue; multiple queued
  messages simply concatenate, which is fine because IRC messages are
  self-delimiting via `\r\n`).
- Step 2 flips the `POLLOUT` bit in that client's `pollfd.events`
  (`events |= POLLOUT`) so the *next* `poll()` call reports when the socket
  can take bytes.
- Mental model: `queueSend()` = drop a letter in the outbox. `poll()` = the
  road is clear. `sendData()` = the mail carrier actually delivers.

Why the null-check matters: a handler might try to reply to a client that
disconnected earlier in the same poll round (already `disconnectClient()`ed).
`getClient()` returns NULL then, and the reply is dropped instead of crashing.

## 5. Related decisions

- D1 (one `recv()` per `POLLIN`) — `polling_socket.md` and plan-phase1.md
- D2 (this note), D3 (`O_NONBLOCK`) — plan-phase1.md section 3
- The two converted handler call sites: `ServerComandHandlers.cpp`
  (NICK change notification, USER welcome message)
