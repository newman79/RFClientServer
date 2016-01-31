#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include "RFSocketServer.h"
#include "RFSocketServerclient.h"

using namespace std;

/*------------------------------------------------------------------------------------------------------------------------------*/
static void app(int port, int nbclients)
{
	Client serverClient;
	serverClient.sock=-1;
	serverClient.clientHost_Id="";
	
	SOCKET listeningsock = init_connection(port, nbclients);
	char buffer[BUF_SIZE];
	int max = listeningsock;
	fd_set rdfs;	// structure de donnée = conteneur de filedescriptors

	while(1)
	{
	   // TODO : lire fichier pid
	   
		int i = 0;
		FD_ZERO(&rdfs);				// réinitialise le contenu du conteneur      
		FD_SET(STDIN_FILENO, &rdfs);	/* ajoute le descripteur de fichier de l'entree standard (console) au conteneur */      
		FD_SET(listeningsock, &rdfs);	/* ajoute le descripteur de socket listeningsock au conteneur */      
      	for (map<string,Client>::iterator it=allclients.begin(); it!=allclients.end(); ++it)
			FD_SET(it->second.sock, &rdfs);
			
		if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)	// selecte filtre tous les descripteur dans lesquels des données ont été écrites
		{
			//perror("select()"); cas d'un CONTROL+C
			//exit(errno);
		}
		/* something from standard input : i.e keyboard */
		if(FD_ISSET(STDIN_FILENO, &rdfs))
		{
			fgets(buffer, BUF_SIZE - 1, stdin);
			char * p = strstr(buffer, "\n");
			if (p != NULL) 	{*p = 0;}
			else 				{buffer[BUF_SIZE - 1] = 0; /* fclean */}
			string stdinmsg =  buffer;
			stdinmsg = toUpper(stdinmsg);
			if (stdinmsg.find("QUIT") != string::npos)
			{
				trace (1, "Server is exiting ");
				break; /* stop process when type on keyboard : on sort du while(1) */
			}
			else
			{
				broadcast(serverClient, stdinmsg.c_str(), true);
			}
		}
		else if(FD_ISSET(listeningsock, &rdfs)) /* quelquechose est écrit sur la socket d'écoute ==> c'est un nouveau client */
		{
			SOCKADDR_IN csin = { 0 };
			size_t sinsize = sizeof csin;
			int csock = accept(listeningsock, (SOCKADDR *)&csin, &sinsize); // on attribue le nouveau descripteur de socket
			if(csock == SOCKET_ERROR)
			{
				perror("accept()");
				continue;
			}

			trace (1,"Receving new client connection request");
			if(readFromClient(csock, buffer) == -1) /* after connecting the client sends its name */
			{
				continue; /* disconnected : on recommence au début du while(1) */
			}

			/* what is the new maximum fd ? */
			max = csock > max ? csock : max;
         
			FD_SET(csock, &rdfs); // Est-ce vraiment nécessaire ? ... ?

			Client newclient = { csock }; // Ajoute le client dans le tableau
			newclient.clientHost_Id=buffer;
			string str = "Client ID is " + newclient.clientHost_Id + " socket=" + longToString(csock);
			trace (1,str.c_str());         
			allclients[newclient.clientHost_Id]=newclient;
			str = "New client : ";
			str += newclient.clientHost_Id;
			broadcast(newclient, str.c_str(), true);
		}
		else
		{ 
			int i = 0;
			for (map<string,Client>::iterator it=allclients.begin(); it!=allclients.end();)
			{
				Client & client = it->second;
				bool mustRemove = false;
				if(FD_ISSET(client.sock, &rdfs)) /* a client is talking */
				{
					int c = readFromClient(client.sock, buffer);
					if(c == 0)  /* client disconnected */ 
					{
						string str = "Following client has exited:" + client.clientHost_Id;
						trace(0,str.c_str());
						closesocket(client.sock);
						mustRemove=true;
						broadcast(client, str.c_str(), true);
					} 
					else
					{
						string str = "Received from " + client.clientHost_Id + ":" + buffer;
						trace(0,str.c_str());						
						
						string received = buffer;
						// Si c'est une commande enregistrée on la traite comme une demande client
						// Sinon, considère que c'est une demande de broacast à tout le monde					
						PtrFncProc fncToCall = GetServerCommand(received);						
						if (fncToCall != NULL)	(*fncToCall)(received,client);
						else 					broadcast(client, buffer, false); 
					}
					if (mustRemove)
					{
						trace(-1,"Erasing client from map");
						allclients.erase(it);
					}
					break;
				}
				else it++;
			} // for du for it
		}
	} // fin du while

	clear_clients();
	end_connection(listeningsock);
}

/*------------------------------------------------------------------------------------------------------------------------------*/
/* Ferme les sockets d'échanges avec tous les clients */
static void clear_clients()
{
	for (map<string,Client>::iterator it=allclients.begin(); it!=allclients.end();it++)
		closesocket((it->second).sock);
	allclients.clear();
}

/*------------------------------------------------------------------------------------------------------------------------------*/
		 //    FromClient:Sep=<SeparatorValue>:<Horodate>:<Msg>:<ClientId>:<RequestId>
		 //    FromServer:Sep=<SeparatorValue>:<Horodate>:<MsgWithItemsSeparatedBy<SeparatorValue>>:<RequestId>
static void broadcast(Client & sender, const char *buffer, bool from_server)
{
	if(from_server) {trace(0, "Broadcasting previous info to all clients");}
	else
	{
		string toTrace = "Broadcasting received message from ";	toTrace += sender.clientHost_Id; toTrace += " to all clients";	
		trace(0, toTrace.c_str());
	}
	
	for (map<string,Client>::iterator it=allclients.begin(); it!=allclients.end();it++)	
	{
		Client & client = it->second;	  
		if(sender.sock != client.sock) /* we don't send message to the sender */
		{
			std::string msg = "";
			if(from_server)
			{
				msg =	"FromClient";
				msg +=	DEFAULTCOMMSEP;
				msg +=	nowInMicroSecondToString();
				msg +=	DEFAULTCOMMSEP;
				msg +=	buffer;
				msg +=	DEFAULTCOMMSEP;
				msg +=  sender.clientHost_Id;
				msg +=	DEFAULTCOMMSEP;
			}
			else
			{
				msg =	"FromServer";
				msg +=	DEFAULTCOMMSEP;
				msg +=	nowInMicroSecondToString();
				msg +=	DEFAULTCOMMSEP;
				msg +=	buffer;
				msg +=	DEFAULTCOMMSEP;
			}
			sendToClient(client.sock, msg.c_str());
		}
	}
}

/*------------------------------------------------------------------------------------------------------------------------------*/
static int init_connection(int port, int nbmaxclients)
{
	SOCKET listeningsock = socket(AF_INET, SOCK_STREAM, 0);   				// crée la socket d'écoute
	if(listeningsock == INVALID_SOCKET)
	{
		perror("socket()");
		exit(errno);
	}
   
	SOCKADDR_IN sin = { 0 };												// prépare la socket à écouter sur toutes les interfaces réseau, et sur le port PORT
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;

	int yes = 1;
	if(setsockopt(listeningsock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
	{
		perror("setsockopt");
		exit(errno);
	}
 
	if(bind(listeningsock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR) 	// ca doit physiquement lier le driver reseau à la socket d'écoute
	{
		perror("bind()");
		exit(errno);
	}

	if(listen(listeningsock, nbmaxclients) == SOCKET_ERROR)					// active l'écoute avec MAX_CLIENTS au max
	{
		perror("listen()");
		exit(errno);
	}
	return listeningsock;
}
 
/*------------------------------------------------------------------------------------------------------------------------------*/
static void end_connection(int sock)
{
   closesocket(sock);
}

/*------------------------------------------------------------------------------------------------------------------------------*/
static int readFromClient(SOCKET sock, char *buffer)
{
	int n = 0;
	if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
	{
		perror("recv()");
		n = 0; /* if recv error we disonnect the client */
	}
	buffer[n] = 0;
	return n;
}

/*------------------------------------------------------------------------------------------------------------------------------*/
static void sendToClient(SOCKET sock, const char *buffer)
{
	int messageSize = strlen(buffer);
	if (messageSize >= BUF_SIZE)
	{
		string toTrace = "Cannot send message because it is too long : ";
		toTrace += messageSize;
		toTrace += " bytes";
		trace(3, toTrace.c_str());
	}
	else if(send(sock, buffer, strlen(buffer), 0) < 0)
	{
		perror("send()");
		exit(errno);
	}
}


/*------------------------------------------------------------------------------------------------------------------------------*/
static void process_listavailablecommands(string & msg, Client & client)
{
	string response =	"FromServer";
	response +=	DEFAULTCOMMSEP;
	response +=	nowInMicroSecondToString();
	response +=	DEFAULTCOMMSEP;
	response +=	"Response";
	response +=	DEFAULTCOMMSEP;	
	map<string,PtrFncProc>::iterator itFnc;
	for(itFnc = allfonctions.begin(); itFnc != allfonctions.end();itFnc++)
	{
		response +=	"\n - ";
		response +=	itFnc->first;
		response +=	" ";
		response +=	allfonctionscallargs[itFnc->first];
		response +=	" -";
	}
	string toTrace = "Sending 'available commands' response to client "; toTrace += client.clientHost_Id;  toTrace += ": FromServer ...";
	trace(0,toTrace.c_str()); 
	sendToClient(client.sock,response.c_str());
}

/*------------------------------------------------------------------------------------------------------------------------------*/
static void process_listconnectedclient(string & msg, Client & client)
{
	string response =	"FromServer";
	response +=	DEFAULTCOMMSEP;
	response +=	nowInMicroSecondToString();
	response +=	DEFAULTCOMMSEP;
	response +=	"Response";
	response +=	DEFAULTCOMMSEP;
   	for (map<string,Client>::iterator it=allclients.begin(); it!=allclients.end(); ++it)
   	{
		response += "\n";
		response +="- clientid=" + it->first;
		response += " sock=";
		response += longToString(it->second.sock);
		response += " -";
	}
	string toTrace = "Sending 'clients' response to client : FromServer ...";
	trace(0,toTrace.c_str()); 
	sendToClient(client.sock,response.c_str());
}

/*------------------------------------------------------------------------------------------------------------------------------*/
static void process_rf_listremote(string & msg, Client & client)
{
	string response = "FromServer";
	response +=	DEFAULTCOMMSEP;
	response +=	nowInMicroSecondToString();
	response +=	DEFAULTCOMMSEP;
	response +=	"Response";
	response +=	DEFAULTCOMMSEP;
	
	vector<string> stdoutput = execAndGetOutput("sudo /home/pi/src/RadioFrequence/RFSendRemoteBtnCode -jsonconf=/home/pi/src/RadioFrequence/radioFrequenceSignalConfig.json -listremotes=list 2>&1");
	for (vector<string>::iterator it=stdoutput.begin(); it!=stdoutput.end();it++)	
	{
		response +=	"\n";
		string str = " - ";
		str += trim(*it); 
		str += " - ";
		response += str;
	}
	string toTrace = "Sending 'remote list' response to client : FromServer ...";
	trace(0,toTrace.c_str()); 
	sendToClient(client.sock,response.c_str());
}

/*------------------------------------------------------------------------------------------------------------------------------*/
/* Parameters to search in msg : 
 * remote
 */ 
static void process_rf_getremoteprop(string & msg, Client & client)
{
	vector<string> messageTokens = tokenize(msg,CLIENTREQUESTSEP);	
	string toexec = "sudo /home/pi/src/RadioFrequence/RFSendRemoteBtnCode -jsonconf=/home/pi/src/RadioFrequence/radioFrequenceSignalConfig.json -getremotebuttons=get -remote=";
	toexec += messageTokens[messageTokens.size()-1];
	toexec += " 2>&1";
	trace(0,toexec.c_str()); 
	vector<string> stdoutput = execAndGetOutput(toexec.c_str());		
	string response = "";
	response +=	"FromServer";
	response +=	DEFAULTCOMMSEP;
	response +=	nowInMicroSecondToString();
	response +=	DEFAULTCOMMSEP;
	response +=	"Response";
	response +=	DEFAULTCOMMSEP;
	for (vector<string>::iterator it=stdoutput.begin(); it!=stdoutput.end();it++)	
	{
		response +=	"\n";
		string str = " - ";
		str += trim(*it); 
		str += " - ";
		response += str;
	}
	string toTrace = "Sending 'remote button list' response to client : FromServer ...";
	trace(0,toTrace.c_str()); 
	sendToClient(client.sock,response.c_str());
}

/*------------------------------------------------------------------------------------------------------------------------------*/
/* Parameters to search in msg : 
 * 	remote
 * 	btn
 */
static void process_rf_send(string & msg, Client & client)
{
	vector<string> messageTokens = tokenize(msg,CLIENTREQUESTSEP);
	
	string toexec = "sudo /home/pi/src/RadioFrequence/RFSendRemoteBtnCode -jsonconf=/home/pi/src/RadioFrequence/radioFrequenceSignalConfig.json -remote=";
	toexec += messageTokens[messageTokens.size()-2];	
	toexec += " -btn=";
	toexec += messageTokens[messageTokens.size()-1];
	toexec += " -lev=2 -repeat=10 2>&1";
	vector<string> stdoutput = execAndGetOutput(toexec.c_str());	
	
	string response = "";
	response +=	"FromServer";
	response +=	DEFAULTCOMMSEP;
	response +=	nowInMicroSecondToString();
	response +=	DEFAULTCOMMSEP;
	response +=	"Response";
	response +=	DEFAULTCOMMSEP;
	for (vector<string>::iterator it=stdoutput.begin(); it!=stdoutput.end();it++)	
	{
		response +=	"\n";
		string str = " - ";
		str += trim(*it); 
		str += " - ";
		response += str;
	}
	
	string toTrace = "Sending 'rfsend' response to client : FromServer ...";
	trace(0,toTrace.c_str()); 
	sendToClient(client.sock,response.c_str());	
}

/*------------------------------------------------------------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
	programName = "RFSocketServer";
	
	int port = PORT;
	int nbclients = MAX_CLIENTS;
	
	parseCommandLineArgs(argc,argv);
	programName = "RFSocketServer";
	displayCommandLineArgs();
	
	string fname;
	fname=REQUEST_LISTCLIENTS;		registerServerFunction(fname	, &process_listconnectedclient);
	fname=REQUEST_RFSEND;			registerServerFunction(fname	, &process_rf_send);
	allfonctionscallargs[REQUEST_RFSEND] 		= " <remoteId> <buttonId>";
	fname=REQUEST_LISTREMOTES;		registerServerFunction(fname	, &process_rf_listremote);
	allfonctionscallargs[REQUEST_LISTREMOTES] 	= " ";
	fname=REQUEST_GETREMOTEBUTTONS;	registerServerFunction(fname 	, &process_rf_getremoteprop);
	allfonctionscallargs[REQUEST_GETREMOTEBUTTONS] 	= " <remoteId> ";
	fname=REQUEST_HELP;				registerServerFunction(fname	, &process_listavailablecommands);
	
	map<string,string>::iterator it;
	string serveradress = "";
	it = COMMANDARGS.find("port");
	if(it != COMMANDARGS.end())	port = atoi(COMMANDARGS["port"].c_str());
	
	it = COMMANDARGS.find("nbmaxclients");
	if(it != COMMANDARGS.end())	nbclients = atoi(COMMANDARGS["nbmaxclients"].c_str());
	
	signal(SIGINT, catch_int);
	app(port,nbclients);
	return EXIT_SUCCESS;
}
