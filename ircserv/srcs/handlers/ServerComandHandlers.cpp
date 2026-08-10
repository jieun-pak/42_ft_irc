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


/*
	I moved JOIN handling to its own file.
*/


void Server::handleMode(const Message &msg, int clientFd)
{
    Client *client = getClient(clientFd);
    if (!client)
        return;

    // 1. Validate basic parameters
    if (msg.getParams().size() < 2)
    {
        sendNumericReply(
            clientFd, ERR_NEEDMOREPARAMS, replyTarget(client), "MODE", "Not enough parameters"
        );
        return;
    }

    const std::string &channelName = msg.getParams()[0];

    // 2. Check channel exists
    if (!isChannelExists(channelName))
    {
        sendNumericReply(
            clientFd,
            ERR_NOSUCHCHANNEL,
            replyTarget(client),
            channelName,
            "No such channel"
        );
        return;
    }

    Channel *channel = _channels[channelName];

    // 3. Check client is in channel
    if (!isClientInChannel(client, channelName))
    {
        sendNumericReply(
            clientFd,
            ERR_NOTONCHANNEL,
            replyTarget(client),
            channelName,
            "You're not on that channel"
        );
        return;
    }

// 4. Validate mode string
    const std::string &mode = msg.getParams()[1];

    if (mode.size() < 2 || (mode[0] != '+' && mode[0] != '-'))
    {
        sendNumericReply(
            clientFd,
            ERR_UMODEUNKNOWNFLAG,
            replyTarget(client),
            "",
            "Unknown mode flag"
        );
        return;
    }

    char action = mode[0];
    char modeChar = mode[1];

    // 5. Check operator privileges
    if (!channel->isOperator(client))
    {
        sendNumericReply(
            clientFd,
            ERR_CHANOPRIVSNEEDED,
            replyTarget(client),
            channelName,
            "You're not channel operator"
        );
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
                    sendNumericReply(
                        clientFd,
                        ERR_NEEDMOREPARAMS,
                        replyTarget(client),
                        "MODE",
                        "Password required for +k"
                    );
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
                    sendNumericReply(
                        clientFd,
                        ERR_NEEDMOREPARAMS,
                        replyTarget(client),
                        "MODE",
                        "User limit required for +l"
                    );
                    return;
                }

                int limit = std::atoi(msg.getParams()[2].c_str());

                if (limit <= 0)
                {
                    // RFC 1459/2812 has no numeric for "MODE param present but
                    // invalid" — closest real code, ERR_UNKNOWNMODE (472), is
                    // for an unrecognized mode *character*, not a bad value for
                    // a recognized one, so it would be misleading here. Server
                    // log only.
                    std::cerr << "Error: Invalid user limit for +l on " << channelName << std::endl;
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
        {
            if (msg.getParams().size() < 3)
            {
                sendNumericReply(
                    clientFd,
                    ERR_NEEDMOREPARAMS,
                    replyTarget(client),
                    "MODE",
                    "Nickname required for +o/-o"
                );
                return;
            }

            Client *target = getClientByNickname(msg.getParams()[2]);

            if (!target)
            {
                sendNumericReply(
                    clientFd,
                    ERR_NOSUCHNICK,
                    replyTarget(client),
                    msg.getParams()[2],
                    "No such nick"
                );
                return;
            }

            if (!isClientInChannel(target, channelName))
            {
                sendNumericReply(
                    clientFd,
                    ERR_USERNOTINCHANNEL,
                    replyTarget(client),
                    target->getNickname(),
                    "User is not on that channel"
                );
                return;
            }

            if (action == '+')
                channel->addOperator(target);
            else
                channel->removeOperator(target);

            break;
        }

        default:
            sendNumericReply(
                clientFd,
                ERR_UNKNOWNMODE,
                replyTarget(client),
                std::string(1, modeChar),
                "is unknown mode char to me"
            );
            return;
    }

    // 7. Broadcast MODE message
    std::string modeMessage =
        ":" + client->getNickname()
        + " MODE " + channelName
        + " " + mode;

    if (msg.getParams().size() > 2)
        modeMessage += " " + msg.getParams()[2];

    modeMessage += "\r\n";

    broadcastToChannel(channel, modeMessage, client);
}

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
