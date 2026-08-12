#include "../../includes/Server.hpp"
#include "../../includes/Channel.hpp"
#include "../../includes/Replies.hpp"

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
