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

		Channel();
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);

	public:
		Channel(const std::string &name);
		~Channel();

		const std::string &getName() const;
		const std::vector<Client*> &getMembers() const;

		void addMember(Client* client);
		void removeMember(Client* client);
};

#endif // CHANNEL_HPP