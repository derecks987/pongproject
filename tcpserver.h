#ifndef __tcpserver_h_
#define __tcpserver_h_

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <vector>
using namespace std;
#include "SDL.h"
#include "SDL_net.h"
#include "SDL_mutex.h"
struct thread_data{
	std::vector<std::string> *message_list;
	SDL_mutex *lock;
};
int fc(void*);
void find_connection();
void init_server();
void server_receive(thread_data *data);
int rd(void*);
void quit_server();
void server_send(std::string data);


#endif // #ifndef __tcpserver_h_
