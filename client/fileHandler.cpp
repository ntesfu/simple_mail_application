#include "client.h"

using namespace std;

/*saves the email structure msg into a file*/
int	TcpClient::saveEmailToFile(Email *msg, char *body, Type tp)
{
	//printf("Saving Email to file\n");
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

	//printf("file name:%s\n", fname);
	ofstream fclient;
	fclient.open(fname);
	if (fclient.is_open()){
		fclient << "FROM: "<<header->from<<endl;
		fclient << "TO: "<<header->to<<endl;
		fclient << "SUBJECT: "<<header->subject<<endl;
		fclient << "TIMESTAMP: "<<header->tstamp<<endl;
		fclient << body;
		fclient.close();
		return 0;
	}
	printf("Unable to open file\n");
	return -1;
}

/*copies the provided file path to the working directory of the sender process*/
int TcpClient::copyFileToDirectory(FILE *fa, Email *msg, char **modFileName, char *filepath)
{
	//printf("Copying file to working directory\n");
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
	//printf("in copying file, filename length is :%d\n", Esendp->filename_size);
	if ((fout = fopen(fname, "wb")) == NULL){
		printf("Could not open a file to copy\n");
		return (-1);
	}
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (!buf) return (-1);
	while ((size = fread(buf, 1, MTU_SIZE, fa)) > 0){
		fwrite(buf, 1, size, fout);
	}
	if (buf) free(buf);
	if (size < 0) return -1;
	fclose(fout);
	fclose(fa);
	return 0;
}

