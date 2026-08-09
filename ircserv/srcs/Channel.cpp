#include "../includes/Channel.hpp"
#include "../includes/Client.hpp"
#include "../includes/Replies.hpp"
#include <iostream>    // For std::cerr
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
Channel::Channel(const std::string &name) : _name(name), _inviteOnly(false), _userLimit(0), isRestrictedTopic(false) {}
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

bool Channel::isOperator(Client *client) const
{
	for (std::vector<Client *>::const_iterator it = _operators.begin(); it != _operators.end(); ++it)
	{
		if (*it == client)
			return true;
	}
	return false;
}

// setters
// Channel has no Server* to call sendNumericReply() through, so the real
// client-facing 443/471 replies are sent by Server::handleJoin (which does
// have that access) before addMember() is normally reached — these checks
// are the authoritative ones, though; handleJoin's pre-checks exist only to
// pick the right numeric to send, not to duplicate this method's logic. The
// bool return means a caller can never mistake a silent no-op for success.
bool Channel::addMember(Client *client)
{
	if (isMember(client))
	{
		std::cerr << "Error: Client is already a member of channel " << _name << std::endl;
		return false;
	}

	if (isUserLimitReached())
	{
		std::cerr << "Error: Channel " << _name << " is full." << std::endl;
		return false;
	}

	_members.push_back(client);
	return true;
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

void Channel::setPassword(const std::string &password)
{
	_password = password;
}

void Channel::setUserLimit(size_t userLimit)
{
	_userLimit = userLimit;
}

void Channel::addBannedUser(const std::string &user)
{
	_bannedUsers.push_back(user);
}

void Channel::removeBannedUser(const std::string &user)
{
	for (std::vector<std::string>::iterator it = _bannedUsers.begin(); it != _bannedUsers.end(); ++it)
	{
		if (*it == user)
		{
			_bannedUsers.erase(it);
			break;
		}
	}
}


void Channel::setRestrictedTopic(bool restricted)
{
	// This function can be used to set a flag for restricted topic changes
	isRestrictedTopic = restricted;
}

void Channel::addOperator(Client *client)
{
	if (!isOperator(client))
	{
		_operators.push_back(client);
	}
}

void Channel::removeOperator(Client *client)
{
	if (isOperator(client))
	{
		for (std::vector<Client *>::iterator it = _operators.begin(); it != _operators.end(); ++it)
		{
			if (*it == client)
			{
				_operators.erase(it);
				break;
			}
		}
	}
}
