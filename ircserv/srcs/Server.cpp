#include "../includes/Server.hpp"

// constructor, destructor, copy constructor, assignment operator (Orthodox Canonical Form)
Server::Server() : _sockfd(0), _port(0), _password("") {}

Server::~Server() {}

Server::Server(const Server &other) : _sockfd(other._sockfd), _port(other._port), _password(other._password) {}

Server &Server::operator=(const Server &other)
{
    if (this != &other)
    {
        _sockfd = other._sockfd;
        _port = other._port;
        _password = other._password;
    }
    return *this;
}

// Socket operations

// Initialize socket here (e.g., using socket() system call)
void Server::initSocket()
{
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0)
    {
        std::cerr << "Error creating socket" << std::endl;
        exit(1);
    }
}

// Bind socket to the specified port (e.g., using bind() system call)
void Server::bindSocket()
{
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(_port);

    if (bind(_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        std::cerr << "Error binding socket" << std::endl;
        exit(1);
    }
}

// Listen for incoming connections (e.g., using listen() system call)
void Server::listenSocket()
{
    if (listen(_sockfd, 5) < 0)
    {
        std::cerr << "Error listening" << std::endl;
        exit(1);
    }
}

// Accept incoming connections (e.g., using accept() system call)
void Server::acceptConnection()
{
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    _clientfd = accept(_sockfd, (struct sockaddr *)&cli_addr, &cli_len);
    if (_clientfd < 0)
    {
        std::cerr << "Error accepting connection" << std::endl;
        exit(1);
    }
}

// Send password to the client (e.g., using send() system call)
void Server::sendPassword()
{
    if (send(_clientfd, _password.c_str(), _password.length(), 0) < 0)
    {
        std::cerr << "Error sending password" << std::endl;
        exit(1);
    }
}

// Main server loop to handle incoming connections and client interactions
void Server::run()
{
    initSocket();
    bindSocket();
    listenSocket();

    while (true)
    {
        acceptConnection();
        sendPassword();
        // Handle client interactions here (e.g., using recv() and send() system calls)
        close(_clientfd); // Close client connection after handling
    }
}