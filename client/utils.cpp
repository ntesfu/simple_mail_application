#include "client.h"

//returns the index of char c in the string str (-1 if not found)
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

//gets the substring in char * of String s from start index to length len
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

/*function that fills the buffer inputbody with a variable length of message
that is read from the terminal*/
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

/*function to fill the smsg email pointer from scratch(prompting the user from the terminal)*/
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
	//put the receipents in a stack
	stringstream receivers(seg);
	string	to;
	while (getline(receivers, to, ' '))
		(*multiReceivers).push(strdup(to.c_str()));
	sprintf(inputto, "%s", (*multiReceivers).top());	
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
		if ((fa = fopen((const char *)filepath, "rb")) == NULL)
			err_sys("could not open file specified\n");
		int n;
		long fsize = 0;
		char c;
		while ((n = fread(&c, 1, 1, fa)) > 0){
			fsize++;
		}
		printf("Read file as binary with size of: %ld\n", fsize);
		Esendp->file_size = fsize;
		fclose(fa);
		if ((fa = fopen((const char *)filepath, "rb")) == NULL)
			err_sys("could not open file specified\n");
		//rewind(fa);
		int x;
		if ((x = copyFileToDirectory(fa, &smsg, modFileName, filepath)) != 0)
			err_sys("Error, coudn't copying file to working directory\n");
	}
	else
		Esendp->file_size = -1;	//if no file attachment is choosen
	return body_len;
}

/*function to fill the smsg email pointer form a file when forwarding an email*/
int	TcpClient::fillEmailPointerFromFile(stack<char *> *multiReceivers, char **inputbody, FILE *fa, char **modFileName, char *inputfrom)
{
	int			body_len;
	char		*filepath;
	char		*filename;
	char		*dir;	
	char		*inputto;
	int			cmd;
	ifstream	fin;
	string		seg;	

	inputto = new char[TO_LENGTH];
	dir = new char[10];
	filename = new char[FILE_LENGTH];

	cout << "Input directory (send/received): ";
	cin.getline(dir, 10);
	cout << "Input the file name: ";
	cin.getline(filename, FILE_LENGTH);
	cout <<endl;
	cout << "To: ";
	cin.getline(inputto, TO_LENGTH);
	cout <<endl;
	
	headerp = (Header *)Esendp->header;
	filepath = new char[FILE_LENGTH];
	sprintf(filepath, "%s\\%s", dir, filename);
	fin.open(filepath, ios::in);
	if (fin.is_open())
	{
		getline(fin, seg, '\n');
		getline(fin, seg, '\n');
		sprintf(headerp->from, "%s", inputfrom);
		sprintf(headerp->to, "%s", inputto);
		getline(fin, seg, ' ');
		getline(fin, seg);
		sprintf(headerp->subject, "%s", (char *)seg.c_str());
	
		getline(fin, seg, ' ');
		getline(fin, seg);
		time(&curr_time);
		tmp = localtime(&curr_time);
		strftime(headerp->tstamp, TSTAMP_LENGTH, "%d-%m-%Y_%H-%M-%S", tmp);
	
		string line = "";
		getline(fin, seg);
		line = line + seg;
		while (getline(fin, seg))
			line = line + '\n' + seg;
		*inputbody = (char *)malloc(sizeof(char) * (line.length() + 1));
		sprintf(*inputbody, "%s", (char *)line.c_str());
		(*inputbody)[line.length()] = '\0';
		fin.close();
		body_len = strlen(*inputbody);
		Esendp->body_length = body_len;
		smsg.length = sizeof(Esend);
		smsg.type = EMAIL;

		namespace		fs = std::filesystem;
		string			str;
		char			*chr;
		set <fs::path>	sortedByName;
		const fs::path	forwardDir{dir};
		int flag = 0;
		int idx = IndexOf((string)filepath, '.');
		char *fext = ft_substr((string)filepath, 0, idx);
		sprintf(filepath, "%s(op)", fext);
		if (fext) free(fext);
		for (auto const& dir_entry : fs::directory_iterator{forwardDir}){
			sortedByName.insert(dir_entry.path());
		}
		for (auto &file : sortedByName){
			str = file.string();
			chr = (char *)str.c_str();			
			if (strstr(chr, filepath) != NULL)
			{
				flag = 1;
				if ((fa = fopen((const char *)chr, "rb")) == NULL)
					err_sys("could not open file specified\n");
				int n;
				long fsize = 0;
				char c;
				while ((n = fread(&c, 1, 1, fa)) > 0)
					fsize++;
				Esendp->file_size = fsize;
				rewind(fa);
				int x;
				if ((x = copyFileToDirectory(fa, &smsg, modFileName, chr)) != 0)
					err_sys("Error, coudn't copying file to working directory\n");
				break;
			}
		}
		if (flag == 0)
			Esendp->file_size = -1;
		(*multiReceivers).push(strdup(headerp->to));
		return body_len;
	}
	return -1;
}
