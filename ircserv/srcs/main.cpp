#include "../includes/Server.hpp"
#include "../includes/signal.hpp"

int	main(int argc, char **argv)
{
	if (argc != 3)
	{
		std::cout << "Usage: " << argv[0] << " <port> <password>" << std::endl;
		return 1;
	}
	// getting port and password and verifying
	int port = atoi(argv[1]);
	if (port <= 0 || port > 65535)
	{
		std::cerr << "Invalid port number" << std::endl;
		return 1;
	}

	std::string password(argv[2]);
	if (password.empty())
	{
		std::cerr << "Password cannot be empty" << std::endl;
		return 1;
	}
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	Server server(port, password);
	server.run();

	return 0;
}