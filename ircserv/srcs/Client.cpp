#include "../includes/Client.hpp"

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

int Client::getFd() const
{
	return _fd;
}

void Client::appendToReadBuf(const std::string &data)
{
	_readBuf += data;
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