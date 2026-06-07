
bool server_shutdown = false;

void	signal_handler(int signal)
{
	(void)signal;
	server_shutdown = true;
	// TODO: closing socket
}