# Copyright Radu Marin 2024

# Use current standards and flags
CC := gcc
CFLAGS := -std=c17 -Wall -Wextra -I./include

CXX := g++
CPPFLAGS := -std=c++17 -Wall -Wextra -I./include

LDFLAGS :=

SERVER := server
CLIENT := subscriber

SERVER_SRCS := lib/cregex.c lib/app_prot.c server.cpp
CLIENT_SRCS := lib/app_prot.c client.c

SERVER_OBJS := $(patsubst %.c,%.o,$(filter %.c,$(SERVER_SRCS))) $(patsubst %.cpp,%.o,$(filter %.cpp,$(SERVER_SRCS)))
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

build: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_OBJS)
	$(CXX) $(LDFLAGS) $(SERVER_OBJS) -o $(SERVER)

$(CLIENT): $(CLIENT_OBJS)
	$(CC) $(LDFLAGS) $(CLIENT_OBJS) -o $(CLIENT)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS)
	rm -f $(SERVER) $(CLIENT)

.PHONY: build clean $(SERVER) $(CLIENT)
