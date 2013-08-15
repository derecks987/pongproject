#include "tcpclient.h"

IPaddress c_ip;
TCPsocket c_sock;
char c_message[1024];
char c_message2[1024];
int c_len,c_len2;
Uint16 c_port;
ofstream c_outfile;
SDL_Thread *c_rd_thread=NULL;

void 
init_client(std::string hostname)
{
	c_outfile.open("net_client_err.txt");
	/* initialize SDL */
	if(SDL_Init(0)==-1)
	{
		c_outfile << "SDL_Init: " << SDL_GetError() << endl;
		exit(1);
	}

	/* initialize SDL_net */
	if(SDLNet_Init()==-1)
	{
		c_outfile << "SDLNet_Init: " << SDLNet_GetError() << endl;
		exit(2);
	}

	/* get the port */
	c_port=(Uint16)49160;
	
	/* Resolve the argument into an IPaddress type */
	/*string hostname;
	ifstream infile;
	infile.open ("host.txt");
	getline(infile,hostname); // Saves the line in STRING.
	infile.close();*/
	if(SDLNet_ResolveHost(&c_ip,hostname.c_str(),c_port)==-1)
	{
		c_outfile << "hostname:" << hostname << ":" << endl;
		c_outfile << "SDLNet_ResolveHost: " << SDLNet_GetError() << endl;
		exit(3);
	}

	/* open the server socket */
	c_sock=SDLNet_TCP_Open(&c_ip);
	if(!c_sock)
	{
		c_outfile << "SDLNet_TCP_Open: " << SDLNet_GetError() << endl;
		exit(4);
	}
}

void 
client_send(std::string data){

	//move string data into message buffer
	c_len = data.copy(c_message,data.size(),0);
	//c_outfile << "data: " << data << endl;
	//c_outfile << "c_message: " << c_message << endl;
	// strip the newline 
	c_message[c_len]='\0';
	
	if(c_len)
	{
		int result;
		
		// print out the message 
		//c_outfile << "Sending: " << c_message << endl;

		result=SDLNet_TCP_Send(c_sock,c_message,c_len); // add 1 for the NULL 
		if(result<c_len){
			//c_outfile << "SDLNet_TCP_Send: " << SDLNet_GetError() << endl;
        }
	}
}
void
client_receive(thread_data *data){
	//SDL_WaitThread(connect_thread,NULL);
	c_rd_thread = SDL_CreateThread(c_rd,(void *)data);
}
int 
c_rd(void *d){	
	thread_data *data = (thread_data *) d;
	bool done = false;
	while(!done){
		if(!c_sock){
			//outfile2 << "checK";
			SDL_Delay(100);
			continue;
		}

		/* read the buffer from client */
		c_len2=SDLNet_TCP_Recv(c_sock,c_message2,1024);
		//SDLNet_TCP_Close(client);
		if(!c_len2)
		{
			//c_outfile << "SDLNet_TCP_Recv: " << SDLNet_GetError() << endl;
			SDL_Delay(20); /*sleep 1/10th of a second */
			continue;
		}
		SDL_mutexP(data->lock);
		/* print out the message */
		data->message_list->push_back(c_message2);
		SDL_mutexV(data->lock);
        //c_outfile << "Size: " << data->message_list->size() << endl;
		//c_outfile << "Received: " << data->message_list->front() << endl;
	}
	return 0;
}
void 
quit_client(){

	SDL_KillThread(c_rd_thread);
	if(c_sock != NULL){
		SDLNet_TCP_Close(c_sock);
		c_sock = NULL;
	}
	/* shutdown SDL_net */
	SDLNet_Quit();

	/* shutdown SDL */
	SDL_Quit();

	c_outfile.close();
}
