#include "../includes/Server.hpp"

// constructor, destructor, copy constructor, assignment operator (Orthodox Canonical Form)
//TODO: habib check Q. do we need _client?
Server::Server(int port, std::string password) : _sockfd(-1), _port(port), _password(password) {}

Server::~Server() {}

Server::Server(const Server &other) : _sockfd(other._sockfd), _port(other._port), _password(other._password), _clients(other._clients) {}

Server &Server::operator=(const Server &other)
{
	if (this != &other)
	{
		_sockfd = other._sockfd;
		_port = other._port;
		_password = other._password;
		_clients = other._clients;
	}
	return *this;
}

// Socket operations

// Initialize socket here (e.g., using socket() system call)
void Server::initSocket()
{
	_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (_sockfd < 0)
	{
		std::cerr << "Error creating socket" << std::endl;
		exit(1);
	}
}

// Bind socket to the specified port (e.g., using bind() system call)
void Server::bindSocket()
{
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(_port);

	if (bind(_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		std::cerr << "Error binding socket" << std::endl;
		exit(1);
	}
}

// Listen for incoming connections (e.g., using listen() system call)
void Server::listenSocket()
{
	if (listen(_sockfd, 5) < 0)
	{
		std::cerr << "Error listening" << std::endl;
		exit(1);
	}
}

// Accept incoming connections (e.g., using accept() system call)
void Server::acceptConnection()
{
	// sockaddr_in : for saving IP, port
	struct sockaddr_in cli_addr;

	socklen_t cli_len = sizeof(cli_addr);
	int clientFd = accept(_sockfd, (struct sockaddr *)&cli_addr, &cli_len);
	if (clientFd < 0)
	{
		std::cerr << "Error accepting connection" << std::endl;
		exit(1);
	}

	std::cout << inet_ntoa(cli_addr.sin_addr) << "\n";
		std::cout << ntohs(cli_addr.sin_port) << "\n";

	_clients.push_back(Client(clientFd)); //add from _clients[0]
}

void Server::run()
{
	initSocket();
	bindSocket();
	listenSocket();	// fds[0] = sever.fd, server.events, server.revents
	// fds[1] = clients1.fd
	// fds[2] = clients2.fd
	std::vector<struct pollfd> fds;
	struct pollfd serverPfd;
	serverPfd.fd = _sockfd;			// fd to watch
	serverPfd.events = POLLIN;		// watch for POLLIN = data ready to read
	serverPfd.revents = 0;			// output
	fds.push_back(serverPfd);		// fds[0]  = serverpfd.fd, .events, .revents

	while (true)
	{
		/*call poll()
			- timeout: -1 = wait forever
			- poll() makes fds.size() grows as clients join
			- after poll() returns, revents is set by OS, then you zero it before poll()
			- return value: -1 = err, 0 = timeout, + = number of elements (revents > 0)
		*/
		int ready = poll(&fds[0], fds.size(), -1);
		if (ready < 0)
		{
			std::cerr << "poll error" << std::endl;
				std::cout << "hi A";

			break;
		}
		if (fds[0].revents & POLLIN)				
		{
			acceptConnection();
			struct pollfd clientPfd;
			clientPfd.fd = _clients.back().getFd();  //_clients[0] =... _client[1] = 
			clientPfd.events = POLLIN;
			clientPfd.revents = 0;
			fds.push_back(clientPfd); // fds[1]. [2]. ...
		}

		// TODO: recv and parse IRC commands
		if () {
			
		}
		
		
	}
}
