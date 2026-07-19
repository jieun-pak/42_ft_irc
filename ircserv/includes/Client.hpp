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
	const std::vector<std::string> &getParams() const;

};

class Client
{
	private:
		int				_fd;
		std::string		_readBuf;
		bool			_passwordAuthenticated;
		std::string		_nickname;
		std::string		_username;
		std::string		_realname;
		bool			_isUserReceived;
		std::vector<std::string>	_joinedChannels;

	public:
		Client(int fd);
		~Client();
		Client(const Client &other);
		Client &operator=(const Client &other);

		int							getFd() const;
		void						appendToReadBuf(const std::string &data, size_t len);
		std::vector<std::string>	extractLines();

		bool						isPasswordAuthenticated() const;
		void						setPasswordAuthenticated(bool authenticated);

		std::string					getNickname() const;
		void						setNickname(const std::string &nickname);

		//std::string				getUsername() const;
		void						setUsername(const std::string &username);

		//std::string				getRealname() const;
		void						setRealname(const std::string &realname);

		bool						isUserReceived() const;
		void						setUserReceived(bool received);

		void						joinChannel(const std::string &channelName);
};

#endif