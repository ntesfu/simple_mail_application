#include "server.h"

/*save Email and file attachment from sender to senders sent dir and receivers inbox directory
if successfully proccessed returns the value 0*/
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
		fclient << "TIMESTAMP: "<<header->tstamp<<endl;
		fclient << body;
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

/*checks if a client is saved in the clients.txt and returns 0 if found*/
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

/*checks if a client is mapped, meaning already connected to receive email*/
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
		fclient.close();

	}
	return res;
}

/*erases a client from mapping incase the client disconnects*/
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

/*generic function to append entry to any class given by fname*/
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

/*fills an email pointer from a file to be sent from a clients inbox when a
client signs up to receive*/
int	TcpThread::fillEmailPointer(char *filepath, char **body)
{
	ifstream	fin;
	string		seg;
	headerp = (Header *)Esendp->header;
	fin.open(filepath, ios::in);
	if (fin.is_open())
	{
		getline(fin, seg, ' ');
		getline(fin, seg, '\n');
		sprintf(headerp->from, "%s", (char *)seg.c_str());

		getline(fin, seg, ' ');
		getline(fin, seg);
		sprintf(headerp->to, "%s", (char *)seg.c_str());

		getline(fin, seg, ' ');
		getline(fin, seg);
		sprintf(headerp->subject, "%s", (char *)seg.c_str());
	
		getline(fin, seg, ' ');
		getline(fin, seg);
		sprintf(headerp->tstamp, "%s", (char *)seg.c_str());
	
		string line = "";
		getline(fin, seg);
		line = line + seg;
		while (getline(fin, seg))
			line = line + '\n' + seg;
		*body = (char *)malloc(sizeof(char) * (line.length() + 1));
		sprintf(*body, "%s", (char *)line.c_str());
		(*body)[line.length()] = '\0';
		fin.close();
		return 0;
	}
	return -1;
}
