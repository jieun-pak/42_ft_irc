#include "../includes/Client.hpp"

// constructor of Message struct
Message::Message(const std::string &cmd, const std::vector<std::string> &params) : command(cmd), params(params) {}
const std::string &Message::getCommand() const
{
	return command;
}


// constructors
Client::Client(int fd) : _fd(fd) {}
Client::~Client() {}
Client::Client(const Client &other) : _fd(other._fd), _readBuf(other._readBuf) {}
Client &Client::operator=(const Client &other)
{
	if (this != &other)
	{
		_fd = other._fd;
		_readBuf = other._readBuf;
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
	std::vector<std::string>		lines;
	std::string::size_type			pos;

	while ((pos = _readBuf.find("\r\n")) != std::string::npos)
	{
		lines.push_back(_readBuf.substr(0, pos));
		_readBuf.erase(0, pos + 2);
	}
	return lines;
}