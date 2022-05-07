#include "client.h"

using namespace std;
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
		(*body)[Esendp->body_length] = '\0';
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
	char	*filepath;

	ret = 0;
	//printf("before fa rewind\n");
	//rewind(fa);	//rewinds file descriptor, to read from the beginning
	//printf("after fa rewind\n");
	filepath = new char[FILE_LENGTH];
	sprintf(filepath, "send\\%s", modFileName);
	if ((fa = fopen((const char *)filepath, "rb")) == NULL)
				err_sys("could not open file specified\n");

	//send file name first
	if ((n = send(sock, (char*)modFileName, strlen(modFileName), 0)) != (strlen(modFileName)))
		err_sys("Send File Name error");
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	if (!buf) return (-1);
	while ((size = fread(buf, 1, MTU_SIZE, fa)) > 0){
		ret += size;	
		offset = 0;
		while ((n = send(sock, buf + offset, size - offset, 0)) > 0){
			offset += n;
			//size -= n;
		}
	}
	//fclose(fa); //fa job is done here
	return (ret); //return the size that was successfully sent :: n = 10, offset= 10, size = 40, 
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
	int		recvSize;

	//printf("Receiving file attachment\n");
	*fname = new char[Esendp->filename_size + 10];
	if ((n = recv(sock, *fname, Esendp->filename_size, 0)) <= 0)
		err_sys("File name receive error");
	(*fname)[Esendp->filename_size] = '\0';
	
	//printf("inside receive, got file name:%s:%d:%d\n", *fname, n, Esendp->filename_size);
	temp = strdup((const char *)*fname);
	strcpy(*fname, "received/");
	strcat(*fname, temp);
	free(temp);
	if ((frecv = fopen(*fname, "wb")) == NULL){
		printf("Could not open a file to write\n");
		return (-1);
	}
	buf = (char *)malloc(sizeof(char) * MTU_SIZE + 1);
	for(offset = 0; offset < Esendp->file_size; offset += n)
	{
		recvSize = (MTU_SIZE > Esendp->file_size -offset)? Esendp->file_size - offset : MTU_SIZE;
		if ((n = recv(sock, buf, recvSize, 0)) <= 0)
			err_sys("Reading file error");
		fwrite(buf, 1, n, frecv);
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
	int		i;
	
	i = 1;
	printf("\nWaiting to receive an email!\n\n");
	for (;;)
	{
		if(msg_recv(sock,&rmsg, &body)!=rmsg.length)
			err_sys("Receive Req error,exit");
		headerp=(Header *)Esendp->header;
		printf("%d. From: %s Subject: %s Time: %s\n",i++, headerp->from, headerp->subject, headerp->tstamp);
		//printf("Received an Email from client: %s\n",Esendp->hostname);
		//printf("Mail Received from %s\n", Esendp->hostname);
		// printf("From: %s\n", headerp->from);
		// printf("To: %s\n", headerp->to);
		// printf("Subject: %s\n", headerp->subject);
		// printf("Time: %s\n", headerp->tstamp);
		// printf("%s\n\n", body);
		//save the email received as a file		
		if ((x = saveEmailToFile(&rmsg, body, SGNIN_RECV) != 0))
			err_sys("Error, could not save email to file!");
		if (body)
			free(body);
		//if there is a file attachment call receiveFileAttachment function
		if (Esendp->file_size != -1){
			if ((x = receiveFileAttachment(&rmsg, &fname, frecv)) != Esendp->file_size){
				printf("file attachment received:%s:filesize:%ld:%ld\n", fname,x,Esendp->file_size);
				err_sys("Error, could not receive the file attachment");			
			}
			//printf("file attachment received:%s:filesize:%ld:%ld\n", fname,x,Esendp->file_size);				
		}
	}
}
