#ifndef SIGNAL_HPP
# define SIGNAL_HPP

#include <csignal>

// set to 1 by SIGINT/SIGQUIT handler, checked by eventLoop() every round
extern volatile std::sig_atomic_t g_serverShutdown;

void	signal_handler(int signal);

#endif
