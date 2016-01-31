#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> 		/* close */
#include <netdb.h> 			/* gethostbyname */
#include "Tools.h"

static int	 	app(const char *address, int port);
static int 		init_connection(const char *address, int port);
static void 	end_connection(int sock);
static int 		readFromServer(SOCKET sock, char *buffer);
static void 	sendToServer(SOCKET sock, const char *buffer);

#endif
