##### Copyright Marin Radu 2023
# README

## View as webpage

```bash
sudo pip3 install grip
grip  README.md
# open http://localhost:6419/
```

# Content

1. [Overview](#overview)
2. [Client](#subscribers)
3. [Server](#the-server)
4. [Annex](#annex)

## Overview

This app attempts to create a server that allows for communication between `TCP` and `UDP` clients. The former connect to the server, while the latter send it messages to be distributed accordingly to the TCP clients.

## Subscribers

A TCP client, or 'subscriber', can choose to (un)subscribe to a *topic*. Doing so would inform the server on whether or not the client should receive any incoming messages related to that topic.

Each message can be either a text, or a number following a strict format. The messages are decoded, interpreted and printed by the client. The server only forwards them.

## The server

The server keeps track of all clients, storing relevant data, such as the *ID* and the connection's *file descriptor*, as well as every subscribed to topic. No two clients can be connected at the same time with the same ID.

*Maps* and *sets* were used for more efficient insertion, deletion and searches. They simulate a database.

A slightly modified version of **regex** string matching was implemented to match subscription *patterns* with the actual topic of the message. Note that messages received while a client is *offline* are discarded.

**I/O multiplexing** was achieved via `poll`. Any number of connections can be accepted through the use of *vector*. Previously used `pollfds` are not deallocated, since it would greatly affect the server response time. They are instead reused for any further connection attempts.

## Annex

#### The application-level protocol consists of two headers, appended to the messages sent on UDP and TCP connections. These ensure the correct interpretation of the data, as well as maintain a relatively high transfer rate.

The option to transfer and print the sender *IP address and port* for UDP messages has been opted out of, but can be enabled (see commented code).

##### Further details can be found in the source files and headers (comments, variable, function and class names etc.).
