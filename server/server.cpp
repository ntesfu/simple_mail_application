#include "server.h"

TcpServer::TcpServer()
{
	WSADATA wsadata;
	if (WSAStartup(0x0202,&wsadata)!=0)
		TcpThread::err_sys("Starting WSAStartup() error\n");
	
	//Display name of local host
	if(gethostname(servername,HOSTNAME_LENGTH)!=0) //get the hostname
		TcpThread::err_sys("Get the host name error,exit");
	
	remove("clientMap.txt"); //make sure no client is considered connected
	printf("Mail Server starting at host: %s\n",servername);
	printf("waiting to be contacted for transferring Mail...\n\n");	
	
	//Create the server socket
	if ((serverSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		TcpThread::err_sys("Create socket error,exit");
	
	//Fill-in Server Port and Address info.
	ServerPort=REQUEST_PORT;
	memset(&ServerAddr, 0, sizeof(ServerAddr));      /* Zero out structure */
	ServerAddr.sin_family = AF_INET;                /* Internet address family */
	//inet_pton(AF_INET, "10.10.169.187", &ServerAddr.sin_addr);
	ServerAddr.sin_addr.s_addr = ResolveName(servername);  /* Any incoming interface */
	ServerAddr.sin_port = htons(ServerPort);         /* Local port */
	
	//Bind the server socket
    if (bind(serverSock, (struct sockaddr *) &ServerAddr, sizeof(ServerAddr)) < 0)
		TcpThread::err_sys("Bind socket error,exit");
	
	//Successfull bind, now listen for Server requests.
	if (listen(serverSock, MAXPENDING) < 0)
		TcpThread::err_sys("Listen socket error,exit");
}

TcpServer::~TcpServer()
{
	WSACleanup();
}

unsigned long TcpServer::ResolveName(char name[])
{
	struct hostent *host;            /* Structure containing host information */
	
	if ((host = gethostbyname(name)) == NULL)
		printf("gethostbyname() failed");
	
	/* Return the binary, network byte ordered address */
	return *((unsigned long *) host->h_addr_list[0]);
}

void TcpServer::start()
{
	for (;;) /* Run forever */
	{
		/* Set the size of the result-value parameter */
		clientLen = sizeof(ClientAddr);
		
		/* Wait for a Server to connect */
		if ((clientSock = accept(serverSock, (struct sockaddr *) &ClientAddr, 
			&clientLen)) < 0)
			TcpThread::err_sys("Accept Failed ,exit");
		
        /* Create a Thread for this new connection and run*/
		TcpThread * pt=new TcpThread(clientSock);
		pt->start();
		//printf("returned from thread\n");
	}
}

//////////////////////////////TcpThread Class //////////////////////////////////////////
void TcpThread::err_sys(const char * fmt,...)
{     
	perror(NULL);
	va_list args;
	va_start(args,fmt);
	remove("clientMap.txt"); //remove the connection file when exiting
	fprintf(stderr,"error: ");
	vfprintf(stderr,fmt,args);
	fprintf(stderr,"\n");
	va_end(args);
	exit(1);
}

unsigned long TcpThread::ResolveName(char name[])
{
	struct hostent *host;            /* Structure containing host information */
	
	if ((host = gethostbyname(name)) == NULL)
		err_sys("gethostbyname() failed");
	
	/* Return the binary, network byte ordered address */
	return *((unsigned long *) host->h_addr_list[0]);
}

void TcpThread::run() //cs: Server socket
{
    int		res;
	char	*body;
	char	*modsentFile;
	FILE	*frecv;
	
	smsg.type=RESP;	
	respp=(Resp *)smsg.buffer;
	if(msg_recv(cs,&rmsg, &body)!=rmsg.length)
		err_sys("Receive Req error,exit");
	if (rmsg.type == SGN_UP || rmsg.type == SGN_IN || rmsg.type == SGNIN_RECV){
		if ((res = authenticateClient(&rmsg)) != 0){
			curr_time = time(NULL);
			sprintf(respp->rcode, "%s", "501 ERROR");
			sprintf(respp->tstamp,"%s", ctime(&curr_time));
			smsg.length=sizeof(Resp);		
			if(msg_send(cs,&smsg, NULL)!=smsg.length)
				err_sys("send Response failed,exit");
			closesocket(cs);
			printf("\nwaiting to be contacted for transferring Mail...\n\n");
			return;
		}		
	}

	//contruct the response and send it out
	curr_time = time(NULL);
	sprintf(respp->rcode, "%s", "250 OK");
	sprintf(respp->tstamp,"%s", ctime(&curr_time));
	smsg.length=sizeof(Resp);
	if(msg_send(cs,&smsg,NULL)!=smsg.length)
			err_sys("send Response failed,exit");

	if (rmsg.type == SGN_IN){
		if(msg_recv(cs, &rmsg, &body)!=rmsg.length)
			err_sys("Receive Req error,exit");
	}
	else if (rmsg.type == SGN_UP){
		closesocket(cs);	//client is appended to client list when authenticating
		printf("\nwaiting to be contacted for transferring Mail...\n\n");
	}
	else if (rmsg.type == SGNIN_RECV){
		char *entry = new char[FROM_LENGTH + 10];
		sprintf(entry, "%s %d\n", signp->clientname, cs);
		appendToFile("clientMap.txt", entry);
		printf("about to send email\n");
		if (sendInboxToReceiver(cs, signp->clientname) != 0)
			err_sys("Inbox could not be sent to receiver, exit");
		printf("\nwaiting to be contacted for transferring Mail...\n\n");
		// checking if the client is disconnecting
		int r = recv(cs, NULL, 0, 0);
		if(r == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET){
			printf("Client %s has disconnected!\n\n", signp->clientname);
			eraseClientFromMapping("clientMap.txt", signp->clientname);
		}
	}
	if (rmsg.type == EMAIL){

		int receivers = Esendp->num_receivers;
		//printf("receiver:%d\n", receivers);
		for (int i = 0; i < receivers; i++)
		{
			int valid = 1;
			//printf("i:%d:%d\n", i, receivers);
			if (i != 0)
			{
				if(msg_recv(cs, &rmsg, &body)!=rmsg.length)
					err_sys("Receive Req error,exit");
			}
			//cast it to the request packet structure
			headerp=(Header *)Esendp->header;
			smsg.length=sizeof(Esend);

			printf("\nReceived email to client: %s, from: %s\n",headerp->to, headerp->from);
			if (checkClientEntry(headerp->to, NULL, 2) != 0){
				printf("Invalid Email Receipent!\n");
				strcpy(respp->rcode, "501");
				if(msg_send(cs,&smsg,NULL)!=smsg.length)
					err_sys("send Response failed,exit");
				//printf("\nwaiting to be contacted for transferring Mail...\n\n");
				continue;
			}
			//printf("finished checking client entry\n");

			int sock;
			if ((sock = checkClientMapping(headerp->to)) <= 0){
				printf("Client is not connected to receive!\n");
				strcpy(respp->rcode, "550");
				// if(msg_send(cs,&smsg,NULL)!=smsg.length)
				// 	err_sys("send Response failed,exit");
				valid = 0;
				//printf("\nwaiting to be contacted for transferring Mail...\n\n");
			}
			//printf("Receiving client is connected\n");

			//otherwise good to go: send code 250 or 550 to sender
			//printf("sending reply to sender\n");
			if(msg_send(cs,&smsg,NULL)!=smsg.length)
				err_sys("send Response failed,exit");
			
			

			// save email to inbox of receiver and sent of sender
			if (saveEmailToFile(cs, &rmsg, body, &modsentFile, frecv) != 0)
				err_sys("Saving Email to file failed, exit");

			if (valid == 1)
			{
				//sending data to connected client
				//printf("Sending Email to receiver client\n");
				int body_len = ((Esend *)rmsg.buffer)->body_length;
				if(msg_send(sock,&rmsg,body)!=rmsg.length + body_len)
				err_sys("send Respose failed,exit");
				//send optional file if exists
				if (Esendp->file_size != -1){
					// if ((x = receiveFileAttachment(cs, &rmsg, &modsentFile, frecv)) != Esendp->file_size)
					// 	err_sys("Error, could not receive the file attachment");
					if ((sendFileAttachment(sock, &rmsg, modsentFile, frecv)) != Esendp->file_size)
						err_sys("Error, could not send the file attachment");
					//remove(modsentFile);
				}
			}
			if (body)
				free(body);			
		}
		closesocket(cs);
		printf("\nwaiting to be contacted for transferring Mail...\n\n");
	}	
}

int TcpThread::authenticateClient(Email *rmsg)
{
	signp = (Sign *)rmsg->buffer;
	if (rmsg->type == SGN_IN || rmsg->type == SGNIN_RECV)
	{
		if (checkClientEntry(signp->clientname, signp->password, 1) != 0){
			printf("Error, Wrong client information, connection denied!\n");
			return -1;}
		else
			printf("Successful signin!\n\n");
	}
	else
	{
		if (checkClientEntry(signp->clientname, signp->password, 0) == 0){
			printf("Error, Could not create client!\n");
			return -1;
		}
		char *entry = new char[sizeof(signp->clientname) + sizeof(signp->clientname) + 1];
		sprintf(entry, "%s %s\n", signp->clientname, signp->password);
		if (makeDir(rmsg) != 0)
			return -1;
		if (appendToFile("clients.txt", entry) != 0)
			return -1;		
		printf("Successful user client signup for user: %s\n",signp->clientname);
	}
	return 0;
}

int main(void)
{
	TcpServer ts;
	ts.start();	
	return 0;
}
