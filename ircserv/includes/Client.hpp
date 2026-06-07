#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>

class Client
{
	private:
		int			_fd;
		std::string	_readBuf;

	public:
		Client(int fd);
		~Client();
		Client(const Client &other);
		Client &operator=(const Client &other);

		int							getFd() const;
		void						appendToReadBuf(const std::string &data);
		std::vector<std::string>	extractLines();
};

#endif