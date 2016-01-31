#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <sys/time.h>
#include <time.h>

using namespace std;

#include "RFSocketClient.h"

/*------------------------------------------------------------------------------------------------------------------------------*/
static int app(const char *address, int port)
{
	SOCKET sock = init_connection(address, port);
	trace (0,"Init connection to server : OK");
	char buffer[BUF_SIZE];

	fd_set rdfs;

	struct hostent * hostinfo = gethostbyname("localhost");
	string host_date;
	if (hostinfo == NULL)	host_date="UNKNOWN";
	else	 				host_date=(char*)hostinfo->h_name;
	host_date += "_";	host_date += nowInMicroSecondToString();
	string str = "Registered at server with following client id : " + host_date;
	trace (0, str.c_str());
	
	sendToServer(sock, host_date.c_str()); /* send clientId of this process to server */

	bool justOneCommand = false;
	map<string,string>::iterator it;
	it = COMMANDARGS.find("help");		
	if(it != COMMANDARGS.end())	
	{
		justOneCommand = true;
		sendToServer(sock,REQUEST_HELP);
	}
	it = COMMANDARGS.find("remote");
	if(it != COMMANDARGS.end())	
	{
		justOneCommand = true;
		string remoteId = it->second;
		
		it = COMMANDARGS.find("btn");
		if(it == COMMANDARGS.end())	
		{
			string toSend = REQUEST_GETREMOTEBUTTONS;
			toSend += " ";
			toSend += remoteId;
			trace(0,toSend.c_str());
			sendToServer(sock,toSend.c_str());
		}
		else 
		{
			string toSend = REQUEST_RFSEND;
			toSend += " ";
			toSend += remoteId;
			toSend += " ";
			toSend += it->second;
			trace(0,toSend.c_str());
			sendToServer(sock,toSend.c_str());
		}		
	}
	
	if (justOneCommand) // One request mode
	{
		int cpt=0;
		while(1)
		{
			FD_ZERO(&rdfs);
			FD_SET(sock, &rdfs); 			/* add the socket */
			if(select(sock + 1, &rdfs, NULL, NULL, NULL) == -1)
			{
				perror("select()");
				exit(errno);
			}
			
			if(FD_ISSET(sock, &rdfs))
			{
				int n = readFromServer(sock, buffer);         
				if(n == 0) /* server down */
				{
					trace (0,"Server has been stopped or disconnected");
					break; 
				}
				string receivedMessage = buffer;
				if (receivedMessage.find("FromServer:") != string::npos && receivedMessage.find("Response") != string::npos)
				{
					vector<string> messageTokens = tokenize(receivedMessage,DEFAULTCOMMSEP);
					displayMessage(messageTokens);
					end_connection(sock);
					return 0;
				}				
				cpt++;
				cout << "it=" << cpt << endl;
				if (cpt > 1000)
				{
					trace (3, "Request timeout ... exiting");
					end_connection(sock);
					return 3;
				}
			}
		}
	}

	//--------------------------------------- Interactif mode ----------------------------------------------------//
	while(1)
	{
		FD_ZERO(&rdfs);
		FD_SET(STDIN_FILENO, &rdfs); 	/* add STDIN_FILENO */
		FD_SET(sock, &rdfs); 			/* add the socket */
		if(select(sock + 1, &rdfs, NULL, NULL, NULL) == -1)
		{
			perror("select()");
			exit(errno);
		}
		
		if(FD_ISSET(STDIN_FILENO, &rdfs)) /* something from standard input : i.e keyboard */
		{
			fgets(buffer, BUF_SIZE-1, stdin);
			{
				char *p = NULL;
				p = strstr(buffer, "\n");
				if(p != NULL) 	{*p = 0;}
				else 			{buffer[BUF_SIZE - 1] = 0; /* fclean */}
			}
			
			string stinmsg = buffer;
			if (stinmsg.find(REQUEST_LISTCLIENTS)==0)			{sendToServer(sock,REQUEST_LISTCLIENTS);}
			else if (stinmsg.find(REQUEST_LISTREMOTES)==0)		{sendToServer(sock,REQUEST_LISTREMOTES);}
			else if (stinmsg.find(REQUEST_RFSEND)==0)			{sendToServer(sock,stinmsg.c_str());}
			else if (stinmsg.find(REQUEST_GETREMOTEBUTTONS)==0) {sendToServer(sock,stinmsg.c_str());}
			else if (stinmsg.find(REQUEST_HELP)==0)				{sendToServer(sock,REQUEST_HELP);}
			else 												{sendToServer(sock, buffer);} // client message broadcast query to server
		}
		else if(FD_ISSET(sock, &rdfs))
		{
			int n = readFromServer(sock, buffer);         
			if(n == 0) /* server down */
			{
				trace (0,"Server has been stopped or disconnected");
				break; 
			}
			string receivedMessage = buffer;
			// En fonction de l'entete du message. Si le séparateur est : 
			//    FromClient:Sep=<SeparatorValue>:<Horodate>:<Msg>:<ClientId>:<RequestId>
			//    FromServer:Sep=<SeparatorValue>:<Horodate>:<MsgWithItemsSeparatedBy<SeparatorValue>>:<RequestId>
			string sep=":";
			int originsize = strlen("FromClient");
			if (receivedMessage.find("FromClient")==0)
			{ 
				char csep = receivedMessage.c_str()[originsize];
				string sep = "";sep += csep;
				vector<string> messageTokens = tokenize(receivedMessage,sep);
				displayMessage(messageTokens);
			}
			else if (receivedMessage.find("FromServer")==0)
			{
				char csep = receivedMessage.c_str()[originsize];
				string sep = ""; sep += csep;
				
				if (receivedMessage.find("Sending raw code =") != string::npos && receivedMessage.find("< Sent <") != string::npos)
				{
					trace(1," signal has been sent");
				}
				else
				{
					vector<string> messageTokens = tokenize(receivedMessage,sep);
					displayMessage(messageTokens);
				}
			}
			else
			{
				string str = "Unknown message : ";
				str += receivedMessage;
				trace (2, str.c_str());
			}
		} 
	}

	end_connection(sock);
	return 0;
}

/*------------------------------------------------------------------------------------------------------------------------------*/
static int init_connection(const char *address, int port)
{
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN sin = { 0 };
	struct hostent *hostinfo;

	if(sock == INVALID_SOCKET)
	{
	  perror("socket()");
	  exit(errno);
	}

	hostinfo = gethostbyname(address);
	if (hostinfo == NULL)
	{
		string str = "Unknown host :" ;
		str += address;
		trace (1,str.c_str());
		exit(EXIT_FAILURE);
	}

	sin.sin_addr = *(IN_ADDR *) hostinfo->h_addr;
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;

	if(connect(sock,(SOCKADDR *) &sin, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
	  perror("connect()");
	  exit(errno);
	}

	return sock;
}

/*------------------------------------------------------------------------------------------------------------------------------*/
static void end_connection(int sock)
{
	closesocket(sock);
}

/*------------------------------------------------------------------------------------------------------------------------------*/
static int readFromServer(SOCKET sock, char *buffer)
{
	int n = 0;
	if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
	{
		perror("recv()");
		exit(errno);
	}
	buffer[n] = 0;
	return n;
}

/*------------------------------------------------------------------------------------------------------------------------------*/
static void sendToServer(SOCKET sock, const char *buffer)
{
	int messageSize = strlen(buffer);
	if (messageSize >= BUF_SIZE)
	{
		string toTrace = "Cannot send message because it is too long : ";
		toTrace += messageSize;
		toTrace += " bytes";
		trace(3, toTrace.c_str());
	}
	if(send(sock, buffer, strlen(buffer), 0) < 0)
	{
		perror("send()");
		exit(errno);
	}
}
 
/*------------------------------------------------------------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
	int port=PORT;
	map<string,string>::iterator it;
	string serveradress = "";
	parseCommandLineArgs(argc,argv);
	programName = "RFSocketClient";
	
	it = COMMANDARGS.find("lev");
	if(it != COMMANDARGS.end())	loglevel = atoi(COMMANDARGS["lev"].c_str());
	
	it = COMMANDARGS.find("srv");
	if(it != COMMANDARGS.end())	serveradress = COMMANDARGS["srv"];
	else
	{
		cout << "Syntax : ./%s -srv=<serveraddress> [-port=<tcpport>] ([-remote=<remoteId>] [-btn=<buttonId>] | -help=help )" << endl;
		cout << " <serveraddress> : ip adress of rfsocket server to connect with" << endl;
		cout << " <tcpport>       : tcp port of rfsocket server to connect with" << endl;
		cout << " <remoteId> : name of a remote" << endl;
		cout << " <buttonId> : name of a button attached to the remote" << endl;
		cout << "Required parameters : '-srv'" << endl;
		cout << "Default port : " << PORT << endl;
		cout << "If '-btn' is set but not '-remote', will list all available remote " << endl;
		cout << "If '-btn' and '-remote' is set but not , will try to ask to send btn signal " << endl;
		cout << "If nor '-btn' neither '-remote' are set launch client in interactif mode, waiting for stdin command " << endl;
		return EXIT_FAILURE;
	}

	it = COMMANDARGS.find("port");
	if(it != COMMANDARGS.end())	port = atoi(COMMANDARGS["port"].c_str());
	
	return app(serveradress.c_str(),port);
}
