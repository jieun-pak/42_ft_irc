#include "../includes/Server.hpp"

// constructor, destructor, copy constructor, assignment operator (Orthodox Canonical Form)
//TODO: habib check Q. do we need _client?
Server::Server(int port, const std::string &password) : _sockfd(-1), _port(port), _password(password) {}

Server::~Server() {
	// Close the server socket
	if (_sockfd != -1)
	{
		close(_sockfd);
	}

	// Clean up client connections
	for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		delete it->second;
	}
	_clients.clear();

	// Clean up channels
	// for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
	// {
	// 	delete it->second;
	// }
	// _channels.clear();
}

Server::Server(const Server &other) : _sockfd(other._sockfd), _port(other._port), _password(other._password), _pfds(other._pfds), _clients(other._clients), _channels(other._channels) {}

Server &Server::operator=(const Server &other)
{
	if (this != &other)
	{
		_sockfd = other._sockfd;
		_port = other._port;
		_password = other._password;
		_pfds = other._pfds;
		_clients = other._clients;
		_channels = other._channels;
	}
	return *this;
}

// getters and setters
Client* Server::getClient(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end())
		return it->second;
	return NULL;
}

void	Server::addClient(int fd, Client* client)
{
	_clients[fd] = client;
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
int Server::acceptConnection()
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

	std::cout << "New client connected: " << inet_ntoa(cli_addr.sin_addr) << "\n";
	std::cout << "With Port: " << ntohs(cli_addr.sin_port) << "\n";

	addClient(clientFd, new Client(clientFd));
	return clientFd;
}

void Server::receiveData(int clientFd)
{
	char buffer[512];
	Client* current_client = getClient(clientFd);
	ssize_t size = 0;

	while (true)
	{
		size = recv(clientFd, buffer, sizeof(buffer), 0);
		current_client->appendToReadBuf(buffer, size);
		/* note
		size ==0 : client closed connection (EOF)
		-> close that fd, clean up client's data

		size ==-1 : err occurred, in non-blocking mode, 
		if errno = EAGAIN or EWOULDBLOCK, means no data to read, not a fatal err
		*/
		if (size == 0)
		{
			// Handle client disconnection
			std::cout << "Client disconnected: " << clientFd << std::endl;
			close(clientFd);
			delete current_client;
			_clients.erase(clientFd);
			break;
		}
		else if (size < 0)
		{
			std::cerr << "Error receiving data from client " << clientFd << std::endl;
			break;
		}
		else
		{
			std::vector<std::string> lines = current_client->extractLines();
			for (size_t i = 0; i < lines.size(); i++)
			{
				std::cout << "Received from client " << clientFd << ": " << lines[i] << std::endl;
			}
		}
	}
}

void Server::eventLoop()
{
	// _pfds[0] = for server socket .. _pfds[1]
	struct pollfd serverPfd;
	serverPfd.fd = _sockfd;
	serverPfd.events = POLLIN;
	serverPfd.revents = 0;
	_pfds.push_back(serverPfd);

	while (true)
	{
		int ready = poll(&_pfds[0], _pfds.size(), -1);
		if (ready < 0)
		{
			std::cerr << "poll error" << std::endl;
			break;
		}
		//ver1
		for (size_t i = 0; i < _pfds.size(); i++)
		{
			if (_pfds[i].revents & POLLIN)
			{
				if (_pfds[i].fd == _sockfd)
				{
					int clientFd = acceptConnection();
					struct pollfd clientPfd;
					clientPfd.fd = clientFd;
					clientPfd.events = POLLIN;
					clientPfd.revents = 0;
					_pfds.push_back(clientPfd);
				}
				else
				{
					//call function handling reading data 
					receiveData(_pfds[i].fd);
				}
			}
		}
	}
}

void Server::run()
{
	initSocket();
	bindSocket();
	listenSocket();
	eventLoop();
}

// TODO: recv and parse IRC commands from clients, handle client disconnections, manage channels, etc.
