#include "ft_irc.hpp"

class Server
{
	private:
		int _sockfd;
		int _port;
		std::string _password;
		// struct sockaddr_in _serv_addr;
	
	public:
		Server(int port, std::string password) : _port(port), _password(password);
		~Server();

		void	initSocket();
		void	bindSocket();
		void	listenSocket();
		void	acceptConnection();
		void	sendPassword();

		void	run();

};
