#include "../includes/Client.hpp"

// constructor of Message struct
Message::Message(const std::string &cmd, const std::vector<std::string> &params) : command(cmd), params(params) {}
const std::string &Message::getCommand() const
{
	return command;
}

// constructors
Client::Client(int fd) : _fd(fd), _passwordAuthenticated(false), _isUserReceived(false), _nickname(""), _username(""), _realname("") {}
Client::~Client() {}
Client::Client(const Client &other) : _fd(other._fd), _readBuf(other._readBuf), _passwordAuthenticated(other._passwordAuthenticated), _isUserReceived(other._isUserReceived), _nickname(other._nickname), _username(other._username), _realname(other._realname) {}
Client &Client::operator=(const Client &other)
{
	if (this != &other)
	{
		_fd = other._fd;
		_readBuf = other._readBuf;
		_passwordAuthenticated = other._passwordAuthenticated;
		_isUserReceived = other._isUserReceived;
		_nickname = other._nickname;
		_username = other._username;
		_realname = other._realname;
	}
	return *this;
}

// getters
int Client::getFd() const
{
	return _fd;
}

#include "../includes/Server.hpp"
// methods
void Client::appendToReadBuf(const std::string &data, size_t len)
{
	_readBuf += data.substr(0, len);
}

std::vector<std::string> Client::extractLines()
{
	std::vector<std::string> lines;
	std::string::size_type pos;

	while ((pos = _readBuf.find('\n')) != std::string::npos)
	{
		std::string line = _readBuf.substr(0, pos);

		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		lines.push_back(line);
		_readBuf.erase(0, pos + 1);
	}
	return lines;
}

bool Client::isPasswordAuthenticated() const
{
	return _passwordAuthenticated;
}

void Client::setPasswordAuthenticated(bool authenticated)
{
	_passwordAuthenticated = authenticated;
}

bool	Client::isValidNickname(const std::string &nickname)
{
	// Check if the nickname is empty
	if (nickname.empty())
		return false;

	// Check if the nickname starts with a letter or special character
	if (!std::isalpha(nickname[0]) && !std::ispunct(nickname[0]))
		return false;

	// Check if the nickname contains only valid characters (letters, digits, and special characters)
	for (size_t i = 1; i < nickname.size(); ++i)
	{
		if (!std::isalnum(nickname[i]) && !std::ispunct(nickname[i]))
			return false;
	}

	return true;
}

// nickname getter and setter
const std::string Client::getNickname() const
{
	return _nickname;
}
void Client::setNickname(const std::string &nickname)
{
	_nickname = nickname;
}
bool Client::isNicknameInUse(const std::string &nickname)
{
	// Check if the nickname is already in use by any client
	for (std::map<int, Client *>::iterator it = Server::_clients.begin(); it != Server::_clients.end(); ++it)
	{
		if (it->second && it->second->getFd() != _fd) // Exclude the current client
		{
			if (it->second->getNickname() == nickname)
				return true;
		}
	}
	return false;
}

// username setter
void Client::setUsername(const std::string &username)
{
	_username = username;
}

// realname setter
void Client::setRealname(const std::string &realname)
{
	_realname = realname;
}

// realname getter and setter
bool Client::isUserReceived() const
{
	return _isUserReceived;
}
void Client::setUserReceived(bool received)
{
	_isUserReceived = received;
}