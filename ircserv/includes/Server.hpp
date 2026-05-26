#ifndef SERVER_HPP
#define SERVER_HPP

#include "ft_irc.hpp"

class Server
{
	private:
		int _sockfd;
		int _port;
		std::string _password;
		// struct sockaddr_in _serv_addr;

	public:
		Server();
		Server(int port, std::string password) : _port(port), _password(password);
		~Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		void	initSocket();
		void	bindSocket();
		void	listenSocket();
		void	acceptConnection();
		void	sendPassword();

		void	run();

};

#endif