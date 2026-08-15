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
#include "Replies.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cctype>

class Channel;

class Server
{
	private:
		Server();
		// D8: a Server owns raw Client* pointers — copying would double-delete
		// them. Declared private and left unimplemented (C++98 non-copyable idiom).
		Server(const Server &other);
		Server &operator=(const Server &other);

		int _sockfd;
		int _port;

		std::string _password;
		std::vector<struct pollfd> _pfds;
		std::map<int, Client*> _clients;
		std::map<std::string, Channel*> _channels;
		// D5: fds whose _pfds entry must be erased after eventLoop's scan —
		// erasing mid-iteration shifts the vector and skips events
		std::vector<int> _fdsToRemove;

		// Helper functions
		Message		parse(const std::string& line);

		// CommandType enum and getCommandType function
		enum CommandType
    	{
    	    CMD_UNKNOWN,
    	    CMD_PASS,
    	    CMD_NICK,
    	    CMD_USER,
    	    CMD_JOIN,
			CMD_MODE,
    	    CMD_PART,
    	    CMD_PRIVMSG,
    	    CMD_QUIT,
    	    CMD_PING,
    	    CMD_TOPIC
    	};

    	CommandType getCommandType(const std::string& command);

		bool	isValidNickname(const std::string &nickname);
		bool	isNicknameInUse(const std::string &nickname, int excludeFd);
		// registration is order-independent: PASS/NICK/USER can arrive in any order,
		// so completion is checked after each one, with a one-time guard on Client
		void	checkRegistrationComplete(int clientFd);
		bool	isValidChannelName(const std::string &channelName);
		bool	isChannelExists(const std::string &channelName);
		bool	isClientInChannel(Client *client, const std::string &channelName);

		// D2 send path: POLLOUT is set on a client's pollfd iff its _writeBuf
		// is non-empty (otherwise poll() would spin — sockets are almost
		// always writable)
		void	setPollOut(int fd, bool enable);
		void	queueSend(int clientFd, const std::string &msg);

		// Channel has no Server* (and no access to queueSend/POLLOUT), so
		// broadcasting to a channel's members lives here instead of on
		// Channel — this is the only place a message reaches multiple
		// clients that still goes through the D2 send path.
		void	broadcastToChannel(Channel *channel, const std::string &message, Client *sender);

		// numeric replies (RPL_*/ERR_*, see Replies.hpp): the one place that
		// formats ":<server> <code> <target>[ <middleParam>] :<trailing>\r\n"
		void	sendNumericReply(int clientFd, int code, const std::string &target, const std::string &middleParam, const std::string &trailing);
		std::string	replyTarget(Client *client);

		// command handlers
		void	handlePass(const Message& msg, int clientFd);
		void	handleNick(const Message& msg, int clientFd);
		void	handleUser(const Message& msg, int clientFd);
		void	handleJoin(const Message& msg, int clientFd);
		void	handleMode(const Message& msg, int clientFd);
		void	handlePart(const Message& msg, int clientFd);
		void	handlePrivmsg(const Message& msg, int clientFd);
		void	handleQuit(const Message& msg, int clientFd);
		void	handlePing(const Message& msg, int clientFd);
		void	handleTopic(const Message& msg, int clientFd);

		// JOIN command helpers
		void	sendTopic(Channel* channel, Client* client);
		void	sendNamesList(Channel* channel, Client* client);

	public:
		Server(int port, const std::string &password);
		~Server();

		void	initSocket();
		void	bindSocket();
		void	listenSocket();
		int		acceptConnection();
		void	eventLoop();
		void	receiveData(int clientFd);
		void	sendData(int clientFd);
		
		void run();
		
		// getters
		Client* getClient(int fd);
		Client* getClientByNickname(const std::string &nickname);

		// setters
		void	addClient(int fd, Client* client);
		void	deleteClient(int fd);
		void	removeClientFromPoll(int fd);
		void	disconnectClient(int fd);

		void 	executeCommand(const Message& msg, int clientFd);
};

#endif