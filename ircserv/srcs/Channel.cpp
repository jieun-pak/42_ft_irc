#include "../includes/Channel.hpp"
#include "../includes/Client.hpp"
#include <sys/socket.h> // For send()

const size_t Channel::MAX_MEMBERS = 10;

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

void Channel::broadcastMessage(const std::string &message, Client* sender)
{
	for (std::vector<Client*>::iterator it = _members.begin(); it != _members.end(); ++it)
	{
		if (*it != sender) // Don't send the message back to the sender
		{
			int fd = (*it)->getFd();
			send(fd, message.c_str(), message.length(), 0);
		}
	}
}

const std::string &Channel::getTopic() const
{
	return _topic;
}

void Channel::setTopic(const std::string &topic)
{
	_topic = topic;
}
