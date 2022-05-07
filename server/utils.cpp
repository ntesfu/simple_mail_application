#include "server.h"

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

	//printf("Copying File--> sourcefile:%s, dest:%s\n", srcFname, destFname);
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
	//printf("retrieved file name is : %s\n",stack.top().c_str());
	fname = new char[stack.top().length()];
	sprintf(fname, "%s", (char *)stack.top().c_str());
	return fname;
}