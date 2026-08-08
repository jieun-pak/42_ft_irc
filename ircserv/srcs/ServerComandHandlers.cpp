#include "../includes/Server.hpp"
#include "../includes/Channel.hpp"
#include <algorithm>
#include <cstdlib> // For std::atoi

void Server::handlePass(const Message &msg, int clientFd)
{
	Client *client = getClient(clientFd);
	if (!client)
		return;
	// 0. Reject re-register
	if (client->isRegistered())
	{
		sendNumericReply(clientFd, ERR_ALREADYREGISTRED, replyTarget(client), "", "You may not reregister");
		return;
	}
	// 1. Check parameter count
	if (msg.getParams().size() < 1)
	{
		std::cerr << "Error: PASS command requires a password parameter." << std::endl;
		sendNumericReply(clientFd, ERR_NEEDMOREPARAMS, replyTarget(client), "PASS", "Not enough parameters");
		return;
	}
	// 2. Verify password
	if (msg.getParams()[0] != _password)
	{
		std::cerr << "Error: Incorrect password." << std::endl;
		sendNumericReply(clientFd, ERR_PASSWDMISMATCH, replyTarget(client), "", "Password incorrect");
		return;
	}
	// 3. Mark client as password-authenticated
	client->setPasswordAuthenticated(true);
	checkRegistrationComplete(clientFd);
}

// Registration is order-independent (PASS/NICK/USER can arrive in any order),
// so this is called after each of the three instead of only after USER.
// The isRegistered() guard makes it fire exactly once, which also stops a
// later re-send of PASS/USER from re-triggering the welcome message.
void Server::checkRegistrationComplete(int clientFd)
{
	Client *client = getClient(clientFd);
	if (!client || client->isRegistered())
		return;
	if (client->isPasswordAuthenticated() && !client->getNickname().empty() && client->isUserReceived())
	{
		client->setRegistered(true);
		sendNumericReply(clientFd, RPL_WELCOME, client->getNickname(), "",
			"Welcome to the IRC server, " + client->getNickname() + "!");
	}
}

void Server::handleNick(const Message &msg, int clientFd)
{
	Client *client = getClient(clientFd);
	if (!client)
		return;
	// 1. Check nickname exists
	if (msg.getParams().size() < 1)
	{
		std::cerr << "Error: NICK command requires a nickname parameter." << std::endl;
		sendNumericReply(clientFd, ERR_NEEDMOREPARAMS, replyTarget(client), "NICK", "Not enough parameters");
		return;
	}
	// 2. Check nickname format
	if (!isValidNickname(msg.getParams()[0]))
	{
		std::cerr << "Error: Invalid nickname format." << std::endl;
		// correct code here is 432 ERR_ERRONEUSNICKNAME — out of scope for this
		// pass (only the 7 codes on the Phase 2 checklist were implemented)
		return;
	}
	// 3. No-op guard: re-setting to the SAME nickname isn't a real change —
	// isNicknameInUse() excludes the client's own fd, so without this it would
	// pass as "not in use" and fall through to a spurious broadcast below
	if (msg.getParams()[0] == client->getNickname())
	{
		checkRegistrationComplete(clientFd);
		return;
	}
	// 4. Check nickname is not already used
	if (isNicknameInUse(msg.getParams()[0], clientFd))
	{
		std::cerr << "Error: Nickname already in use." << std::endl;
		sendNumericReply(clientFd, ERR_NICKNAMEINUSE, replyTarget(client), msg.getParams()[0], "Nickname is already in use");
		return;
	}
	// 5. Store nickname in Client object — capture the OLD nickname before
	// overwriting it (previously read after the write, so old==new always)
	std::string oldNickname = client->getNickname();
	std::string newNickname = msg.getParams()[0];
	client->setNickname(newNickname);

	// 6. Notify others only on a genuine change — a client's first NICK
	// (during registration, oldNickname empty) has no previous identity to
	// announce, so real IRC doesn't broadcast it
	if (!oldNickname.empty())
	{
		for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		{
			if (it->second && it->second->getFd() != clientFd) // Exclude the current client
			{
				std::string notification = ":" + oldNickname + " NICK " + newNickname + "\r\n";
				queueSend(it->second->getFd(), notification);
			}
		}
	}
	checkRegistrationComplete(clientFd);
}

void Server::handleUser(const Message &msg, int clientFd)
{
	Client *client = getClient(clientFd);
	if (!client)
		return;
	// 0. Reject if already registered (462) — was silently re-processed before,
	// silently overwriting username/realname and re-sending the welcome
	if (client->isRegistered())
	{
		sendNumericReply(clientFd, ERR_ALREADYREGISTRED, replyTarget(client), "", "You may not reregister");
		return;
	}
	// 1. Validate parameters
	if (msg.getParams().size() < 4)
	{
		std::cerr << "Error: USER command requires four parameters." << std::endl;
		std::cout << "Usage: USER <username> <hostname> <servername> <realname>" << std::endl;
		sendNumericReply(clientFd, ERR_NEEDMOREPARAMS, replyTarget(client), "USER", "Not enough parameters");
		return;
	}
	// 2. Store username/realname
	client->setUsername(msg.getParams()[0]);
	client->setRealname(msg.getParams()[3]);
	// 3. Mark USER as received (you can set a flag in the Client class if needed)
	client->setUserReceived(true);

	checkRegistrationComplete(clientFd);
}

// JOIN command halpers
void Server::sendTopic(Channel* channel, Client* client)
{
	if (channel->getTopic().empty())
		sendNumericReply(client->getFd(), RPL_NOTOPIC, replyTarget(client), channel->getName(), "No topic is set");
	else
		sendNumericReply(client->getFd(), RPL_TOPIC, replyTarget(client), channel->getName(), channel->getTopic());
}

void Server::sendNamesList(Channel* channel, Client* client)
{
    std::string namesList;

	sendNumericReply(client->getFd(), RPL_NAMREPLY, replyTarget(client), "= " + channel->getName(), namesList);
	sendNumericReply(client->getFd(), RPL_ENDOFNAMES, replyTarget(client), channel->getName(), "End of /NAMES list");
}

void Server::handleJoin(const Message &msg, int clientFd)
{
	Client *client = getClient(clientFd);
	if (!client)
	{
		std::cerr << "Error: Client not found for fd: " << clientFd << std::endl;
		return;
	}
	// 0. Validate parameters — registration itself is already guaranteed by
	// executeCommand()'s gating (451), so no auth check needed here anymore
	if (msg.getParams().size() < 1)
	{
		std::cerr << "Error: JOIN command requires a channel parameter." << std::endl;
		sendNumericReply(clientFd, ERR_NEEDMOREPARAMS, replyTarget(client), "JOIN", "Not enough parameters");
		return;
	}
	if (!isValidChannelName(msg.getParams()[0]))
	{
		// correct code here is 476 ERR_BADCHANMASK — out of scope for this pass
		std::cerr << "Error: Invalid channel name format." << std::endl;
		return;
	}
	// 1. Find or create channel
	std::string channelName = msg.getParams()[0];
	std::map<std::string, Channel *>::iterator it = _channels.find(channelName);
	Channel *channel = NULL;
	if (it == _channels.end())
	{
		// Channel does not exist, create it
		channel = new Channel(channelName);
		_channels[channelName] = channel;
	}
	else
	{
		channel = it->second;
	}

	// 3. Check channel restrictions
	if (channel->getMembers().end() != std::find(channel->getMembers().begin(), channel->getMembers().end(), client))
	{
		std::cerr << "Error: Client is already a member of channel " << channelName << std::endl;
		return;
	}
	if (channel->getMembers().size() >= Channel::MAX_MEMBERS) // Example: max 10 members
	{
		std::cerr << "Error: Channel " << channelName << " is full." << std::endl;
		return;
	}
	if (channel->isInviteOnly())
	{
		std::cerr << "Error: Channel " << channelName << " is invite-only." << std::endl;
		return;
	}
	if (channel->isPasswordProtected())
	{
		if (msg.getParams().size() < 2 || msg.getParams()[1] != channel->getPassword())
		{
			std::cerr << "Error: Incorrect or missing password for channel " << channelName << std::endl;
			return;
		}
	}
	if (channel->isUserLimitReached())
	{
		std::cerr << "Error: Channel " << channelName << " has reached its user limit." << std::endl;
		return;
	}
	if (channel->isBanned(client->getNickname()))
	{
		std::cerr << "Error: Client " << client->getNickname() << " is banned from channel " << channelName << std::endl;
		return;
	}
	// 3. Add client to channel
	channel->addMember(client);
	client->joinChannel(channelName);
	// 4. TODO: Broadcast JOIN message
	channel->broadcastMessage(":" + client->getNickname() + " JOIN " + channelName + "\r\n", client);
	// 5. TODO: Send topic and names list
	sendTopic(channel, client);
	sendNamesList(channel, client);
	// Debug print
	std::cout << "Client " << client->getNickname() << " joined channel " << channelName << std::endl;
}

void Server::handleMode(const Message &msg, int clientFd)
{
    // 1. Validate basic parameters
    if (msg.getParams().size() < 2)
    {
        std::cerr << "Error: MODE command requires at least two parameters."
                  << std::endl;
        return;
    }
	// if we have 3 parameters, the third one is used for +k and +l modes

    Client *client = getClient(clientFd);
    if (!client)
        return;

    // 2. Check channel exists
    if (!isChannelExists(msg.getParams()[0]))
    {
        std::cerr << "Error: Channel " << msg.getParams()[0]
                  << " does not exist." << std::endl;
        return;
    }

    Channel *channel = _channels[msg.getParams()[0]];

    // 3. Check client is in channel
    if (!isClientInChannel(client, channel->getName()))
    {
        std::cerr << "Error: Client is not in channel "
                  << channel->getName() << std::endl;
        return;
    }

    // 4. Validate mode string
    const std::string &mode = msg.getParams()[1];

    if (mode.size() < 2 || (mode[0] != '+' && mode[0] != '-'))
    {
        std::cerr << "Error: Invalid mode change format." << std::endl;
        return;
    }

    char action = mode[0];
    char modeChar = mode[1];

    // 5. Check operator privileges
    if (!channel->isOperator(client))
    {
        std::cerr << "Error: Client is not a channel operator."
                  << std::endl;
        return;
    }

    // 6. Apply mode change
    switch (modeChar)
    {
        case 'i':
            channel->setInviteOnly(action == '+');
            break;

        case 't':
            channel->setRestrictedTopic(action == '+');
            break;

        case 'k':
            if (action == '+')
            {
                if (msg.getParams().size() < 3)
                {
                    std::cerr << "Error: +k requires a password."
                              << std::endl;
                    return;
                }

                channel->setPassword(msg.getParams()[2]);
            }
            else
            {
                channel->setPassword("");
            }
            break;

        case 'l':
            if (action == '+')
            {
                if (msg.getParams().size() < 3)
                {
                    std::cerr << "Error: +l requires a user limit."
                              << std::endl;
                    return;
                }

                int limit = std::atoi(msg.getParams()[2].c_str());

                if (limit <= 0)
                {
                    std::cerr << "Error: Invalid user limit."
                              << std::endl;
                    return;
                }

                channel->setUserLimit(limit);
            }
            else
            {
                channel->setUserLimit(0);
            }
            break;

        case 'o':
            if (msg.getParams().size() < 3)
            {
                std::cerr << "Error: +o/-o requires a nickname."
                          << std::endl;
                return;
            }

            {
                Client *target = getClientByNickname(msg.getParams()[2]);

                if (!target)
                {
                    std::cerr << "Error: Client "
                              << msg.getParams()[2]
                              << " does not exist." << std::endl;
                    return;
                }

                if (action == '+')
                    channel->addOperator(target);
                else
                    channel->removeOperator(target);
            }
            break;

        default:
            std::cerr << "Error: Unsupported channel mode."
                      << std::endl;
            return;
    }

    // 7. Broadcast MODE message
    std::string modeMessage =
        ":" + client->getNickname()
        + " MODE " + channel->getName()
        + " " + mode;

    if (msg.getParams().size() > 2)
        modeMessage += " " + msg.getParams()[2];

    modeMessage += "\r\n";

    channel->broadcastMessage(modeMessage, client);
}


void Server::handlePart(const Message &msg, int clientFd)
{
	(void)msg;
	(void)clientFd;
	// 1. Check channel exists

	// 2. Check client is in channel
	// 3. Remove client from channel
	// 4. Broadcast PART message
}

void Server::handlePrivmsg(const Message &msg, int clientFd)
{
	(void)msg;
	(void)clientFd;
	// 1. Validate target and text
	// 2. Find target client/channel
	// 3. Forward message
}

void Server::handleQuit(const Message &msg, int clientFd)
{
	(void)msg;
	(void)clientFd;
	// 1. Notify all joined channels
	// 2. Remove client from channels
	// 3. Close socket
	// 4. Remove client from server
}
