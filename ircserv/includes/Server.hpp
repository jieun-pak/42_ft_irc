#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <sys/poll.h>
#include "Client.hpp"
#include <arpa/inet.h>

//TODO: jin check orthodox canonical form
class Server
{
	private:
		Server();
		int _sockfd;
		int _port;
		std::string _password;
		// struct sockaddr_in _serv_addr;
		std::vector<Client> _clients;

	public:
		Server(int port, std::string password);
		~Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		void	initSocket();
		void	bindSocket();
		void	listenSocket();
		void	acceptConnection();

		void	run();

};

#endif