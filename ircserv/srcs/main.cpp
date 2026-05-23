#include "../includes/ft_irc.hpp"

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

	// stablishing socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		std::cout << "Error creating socket" << std::endl;
		return 1;
	}
	// to doublecheck
	int opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		std::cout << "Error setting socket options" << std::endl;
		return 1;
	}
	// address definition
	// ver1
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	//ver2
	// server.serv_addr.sin_family = AF_INET;
	// server.serv_addr.sin_addr.s_addr = INADDR_ANY;
	// server.serv_addr.sin_port = htons(port);

	// binding sockets
	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		std::cout << "Error binding socket" << std::endl;
		return 1;
	}

	// listening
	if (listen(sockfd, 5) < 0)
	{
		std::cout << "Error listening" << std::endl;
		close(sockfd);
		return 1;
	}
	std::cout << "Server listening on port " << port << std::endl;

	// accepting connections
	int newsockfd = accept(sockfd, (struct sockaddr *)NULL, NULL);
	if (newsockfd < 0)
	{
		std::cout << "Error accepting connection" << std::endl;
		close(sockfd);
		return 1;
	}
	std::cout << "Client connected" << std::endl;

	// sending password
	if (send(newsockfd, password.c_str(), password.length(), 0) < 0)
	{
		std::cout << "Error sending password" << std::endl;
		close(sockfd);
		return 1;
	}
	std::cout << "Password sent" << std::endl;

	// recieving msg
	char buffer[1024];
	int n = recv(newsockfd, buffer, sizeof(buffer), 0);
	if (n < 0)
	{
		std::cout << "Error recieving message" << std::endl;
		close(sockfd);
		return 1;
	}
	std::cout << "Message recieved: " << buffer << std::endl;

	// sending OK if reciving is successfull
	if (send(newsockfd, "OK", 2, 0) < 0)
	{
		std::cout << "Error sending OK" << std::endl;
		close(sockfd);
		return 1;
	}
	std::cout << "OK sent" << std::endl;

	// closing socket
	close(sockfd);
	return 0;
}