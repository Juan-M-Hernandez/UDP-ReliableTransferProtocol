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
#include <fstream>
#include <sys/time.h>
#include <sys/types.h>
using namespace std;

#define INIT_SEQ 0
#define HEADER_SIZE 15
#define MAX_DATA_SIZE 1009
#define CWND 5120
#define MAX_SEQ 30720
#define TIMEOUT 500



void fakecpy(char* a, char* b, size_t len){
	for(size_t i = 0; i < len; i++){
		a[i] = b[i];
	}
}

struct packet{
	unsigned int seq; // 4 bytes
	unsigned int ack; // 4 bytes
	bool syn_flag; // 1 byte
	bool ack_flag; // 1 byte
	bool fin_flag; // 1 byte
	unsigned int data_size; // 4 bytes
	char data[MAX_DATA_SIZE]; // 1024-15 = 1009 bytes 
	packet(){}
	packet(unsigned s, unsigned a, bool sf, bool af, bool ff, int ds, char* d){
		seq = s;
		ack = a;
		syn_flag = sf;;
		ack_flag = af;
		fin_flag = ff;
		data_size = ds;
		if(d != NULL) fakecpy(data,d,ds);
	}
	unsigned int pktsize(){
		return HEADER_SIZE + data_size + (data_size != 0 ? 1 : 0);
	}
};

unsigned int encode(packet p, char* ptr){
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
	fakecpy(ptr,p.data,p.data_size);
	return p.pktsize();
}

void decode(char* buffer, packet& p){
	unsigned char* ptr = (unsigned char*)buffer;
	p.seq = (ptr[3]<<24)+(ptr[2]<<16)+(ptr[1]<<8)+(ptr[0]);
	ptr += 4;
	p.ack = (ptr[3]<<24)+(ptr[2]<<16)+(ptr[1]<<8)+(ptr[0]);
	ptr += 4;
	p.syn_flag = ptr[0] != 0;
	ptr += 1;
	p.ack_flag = ptr[0] != 0;
	ptr += 1;
	p.fin_flag = ptr[0] != 0;
	ptr += 1;
	p.data_size = (ptr[3]<<24)+(ptr[2]<<16)+(ptr[1]<<8)+(ptr[0]);
	ptr += 4;
	fakecpy(p.data,(char*)ptr,p.data_size);
}

void error(string msg)
{
	cerr << "CLIENT: "  << msg << " - " << strerror(errno) << endl;
	exit(0);
}

void log_send(packet p, bool re = false){
	cout << "Sending packet " << p.seq << " " << CWND;
	if(re) cout << " Retransmission";
	if(p.syn_flag) cout << " SYN";
	if(p.fin_flag) cout << " FIN";
	cout << endl;
}
void log_recv(packet p){
	cout << "Receiving packet " << p.seq + p.pktsize() << endl;
}


unsigned int add(unsigned int seq, unsigned int bytes){
	return (seq + bytes) % MAX_SEQ;
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

	fd_set inSet;
	struct timeval timeout;
			
			// wait for specified time
	timeout.tv_sec = 0;
	timeout.tv_usec = TIMEOUT * 1000;

	unsigned int seq = INIT_SEQ;

	char pkt[1024];
	memset(pkt,0,1024);

	// send first SYN
	packet syn(seq,0,true,false,false,0,NULL);
	unsigned int pktsize = encode(syn,pkt);
	n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&serv_addr,sizeof(serv_addr));  // write to the socket
	log_send(syn);
	seq = add(seq,pktsize);
	if (n < 0)
		 error("ERROR writing to socket");

	// receive SYN-ACK

	packet synack;
	char recvbuf[1024];
	while(true){
		FD_ZERO(&inSet);
		FD_SET(sockfd, &inSet);
		int rv = select(sockfd+1,&inSet,NULL,NULL,&timeout);
		if(rv < 0){
			error("ERROR reading from socket");
		} else if(rv == 0){
			n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&serv_addr,sizeof(serv_addr));  // write to the socket
			log_send(syn, true);
			if (n < 0)
				error("ERROR writing to socket");
			
			timeout.tv_sec = 0;
			timeout.tv_usec = TIMEOUT * 1000;
		} else{

			memset(recvbuf,0,1024);
			n = recvfrom(sockfd,(char*)recvbuf,1024,MSG_WAITALL,(struct sockaddr*)&serv_addr,&servlen);
			if(n<0) error("ERROR reading from socket");
			decode(recvbuf,synack);
			log_recv(synack);
			if(synack.ack == seq){
				break;
			}
		}
	}

	packet* sendpkt;

	// send ACK with name of requesting file
	memset(pkt,0,1024);
	packet ack(seq,synack.seq+synack.pktsize(),false,true,false,strlen(filename),filename);
	sendpkt = &ack;
	pktsize = encode(*sendpkt,pkt);
	n = sendto(sockfd, (const char*)pkt, 1024, MSG_CONFIRM,(const struct sockaddr*)&serv_addr,sizeof(serv_addr));  // write to the socket
	log_send(*sendpkt);
	seq = add(seq,pktsize);
	if (n < 0)
		 error("ERROR writing to socket");

	ofstream recv_data;
	recv_data.open("received.data",ios::binary);

	while(true){
		timeout.tv_sec = 0;
		timeout.tv_usec = TIMEOUT * 1000;
		packet recvpkt;
		while(true){
			FD_ZERO(&inSet);
			FD_SET(sockfd, &inSet);
			int rv = select(sockfd+1,&inSet,NULL,NULL,&timeout);
			if(rv < 0){
				error("ERROR reading from socket");
			} else if(rv == 0){
				n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&serv_addr,sizeof(serv_addr));  // write to the socket
				log_send(*sendpkt, true);
				if (n < 0)
					error("ERROR writing to socket");
				
				timeout.tv_sec = 0;
				timeout.tv_usec = TIMEOUT * 1000;
			} else{

				memset(recvbuf,0,1024);
				n = recvfrom(sockfd,(char*)recvbuf,1024,MSG_WAITALL,(struct sockaddr*)&serv_addr,&servlen);
				if(n<0) error("ERROR reading from socket");
				decode(recvbuf,recvpkt);
				log_recv(recvpkt);
				if(recvpkt.ack == seq){
					break;
				}
			}
		}

		recv_data.write(recvpkt.data,recvpkt.data_size);
		if(recvpkt.fin_flag){
			unsigned int ackno= recvpkt.seq + recvpkt.pktsize();
			packet finack(seq,ackno,false,true,true,0,NULL);
			memset(pkt,0,1024);
			pktsize = encode(finack,pkt);
			n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&serv_addr,sizeof(serv_addr)); 
			seq = add(seq,pktsize);
			log_send(finack);

			break;
		}

		// ACK the packet
		unsigned int ackno = recvpkt.seq+recvpkt.pktsize();
		packet newpkt(seq,ackno,false,true,false,0,NULL);
		sendpkt = &newpkt;
		memset(pkt,0,1024);
		pktsize = encode(newpkt,pkt);
		n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&serv_addr,sizeof(serv_addr));  // write to the socket
		seq = add(seq,pktsize);
		log_send(newpkt);
		if (n < 0)
			 error("ERROR writing to socket");

	}
	recv_data.close();

	close(sockfd);  // close socket

	return 0;
}
