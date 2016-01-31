#ifndef TOOLS_H
#define TOOLS_H

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <map>
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <cstdio>
#include <memory>
#include <tr1/memory>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

using namespace std;

typedef int 				SOCKET;
typedef struct sockaddr_in 	SOCKADDR_IN;
typedef struct sockaddr 	SOCKADDR;
typedef struct in_addr 		IN_ADDR;

#define INVALID_SOCKET 	-1
#define SOCKET_ERROR 	-1
#define closesocket(s) close(s)

#define CRLF				"\r\n"
#define PORT	 			1977

#define BUF_SIZE			16384

#define DEFAULTCOMMSEP 				string(":")    					// separateur utilisé par défaut pour le découpage des messages reseaux
#define CLIENTREQUESTSEP 			string(" ")    					// separateur utilisé par défaut lors de l'envoi des requetes par les clients
#define REQUEST_LISTCLIENTS 		"listclients"
#define REQUEST_RFSEND 				"rfsend"
#define REQUEST_LISTREMOTES			"rflistremote"
#define REQUEST_GETREMOTEBUTTONS	"rfgetbutton"
#define REQUEST_HELP				"help"


/*------------------------------------------------------------------------------------------------------------------------------*/
/*                                      Process and threads operations                                            */
/*------------------------------------------------------------------------------------------------------------------------------*/

volatile int ctrlCcount = 0;
void catch_int(int sig_num){ctrlCcount++; std::cout << "ctrlCcount=" << ctrlCcount << " signum=" << sig_num << std::endl;}

/*------------------------------------------------------------------------------------------------------------------------------*/
/*                                      String operations                                            */
/*------------------------------------------------------------------------------------------------------------------------------*/
// trim from start
static inline string & ltrim(string & s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}
// trim from end
static inline string & rtrim(string & s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}
// trim from both ends
static inline string & trim(string & s) {return ltrim(rtrim(s));}

// toUpper
static std::string toUpper(const std::string & str)
{
	std::locale loc;	
	std::stringstream mystream;
	for (std::string::size_type i=0; i<str.length(); ++i)
		mystream << std::toupper(str[i],loc);
    return mystream.str();
}

//Fonction de conversion long vers string
std::string longToString(const long mylong)
{   
    std::stringstream mystream;
    mystream << mylong;
	std::string toReturn = mystream.str();
    mystream.str( std::string() );
	mystream.clear();
    return toReturn;
}

// Tokenizer
static vector<string> tokenize(string & message,const string & sep)
{
	vector<string> toReturn;
	size_t pos 		= 0;
	size_t lastpos 	= 0;
	std::string token;
	pos = message.find(sep.c_str(), lastpos);
	while (pos != string::npos) 
	{
		token = message.substr(lastpos, pos-lastpos);
		toReturn.insert(toReturn.end(),token);
		lastpos = pos + sep.length();
		pos = message.find(sep.c_str(), lastpos+1);
	}
	token = message.substr(lastpos, message.length()-lastpos);
	toReturn.insert(toReturn.end(),token);
	return toReturn;
}


/* ----------------------------------------------------------------------------------------------------------- */

#define COMMANDARGS commandArgs
#define PGRMNAME programName

std::string 					programName;
map<std::string,std::string>	commandArgs; 			/* arguments de la ligne de commande */

int parseCommandLineArgs(int nbArgs, char ** args)
{
	programName = args[0];
	
	for (int i=1;i<nbArgs; i++)
	{
		if (args[i][0] == '-')
		{
			size_t pos = 0;
			size_t lastpos = 1;
			std::string key="";
			std::string token;
			std::string toparse = args[i];
			std::string delimiter = "=";
			pos = toparse.find(delimiter.c_str(), lastpos) ;
			while ( pos  != std::string::npos) 
			{
				token = toparse.substr(lastpos, pos-1);
				key = key + token;
				lastpos = pos +delimiter.length();
				pos = toparse.find(delimiter.c_str(), lastpos) ;
			}
			std::string value =  toparse.substr(lastpos, toparse.length());
			if (key.length() == 0)
				key=value;
			commandArgs[key]=value;
		}
		else // pas de '-' au début
		{
			std:string key = longToString(i);
			commandArgs[key]=args[i];
		}
	}
	
	return 0;
}

/* ----------------------------------------------------------------------------------------------------------- */
void displayCommandLineArgs()
{
	cout << "------------------------------------------------------------------------------------" << endl;
	cout << "------------------------ Command line arguments -------------------" << endl;
	for (std::map<std::string,std::string>::iterator it=commandArgs.begin(); it!=commandArgs.end(); )
	{		
    	cout << it->first << " => " << it->second << endl;
	}
	cout << "------------------------------------------------------------------------------------" << endl;
} 

/* ----------------------------------------------------------------------------------------------------------- */
string nowInMicroSecondToString()
{
	struct timeval 	tt;
	gettimeofday(&tt, NULL);
	long t_s = tt.tv_usec/1000L;

	struct tm tstruct;				/* structure date pour les logs */
	time_t timev = time(NULL);
	tstruct = *localtime(&timev);
	char timestr[4096];
	strftime(timestr, sizeof(timestr), "%Y%m%d.%H%M%S", &tstruct);
	char us[4];
	sprintf(us, "%03ld", t_s);
	string totaltimestr = timestr;
	totaltimestr+= ".";
	totaltimestr+= us;
	return totaltimestr;
}

/* ----------------------------------------------------------------------------------------------------------- */
int loglevel = 0;
// Fonction de trace
void trace(int level, const char * msg)
{
	if (loglevel <= level)	
	{
		cout << "[" << nowInMicroSecondToString() << "][" << PGRMNAME << "][" << level << "] " << msg << endl;
	}
}

/* ----------------------------------------------------------------------------------------------------------- */
void displayMessage(vector<string> & tokens)
{
	cerr << "[" << nowInMicroSecondToString() << "][" << PGRMNAME << "] ";
	cerr << " ";
	for(int i=0;i < tokens.size();i++)
		cerr << tokens[i] << " ";
	cerr << endl;
}


/* ----------------------------------------------------------------------------------------------------------- */
vector<string> execAndGetOutput(const char * cmd) 
{
	vector<string> stdoutput;
	FILE * in = popen(cmd, "r");
    tr1::shared_ptr<FILE> 	pipe(in, pclose);
    if (!pipe) 
    {
		string toTrace = "execAndGetOutput() : error when executing following command : ";
		toTrace += cmd;
		trace(3,toTrace.c_str());
		return stdoutput;
	}
    char buffer[16384];
    string result = "";
    while (!feof(pipe.get())) 
    {
        if (fgets(buffer, 16383, pipe.get()) != NULL)
        {
			result = buffer;
			stdoutput.insert(stdoutput.end(),result);
		}
    }
    return stdoutput;
}

#endif
