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

/*msg_recv returns the length of bytes in the msg_ptr->buffer,which have been recevied successfully.*/
int TcpThread::msg_recv(int sock,Email * msg_ptr, char **body)
{
	int rbytes,n;
	
	for(rbytes=0;rbytes<MSGHDRSIZE;rbytes+=n)
		if((n=recv(sock,(char *)msg_ptr+rbytes,MSGHDRSIZE-rbytes,0))<=0)
			err_sys("Recv MSGHDR Error");
		
	for(rbytes=0;rbytes<msg_ptr->length;rbytes+=n)
		if((n=recv(sock,(char *)msg_ptr->buffer+rbytes,msg_ptr->length-rbytes,0))<=0)
			err_sys("Recevier Buffer Error");

	if (msg_ptr->type == EMAIL){
		Esendp = (Esend *)msg_ptr->buffer;
		*body = (char *)malloc(sizeof(char) * Esendp->body_length + 1);
		for(rbytes=0 ; rbytes<Esendp->body_length;rbytes+=n)
			if((n=recv(sock, (char *)*body + rbytes, Esendp->body_length-rbytes, 0)) <= 0)
				err_sys("Recevier Buffer Error");
	}
	return msg_ptr->length;
}

/* msg_send returns the length of bytes in msg_ptr->buffer,which have been sent out successfully*/
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
		int offset;
		for(offset = 0; offset < send_ptr->body_length; offset += n){
			if ((n = send(sock, body + offset, send_ptr->body_length - offset, 0)) != (send_ptr->body_length))
				err_sys("Send MSGHDRSIZE+length Error");
			offset += n;
			ret += n;
		}
	}
	return (ret - MSGHDRSIZE);
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
		printf("\nwaiting to be contacted for transferring Mail...\n\n");
		// checking if the client is disconnecting
		int r = recv(cs, NULL, 0, 0);
		if(r == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET){
			printf("Client %s has disconnected!\n\n", signp->clientname);
			eraseClientFromMapping("clientMap.txt", signp->clientname);
		}
		return;
	}
	if (rmsg.type == EMAIL){		
		//cast it to the request packet structure
		headerp=(Header *)Esendp->header;
		smsg.length=sizeof(Esend);

		printf("Receive a request from client: %s\n",Esendp->hostname);
		if (checkClientEntry(headerp->to, NULL, 2) != 0){
			printf("Invalid Email Receipent!\n");
			strcpy(respp->rcode, "501");
			if(msg_send(cs,&smsg,NULL)!=smsg.length)
				err_sys("send Response failed,exit");
			printf("\nwaiting to be contacted for transferring Mail...\n\n");
			return;
		}

		// save email to inbox of receiver and sent of sender
		if (saveEmailToFile(cs, &rmsg, body, &modsentFile, frecv) != 0)
			err_sys("Saving Email to file failed, exit");

		int sock;
		if ((sock = checkClientMapping(headerp->to)) <= 0){
			printf("Client is not connected to receive!\n");
			strcpy(respp->rcode, "550");
			if(msg_send(cs,&smsg,NULL)!=smsg.length)
				err_sys("send Response failed,exit");
			printf("\nwaiting to be contacted for transferring Mail...\n\n");
			return;
		}
		printf("Receiving client is connected\n");
		
		//sending data to connected client
		printf("Sending Email to receiver client\n");
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
	
		if(msg_send(cs,&smsg,NULL)!=smsg.length)
			err_sys("send Response failed,exit");
		if (body)
			free(body);
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
		if (makeDir(rmsg) != 0)
			return -1;
		if (appendToFile("clients.txt", entry) != 0)
			return -1;		
		printf("Successful user client signup for user: %s\n",signp->clientname);
	}
	return 0;
}

int TcpThread::appendToFile(const char *fname, char *entry)
{
	ofstream fclient;
	printf("file to open:%s", fname);
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
		getline(fclient,line);
		while (!fclient.eof())
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
			if (client) free(client);
			if (clientpass) free(clientpass);
			getline(fclient,line);
			if (line.length() == 0)
				break;
		}
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

int	TcpThread::makeDir(Email *rmsg)
{
	char	*saveDir;
	char	*emailFolder = new char[10];
	
	signp = (Sign *)rmsg->buffer;
	saveDir = new char[FROM_LENGTH + 20];
	strcpy(emailFolder, "emails");
	_mkdir(emailFolder);
	sprintf(saveDir, "%s\\%s", emailFolder, signp->clientname);
	//printf("Dirname1:%s\n",saveDir);
	if (_mkdir(saveDir) != 0){
		printf("could not create directory, error");
		return -1;
	}
	sprintf(saveDir, "%s\\%s\\%s", emailFolder, signp->clientname, "sent");
	//printf("Dirname2:%s\n",saveDir);
	if (_mkdir(saveDir) != 0){
		printf("could not create directory, error");
		return -1;
	} //create directory if not exist
	sprintf(saveDir, "%s\\%s\\%s", emailFolder, signp->clientname, "inbox");
	//printf("Dirname3:%s\n",saveDir);
	if (_mkdir(saveDir) != 0){
		printf("could not create directory, error");
		return -1;
	} //create directory if not exist
	return 0;
}

int	TcpThread::copyFile(char *srcFname, char *destFname)
{
	char	*buf;
	FILE	*fin;
	FILE	*fout;
	int		size;
	int 	n;

	printf("sourcefile:%s, dest:%s\n", srcFname, destFname);
	if ((fin = fopen(srcFname, "r")) == NULL){
		printf("Could not open a file to read when copying\n");
		return (-1);
	}
	if ((fout = fopen(destFname, "w")) == NULL){
		printf("Could not open a file to write when copying\n");
		return (-1);
	}
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (buf == NULL){
		printf("memory error!"); return -1;
	}	
	while ((size = fread(buf, sizeof(char), MTU_SIZE, fin)) > 0)
		if ((n = fwrite(buf, sizeof(char), size, fout)) < 0)
			return -1;
	if (buf) free(buf);
	fclose(fin);
	fclose(fout);
	return 0;
}

/*save Email and attachment from sender to senders and receivers client*/
int	TcpThread::saveEmailToFile(int cs, Email *rmsg, char *body, char **fname, FILE *frecv)
{
	printf("Saving Email to file\n");
	Esend	*email;
	Header	*header;
	char	*sentFile;
	char	*inboxFile;

	email = (Esend *)rmsg->buffer;
	header = (Header *)email->header;
	sentFile = new char[FSIZE];	
	inboxFile = new char[FSIZE];	
	sprintf(sentFile, "emails\\%s\\sent\\%s_%s.txt", header->from, header->subject, header->tstamp);
	sprintf(inboxFile, "emails\\%s\\inbox\\%s_%s.txt", header->to, header->subject, header->tstamp);
	ofstream fclient;
	fclient.open(sentFile);
	if (fclient.is_open()){
		fclient << "FROM: "<<header->from<<endl;
		fclient << "TO: "<<header->to<<endl;
		fclient << "SUBJECT: "<<header->subject<<endl;
		fclient << "TIME STAMP: "<<header->tstamp<<endl;
		fclient << body<<endl;
		fclient.close();
	}
	else{
		printf("Unable to open file\n");
		return -1;
	}
	if (copyFile(sentFile, inboxFile) != 0)
		return -1;	
	if (Esendp->file_size != -1)
		if ((receiveFileAttachment(cs, rmsg, fname, frecv)) != Esendp->file_size)
			err_sys("Error, could not receive the file attachment");
	return 0;
}

/*receive file attachment from sender*/
long	TcpThread::receiveFileAttachment(int cs, Email *rmsg, char **fname, FILE *frecv)
{
	int		n;
	int		size;
	long	ret;
	char	*buf;
	long	offset;
	char	*sentFile;
	char	*inboxFile;
	Header	*header;

	printf("Receiving file attachment\n");
	sentFile = new char[FSIZE];
	*fname = new char[Esendp->filename_size + 1];
	inboxFile = new char[FSIZE];
	header = (Header *)Esendp->header;	
	if ((n = recv(cs, *fname, Esendp->filename_size, 0)) <= 0)
			err_sys("File name receive error");
	(*fname)[Esendp->filename_size] = '\0';
	sprintf(sentFile, "emails\\%s\\sent\\%s", header->from, *fname);
	printf("file name:%s\n", *fname);
	if ((frecv = fopen(sentFile, "w")) == NULL){
		printf("Could not open a file to write\n");
		return (-1);
	}
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (!buf) return (-1);
	for(offset = 0; offset < Esendp->file_size; offset += n)
	{
		if ((n = recv(cs, buf, MTU_SIZE, 0)) <= 0)
			err_sys("Reading file error");
		fwrite(buf, sizeof(char), n, frecv);
		fflush(frecv);
	}
	if (buf) free(buf);
	fclose(frecv);
	sprintf(inboxFile, "emails\\%s\\inbox\\%s", header->to, *fname);
	if (copyFile(sentFile, inboxFile) != 0)
		return -1;
	return (offset);
}

/*sending file attachment to receiver process*/
long	TcpThread::sendFileAttachment(int cs, Email *rmsg, char *fname, FILE *frecv)
{
	printf("Sending file attachment\n\n");
	int		n;
	int		size;
	long	ret;
	char	*buf;
	int		offset;
	char	*inboxFile;

	ret = 0;
	inboxFile = new char[FSIZE];
	sprintf(inboxFile, "emails\\%s\\inbox\\%s", ((Header *)(Esendp->header))->to, fname);
	if ((frecv = fopen(inboxFile, "r")) == NULL){
		printf("Could not open a file to write\n");
		return (-1);
	}
	if ((n = send(cs, (char*)fname, Esendp->filename_size, 0)) != Esendp->filename_size)
		err_sys("Send File Name error");
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (!buf) return (-1);
	while ((size = fread(buf, sizeof(char), MTU_SIZE, frecv)) > 0){
		ret += size;	
		offset = 0;
		while ((n = send(cs, buf + offset, size, 0)) > 0){
			offset += n; 
			size -= n;
		}
	}
	if (buf) free(buf);
	fclose(frecv);
	return (ret);
}

int main(void)
{
	TcpServer ts;
	ts.start();	
	return 0;
}
