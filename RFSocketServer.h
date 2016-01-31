#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */
#include <map>
#include "Tools.h"

#include "RFSocketServerclient.h"
#define MAX_CLIENTS 		100


map<string,Client> allclients;

typedef void (*PtrFncProc)(string & msg, Client & client);

map<string,PtrFncProc> 	allfonctions;
map<string,string> 		allfonctionscallargs;

static void registerServerFunction(string & fname, PtrFncProc ptr) 
{
	allfonctions[fname]=ptr;
}

static PtrFncProc GetServerCommand(string & msg)
{
	map<string,PtrFncProc>::iterator itFnc;
	for(itFnc = allfonctions.begin(); itFnc != allfonctions.end();itFnc++)
	{
		if (msg.find(itFnc->first) == 0) {return itFnc->second;}
	}
	return NULL;
} 

static void app(int port, int nbmaxclients);
static int 	init_connection(int port, int nbclients);
static void end_connection(int sock);
static int 	readFromClient(SOCKET sock, char *buffer);
static void sendToClient(SOCKET sock, const char *buffer);
static void broadcast(Client & client, const char *buffer, bool from_server);
static void remove_client(int to_remove, int *actual);
static void clear_clients();

static void process_listavailablecommands(string & msg, Client & client);
static void process_listconnectedclient(string & msg, Client & client);
static void process_rf_send(string & msg, Client & client);
static void process_rf_listremote(string & msg, Client & client);
static void process_rf_getremoteprop(string & msg, Client & client);

#endif /* guard */
