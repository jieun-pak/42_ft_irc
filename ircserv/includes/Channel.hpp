#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>

class Client;

class Channel
{
	private:
		std::string _name;
		std::vector<Client*> _members;
		std::string _topic;
		bool _inviteOnly;
		std::string _password;
		size_t _userLimit;
		std::vector<std::string> _bannedUsers;

		Channel();
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);

	public:
		Channel(const std::string &name);
		~Channel();

		static const size_t MAX_MEMBERS;

		// getters
		const std::string &getName() const;
		const std::vector<Client *> &getMembers() const;
		const std::string &getTopic() const;
		bool isInviteOnly() const;
		bool isPasswordProtected() const;
		const std::string &getPassword() const;
		bool isUserLimitReached() const;
		bool isMember(Client *client) const;
		bool isBanned(const std::string &user) const;

		// setters
		void addMember(Client *client);
		void removeMember(Client *client);
		void setTopic(const std::string &topic);
		void setInviteOnly(bool inviteOnly);
		void setPassword(const std::string &password);
		void setUserLimit(size_t userLimit);
		void addBannedUser(const std::string &user);
		void removeBannedUser(const std::string &user);


		void broadcastMessage(const std::string &message, Client *sender);
};

#endif // CHANNEL_HPP
