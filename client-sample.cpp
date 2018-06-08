#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cctype>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
using namespace std;

#define INIT_SEQ 321

struct packet{
	unsigned int seq; // 4 bytes
	unsigned int ack; // 4 bytes
	bool syn_flag; // 1 byte
	bool ack_flag; // 1 byte
	bool fin_flag; // 1 byte
	int data_size; // 4 bytes
	char data[1009]; // 1024-15 = 1009 bytes 
	packet(unsigned s, unsigned a, bool sf, bool af, bool ff, int ds, char* d){
		seq = s;
		ack = a;
		syn_flag = sf;;
		ack_flag = af;
		fin_flag = ff;
		data_size = ds;
		if(d != NULL) strcpy(data,d);
	}
};

void encode(packet p, char* ptr){
	// seq
	strcpy(ptr,static_cast<char*>(static_cast<void*>(&p.seq)));
	ptr += 4;
	strcpy(ptr,static_cast<char*>(static_cast<void*>(&p.ack)));
	ptr += 4;
	strcpy(ptr,static_cast<char*>(static_cast<void*>(&p.syn_flag)));
	ptr++;
	strcpy(ptr,static_cast<char*>(static_cast<void*>(&p.ack_flag)));
	ptr++;
	strcpy(ptr,static_cast<char*>(static_cast<void*>(&p.fin_flag)));
	ptr++;
	strcpy(ptr,static_cast<char*>(static_cast<void*>(&p.data_size)));
	ptr+=4;
	strcpy(ptr,p.data);
}

packet decode(char* ptr){
	char four[4];
	strncpy(four,ptr,4);
	unsigned int seq = (ptr[3]<<24)+(ptr[2]<<16)+(ptr[1]<<8)+(ptr[0]);
	ptr += 4;
	strncpy(four,ptr,4);
	unsigned int ack = (ptr[3]<<24)+(ptr[2]<<16)+(ptr[1]<<8)+(ptr[0]);
	ptr += 4;
	bool syn_flag = ptr[0] != 0;
	ptr += 1;
	bool ack_flag = ptr[0] != 0;
	ptr += 1;
	bool fin_flag = ptr[0] != 0;
	ptr += 1;
	strncpy(four,ptr,4);
	int data_size = (ptr[3]<<24)+(ptr[2]<<16)+(ptr[1]<<8)+(ptr[0]);
	ptr += 4;
	char data[1009];
	strcpy(data,ptr);

	packet p(seq,ack,syn_flag,ack_flag,fin_flag,data_size,data);
	return p;
}

void error(string msg)
{
	cerr << "CLIENT: "  << msg << " - " << strerror(errno) << endl;
	exit(0);
}

void log_packet(packet p){
	cerr << "CLIENT <== SERVER:";
	if(p.syn_flag) cerr << " SEQ=" << p.seq;
	if(p.ack_flag) cerr << " ACK=" << p.ack;
	if(p.data_size > 0) cerr << " data=\"" << p.data << "\"";
	cerr << endl;
}

int main(int argc, char *argv[])
{
	int sockfd;  // socket descriptor
	int portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;  // contains tons of information, including the server's IP address
	socklen_t servlen = sizeof(serv_addr);

	if (argc != 4) {
	   cerr << "Usage: " << argv[0] << " hostname port filename\n";
	   exit(0);
	}

	char* filename;
	filename = (char*)malloc(strlen(argv[3]));
	strcpy(filename,argv[3]);

	portno = atoi(argv[2]);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // create a new socket
	if (sockfd < 0)
		error("ERROR opening socket");

	server = gethostbyname(argv[1]);  // takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
	if (server == NULL) {
		cerr << "ERROR, no such host\n";
		exit(0);
	}

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET; //initialize server's address
	bcopy((char *)server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);

	/////////////////////
	// HANDSHAKING
	/////////////////////

	int seq = INIT_SEQ;

	char pkt[1024];
	memset(pkt,0,1024);

	// send first SYN
	packet syn(seq,0,true,false,false,0,NULL);
	encode(syn,pkt);
	n = sendto(sockfd, (const char*)pkt, 1024, MSG_CONFIRM,(const struct sockaddr*)&serv_addr,sizeof(serv_addr));  // write to the socket
	seq++;
	if (n < 0)
		 error("ERROR writing to socket");

	// receive SYN-ACK
	memset(pkt,0,1024);
	n = recvfrom(sockfd,(char*)pkt,1024,MSG_WAITALL,(struct sockaddr*)&serv_addr,&servlen);
	if(n<0) error("ERROR reading from socket");
	packet synack = decode(pkt);

	log_packet(synack);

	// send ACK with name of requesting file
	memset(pkt,0,1024);
	packet ack(seq,synack.seq+1,true,true,false,strlen(filename),filename);
	encode(ack,pkt);
	n = sendto(sockfd, (const char*)pkt, 1024, MSG_CONFIRM,(const struct sockaddr*)&serv_addr,sizeof(serv_addr));  // write to the socket
	seq++;
	if (n < 0)
		 error("ERROR writing to socket");


	close(sockfd);  // close socket

	return 0;
}
