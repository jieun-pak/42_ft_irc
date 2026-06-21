#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>

// To save the msg coming from client.
struct Message
{
    std::string command;
    std::vector<std::string> params;
	
	// constructor
	Message(const std::string &cmd, const std::vector<std::string> &params);

	// getters and setters
	const std::string &getCommand() const;

};

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
		void						appendToReadBuf(const std::string &data, size_t len);
		std::vector<std::string>	extractLines();
		
};

#endif