#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cctype>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
using namespace std;

#define INIT_SEQ 123

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
	cerr << "SERVER: " << msg << " - " << strerror(errno) << endl;
	exit(1);
}

void log_packet(packet p){
	cerr << "CLIENT ==> SERVER:";
	if(p.syn_flag) cerr << " SEQ=" << p.seq;
	if(p.ack_flag) cerr << " ACK=" << p.ack;
	if(p.data_size > 0) cerr << " data=\"" << p.data << "\"";
	cerr << endl;
}

int main(int argc, char *argv[])
{
	int sockfd, portno;
	struct sockaddr_in serv_addr, cli_addr;
	socklen_t clilen = sizeof(cli_addr);

	if (argc < 2) {
		error("ERROR, no port provided");
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // create socket
	if (sockfd < 0)
		error("ERROR opening socket");
	memset((char *) &serv_addr, 0, sizeof(serv_addr));  // reset memory
	memset((char *) &cli_addr, 0, sizeof(cli_addr));

	// fill in address info
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");

	// HANDSHAKING

	int n;

	char pkt[1024];
	n = recvfrom(sockfd,(char*)pkt,1024,MSG_WAITALL,(struct sockaddr*)&cli_addr,&clilen);
	if(n<0) error("ERROR reading from socket");
	packet syn = decode(pkt);

	log_packet(syn);

	packet synack(INIT_SEQ,syn.seq+1,true,true,false,0,NULL);
	encode(synack,pkt);
	n = sendto(sockfd, (const char*)pkt, 1024, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
	if (n < 0)
		 error("ERROR writing to socket");

	n = recvfrom(sockfd,(char*)pkt,1024,MSG_WAITALL,(struct sockaddr*)&cli_addr,&clilen);
	if(n<0) error("ERROR reading from socket");
	packet ack = decode(pkt);

	log_packet(ack);

	close(sockfd);

	return 0;
}
