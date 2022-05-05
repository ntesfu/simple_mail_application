#include "client.h"

int IndexOf(std::string str, char c)
{
	int i = 0;
	while (i < str.length()){
		if (str[i] == c)
			return (i);
		i++;
	}
	return (-1);
}

char	*ft_substr(std::string s, int start, int len)
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

void TcpClient::printStructure(Email msg)
{
	cout <<smsg.length<<endl;
	cout <<smsg.type<<endl;
	cout <<smsg.buffer<<endl;
	cout <<headerp->from<<endl;
	cout <<headerp->to<<endl;
	cout <<headerp->subject<<endl;
	cout <<headerp->tstamp<<endl;
	cout <<Esendp->body_length<<endl;
	cout <<Esendp->file_size<<endl;
	cout <<Esendp->filename_size<<endl;
	cout <<Esendp->num_receivers<<endl;
	cout <<Esendp->hostname<<endl;
}

int	TcpClient::getBodyFromTerminal(char **inputbody)
{
	//read the body of the email and put it in variable length buffer
	int size = 0;
	int body_len = 0;
	*inputbody = (char *)calloc(1, sizeof(char));
	char *temp;
	char *buf = (char *)malloc(sizeof(char) * 1025);
	if ((size = read(0, buf, 1)) == -1)
		err_sys("Error reading terminal!");
	buf[size] = '\0';
	body_len += size;
	while (size != 0 && buf[size - 1] != '\n'){
		temp = (char *)malloc(sizeof(char) * (size + strlen(*inputbody)));
		temp = strdup(*inputbody);
		strcat(temp, buf);
		if (!inputbody)
			free(inputbody);
		*inputbody = temp;
		if ((size = read(0, buf, 1)) == -1)
			err_sys("Error reading terminal!");
		buf[size] = '\0';
		body_len += size;
	}
	cout << endl;
	return body_len;
}

int	TcpClient::fillEmailPointerTerminal(stack<char *> *multiReceivers, char **inputbody, FILE *fa, char **modFileName, char *inputfrom)
{
	int		body_len;
	char	*filepath;
	char	*inputsubj;
	char	*seg;
	char	*inputto;
	int		cmd;

	seg = new char[10 * TO_LENGTH];
	inputto = new char[TO_LENGTH];
	inputsubj = new char[SUBJ_LENGTH];

	cout << "To: ";
	cin.getline(seg, 10 * TO_LENGTH);
	cout << endl;
	//**********************************	
	stringstream receivers(seg);
	string	to;
	while (getline(receivers, to, ' '))
		(*multiReceivers).push(strdup(to.c_str()));
	sprintf(inputto, "%s", (*multiReceivers).top());	
	//*************************************
	cout << "Subject: ";
	cin.getline(inputsubj, SUBJ_LENGTH);
	cout << endl;
	cout << "Body: ";
	//read the body of the email and put it in variable length buffer
	body_len = getBodyFromTerminal(inputbody);
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
		if ((x = copyFileToDirectory(fa, &smsg, modFileName, filepath)) != 0)
			err_sys("Error, coudn't copying file to working directory\n");
	}
	else
		Esendp->file_size = -1;	//if no file attachment is choosen
	return body_len;
}