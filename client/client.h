#ifndef CLIENT_H
#define CLIENT_H
#pragma once
#pragma comment (lib, "Ws2_32.lib")
#define _CRT_SECURE_NO_WARNINGS 1 
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <windows.h>
#include <unistd.h>
#include <fstream>
#include <time.h>
#include <stack>
#include <bits/stdc++.h>

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
#define TRACE 0
#define MSGHDRSIZE 8 //Message Header Size

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
	int		num_receivers;
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
	int	 type;
	char buffer[BUFFER_LENGTH];
} Email; //message format used for sending and receiving


class TcpClient
{
	int sock;                    /* Socket descriptor */
	struct sockaddr_in ServAddr; /* server socket address */
	unsigned short ServPort;     /* server port */
	char* inputserverhostname = new char[HOSTNAME_LENGTH];
	Type tp;
	Esend* Esendp;               /* pointer to request */
	Sign *signp;
	Header* headerp;
	Resp* respp;          			/* pointer to response*/
	Email smsg, rmsg;               /* receive_message and send_message */
	time_t curr_time;
    struct tm *tmp;
	WSADATA wsadata;
public:
	TcpClient() {}
	void run(int argc, char* argv[]);
	~TcpClient();
	int authenticateClient(Type tp, char *from);
	//int IndexOf(char *str, char c);
    int		saveEmailToFile(Email *, char *, Type);
	int		copyFileToDirectory(FILE *fa, Email *smsg, char **modFileName, char *filepath);
	int 	sendFileAttachment(FILE *fa, Email *smsg, char *modFileName);
	long	receiveFileAttachment(Email *rmsg, char **fname, FILE *frecv);
    void	receiveEmail(int);
	void	printStructure(Email msg);
	int 	msg_recv(int, Email*, char **);
	int 	msg_send(int, Email*, char*);
	void	modeSignin(char **inputform);
	int		getBodyFromTerminal(char **inputbody);
	int		fillEmailPointerTerminal(stack<char *> *multiReceivers, char **inputbody, FILE *fa, char **modFileName, char *inputfrom);
	unsigned long ResolveName(char name[]);
	void err_sys(const char* fmt, ...);
};

int		IndexOf(std::string str, char c);
char	*ft_substr(std::string s, int start, int len);

#endif