#include "../includes/Server.hpp"
#include "../includes/signal.hpp"

// constructor, destructor, copy constructor, assignment operator (Orthodox Canonical Form)
//TODO: habib check Q. do we need _client?
Server::Server(int port, const std::string &password) : _sockfd(-1), _port(port), _password(password) {}

Server::~Server() {
	// Close the server socket
	if (_sockfd != -1)
	{
		close(_sockfd);
	}

	// Clean up client connections (close each fd, then free the object)
	for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		close(it->first);
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

// copy constructor and assignment operator: private and unimplemented (D8)

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

void	Server::deleteClient(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end())
	{
		delete it->second;
		_clients.erase(it);
	}
}

void Server::removeClientFromPoll(int fd)
{
	for (std::vector<struct pollfd>::iterator it = _pfds.begin(); it != _pfds.end(); ++it)
	{
		if (it->fd == fd)
		{
			_pfds.erase(it);
			break;
		}
	}
}

// D5: close + free the client now, but defer the _pfds erase until after
// eventLoop's scan finishes — erasing mid-iteration shifts entries and
// skips events. eventLoop flushes _fdsToRemove at the end of each round.
void Server::disconnectClient(int fd)
{
	close(fd);
	deleteClient(fd);
	_fdsToRemove.push_back(fd);
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

	// SO_REUSEADDR (D7): allow bind() to reuse the port even while old
	// connections linger in TIME_WAIT (~60s after a restart with clients open)
	int opt = 1;
	if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		std::cerr << "Warning: setsockopt(SO_REUSEADDR) failed" << std::endl;

	// O_NONBLOCK (D3): subject requires non-blocking I/O; only this fcntl form is allowed
	if (fcntl(_sockfd, F_SETFL, O_NONBLOCK) < 0)
	{
		std::cerr << "Error setting O_NONBLOCK on server socket" << std::endl;
		exit(1);
	}
}

// Bind server socket to the specified port (e.g., using bind() system call)
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
		// D4: never exit() mid-run — a failed accept only affects this one
		// connection. EAGAIN = client vanished between poll() and accept().
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			std::cerr << "Error accepting connection" << std::endl;
		return -1;
	}

	// O_NONBLOCK (D3): accepted fds do not inherit the flag from the listener
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
	{
		std::cerr << "Error setting O_NONBLOCK on client socket" << std::endl;
		close(clientFd);
		return -1;
	}

	std::cout << "New client connected: " << inet_ntoa(cli_addr.sin_addr) << "\n";
	std::cout << "With Port: " << ntohs(cli_addr.sin_port) << "\n";

	addClient(clientFd, new Client(clientFd));
	return clientFd;
}

// Implementation for parsing IRC messages
Message Server::parse(const std::string& line)
{
	std::string command;
	std::vector<std::string> params;

	size_t pos = line.find(' ');
	if (pos != std::string::npos)
	{
		command = line.substr(0, pos);
		std::string paramStr = line.substr(pos + 1);
		size_t start = 0;
		while ((pos = paramStr.find(' ', start)) != std::string::npos)
		{
			params.push_back(paramStr.substr(start, pos - start));
			start = pos + 1;
		}
		if (start < paramStr.length())
			params.push_back(paramStr.substr(start));
	}
	else
	{
		command = line; // No parameters, entire line is command
	}

	return Message(command, params);
}

Server::CommandType Server::getCommandType(const std::string& command)
{
	if (command == "PASS")
		return CMD_PASS;
	if (command == "NICK")
		return CMD_NICK;
	if (command == "USER")
		return CMD_USER;
	if (command == "JOIN")
		return CMD_JOIN;
	if (command == "PART")
		return CMD_PART;
	if (command == "PRIVMSG")
		return CMD_PRIVMSG;
	if (command == "QUIT")
		return CMD_QUIT;
	return CMD_UNKNOWN;
}

bool Server::isValidNickname(const std::string &nickname)
{
	if (nickname.empty())
		return false;

	if (!std::isalpha(nickname[0]) && !std::ispunct(nickname[0]))
		return false;

	for (size_t i = 1; i < nickname.size(); ++i)
	{
		if (!std::isalnum(nickname[i]) && !std::ispunct(nickname[i]))
			return false;
	}

	return true;
}

bool Server::isNicknameInUse(const std::string &nickname, int excludeFd)
{
	for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second && it->first != excludeFd && it->second->getNickname() == nickname)
			return true;
	}
	return false;
}

bool Server::isValidChannelName(const std::string &channelName)
{
	if (channelName.empty())
		return false;

	if (channelName[0] != '#')
		return false;

	for (size_t i = 1; i < channelName.size(); ++i)
	{
		if (!std::isalnum(channelName[i]) && channelName[i] != '_')
			return false;
	}

	return true;
}

// Command handlers are in a separate file (ServerCommandHandlers.cpp) to keep the Server class clean and focused on its core responsibilities.

void	Server::executeCommand(const Message& msg, int clientFd)
{

	switch (getCommandType(msg.getCommand()))
	{
	case CMD_PASS:
		handlePass(msg, clientFd);
		break;
	case CMD_NICK:
		handleNick(msg, clientFd);
		break;
	case CMD_USER:
		handleUser(msg, clientFd);
		break;
	case CMD_JOIN:
		handleJoin(msg, clientFd);
		break;
	case CMD_PART:
		handlePart(msg, clientFd);
		break;
	case CMD_PRIVMSG:
		handlePrivmsg(msg, clientFd);
		break;
	case CMD_QUIT:
		handleQuit(msg, clientFd);
		break;
	
	default:
		break;
	}
}

void Server::receiveData(int clientFd)
{
	char buffer[512];
	Client* current_client = getClient(clientFd);
	if (!current_client)
	{
		std::cerr << "Client not found for fd: " << clientFd << std::endl;
		return;
	}
	
	ssize_t size = recv(clientFd, buffer, sizeof(buffer), 0);

	if (size > 0)
	{
		std::cout << "Received data from client fd " << clientFd << ": " << std::string(buffer, size) << std::endl;
		current_client->appendToReadBuf(buffer, size);

		std::vector<std::string> lines = current_client->extractLines();
		for (size_t i = 0; i < lines.size(); ++i)
		{
			std::cout << "Processing line from client fd " << clientFd << ": " << lines[i] << std::endl;
			Message msg = parse(lines[i]);
			executeCommand(msg, clientFd);
		}
	}
	else if (size == 0)
	{
		std::cout << "Client disconnected: " << clientFd << std::endl;
		disconnectClient(clientFd);
	}
	else
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		std::cerr << "Error receiving data from client " << clientFd << std::endl;
		disconnectClient(clientFd);
	}
}

// D2: find the client's pollfd and add/remove the POLLOUT bit
void Server::setPollOut(int fd, bool enable)
{
	for (std::vector<struct pollfd>::iterator it = _pfds.begin(); it != _pfds.end(); ++it)
	{
		if (it->fd == fd)
		{
			if (enable)
				it->events |= POLLOUT;
			else
				it->events &= ~POLLOUT;
			return;
		}
	}
}

// D2: the only way handlers send anything — append to the client's out-buffer
// and arm POLLOUT; the actual send() happens in sendData() once poll() says
// the socket is writable
void Server::queueSend(int clientFd, const std::string &msg)
{
	Client* client = getClient(clientFd);
	if (!client)
		return;
	client->appendToWriteBuf(msg);
	setPollOut(clientFd, true);
}

// D2: one send() per POLLOUT event, mirror of receiveData().
// Partial sends are fine: the unsent tail stays in _writeBuf and POLLOUT
// stays armed, so poll() brings us back here until it's drained.
void Server::sendData(int clientFd)
{
	Client* client = getClient(clientFd);
	if (!client || !client->hasPendingWrite())
	{
		setPollOut(clientFd, false);
		return;
	}

	const std::string &buf = client->getWriteBuf();
	ssize_t sent = send(clientFd, buf.c_str(), buf.size(), 0);

	if (sent > 0)
	{
		client->consumeWriteBuf(sent);
		if (!client->hasPendingWrite())
			setPollOut(clientFd, false); // drained — disarm or poll() spins
	}
	else if (sent < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return; // kernel buffer full — keep POLLOUT, retry next poll round
		std::cerr << "Error sending data to client " << clientFd << std::endl;
		disconnectClient(clientFd);
	}
}

void Server::eventLoop()
{
	// _pfds[0] = for server socket
	struct pollfd serverPfd;
	serverPfd.fd = _sockfd;
	serverPfd.events = POLLIN;
	serverPfd.revents = 0;
	_pfds.push_back(serverPfd);

	// D6: loop until the signal handler flips the flag (Ctrl+C / Ctrl+\)
	while (!g_serverShutdown)
	{
		int readyCount = poll(&_pfds[0], _pfds.size(), -1);
		if (readyCount < 0)
		{
			// D6: a signal interrupting poll() is not an error — loop around
			// so the while condition re-checks the shutdown flag
			if (errno == EINTR)
				continue;
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
					// saving client sockets from _pfds[1...n]
					int clientFd = acceptConnection();
					if (clientFd < 0)
						continue; // accept failed — nothing to register
					struct pollfd clientPfd;
					clientPfd.fd = clientFd;
					clientPfd.events = POLLIN;
					clientPfd.revents = 0;
					_pfds.push_back(clientPfd);
				}
				else
				{
					receiveData(_pfds[i].fd);
				}
			}
			// D4: hangup/error with no readable data — drop the client.
			// (when POLLIN is also set, receiveData() handles it via recv()
			// returning 0/-1 instead, so final bytes are not lost)
			else if (_pfds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
			{
				if (_pfds[i].fd != _sockfd)
				{
					std::cout << "Client disconnected (hangup): " << _pfds[i].fd << std::endl;
					disconnectClient(_pfds[i].fd);
				}
			}
			// D2: socket writable and this client has queued outgoing data
			if (_pfds[i].revents & POLLOUT)
				sendData(_pfds[i].fd);
		}
		// D5: now that the scan is over, erasing from _pfds is safe
		for (size_t k = 0; k < _fdsToRemove.size(); ++k)
			removeClientFromPoll(_fdsToRemove[k]);
		_fdsToRemove.clear();
	}
	std::cout << "Server shutting down" << std::endl;
}

void Server::run()
{
	initSocket();
	bindSocket();
	listenSocket();
	eventLoop();
}
