#include "../includes/Server.hpp"

void Server::handlePass(const Message& msg, int clientFd)
{
	// 1. Check parameter count
	if (msg.getParams().size() < 1)
	{
		std::cerr << "Error: PASS command requires a password parameter." << std::endl;
		return;
	}
    // 2. Verify password
    if (msg.getParams()[0] != _password)
    {
        std::cerr << "Error: Incorrect password." << std::endl;
        return;
    }
    // 3. Mark client as password-authenticated
    Client* client = getClient(clientFd);
    if (client)
    {
        client->setPasswordAuthenticated(true);
    }
}

void Server::handleNick(const Message& msg, int clientFd)
{
	// 1. Check nickname exists
    if (msg.getParams().size() < 1)
    {
        std::cerr << "Error: NICK command requires a nickname parameter." << std::endl;
        return;
    }
    // 2. Check nickname format
	if (!isValidNickname(msg.getParams()[0]))
	{
		std::cerr << "Error: Invalid nickname format." << std::endl;
		return;
	}
    // 3. Check nickname is not already used
	if (isNicknameInUse(msg.getParams()[0], clientFd))
	{
		std::cerr << "Error: Nickname already in use." << std::endl;
		return;
	}
    // 4. Store nickname in Client object
	Client* client = getClient(clientFd);
	if (client)
	{
		client->setNickname(msg.getParams()[0]);
	}
    // 5. Notify others if nickname changes
	if (client && !client->getNickname().empty())
	{
		std::string oldNickname = client->getNickname();
		std::string newNickname = msg.getParams()[0];
		// Notify other clients about the nickname change
		for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		{
			if (it->second && it->second->getFd() != clientFd) // Exclude the current client
			{
				std::string notification = ":" + oldNickname + " NICK " + newNickname + "\r\n";
				send(it->second->getFd(), notification.c_str(), notification.size(), 0);
			}
		}
	}
}

void Server::handleUser(const Message& msg, int clientFd)
{
	// 1. Validate parameters
    if (msg.getParams().size() < 4)
    {
        std::cerr << "Error: USER command requires four parameters." << std::endl;
        return;
    }
    // 2. Store username/realname
    Client* client = getClient(clientFd);
    if (client)
    {
		client->setUsername(msg.getParams()[0]);
		client->setRealname(msg.getParams()[3]);
    }
    // 3. Mark USER as received (you can set a flag in the Client class if needed)
    client->setUserReceived(true);

    // 4. Check if registration is now complete
	if (client && client->isPasswordAuthenticated() && !client->getNickname().empty() && client->isUserReceived())
	{
		// Registration is complete, you can send a welcome message or perform other actions
		std::string welcomeMessage = "Welcome to the IRC server, " + client->getNickname() + "!\r\n";
		send(clientFd, welcomeMessage.c_str(), welcomeMessage.size(), 0);
	}
}

void Server::handleJoin(const Message& msg, int clientFd)
{
	(void)msg;
	(void)clientFd;
	// 1. Find or create channel
    // 2. Check channel restrictions
    // 3. Add client to channel
    // 4. Broadcast JOIN message
    // 5. Send topic and names list
}

void Server::handlePart(const Message& msg, int clientFd)
{
	(void)msg;
	(void)clientFd;
	// 1. Check channel exists
    // 2. Check client is in channel
    // 3. Remove client from channel
    // 4. Broadcast PART message
}

void Server::handlePrivmsg(const Message& msg, int clientFd)
{
	(void)msg;
	(void)clientFd;
	// 1. Validate target and text
    // 2. Find target client/channel
    // 3. Forward message
}

void Server::handleQuit(const Message& msg, int clientFd)
{
	(void)msg;
	(void)clientFd;
	// 1. Notify all joined channels
    // 2. Remove client from channels
    // 3. Close socket
    // 4. Remove client from server
}