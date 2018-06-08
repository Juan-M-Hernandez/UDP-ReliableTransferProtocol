#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cctype>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
using namespace std;

#define INIT_SEQ 321

struct packet{
	unsigned int seq; // 4 bytes
	unsigned int ack; // 4 bytes
	bool syn_flag; // 1 byte
	bool ack_flag; // 1 byte
	bool fin_flag; // 1 byte
	int data_size; // 4 bytes
	string data; // 1024-15 = 1009 bytes 
	packet(unsigned s, unsigned a, bool sf, bool af, bool ff, int ds, string d){
		seq = s;
		ack = a;
		syn_flag = sf;;
		ack_flag = af;
		fin_flag = ff;
		data_size = ds;
		data = d;
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
	strcpy(ptr,p.data.c_str());
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
	string data(ptr);

	packet p(seq,ack,syn_flag,ack_flag,fin_flag,data_size,data);
	return p;
}

void error(string msg)
{
	cerr << msg << endl;
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd;  // socket descriptor
	int portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;  // contains tons of information, including the server's IP address

	char buffer[256];
	if (argc < 3) {
	   cerr << "Usage: " << argv[0] << " hostname port\n";
	   exit(0);
	}

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

	// HANDSHAKING
	char pkt[1024];
	memset(pkt,0,1024);
	packet q(INIT_SEQ,0,true,false,false,0,"");
	encode(q,pkt);

	n = sendto(sockfd, (const char*)pkt, 1024, MSG_CONFIRM,(const struct sockaddr*)&serv_addr,sizeof(serv_addr));  // write to the socket
	if (n < 0)
		 error("ERROR writing to socket");

	// memset(buffer, 0, 256);
	// n = read(sockfd, buffer, 255);  // read from the socket
	// if (n < 0)
	// 	 error("ERROR reading from socket");
	// cout << buffer << endl;  // print server's response

	close(sockfd);  // close socket

	return 0;
}
