#include "../includes/Channel.hpp"

Channel::Channel(const std::string &name) : _name(name) {}
Channel::~Channel() {}

const std::vector<Client*> &Channel::getMembers() const
{
	return _members;
}

const std::string &Channel::getName() const
{
	return _name;
}

void Channel::addMember(Client* client)
{
	_members.push_back(client);
}

void Channel::removeMember(Client* client)
{
	for (std::vector<Client*>::iterator it = _members.begin(); it != _members.end(); ++it)
	{
		if (*it == client)
		{
			_members.erase(it);
			break;
		}
	}
}
