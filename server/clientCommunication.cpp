#include "server.h"

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
		(*body)[Esendp->body_length] = '\0';
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

/*send the entire inbox directory to receiving client*/
int	TcpThread::sendInboxToReceiver(int cs, char *clientname)
{
	namespace		fs = std::filesystem;
	char 			*dir;
	char			c;
	long			fsize;
	char			*fileName;
	char			*body;
	FILE 			*fa;
	string			str;
	char			*chr;
	int				flag;
	set <fs::path>	sortedByName;
	
	Esendp = (Esend *)rmsg.buffer;
	printf("Sending Inbox of a receiving client: %s\n", clientname);
	dir = new char[FSIZE];
	sprintf(dir, "emails\\%s\\inbox", clientname); 
	flag = 0;
	const fs::path	inboxDir{dir};
	int i = 0;
    for (auto const& dir_entry : fs::directory_iterator{inboxDir}){
		cout<<i++<<". "<<dir_entry<<endl;
        sortedByName.insert(dir_entry.path());
	}
	for (auto &filepath : sortedByName){
		str = filepath.string();
		chr = (char *)str.c_str();			
		if (strstr(chr, "(op)") != NULL){
			if ((fa = fopen((const char *)chr, "rb")) == NULL)
				printf("could not open file specified\n");
			fsize = 0;
			while (fread(&c, 1, 1, fa) > 0)
				fsize++;
			fclose(fa);
			Esendp->file_size = fsize;			
			fileName = getFileName(chr);
			Esendp->filename_size = strlen(fileName);
			fclose(fa);
			flag = 1;
		}
		else
		{
			if (fillEmailPointer(chr, &body) != 0)
			{
				printf("error filling the email pointer");
				return -1;
			}
			Esendp->body_length = strlen(body);
			if (flag == 0){
				Esendp->file_size = -1;
				flag = 0;
			}
			rmsg.type = EMAIL;
			rmsg.length = sizeof(Esend);
			if(msg_send(cs,&rmsg,body)!=rmsg.length + strlen(body))
				err_sys("send Respose failed,exit");
			if (body) free(body);
			if (Esendp->file_size != -1)
				if ((sendFileAttachment(cs, &rmsg, fileName, fa)) != Esendp->file_size)
					err_sys("Error, could not send the file attachment");
					
		}
	}
	return (0);
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
	int		recvSize;

	printf("Receiving file attachment\n");
	sentFile = new char[FSIZE];
	*fname = new char[Esendp->filename_size + 1];
	inboxFile = new char[FSIZE];
	header = (Header *)Esendp->header;	
	if ((n = recv(cs, *fname, Esendp->filename_size, 0)) <= 0)
			err_sys("File name receive error");
	(*fname)[Esendp->filename_size] = '\0';
	sprintf(sentFile, "emails\\%s\\sent\\%s", header->from, *fname);
	if ((frecv = fopen(sentFile, "wb")) == NULL){
		printf("Could not open a file to write\n");
		return (-1);
	}
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (!buf) return (-1);
	for(offset = 0; offset < Esendp->file_size; offset += n)
	{
		recvSize = (MTU_SIZE > Esendp->file_size -offset)? Esendp->file_size - offset : MTU_SIZE;
		if ((n = recv(cs, buf, recvSize, 0)) <= 0)
			err_sys("Reading file error");
		fwrite(buf, 1, n, frecv);
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
	printf("Sending file attachment\n");
	int		n;
	int		size;
	long	ret;
	char	*buf;
	int		offset;
	char	*inboxFile;

	ret = 0;
	inboxFile = new char[FSIZE];
	sprintf(inboxFile, "emails\\%s\\inbox\\%s", ((Header *)(Esendp->header))->to, fname);
	if ((frecv = fopen(inboxFile, "rb")) == NULL){
		printf("Could not open a file to write\n");
		return (-1);
	}
	if ((n = send(cs, (char*)fname, Esendp->filename_size, 0)) != Esendp->filename_size)		
		err_sys("Send File Name error");
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (!buf) return (-1);
	while ((size = fread(buf, 1, MTU_SIZE, frecv)) > 0){
		ret += size;	
		offset = 0;
		while ((n = send(cs, buf + offset, size - offset, 0)) > 0){
			offset += n; 
		}
	}
	if (buf) free(buf);
	fclose(frecv);
	return (ret);
}
