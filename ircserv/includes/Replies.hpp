#ifndef REPLIES_HPP
#define REPLIES_HPP

// IRC numeric reply codes (RFC 1459 / 2812) currently implemented.
// Server::sendNumericReply() formats these into ":<server> <code> ..." lines.

#define SERVER_NAME "ircserv"

enum IrcNumeric
{
	RPL_WELCOME				= 1,
	RPL_NOTOPIC				= 331,
	RPL_TOPIC				= 332,
	RPL_NAMREPLY			= 353,
	RPL_ENDOFNAMES			= 366,
	ERR_UNKNOWNCOMMAND		= 421,
	ERR_NICKNAMEINUSE		= 433,
	ERR_NEEDMOREPARAMS		= 461,
	ERR_ALREADYREGISTRED	= 462,
	ERR_PASSWDMISMATCH		= 464,
	ERR_NOTREGISTERED		= 451
};

#endif
