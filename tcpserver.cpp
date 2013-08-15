#include "tcpserver.h"
	IPaddress ip,*remoteip;
	TCPsocket server,client;
	char message[1024];
	char message2[1024];
	int len,len2;
	Uint32 ipaddr;
	Uint16 port;
	ofstream outfile;
	ofstream outfile2;
	SDL_Thread *fc_thread=NULL;
	SDL_Thread *rd_thread=NULL;

int
fc(void *d)
{

	while(1)
    {
            /* try to accept a connection */
            client=SDLNet_TCP_Accept(server);
            if(!client)
            { /* no connection accepted */
                    //outfile << "SDLNet_TCP_Accept: "<< SDLNet_GetError() << endl; 
                    SDL_Delay(100); /*sleep 1/10th of a second */
                    continue;
            }
            
            /* get the clients IP and port number */
            remoteip=SDLNet_TCP_GetPeerAddress(client);
            if(!remoteip)
            {
                     //outfile <<"SDLNet_TCP_GetPeerAddress: " << SDLNet_GetError() << endl;
                    continue;
            }

            break;
    }
	return 0;
}
void
find_connection(){
	rd_thread = SDL_CreateThread(fc,NULL);
	SDL_WaitThread(rd_thread,NULL);
}
void 
init_server()
{
	outfile.open("net_server_err.txt");
	outfile2.open("net_server_err2.txt");
	/* initialize SDL */
	if(SDL_Init(0)==-1)
	{
		outfile << "SDL_Init: " << SDL_GetError() << endl;
		exit(1);
	}

	/* initialize SDL_net */
	if(SDLNet_Init()==-1)
	{
		outfile <<"SDLNet_Init: " << SDLNet_GetError()<< endl;
		exit(2);
	}

	/* get the port */
	port=(Uint16)49160;
	
	/* Resolve the argument into an IPaddress type */
	if(SDLNet_ResolveHost(&ip,NULL,port)==-1)
	{
		outfile <<"SDLNet_ResolveHost: "<<SDLNet_GetError()<< endl;
		exit(3);
	}
	/* open the server socket */
	server=SDLNet_TCP_Open(&ip);
	if(!server)
	{
		outfile <<"SDLNet_TCP_Open: " << SDLNet_GetError()<< endl;
		exit(4);
	}
	
}
void
server_receive(thread_data *data){
	fc_thread = SDL_CreateThread(rd,(void *)data);
}
int 
rd(void *d)
{	thread_data *data = (thread_data *) d;
	bool done = false;
	while(!done){
		if(!client){
			//outfile2 << "checK";
			SDL_Delay(100);
			continue;
		}

		/* read the buffer from client */
		len=SDLNet_TCP_Recv(client,message,1024);
		//SDLNet_TCP_Close(client);
		if(!len)
		{
			//outfile2 << "SDLNet_TCP_Recv: " << SDLNet_GetError() << endl;
			SDL_Delay(20); /*sleep 1/10th of a second */
			continue;
		}
		SDL_mutexP(data->lock);
		/* print out the message */
		data->message_list->push_back(message);
		SDL_mutexV(data->lock);
        //outfile2 << "Added: " << data->message_list->size() << endl;
		//outfile2 << "Received: " << data->message_list->front() << endl;
	}
	return 0;
}
void 
server_send(std::string data){
	while(!client){
	}
	// move string data into message buffer
	len2 = data.copy(message2,data.size(),0);
	//outfile << "data: " << data << endl;
	//outfile << "message: " << message2 << endl;
	// strip the newline 
	message2[len2]='\0';
	
	if(len2)
	{
		int result;
		
		// print out the message 
		//outfile << "Sending: " << message2 << endl;

		result=SDLNet_TCP_Send(client,message2,len2); // add 1 for the NULL 
		if(result<len2){
			//outfile << "SDLNet_TCP_Send: " << SDLNet_GetError() << endl;
        }
	}
}
void 
quit_server(){

	SDL_KillThread(rd_thread);
	SDL_KillThread(fc_thread);
	outfile.close();
	outfile2.close();
	if ( server != NULL ) {
		SDLNet_TCP_Close(server);
		server = NULL;
	}
	/* shutdown SDL_net */
	SDLNet_Quit();

	/* shutdown SDL */
	SDL_Quit();
}
