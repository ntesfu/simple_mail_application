#include "server.h"

//returns the index of char c in the string str (-1 if not found)
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

//gets the substring in char * of String s from start index to length len
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

//called when a client signs up to service to create inbox and sent directory for the client
int	TcpThread::makeDir(Email *rmsg)
{
	char	*saveDir;
	char	*emailFolder = new char[10];
	
	signp = (Sign *)rmsg->buffer;
	saveDir = new char[FROM_LENGTH + 20];
	strcpy(emailFolder, "emails");
	_mkdir(emailFolder);
	sprintf(saveDir, "%s\\%s", emailFolder, signp->clientname);
	if (_mkdir(saveDir) != 0){
		printf("could not create directory, error");
		return -1;
	}
	sprintf(saveDir, "%s\\%s\\%s", emailFolder, signp->clientname, "sent");
	if (_mkdir(saveDir) != 0){
		printf("could not create directory, error");
		return -1;
	} //create directory if not exist
	sprintf(saveDir, "%s\\%s\\%s", emailFolder, signp->clientname, "inbox");
	if (_mkdir(saveDir) != 0){
		printf("could not create directory, error");
		return -1;
	}
	return 0;
}

//returns 0 if successfully copied file from a source path to destination
int	TcpThread::copyFile(char *srcFname, char *destFname)
{
	char	*buf;
	FILE	*fin;
	FILE	*fout;
	int		size;
	int 	n;

	if ((fin = fopen(srcFname, "rb")) == NULL){
		printf("Could not open a file to read when copying\n");
		return (-1);
	}
	if ((fout = fopen(destFname, "wb")) == NULL){
		printf("Could not open a file to write when copying\n");
		return (-1);
	}
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (buf == NULL){
		printf("memory error!"); return -1;
	}	
	while ((size = fread(buf, 1, MTU_SIZE, fin)) > 0)
		if ((n = fwrite(buf, 1, size, fout)) < 0)
			return -1;
	if (buf) free(buf);
	fclose(fin);
	fclose(fout);
	return 0;
}

//strip and return the file name to be sent from a path str
char *TcpThread::getFileName(char *str)
{
	stringstream	path(str);
	string			seg;
	stack<string> 	stack;
	char			*fname;
	while (getline(path, seg, '\\'))
	{
		stack.push(seg);
	}
	fname = new char[stack.top().length()];
	sprintf(fname, "%s", (char *)stack.top().c_str());
	return fname;
}