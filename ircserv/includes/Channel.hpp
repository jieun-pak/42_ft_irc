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

		Channel();
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);

	public:
		Channel(const std::string &name);
		~Channel();

		static const size_t MAX_MEMBERS;

		const std::string &getName() const;
		const std::vector<Client*> &getMembers() const;
		const std::string &getTopic() const;

		void addMember(Client* client);
		void removeMember(Client* client);
		void setTopic(const std::string &topic);

		void broadcastMessage(const std::string &message, Client* sender);
};

#endif // CHANNEL_HPP
