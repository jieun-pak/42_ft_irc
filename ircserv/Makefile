#compiler & flag
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
NAME	= ircserv

SRCS	=	srcs/main.cpp \
			srcs/Server.cpp \
			srcs/handlers/ServerComandHandlers.cpp \
			srcs/handlers/joinHandler.cpp \
			srcs/handlers/modeHandler.cpp \
			srcs/Client.cpp \
			srcs/signal.cpp \
 			srcs/Channel.cpp \

OBJS = $(SRCS:.cpp=.o)

HEADERS	=	includes/ft_irc.hpp \
			includes/Server.hpp \
			includes/Client.hpp \
			includes/Channel.hpp \
			includes/Replies.hpp \
			includes/signal.hpp

all: $(NAME)

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# clean
clean:
	@rm -f $(OBJS)

fclean: clean
	@rm -f $(NAME) a.out

re: fclean all

.PHONY: all clean fclean re