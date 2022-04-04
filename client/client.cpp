#include "client.h"

using namespace std;

/*saves the email structure msg into a file*/
int	TcpClient::saveEmailToFile(Email *msg, char *body, Type tp)
{
	printf("Saving Email to file\n");
	Esend *email = (Esend *)msg->buffer;
	Header *header = (Header *)email->header;
	char *fname = new char[SUBJ_LENGTH + TSTAMP_LENGTH + 12];
	if (tp == SGN_IN)
		strcpy(fname,"send/");
	else
		strcpy(fname, "received/");
	strcat(fname,header->subject);
	strcat(fname, "_");
	strcat(fname, header->tstamp);
	strcat(fname, ".txt");

	ofstream fclient;
	fclient.open(fname);
	if (fclient.is_open()){
		fclient << "FROM: "<<header->from<<endl;
		fclient << "TO: "<<header->to<<endl;
		fclient << "SUBJECT: "<<header->subject<<endl;
		fclient << "TIME STAMP: "<<header->tstamp<<endl;
		fclient << body<<endl;
		fclient.close();
		return 0;
	}
	printf("Unable to open file\n");
	return -1;
}

/*copies the provided file path to the working directory of the sender process*/
int TcpClient::copyFileToDirectory(FILE *fa, Email *msg, char **modFileName, char *filepath)
{
	printf("Copying file to working directory\n");
	FILE	*fout;
	char	*fname;
	int 	idx;
	char	*fext;
	char	*buf;
	long	size;

	fname = new char[SUBJ_LENGTH + TSTAMP_LENGTH + 20];
	strcpy(fname, "send/");
	strcat(fname,headerp->subject);
	strcat(fname, "_");
	strcat(fname, headerp->tstamp);
	strcat(fname, "(op)");
	idx = IndexOf((string)filepath, '.');
	fext = ft_substr((string)filepath, idx, strlen(filepath) - idx);
	strcat(fname, fext);
	if (fext) free(fext);
	idx = IndexOf((string)fname, '/');
	*modFileName = ft_substr((string)fname, idx + 1, strlen(fname) - idx);
	Esendp->filename_size = strlen(*modFileName);
	if ((fout = fopen(fname, "w")) == NULL){
		printf("Could not open a file to copy\n");
		return (-1);
	}
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (!buf) return (-1);
	while ((size = fread(buf, sizeof(char), MTU_SIZE, fa)) > 0){
		fwrite(buf, sizeof(char), size, fout);
	}
	if (buf) free(buf);
	if (size < 0) return -1;
	fclose(fout);
	return 0;
}

/*sends the file attachment to the server, in order to be sent to receiver
fa is the file descriptor used to copy the file to working directory*/
int TcpClient::sendFileAttachment(FILE *fa, Email *msg, char *modFileName)
{
	printf("Sending file attachment\n");
	int		n;
	int		size;
	long	ret;
	char	*buf;
	int		offset;

	ret = 0;
	rewind(fa);	//rewinds file descriptor, to read from the beginning
	//send file name first
	if ((n = send(sock, (char*)modFileName, strlen(modFileName), 0)) != (strlen(modFileName)))
		err_sys("Send File Name error");
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (!buf) return (-1);
	while ((size = fread(buf, sizeof(char), MTU_SIZE, fa)) > 0){
		ret += size;	
		offset = 0;
		while ((n = send(sock, buf + offset, size - offset, 0)) > 0){
			offset += n;
			size -= n;
		}
	}
	fclose(fa); //fa job is done here
	return (ret); //return the size that was successfully sent
}

/*function to receive file attachment from server, it is used if the process
instance that is running is a receiving client process*/
long	TcpClient::receiveFileAttachment(Email *rmsg, char **fname, FILE *frecv)
{
	int		n;
	int		size;
	long	ret;
	char	*buf;
	long	offset;
	char	*temp;

	printf("Receiving file attachment\n");
	*fname = new char[Esendp->filename_size + 10];
	if ((n = recv(sock, *fname, Esendp->filename_size, 0)) <= 0)
		err_sys("File name receive error");
	(*fname)[Esendp->filename_size] = '\0';
	temp = strdup((const char *)*fname);
	strcpy(*fname, "received/");
	strcat(*fname, temp);
	free(temp);
	if ((frecv = fopen(*fname, "w")) == NULL){
		printf("Could not open a file to write\n");
		return (-1);
	}
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	for(offset = 0; offset < Esendp->file_size; offset += n)
	{
		if ((n = recv(sock, buf, MTU_SIZE, 0)) <= 0)
			err_sys("Reading file error");
		fwrite(buf, sizeof(char), n, frecv);
		fflush(frecv);
	}
	fclose(frecv);
	return (offset);
}

/*infinite looping function that is closed if the client instance wants to listen*/
void TcpClient::receiveEmail(int sock)
{
	char *body;
	char *fname;
	FILE *frecv;
	long x;
	for (;;)
	{
		printf("\nWaiting to receive an email!\n\n");
		if(msg_recv(sock,&rmsg, &body)!=rmsg.length)
			err_sys("Receive Req error,exit");
		headerp=(Header *)Esendp->header;
		printf("Received an Email from client: %s\n",Esendp->hostname);
		printf("Mail Received from %s\n", Esendp->hostname);
		printf("From: %s\n", headerp->from);
		printf("To: %s\n", headerp->to);
		printf("Subject: %s\n", headerp->subject);
		printf("Time: %s\n", headerp->tstamp);
		printf("%s\n\n", body);
		//save the email received as a file		
		if ((x = saveEmailToFile(&rmsg, body, SGNIN_RECV) != 0))
			err_sys("Error, could not save email to file!");
		if (body)
			free(body);
		//if there is a file attachment call receiveFileAttachment function
		if (Esendp->file_size != -1){
			if ((x = receiveFileAttachment(&rmsg, &fname, frecv)) != Esendp->file_size)
				err_sys("Error, could not receive the file attachment");
		}
	}
}

/*the main function that handles the client process*/
void TcpClient::run(int argc, char* argv[])
{
	int		cmd;
	int		rep;
	char*	inputfrom;
	//initilize winsocket
	if (WSAStartup(0x0202, &wsadata) != 0)
	{
		WSACleanup();
		err_sys("Error in starting WSAStartup()\n");
	}
	// User choice	
	rep = -1;
	inputfrom = new char[FROM_LENGTH];
	cout << "Welcome to SHAHOM Mail application!!\n";
	while (true){
		cout << "To signup for mail service, press 1. Signin, press 2. Signin to receive, press 3!: ";
		cin >> cmd;
		cin.ignore();
		cout <<endl;
		if (cmd == 1){
			tp = SGN_UP;
			if ((rep = authenticateClient(SGN_UP, inputfrom)) != 0)
				printf("Error signing up client!\n\n");
		}
		else if (cmd == 2 || cmd == 3){
			tp = (cmd == 2) ? SGN_IN : SGNIN_RECV;
			if ((rep = authenticateClient(tp, inputfrom)) != 0)
				printf("Error signing in client!\n\n");
			if (tp == SGNIN_RECV)
				receiveEmail(sock);
		}
		if (rep == 0 && cmd == SGN_IN)
			break;
	}
	//by this point sign in is successful, tying to send email
	FILE* fa;
	char* inputbody;
	char* filepath;
	char* modFileName;
	char* inputto = new char[TO_LENGTH];
	char* inputsubj = new char[SUBJ_LENGTH];
	Esendp = (Esend *)smsg.buffer;

	if (gethostname(Esendp->hostname, HOSTNAME_LENGTH) != 0) //get the hostname
		err_sys("can not get the host name,program exit");
	cout << "Mail Client starting on host: "<<Esendp->hostname<<endl;
	cout << endl;
	cout << "To: ";
	cin.getline(inputto, TO_LENGTH);
	cout << endl;
	cout << "Subject: ";
	cin.getline(inputsubj, SUBJ_LENGTH);
	cout << endl;
	cout << "Body: ";

	//read the body of the email and put it in variable length buffer
	int size = 0;
	int body_len = 0;
	inputbody = (char *)calloc(1, sizeof(char));
	char *temp;
	char *buf = (char *)malloc(sizeof(char) * 1025);	
	if ((size = read(0, buf, 1)) == -1)
		err_sys("Error reading terminal!");
	buf[size] = '\0';
	body_len += size;
	while (size != 0 && buf[size - 1] != '\n'){
		temp = (char *)malloc(sizeof(char) * (size + strlen(inputbody)));
		temp = strdup(inputbody);
		strcat(temp, buf);
		if (!inputbody)
			free(inputbody);
		inputbody = temp;
		if ((size = read(0, buf, 1)) == -1)
			err_sys("Error reading terminal!");
		buf[size] = '\0';
		body_len += size;
	}
	cout << endl;

	// copy read data to email buffer
	headerp = (Header *)Esendp->header;
	strcpy(headerp->to, inputto);
	strcpy(headerp->from, inputfrom);
	strcpy(headerp->subject, inputsubj);
	time(&curr_time);
	tmp = localtime(&curr_time);
	strftime(headerp->tstamp, TSTAMP_LENGTH, "%d-%m-%Y_%H-%M-%S", tmp);
	Esendp->body_length = body_len;
	smsg.length = sizeof(Esend);
	smsg.type = EMAIL;

	//Send optional file attachment
	cout <<"OPTIONAL: to send a file press 1, otherwise 0: ";
	cin	>>cmd;
	cin.ignore();
	cout <<endl;
	if (cmd == 1)
	{
		filepath = new char[FILE_LENGTH];
		cout <<"Input the file path: ";
		cin.getline(filepath, FILE_LENGTH);
		cout <<endl;
		if ((fa = fopen((const char *)filepath, "r")) == NULL)
			err_sys("could not open file specified\n");
		int n;
		long fsize = 0;
		char c;
		while ((n = fread(&c, sizeof(char), 1, fa)) > 0){
			fsize++;
		}
		Esendp->file_size = fsize;
		rewind(fa);
		int x;
		if ((x = copyFileToDirectory(fa, &smsg, &modFileName, filepath)) != 0)
			err_sys("Error, coudn't copying file to working directory\n");
	}
	else
		Esendp->file_size = -1;	//if no file attachment is choosen
	
	//save email constructed to file
	int x;
	if ((x = saveEmailToFile(&smsg, inputbody, SGN_IN) != 0))
		err_sys("Error, could not save email to file!");

	//send out the Email constructed with the file attached
	if (msg_send(sock, &smsg, inputbody) != sizeof(Esend) + body_len)
		err_sys("Sending req packet error.,exit");
	if (Esendp->file_size != -1){
		if ((x = sendFileAttachment(fa, &smsg, modFileName)) != Esendp->file_size)
			err_sys("Error, could not send file attachment\n");
		if (modFileName) free(modFileName);
	}

	//receive the response from server
	if (msg_recv(sock, &rmsg, NULL) != rmsg.length)
		err_sys("recv response error,exit");	
	respp = (Resp*)rmsg.buffer;		//cast it to the response structure
	if (strcmp(respp->rcode, "250 OK") == 0)
		printf("Email received successfully at %s\n", respp->tstamp);
	else if (strcmp(respp->rcode, "501") == 0)
		printf("Receipent was invalid. Email could not be sent\n");
	else if (strcmp(respp->rcode, "550") == 0)
		printf("Receiver is not connected to server\n");
	else
		printf("Email was not sent!\n");	
	closesocket(sock);		//close the client socket
}

/*autenticates user and returns 0 if user is not found */
int TcpClient::authenticateClient(Type tp, char *from)
{
	int ret;
	char *inputclientname = new char[FROM_LENGTH];
	char *inputpass = new char[PASS_LENGTH];

	cout << "Type name of MailServer: ";
	cin.getline(inputserverhostname,HOSTNAME_LENGTH);
	cout <<endl;
	cout <<"Enter client username: ";
	cin.getline(inputclientname, FROM_LENGTH);
	cout <<endl;
	cout <<"Enter password: ";
	cin.getline(inputpass, PASS_LENGTH);
	cout <<endl;
	//if (IndexOf(inputclientname, '@') == -1 || IndexOf(inputclientname, '.') == -1)
	//	err_sys("Wrong client email address, error exit");
	if (tp == SGN_UP)
	{
		char *inputconfirm = new char[PASS_LENGTH];
		cout <<"Confirm password: ";
		cin.getline(inputconfirm, PASS_LENGTH);
		if (strcmp(inputpass, inputconfirm) != 0){
			printf("Password don't match\n");
			//closesocket(sock);
			return -1;
		}
	}
	//file the sign structure to send for authentication to the server
	strcpy(from, inputclientname);
	signp = (Sign *)smsg.buffer;
	if (gethostname(signp->hostname, HOSTNAME_LENGTH) != 0) //get the hostname
		err_sys("can not get the host name,program exit");
	strcpy(signp->clientname, inputclientname);
	strcpy(signp->password, inputpass);
	
	//Create the socket
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) //create the socket 
		err_sys("Socket Creating Error");

	//connect to the server
	ServPort = REQUEST_PORT;
	memset(&ServAddr, 0, sizeof(ServAddr));     /* Zero out structure */
	ServAddr.sin_family = AF_INET;             /* Internet address family */
	ServAddr.sin_addr.s_addr = ResolveName(inputserverhostname);   /* Server IP address */
	ServAddr.sin_port = htons(ServPort); /* Server port */
	if (connect(sock, (struct sockaddr*)&ServAddr, sizeof(ServAddr)) < 0)
		err_sys("No mail server available with the name %s", inputserverhostname);

	//send out the message
	smsg.length = sizeof(Sign);
	smsg.type = tp;	
	if (msg_send(sock, &smsg, NULL) != sizeof(Sign))
		err_sys("Sending req packet error.,exit");

	//receive the response
	if (msg_recv(sock, &rmsg, NULL) != rmsg.length)
		err_sys("recv response error,exit");

	//cast it to the response structure and process
	respp = (Resp*)rmsg.buffer;
	if (strcmp(respp->rcode, "250 OK") == 0){
		if (tp == SGN_UP)
			printf("Email Client successfully created!\n");
		else
			printf("Successfully logged in!\n");
		ret = 0;
	}
	else{
		if (tp == SGN_UP)
			printf("Error, the username is already used!\n");
		else
			printf("Client login unsuccessful!\n");
		ret = -1;
		closesocket(sock);		//close the client socket if sign up(if signin keep the connection)
	}
	return ret;
}

TcpClient::~TcpClient()
{
	/* When done uninstall winsock.dll (WSACleanup()) and exit */
	WSACleanup();
}


void TcpClient::err_sys(const char* fmt, ...) //from Richard Stevens's source code
{
	perror(NULL);
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(1);
}

unsigned long TcpClient::ResolveName(char name[])
{
	struct hostent* host;            /* Structure containing host information */

	if ((host = gethostbyname(name)) == NULL)
		err_sys("Wrong server name");

	/* Return the binary, network byte ordered address */
	return *((unsigned long*)host->h_addr_list[0]);
}

/*
msg_recv returns the length of bytes in the msg_ptr->buffer,which have been recevied successfully.*/
int TcpClient::msg_recv(int sock,Email * msg_ptr, char **body)
{
	int rbytes,n;
	
	for(rbytes=0;rbytes<MSGHDRSIZE;rbytes+=n)
		if((n=recv(sock,(char *)msg_ptr+rbytes,MSGHDRSIZE-rbytes,0))<=0)
			err_sys("Recv MSGHDR Error");
		
	for(rbytes=0;rbytes<msg_ptr->length;rbytes+=n)
		if((n=recv(sock,(char *)msg_ptr->buffer+rbytes,msg_ptr->length-rbytes,0))<=0)
			err_sys("Recevier Buffer Error");
	//receiving body if type is EMAIL
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
int TcpClient::msg_send(int sock, Email* msg_ptr, char *body)
{
	int n;
	int ret = 0;
	if ((n = send(sock, (char*)msg_ptr, MSGHDRSIZE + msg_ptr->length, 0)) != (MSGHDRSIZE + msg_ptr->length))
		err_sys("Send MSGHDRSIZE+length Error");
	ret += n;
	//sends the body of the email if msg type is EMAIL(not sign)
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

int main(int argc, char* argv[]) 
{
	TcpClient* tc = new TcpClient();
	tc->run(argc, argv);
	return 0;
}
