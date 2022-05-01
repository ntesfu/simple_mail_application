#ifndef SER_TCP_H
#define SER_TCP_H

#pragma once
#pragma comment (lib, "Ws2_32.lib")
#define _CRT_SECURE_NO_WARNINGS 1 
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <windows.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string>
#include <process.h>
#include "Thread.h"
#include <direct.h>

#define HOSTNAME_LENGTH 20
#define PASS_LENGTH 20
#define CODE_LENGTH 12
#define TSTAMP_LENGTH 20
#define TO_LENGTH 40
#define FROM_LENGTH 40
#define SUBJ_LENGTH 50
#define HEADER_LENGTH 160
//#define BODY_LENGTH 500
#define REQUEST_PORT 5001
#define BUFFER_LENGTH 200
#define FILE_LENGTH 50
#define MTU_SIZE 1459
#define MAXPENDING 10
#define MSGHDRSIZE 8 //Message Header Size
#define	FSIZE (FROM_LENGTH + SUBJ_LENGTH + TSTAMP_LENGTH + 12)

using namespace std;
typedef enum
{
	SGN_UP = 1, SGN_IN, SGNIN_RECV, EMAIL, RESP
} Type;

typedef struct {
	char to[TO_LENGTH];
	char from[FROM_LENGTH];
	char subject[SUBJ_LENGTH];
	char tstamp[TSTAMP_LENGTH];
} Header;

typedef struct
{
	char	hostname[HOSTNAME_LENGTH];
	char	header[HEADER_LENGTH];
	int		body_length;
	long	file_size;
	int		filename_size;
} Esend;  //request

typedef struct
{
	char hostname[HOSTNAME_LENGTH];
	char clientname[FROM_LENGTH];
	char password[PASS_LENGTH];
}	Sign;

typedef struct
{
	char rcode[CODE_LENGTH];
	char tstamp[TSTAMP_LENGTH];

}	Resp; //response

typedef struct
{
	int  length; //length of effective bytes in the buffer
	int type;
	char buffer[BUFFER_LENGTH];
} Email; //message format used for sending and receiving

class TcpServer
{
	int serverSock,clientSock;     /* Socket descriptor for server and client*/
	struct sockaddr_in ClientAddr; /* Client address */
	struct sockaddr_in ServerAddr; /* Server address */
	unsigned short ServerPort;     /* Server port */
	int clientLen;            /* Length of Server address data structure */
	char servername[HOSTNAME_LENGTH];

public:
		TcpServer();
		~TcpServer();
		void start();
		unsigned long ResolveName(char *str);
};

class TcpThread :public Thread
{
	int cs;
	Type	tp;
	Header	*headerp;
	Resp	*respp;//a pointer to response
	Esend	*Esendp; //a pointer to the Request Packet
	Sign	*signp;
	Email	smsg,rmsg; //send_message receive_message
	time_t	curr_time;
public: 
	TcpThread(int clientsocket):cs(clientsocket)
	{}
	virtual void run();
	unsigned long	ResolveName(char name[]);
    static void	err_sys(const char * fmt,...);
    int		msg_recv(int ,Email *, char **);
	int		msg_send(int ,Email *, char *);
	int		authenticateClient(Email *rmsg);
	int		checkClientEntry(char *user, char *passwd, int full);
	int		checkClientMapping(char *user);
	int		makeDir(Email *rmsg);
	int		copyFile(char *srcFname, char *destFname);
	int		saveEmailToFile(int cs, Email *rmsg, char *body, char **fname, FILE *frecv);
	long	receiveFileAttachment(int cs, Email *rmsg, char **fname, FILE *frecv);
	long	sendFileAttachment(int cs, Email *rmsg, char *fname, FILE *frecv);
	int		appendToFile(const char *fname, char *entry);
	int		eraseClientFromMapping(const char *fname, char *client);
	int		IndexOf(string str, char c);
	char	*ft_substr(string s, unsigned int start, size_t len);
};

#endif