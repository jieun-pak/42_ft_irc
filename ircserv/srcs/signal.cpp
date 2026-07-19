#include "../includes/signal.hpp"

volatile std::sig_atomic_t g_serverShutdown = 0;

void	signal_handler(int signal)
{
	(void)signal;
	// Only set the flag — no I/O, no allocation, no cleanup here.
	// eventLoop() notices the flag and shuts down cleanly; the Server
	// destructor closes sockets and frees clients.
	g_serverShutdown = 1;
}
