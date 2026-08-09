#include "../includes/Client.hpp"

// constructor of Message struct
Message::Message(const std::string &cmd, const std::vector<std::string> &params) : command(cmd), params(params) {}
const std::string &Message::getCommand() const
{
	return command;
}
const std::vector<std::string> &Message::getParams() const
{
	return params;
}

// constructors
Client::Client(int fd) : _fd(fd), _passwordAuthenticated(false), _nickname(""), _username(""), _realname(""), _isUserReceived(false), _registered(false) {}
Client::~Client() {}
Client::Client(const Client &other) : _fd(other._fd), _readBuf(other._readBuf), _writeBuf(other._writeBuf), _passwordAuthenticated(other._passwordAuthenticated), _nickname(other._nickname), _username(other._username), _realname(other._realname), _isUserReceived(other._isUserReceived), _registered(other._registered) {}
Client &Client::operator=(const Client &other)
{
	if (this != &other)
	{
		_fd = other._fd;
		_readBuf = other._readBuf;
		_writeBuf = other._writeBuf;
		_passwordAuthenticated = other._passwordAuthenticated;
		_isUserReceived = other._isUserReceived;
		_registered = other._registered;
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

// outgoing buffer (D2)
void Client::appendToWriteBuf(const std::string &data)
{
	_writeBuf += data;
}

bool Client::hasPendingWrite() const
{
	return !_writeBuf.empty();
}

const std::string &Client::getWriteBuf() const
{
	return _writeBuf;
}

void Client::consumeWriteBuf(size_t len)
{
	_writeBuf.erase(0, len);
}

bool Client::isPasswordAuthenticated() const
{
	return _passwordAuthenticated;
}

void Client::setPasswordAuthenticated(bool authenticated)
{
	_passwordAuthenticated = authenticated;
}

// nickname getter and setter
std::string Client::getNickname() const
{
	return _nickname;
}
void Client::setNickname(const std::string &nickname)
{
	_nickname = nickname;
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

bool Client::isRegistered() const
{
	return _registered;
}
void Client::setRegistered(bool registered)
{
	_registered = registered;
}

void Client::joinChannel(const std::string &channelName)
{
	_joinedChannels.push_back(channelName);
}

const std::vector<std::string> &Client::getJoinedChannels() const
{
	return _joinedChannels;
}
