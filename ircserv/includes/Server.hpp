#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <sys/poll.h>
#include <fcntl.h>
#include "Client.hpp"
#include <arpa/inet.h>

class Channel;

class Server
{
	private:
		Server();
		int _sockfd;
		int _port;
		std::string _password;
		std::vector<struct pollfd> _pfds;
		std::map<int, Client*> _clients;
		std::map<std::string, Channel*> _channels;

	public:
		Server(int port, const std::string &password);
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