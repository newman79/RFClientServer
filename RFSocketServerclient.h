#ifndef CLIENT_H
#define CLIENT_H

#include "RFSocketServer.h"
#include <iostream>
#include <sstream>

using namespace std;

typedef struct
{
   SOCKET sock;
   string clientHost_Id;
} Client;

#endif 
