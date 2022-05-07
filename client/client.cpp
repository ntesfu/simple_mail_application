#include "client.h"

using namespace std;

void TcpClient::modeSignin(char **inputfrom)
{
	int		cmd;
	int		rep;
	
	// User choice	
	rep = -1;
	*inputfrom = new char[FROM_LENGTH];
	cout << "Welcome to SHAHOM Mail application!!\n";
	while (true){
		cout << "To signup for mail service, press 1. Signin, press 2. Signin to receive, press 3!: ";
		cin >> cmd;
		cin.ignore();
		cout <<endl;
		if (cmd == 1){
			tp = SGN_UP;
			if ((rep = authenticateClient(SGN_UP, *inputfrom)) != 0)
				printf("Error signing up client!\n\n");
		}
		else if (cmd == 2 || cmd == 3){
			tp = (cmd == 2) ? SGN_IN : SGNIN_RECV;
			if ((rep = authenticateClient(tp, *inputfrom)) != 0)
				printf("Error signing in client!\n\n");
			if (tp == SGNIN_RECV && rep == 0)
				receiveEmail(sock);
		}
		if (rep == 0 && cmd == SGN_IN)
			break;
	}
}

/*the main function that handles the client process*/
void TcpClient::run(int argc, char* argv[])
{
	int				cmd;
	char*			inputfrom;
	stack<char *>	multiReceivers;
	//initilize winsocket
	if (WSAStartup(0x0202, &wsadata) != 0)
	{
		WSACleanup();
		err_sys("Error in starting WSAStartup()\n");
	}
	// User choice	
	modeSignin(&inputfrom);
	//by this point sign in is successful, tying to send email
	FILE*			fa;
	char*			inputbody;
	char*			filepath;
	char*			modFileName;

	Esendp = (Esend *)smsg.buffer;
	if (gethostname(Esendp->hostname, HOSTNAME_LENGTH) != 0) //get the hostname
		err_sys("can not get the host name,program exit");
	cout << "Mail Client starting on host: "<<Esendp->hostname<<endl;
	cout << endl;
	
	
	//fill email pointer from terminal
	int body_len = fillEmailPointerTerminal(&multiReceivers, &inputbody, fa, &modFileName, inputfrom);
	//printf("bodylen:%d:%d:%s\n", body_len, multiReceivers.size(), modFileName);
	
	Esendp->num_receivers = multiReceivers.size();
	while (!multiReceivers.empty())
	{
		//printStructure(smsg);
		int valid = 1;
		printf("\nSending email to: %s\n", headerp->to);
		multiReceivers.pop();
		//save email constructed to file
		if ((saveEmailToFile(&smsg, inputbody, SGN_IN) != 0))
			err_sys("Error, could not save email to file!");

		//send out the Email constructed with the file attached
		if (msg_send(sock, &smsg, inputbody) != sizeof(Esend) + body_len)
			err_sys("Sending req packet error.,exit");
		
		//receive the response from server
		if (msg_recv(sock, &rmsg, NULL) != rmsg.length)
			err_sys("recv response error,exit");	
		respp = (Resp*)rmsg.buffer;		//cast it to the response structure
		if (strcmp(respp->rcode, "250 OK") == 0)
			printf("Email received successfully at %s\n", respp->tstamp);
		else if (strcmp(respp->rcode, "501") == 0){
			printf("Receipent was invalid. Email could not be sent\n");
			valid = 0;
		}
		else if (strcmp(respp->rcode, "550") == 0)
			printf("Receiver is not connected to server\n");
		else
			printf("Email was not sent!\n");
		
		// send file attachment after we are sure we can send it
		if (Esendp->file_size != -1 && valid == 1){
			if ((sendFileAttachment(fa, &smsg, modFileName)) != Esendp->file_size)
				err_sys("Error, could not send file attachment\n");
		}
		strcpy(headerp->to, multiReceivers.top());
		
	}
	if (modFileName) free(modFileName);
	fclose(fa);
	closesocket(sock);		//close the client socket
	//shahad@shahom.ae abdulla@shahom.ae yakob@shahom.ae
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

int main(int argc, char* argv[]) 
{
	TcpClient* tc = new TcpClient();
	tc->run(argc, argv);
	return 0;
}
