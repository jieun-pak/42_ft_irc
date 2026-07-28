#include "../includes/Channel.hpp"
#include "../includes/Client.hpp"
#include <sys/socket.h> // For send()

const size_t Channel::MAX_MEMBERS = 10;

// Private constructor to prevent default construction
Channel::Channel() : _name(""), _inviteOnly(false) {}
Channel::Channel(const Channel &other) : _name(other._name), _members(other._members), _topic(other._topic), _inviteOnly(other._inviteOnly) {}
Channel &Channel::operator=(const Channel &other)
{
	if (this != &other)
	{
		_name = other._name;
		_members = other._members;
		_topic = other._topic;
		_inviteOnly = other._inviteOnly;
	}
	return *this;
}

// Public constructor
Channel::Channel(const std::string &name) : _name(name) {}
Channel::~Channel() {}

// Getters
const std::vector<Client *> &Channel::getMembers() const
{
	return _members;
}

const std::string &Channel::getName() const
{
	return _name;
}

const std::string &Channel::getTopic() const
{
	return _topic;
}

bool Channel::isInviteOnly() const
{
	return _inviteOnly;
}

bool Channel::isMember(Client *client) const
{
	for (std::vector<Client *>::const_iterator it = _members.begin(); it != _members.end(); ++it)
	{
		if (*it == client)
			return true;
	}
	return false;
}

bool Channel::isPasswordProtected() const
{
	return !_password.empty();
}

const std::string &Channel::getPassword() const
{
	return _password;
}

bool Channel::isUserLimitReached() const
{
	return _userLimit > 0 && _members.size() >= _userLimit;
}

bool Channel::isBanned(const std::string &user) const
{
	for (std::vector<std::string>::const_iterator it = _bannedUsers.begin(); it != _bannedUsers.end(); ++it)
	{
		if (*it == user)
			return true;
	}
	return false;
}

// setters
void Channel::addMember(Client *client)
{
	_members.push_back(client);
}

void Channel::removeMember(Client *client)
{
	for (std::vector<Client *>::iterator it = _members.begin(); it != _members.end(); ++it)
	{
		if (*it == client)
		{
			_members.erase(it);
			break;
		}
	}
}

void Channel::setInviteOnly(bool inviteOnly)
{
	_inviteOnly = inviteOnly;
}

void Channel::setTopic(const std::string &topic)
{
	_topic = topic;
}

// Broadcast a message to all members of the channel except the sender
void Channel::broadcastMessage(const std::string &message, Client *sender)
{
	for (std::vector<Client *>::iterator it = _members.begin(); it != _members.end(); ++it)
	{
		if (*it != sender) // Don't send the message back to the sender
		{
			int fd = (*it)->getFd();
			send(fd, message.c_str(), message.length(), 0);
		}
	}
}

