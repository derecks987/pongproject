#ifndef __tcpclient_h_
#define __tcpclient_h_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>
using namespace std;
#include "SDL.h"
#include "SDL_net.h"
#include "SDL_mutex.h"
#include "tcpserver.h"

void init_client(std::string hostname);
void client_send(std::string data);
void client_receive(thread_data *data);
int c_rd(void*);
void quit_client();

	
    

#endif // #ifndef __tcpclient_h_
