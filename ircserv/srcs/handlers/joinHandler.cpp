#include "../../includes/Server.hpp"
#include "../../includes/Channel.hpp"
#include "../../includes/Replies.hpp"


/*
	I moved this function because it was getting too long and I wanted to keep the Server class organized.
	Also I changed the fuction flow a bit to make it more readable and easier to follow.
	I also added some comments to explain what each section of the code is doing.
*/

// helpers for sending RPL_TOPIC/RPL_NOTOPIC and RPL_NAMREPLY/RPL_ENDOFNAMES after JOIN
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
        std::cerr << "Error: Client not found for fd: "
                  << clientFd << std::endl;
        return;
    }

    // 0. Validate parameters
    if (msg.getParams().size() < 1)
    {
        sendNumericReply(
            clientFd,
            ERR_NEEDMOREPARAMS,
            replyTarget(client),
            "JOIN",
            "Not enough parameters"
        );
        return;
    }

    const std::string &channelName = msg.getParams()[0];

    if (!isValidChannelName(channelName))
    {
        std::cerr << "Error: Invalid channel name format." << std::endl;
		// correct code here is 476 ERR_BADCHANMASK — out of scope for this pass
        return;
    }

    // 1. Find existing channel
    std::map<std::string, Channel *>::iterator it =
        _channels.find(channelName);

    Channel *channel = NULL;
    bool newChannel = false;

    if (it != _channels.end()) // if channel exists
    {
        channel = it->second;

        // 2. Check if client is already a member
        if (channel->isMember(client))
        {
            sendNumericReply(
                clientFd,
                ERR_USERONCHANNEL,
                replyTarget(client),
                client->getNickname() + " " + channelName,
                "is already on channel"
            );
            return;
        }

        // 3. Check invite-only
        if (channel->isInviteOnly())
        {
            sendNumericReply(
                clientFd,
                ERR_INVITEONLYCHAN,
                replyTarget(client),
                channelName,
                "Cannot join channel (+i)"
            );
            return;
        }

        // 4. Check channel password
        if (channel->isPasswordProtected())
        {
            if (msg.getParams().size() < 2 ||
                msg.getParams()[1] != channel->getPassword())
            {
                sendNumericReply(
                    clientFd,
                    ERR_BADCHANNELKEY,
                    replyTarget(client),
                    channelName,
                    "Cannot join channel (+k)"
                );
                return;
            }
        }

        // 5. Check MODE +l user limit
        if (channel->isUserLimitReached())
        {
            sendNumericReply(
                clientFd,
                ERR_CHANNELISFULL,
                replyTarget(client),
                channelName,
                "Cannot join channel (+l)"
            );
            return;
        }

        // 6. Check ban
        if (channel->isBanned(client->getNickname()))
        {
            sendNumericReply(
                clientFd,
                ERR_BANNEDFROMCHAN,
                replyTarget(client),
                channelName,
                "Cannot join channel (+b)"
            );
            return;
        }
    }
    else // if channel does not exist
    {
        // 7. Channel doesn't exist.
        // It will be created only after all JOIN checks succeed.
        newChannel = true;
    }

    // 8. Create channel if necessary
    if (newChannel)
    {
        channel = new Channel(channelName);
        _channels[channelName] = channel;
    }

    // 9. Final membership check.
    // addMember() also enforces the hard MAX_MEMBERS limit.
    if (!channel->addMember(client))
    {
        sendNumericReply(
            clientFd,
            ERR_CHANNELISFULL,
            replyTarget(client),
            channelName,
            "Cannot join channel (channel is full)"
        );

        // If creation somehow failed, don't leave an empty channel behind.
        if (newChannel && channel->getMembers().empty())
        {
            _channels.erase(channelName);
            delete channel;
        }

        return;
    }

    // 10. First member becomes channel operator
    if (newChannel)
        channel->addOperator(client);

    // 11. Update client's channel membership
    client->joinChannel(channelName);

    // 12. Broadcast JOIN to channel members
    std::string joinMessage =
        ":" + client->getNickname()
        + " JOIN " + channelName + "\r\n";

    broadcastToChannel(channel, joinMessage, client);

    // 13. Send topic information to joining client
    sendTopic(channel, client);

    // 14. Send NAMES list to joining client
    sendNamesList(channel, client);

    std::cout << "Client " << client->getNickname()
              << " joined channel " << channelName << std::endl;
}

