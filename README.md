# 4-phase implementation order

- Skeleton + server loop (socket → poll() → accept)
- Registration (PASS / NICK / USER → 001 RPL_WELCOME)
- Channels + messaging (JOIN / PRIVMSG / PART / QUIT)
- Operator commands (KICK / INVITE / TOPIC / MODE i,t,k,o,l)