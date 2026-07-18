#compiler & flag
CC = c++
CFLAGS = -Wall -Wextra -Werror -std=c++98

# target executable
NAME	= ircserv

# sources
SRCS	=	srcs/main.cpp \
			srcs/Server.cpp \
			srcs/ServerComandHandlers.cpp \
			srcs/Client.cpp \
			srcs/signal.cpp
# 			TODO: UPDATE
# 			srcs/Authentication.cpp \
# 			srcs/Channel.cpp \
# 			srcs/commands/...cpp

# object
OBJS = $(SRCS:.cpp=.o)

# header
#TODO: UPDATE
HEADERS	=	includes/ft_irc.hpp \
			includes/Server.hpp \
			includes/Client.hpp \
			includes/signal.hpp

# default target
all: $(NAME)

# linking objects to create executable
$(NAME): $(OBJS)
	@$(CC) $(CFLAGS) -o $(NAME) $(OBJS)

# compil sources to objects
%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# clean
clean:
	@rm -f $(OBJS)

fclean: clean
	@rm -f $(NAME) a.out

re: fclean all

.PHONY: all clean fclean re