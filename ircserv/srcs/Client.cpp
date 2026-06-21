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
