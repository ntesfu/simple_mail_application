#include "server.h"

using namespace std;
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

/*
msg_recv returns the length of bytes in the msg_ptr->buffer,which have been recevied successfully.
*/
int TcpThread::msg_recv(int sock,Email * msg_ptr, char **body)
{
	int rbytes,n;
	
	for(rbytes=0;rbytes<MSGHDRSIZE;rbytes+=n)
		if((n=recv(sock,(char *)msg_ptr+rbytes,MSGHDRSIZE-rbytes,0))<=0)
			err_sys("Recv MSGHDR Error");
		
	for(rbytes=0;rbytes<msg_ptr->length;rbytes+=n)
		if((n=recv(sock,(char *)msg_ptr->buffer+rbytes,msg_ptr->length-rbytes,0))<=0)
			err_sys("Recevier Buffer Error");

	//printf("%d:receiving body\n:%s",msg_ptr->type,msg_ptr->buffer);
	if (msg_ptr->type == EMAIL){
		//printf("receiving body\n");
		Esendp = (Esend *)msg_ptr->buffer;
		*body = (char *)malloc(sizeof(char) * Esendp->body_length + 1);
		//printf("%d:%s\n", Esendp->body_length, body);
		for(rbytes=0 ; rbytes<Esendp->body_length;rbytes+=n)
			if((n=recv(sock, (char *)*body + rbytes, Esendp->body_length-rbytes, 0)) <= 0)
				err_sys("Recevier Buffer Error");
		//printf("%d:%s:%d\n", Esendp->body_length, *body,n);
	}
	return msg_ptr->length;
}

/* msg_send returns the length of bytes in msg_ptr->buffer,which have been sent out successfully
*/
int TcpThread::msg_send(int sock, Email* msg_ptr, char *body)
{
	int n;
	int ret = 0;
	if ((n = send(sock, (char*)msg_ptr, MSGHDRSIZE + msg_ptr->length, 0)) != (MSGHDRSIZE + msg_ptr->length))
		err_sys("Send MSGHDRSIZE+length Error");
	ret += n;
	if (msg_ptr->type == EMAIL)
	{
		Esend *send_ptr = (Esend *)msg_ptr->buffer;
		//printf("body:%d;bdyptr:%d\n",sizeof(body),send_ptr->body_length);
		if ((n = send(sock, (char*)body, send_ptr->body_length, 0)) != (send_ptr->body_length))
			err_sys("Send MSGHDRSIZE+length Error");
		ret += n;
	}
	//printf("send size:%d\n", ret - MSGHDRSIZE);
	return (ret - MSGHDRSIZE);
}

void TcpThread::run() //cs: Server socket
{
    int		res;
	char	*body;
	char	*modFileName;
	FILE	*frecv;
	
	smsg.type=RESP;	
	respp=(Resp *)smsg.buffer;
	if(msg_recv(cs,&rmsg, &body)!=rmsg.length)
		err_sys("Receive Req error,exit");
	//printf("Email: %d:%d:%s\n", rmsg.type,rmsg.length,rmsg.buffer);
	if (rmsg.type == SGN_UP || rmsg.type == SGN_IN || rmsg.type == SGNIN_RECV){
		if ((res = authenticateClient(&rmsg)) != 0){
			curr_time = time(NULL);
			sprintf(respp->rcode, "%s", "501 ERROR");
			sprintf(respp->tstamp,"%s", ctime(&curr_time));
			smsg.length=sizeof(Resp);			
			if(msg_send(cs,&smsg, NULL)!=smsg.length)
				err_sys("send Response failed,exit");
			closesocket(cs);
			printf("\nwaiting to be contacted for transferring Mail...1\n\n");
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

	//printf("%d\n", rmsg.type);
	if (rmsg.type == SGN_IN){
		printf("sign in!\n");
		if(msg_recv(cs, &rmsg, &body)!=rmsg.length)
			err_sys("Receive Req error,exit");
	}
	else if (rmsg.type == SGN_UP){
		printf("sign up!\n");
		closesocket(cs);
		printf("\nwaiting to be contacted for transferring Mail...2\n\n");
	}
	else if (rmsg.type == SGNIN_RECV){
		//signp->hostname
		printf("Signin to receive\n");
		char *entry = new char[FROM_LENGTH + 10];
		sprintf(entry, "%s %d\n", signp->clientname, cs);
		appendToFile("clientMap.txt", entry);
		printf("\nwaiting to be contacted for transferring Mail...4\n\n");
		// checking if the client is disconnecting
		int r = recv(cs, NULL, 0, 0);
		if(r == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET){
			printf("Clinet %s has disconnected!\n\n", signp->clientname);
			eraseClientFromMapping("clientMap.txt", signp->clientname);
		}
		return;
	}
	//printf("time to step up\n");
	if (rmsg.type == EMAIL){
		//cast it to the request packet structure
		//Esendp=(Esend *)rmsg.buffer;
		printf("Email receive\n");

		headerp=(Header *)Esendp->header;
		smsg.length=sizeof(Esend);

		printf("Receive a request from client: %s\n",Esendp->hostname);
		//printf("%s\n", headerp->to);
		if (checkClientEntry(headerp->to, NULL, 2) != 0){
			printf("Invalid Email Receipent!\n");
			strcpy(respp->rcode, "501");
			if(msg_send(cs,&smsg,NULL)!=smsg.length)
				err_sys("send Response failed,exit");
			printf("\nwaiting to be contacted for transferring Mail...2\n\n");
			return;
		}
		int sock;
		if ((sock = checkClientMapping(headerp->to)) <= 0){
			printf("Client is not connected to receive!\n");
			strcpy(respp->rcode, "550");
			if(msg_send(cs,&smsg,NULL)!=smsg.length)
				err_sys("send Response failed,exit");
			printf("\nwaiting to be contacted for transferring Mail...2\n\n");
			return;
		}
		
		//sending data to connected client
		int body_len = ((Esend *)rmsg.buffer)->body_length;
		if(msg_send(sock,&rmsg,body)!=rmsg.length + body_len)
			err_sys("send Respose failed,exit");

		//get optional file if exists
		//printf("file size:%ld\n", Esendp->file_size);
		int x;
		if (Esendp->file_size != -1){
			if ((x = receiveFileAttachment(cs, &rmsg, &modFileName, frecv)) != Esendp->file_size)
				err_sys("Error, could not receive the file attachment");
			if ((x = sendFileAttachment(sock, &rmsg, modFileName, frecv)) != Esendp->file_size)
				err_sys("Error, could not send the file attachment");
		}
	
		if(msg_send(cs,&smsg,NULL)!=smsg.length)
			err_sys("send Respose failed,exit");
		printf("Mail Received from %s\n", Esendp->hostname);
		printf("From: %s\n", headerp->from);
		printf("To: %s\n", headerp->to);
		printf("Subject: %s\n", headerp->subject);
		printf("Time: %s\n", headerp->tstamp);
		printf("%s\n", body);
		if (body)
			free(body);
		closesocket(cs);
		printf("\nwaiting to be contacted for transferring Mail...3\n\n");
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
			printf("Successful signin!\n");
	}
	else
	{
		if (checkClientEntry(signp->clientname, signp->password, 0) == 0){
			printf("Error, Could not create client!\n");
			return -1;
		}
		char *entry = new char[sizeof(signp->clientname) + sizeof(signp->clientname) + 1];
		sprintf(entry, "%s %s\n", signp->clientname, signp->password);
		appendToFile("clients.txt", entry);
		printf("Successful user client signup for user: %s\n",signp->clientname);
	}
	return 0;
}

int TcpThread::appendToFile(const char *fname, char *entry)
{
	ofstream fclient;
	fclient.open((char *)fname, ios::app | ios::out);	
	if (fclient.is_open()){
		fclient << entry;
		fclient.close();
		return 0;
	}
	printf("Unable to open file\n");
	return -1;	
}

int TcpThread::IndexOf(string str, char c)
{
	int i = 0;
	while (i < str.length()){
		if (str[i] == c)
			return (i);
		i++;
	}
	return (-1);
}

char	*TcpThread::ft_substr(string s, unsigned int start, size_t len)
{
	char	*res;
	int		i;

	i = 0;	
	res = (char *)malloc(sizeof(char) * (len + 1));
	if (res == NULL)
		return (NULL);
	while (i < (int)len && s[start])
		res[i++] = s[start++];
	res[i] = '\0';
	return (res);
}

int TcpThread::checkClientEntry(char *user, char *passwd, int full){	
    char	*client;
	char	*clientpass;
	string	line;
	fstream	fclient;
	int		res = -1;
    int		n_ind;
	
	if (full == 0)
		strcat(user,"@shahom.ae");

	fclient.open("clients.txt", ios::in);
	if (fclient.is_open())
	{
		while (getline(fclient,line))
		{
            n_ind = IndexOf(line, ' ');
			client = ft_substr(line, 0, n_ind);
            clientpass = ft_substr(line, n_ind + 1, line.length() - n_ind);
			if (full == 1 && strcmp(client,user) == 0 && strcmp(clientpass,passwd) == 0){
				res = 0;
                break;}
			else if ((full == 0 || full == 2) && strcmp(client,user) == 0){
				res = 0;
                break;}
			free(client);
    		free(clientpass);
		}
		if (client)
			free(client);
    	if (clientpass)
			free(clientpass);
		fclient.close();
	}
	return res;
}

int TcpThread::checkClientMapping(char *user)
{
	char	*client;
	char	*sockChar;
	int		sock;
	string	line;
	fstream	fclient;
	int		res = -1;
    int		n_ind;

	fclient.open("clientMap.txt", ios::in);
	if (fclient.is_open())
	{
		while (getline(fclient,line))
		{
            n_ind = IndexOf(line, ' ');
			client = ft_substr(line, 0, n_ind);
            sockChar = ft_substr(line, n_ind + 1, line.length() - n_ind);
			if (strcmp(client,user) == 0){
				res = atoi(sockChar);
                break;}
			free(client);
    		free(sockChar);
		}
		if (client)
			free(client);
		if (sockChar)
			free(sockChar);
		fclient.close();
	}
	return res;
}

int TcpThread::eraseClientFromMapping(const char *fname, char *user)
{
	char		*client;
	string		line;
	ifstream	fin;
	ofstream	temp;
	int			res = -1;
    int			n_ind;

	fin.open(fname, ios::in);
	temp.open("temp.txt");
	if (fin.is_open())
	{
		res = 0;
		while (getline(fin,line))
		{
            n_ind = IndexOf(line, ' ');
			client = ft_substr(line, 0, n_ind);
			if (strcmp(client,user) != 0)
				temp <<line<<endl;
			free(client);
		}
		if (client)
			free(client);
		fin.close();
		temp.close();
		remove(fname);
		rename("temp.txt", fname);
	}
	return res;
}

long	TcpThread::receiveFileAttachment(int cs, Email *rmsg, char **fname, FILE *frecv)
{
	int		n;
	int		size;
	long	ret;
	char	*buf;
	long	offset;

	printf("Receiving file\n\n");
	//printf(":%d\n", Esendp->filename_size);
	*fname = new char[Esendp->filename_size + 1];
	//for (offset = 0; offset < Esendp->filename_size; offset += n)
	if ((n = recv(cs, *fname, Esendp->filename_size, 0)) <= 0)
			err_sys("File name receive error");
	(*fname)[Esendp->filename_size] = '\0';
	//printf("file to recv:%s\n", *fname);
	//printf("fname size read:%d:%s\n", n, *fname);
	if ((frecv = fopen(*fname, "w")) == NULL){
		printf("Could not open a file to write\n");
		return (-1);
	}
	//Esendp = (Esend *)rmsg->buffer;
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (!buf) return (-1);
	for(offset = 0; offset < Esendp->file_size; offset += n)
	{
		if ((n = recv(cs, buf, MTU_SIZE, 0)) <= 0)
			err_sys("Reading file error");
		//printf("%s",buf);
		fwrite(buf, sizeof(char), n, frecv);
		fflush(frecv);
	}
	if (buf) free(buf);
	fclose(frecv);
	return (offset);
}

long	TcpThread::sendFileAttachment(int cs, Email *rmsg, char *fname, FILE *frecv)
{
	printf("Sending file attachment\n\n");
	int		n;
	int		size;
	long	ret;
	char	*buf;
	int		offset;

	ret = 0;
	//printf("file name to send:%s\n", fname);
	if ((frecv = fopen(fname, "r")) == NULL){
		printf("Could not open a file to write\n");
		return (-1);
	}
	//printf("fname length:%d\n", strlen(modFileName));
	if ((n = send(cs, (char*)fname, Esendp->filename_size, 0)) != Esendp->filename_size)
		err_sys("Send File Name error");
	//printf("%d",n);
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (!buf) return (-1);
	while ((size = fread(buf, sizeof(char), MTU_SIZE, frecv)) > 0){
		ret += size;	
		offset = 0;
		//printf("%s",buf);
		while ((n = send(cs, buf + offset, size, 0)) > 0){
			offset += n; 
			size -= n;
		}
		//fwrite(buf, 1, MTU_SIZE, fout);
	}
	if (buf) free(buf);
	//printf("file size sent:%ld:%ld\n", ret, Esendp->file_size);
	fclose(frecv);
	return (ret);
}


////////////////////////////////////////////////////////////////////////////////////////

int main(void)
{
	TcpServer ts;
	ts.start();	
	return 0;
}


