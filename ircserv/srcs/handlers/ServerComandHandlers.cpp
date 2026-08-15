#include "../../includes/Server.hpp"
#include "../../includes/Channel.hpp"
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
		sendNumericReply(clientFd, ERR_ERRONEUSENICKNAME, replyTarget(client),  msg.getParams()[0], "Invalid nickname format");
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


/*
	I moved JOIN handling to its own file.
*/


// I moved TOPIC handling to its own file.



void Server::handleTopic(const Message &msg, int clientFd)
{
	Client *client = getClient(clientFd);
	if (!client)
		return;

	// 1. Validate parameters
	if (msg.getParams().size() < 1)
	{
		sendNumericReply(clientFd, ERR_NEEDMOREPARAMS, replyTarget(client), "TOPIC", "Not enough parameters");
		return;
	}

	const std::string &channelName = msg.getParams()[0];

	// 2. Check channel exists
	if (!isChannelExists(channelName))
	{
		sendNumericReply(clientFd, ERR_NOSUCHCHANNEL, replyTarget(client), channelName, "No such channel");
		return;
	}

	Channel *channel = _channels[channelName];

	// 3. Check client is in channel
	if (!channel->isMember(client))
	{
		sendNumericReply(clientFd, ERR_NOTONCHANNEL, replyTarget(client), channelName, "You're not on that channel");
		return;
	}

	// 4. No topic param: query mode — report the current topic
	if (msg.getParams().size() < 2)
	{
		sendTopic(channel, client);
		return;
	}

	// 5. Set mode: +t restricts topic changes to channel operators
	if (channel->isTopicRestricted() && !channel->isOperator(client))
	{
		sendNumericReply(clientFd, ERR_CHANOPRIVSNEEDED, replyTarget(client), channelName, "You're not channel operator");
		return;
	}

	// 6. Apply and broadcast (including back to the setter, per RFC)
	const std::string &newTopic = msg.getParams()[1];
	channel->setTopic(newTopic);

	std::string topicMsg = ":" + client->getNickname() + " TOPIC " + channelName + " :" + newTopic + "\r\n";
	queueSend(clientFd, topicMsg);
	broadcastToChannel(channel, topicMsg, client);
}

void Server::handlePart(const Message &msg, int clientFd)
{
    Client *client = getClient(clientFd);
    if (!client)
        return;

    // 0. Validate parameters
    if (msg.getParams().size() < 1)
    {
        sendNumericReply(clientFd, ERR_NEEDMOREPARAMS, replyTarget(client), "PART", "Not enough parameters");
        return;
    }

    // 1. Check channel exists
    const std::string &channelName = msg.getParams()[0];
    if (!isChannelExists(channelName))
    {
        sendNumericReply(clientFd, ERR_NOSUCHCHANNEL, replyTarget(client), channelName, "No such channel");
        return;
    }

	// 2. Check client is in channel
    Channel *channel = _channels[channelName];
    if (!channel->isMember(client))
    {
        sendNumericReply(clientFd, ERR_NOTONCHANNEL, replyTarget(client), channelName, "You're not on that channel");
        return;
    }

	// 3. Remove client from channel
    client->leaveChannel(channelName);
    channel->removeMember(client);

    // to remove the channel if it is empty after the client leaves
    if (channel->getMembers().empty())
    {
        _channels.erase(channelName);
        delete channel;
    }

	// 4. Broadcast PART message
    std::string partMsg = ":" + client->getNickname() + " PART " + channelName + "\r\n";
    broadcastToChannel(channel, partMsg, client);
}

void Server::handlePrivmsg(const Message &msg, int clientFd)
{
    // 0. Get sender
    Client *client = getClient(clientFd);
    if (!client)
        return;

    // 1. Validate parameters
    if (msg.getParams().size() < 2)
    {
        sendNumericReply(
            clientFd,
            ERR_NEEDMOREPARAMS,
            replyTarget(client),
            "PRIVMSG",
            "Not enough parameters"
        );
        return;
    }

    // 2. Get target and message text
    const std::string &target = msg.getParams()[0];
    const std::string &text = msg.getParams()[1];

    if (target.empty() || text.empty())
    {
        sendNumericReply(
            clientFd,
            ERR_NEEDMOREPARAMS,
            replyTarget(client),
            "PRIVMSG",
            "Not enough parameters"
        );
        return;
    }

    // 3. Build PRIVMSG
    std::string privmsg =
        ":" + client->getNickname()
        + " PRIVMSG " + target
        + " :" + text + "\r\n";

    // 4. Target is a channel
    if (target[0] == '#')
    {
        std::map<std::string, Channel *>::iterator it =
            _channels.find(target);

        if (it == _channels.end())
        {
            sendNumericReply(
                clientFd,
                ERR_NOSUCHCHANNEL,
                replyTarget(client),
                target,
                "No such channel"
            );
            return;
        }

        Channel *channel = it->second;

        // Client must be a member of the channel
        if (!channel->isMember(client))
        {
            sendNumericReply(
                clientFd,
                ERR_NOTONCHANNEL,
                replyTarget(client),
                target,
                "You're not on that channel"
            );
            return;
        }

        // Send to all other channel members
        broadcastToChannel(channel, privmsg, client);
        return;
    }

    // 5. Target is another client
    Client *targetClient = getClientByNickname(target);

    if (!targetClient)
    {
        sendNumericReply(
            clientFd,
            ERR_NOSUCHNICK,
            replyTarget(client),
            target,
            "No such nick"
        );
        return;
    }

    // 6. Send direct message
    queueSend(targetClient->getFd(), privmsg);
}

void Server::handleQuit(const Message &msg, int clientFd)
{
    (void)msg;

    Client *client = getClient(clientFd);
    if (!client)
        return;

    // 1. Notify all joined channels — must happen before disconnectClient()
    // below, which removes this client from those channels and frees it
    std::string quitMsg =
        ":" + client->getNickname()
        + " QUIT :Client disconnected\r\n";

    for (std::map<std::string, Channel *>::iterator it = _channels.begin();
         it != _channels.end(); ++it)
    {
        Channel *channel = it->second;

        if (channel->isMember(client))
            broadcastToChannel(channel, quitMsg, client);
    }

    // 2. disconnectClient() does channel-membership cleanup, close(), frees
    // the Client*, and defers the _pfds removal to after eventLoop's scan
    // (D5) — doing this by hand used to leak the Client* and leave a stale
    // pollfd in _pfds that could later misroute to a reused fd.
    disconnectClient(clientFd);
}

void Server::handlePing(const Message &msg, int clientFd)
{
    Client *client = getClient(clientFd);
    if (!client)
        return;

    // PING allows the client to verify the server is alive. Server responds
    // with PONG. If the client sent a parameter, echo it back; otherwise send
    // the server name.
    std::string pongMsg = "PONG :";
    if (msg.getParams().size() > 0)
        pongMsg += msg.getParams()[0];
    else
        pongMsg += SERVER_NAME;
    pongMsg += "\r\n";

    queueSend(clientFd, pongMsg);
}

void Server::handleKick(const Message &msg, int clientFd)
{
    Client *client = getClient(clientFd);
    if (!client)
        return;

    // 1. Validate parameters
    if (msg.getParams().size() < 2)
    {
        sendNumericReply(clientFd, ERR_NEEDMOREPARAMS, replyTarget(client), "KICK", "Not enough parameters");
        return;
    }

    const std::string &channelName = msg.getParams()[0];
    const std::string &targetNick = msg.getParams()[1];

    // 2. Check channel exists
    if (!isChannelExists(channelName))
    {
        sendNumericReply(clientFd, ERR_NOSUCHCHANNEL, replyTarget(client), channelName, "No such channel");
        return;
    }

    Channel *channel = _channels[channelName];

    // 3. Check client is in channel
    if (!channel->isMember(client))
    {
        sendNumericReply(clientFd, ERR_NOTONCHANNEL, replyTarget(client), channelName, "You're not on that channel");
        return;
    }

    // 4. Check client is operator
    if (!channel->isOperator(client))
    {
        sendNumericReply(clientFd, ERR_CHANOPRIVSNEEDED, replyTarget(client), channelName, "You're not channel operator");
        return;
    }

    // 5. Check target nick exists
    Client *target = getClientByNickname(targetNick);
    if (!target)
    {
        sendNumericReply(clientFd, ERR_NOSUCHNICK, replyTarget(client), targetNick, "No such nick");
        return;
    }

    // 6. Check target is in channel
    if (!channel->isMember(target))
    {
        sendNumericReply(clientFd, ERR_USERNOTINCHANNEL, replyTarget(client), targetNick, "User is not on that channel");
        return;
    }

    // 7. Build KICK message with optional reason
    std::string reason = "kicked";
    if (msg.getParams().size() > 2)
        reason = msg.getParams()[2];

    std::string kickMsg = ":" + client->getNickname() + " KICK " + channelName + " " + targetNick + " :" + reason + "\r\n";

    // 8. Broadcast KICK to channel
    broadcastToChannel(channel, kickMsg, NULL);

    // 9. Remove target from channel
    target->leaveChannel(channelName);
    channel->removeMember(target);
    channel->removeOperator(target);

    // 10. Delete channel if empty
    if (channel->getMembers().empty())
    {
        _channels.erase(channelName);
        delete channel;
    }
}

void Server::handleInvite(const Message &msg, int clientFd)
{
    Client *client = getClient(clientFd);
    if (!client)
        return;

    // 1. Validate parameters
    if (msg.getParams().size() < 2)
    {
        sendNumericReply(clientFd, ERR_NEEDMOREPARAMS, replyTarget(client), "INVITE", "Not enough parameters");
        return;
    }

    const std::string &targetNick = msg.getParams()[0];
    const std::string &channelName = msg.getParams()[1];

    // 2. Check target nick exists
    Client *target = getClientByNickname(targetNick);
    if (!target)
    {
        sendNumericReply(clientFd, ERR_NOSUCHNICK, replyTarget(client), targetNick, "No such nick");
        return;
    }

    // 3. Check channel exists
    if (!isChannelExists(channelName))
    {
        sendNumericReply(clientFd, ERR_NOSUCHCHANNEL, replyTarget(client), channelName, "No such channel");
        return;
    }

    Channel *channel = _channels[channelName];

    // 4. Check inviter is in channel
    if (!channel->isMember(client))
    {
        sendNumericReply(clientFd, ERR_NOTONCHANNEL, replyTarget(client), channelName, "You're not on that channel");
        return;
    }

    // 5. Check target is not already in channel
    if (channel->isMember(target))
    {
        sendNumericReply(clientFd, ERR_USERONCHANNEL, replyTarget(client), targetNick + " " + channelName, "is already on channel");
        return;
    }

    // 6. Send INVITE message to target
    std::string inviteMsg = ":" + client->getNickname() + " INVITE " + targetNick + " " + channelName + "\r\n";
    queueSend(target->getFd(), inviteMsg);

    // 7. Send confirmation to inviter (341 RPL_INVITING)
    sendNumericReply(clientFd, RPL_INVITING, replyTarget(client), targetNick, channelName);

    // 8. Add target to invite list (so they can join +i channels)
    channel->addInvited(target);
}
