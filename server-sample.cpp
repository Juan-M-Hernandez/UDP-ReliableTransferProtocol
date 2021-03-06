#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cctype>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <dirent.h>
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
	fakecpy(ptr,static_cast<char*>(static_cast<void*>(&p.seq)),4);
	ptr += 4;
	fakecpy(ptr,static_cast<char*>(static_cast<void*>(&p.ack)),4);
	ptr += 4;
	fakecpy(ptr,static_cast<char*>(static_cast<void*>(&p.syn_flag)),1);
	ptr++;
	fakecpy(ptr,static_cast<char*>(static_cast<void*>(&p.ack_flag)),1);
	ptr++;
	fakecpy(ptr,static_cast<char*>(static_cast<void*>(&p.fin_flag)),1);
	ptr++;
	fakecpy(ptr,static_cast<char*>(static_cast<void*>(&p.data_size)),4);
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
	cerr << "SERVER: " << msg << " - " << strerror(errno) << endl;
	exit(1);
}

void log_send(packet p, bool re=false){
	cout << "\tSending packet " << p.seq << " " << CWND;
	if(re) cout << " Retransmission";
	if(p.syn_flag) cout << " SYN";
	if(p.fin_flag) cout << " FIN";
	cout << endl;
}
void log_recv(packet p){
	cout << "\tReceiving packet " << p.seq + p.pktsize() << endl;
}

unsigned int add(unsigned int seq, unsigned int bytes){
	return (seq + bytes) % MAX_SEQ;
}

// case-insensitive string compare function
bool compare(string a, string b){
	if(a.length() != b.length()) return false;
	for(size_t i = 0; i < a.length(); i++){
		if(tolower(a[i]) != tolower(b[i])) return false;
	}
	return true;
}

// uses dirent.h functions to comb the directory
string search_directory(string filename){
	DIR *dir;
	dirent *pdir;
	dir=opendir(".");
	while((pdir=readdir(dir)))
	{
		string name = pdir->d_name;
		if(compare(name,filename)){
			closedir(dir);
			return name;
		}
	}
	closedir(dir);
	return "";
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

	/////////////////////
	// HANDSHAKING
	/////////////////////


	fd_set inSet;
	struct timeval timeout;
			
			// wait for specified time
	timeout.tv_sec = 0;
	timeout.tv_usec = TIMEOUT * 1000;

	int n;
	unsigned int seq = INIT_SEQ;

	// receive first SYN

	char pkt[1024];
	memset(pkt,0,1024);
	n = recvfrom(sockfd,(char*)pkt,1024,MSG_WAITALL,(struct sockaddr*)&cli_addr,&clilen);
	if(n<0) error("ERROR reading from socket");
	packet syn;
	decode(pkt,syn);

	log_recv(syn);

	// send SYN-ACK

	memset(pkt,0,1024);
	packet synack(seq,add(syn.seq,syn.pktsize()),true,true,false,0,NULL);
	unsigned int pktsize = encode(synack,pkt);
	n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
	seq = add(seq,pktsize);
	log_send(synack);
	if (n < 0)
		 error("ERROR writing to socket");

	// receive ACK with name of requested file
	char recvack[1024];
	packet ack;
	while(true){
		FD_ZERO(&inSet);
		FD_SET(sockfd, &inSet);
		int rv = select(sockfd+1,&inSet,NULL,NULL,&timeout);
		if(rv < 0){
			error("ERROR reading from socket");
		} else if(rv == 0){
			n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
			log_send(synack, true);
			if (n < 0)
				error("ERROR writing to socket");
			timeout.tv_sec = 0;
			timeout.tv_usec = TIMEOUT * 1000;
		} else{

			memset(recvack,0,1024);
			n = recvfrom(sockfd,(char*)recvack,1024,MSG_WAITALL,(struct sockaddr*)&cli_addr,&clilen);
			if(n<0) error("ERROR reading from socket");
			decode(recvack,ack);
			log_recv(ack);
			if(ack.ack == seq){
				break;
			}
		}
	}

	string filename(ack.data);

	// find the file in the directory
	filename = search_directory(filename);
	if(filename != ""){

		ifstream file(filename.c_str(),ios_base::binary);
		char buffer[MAX_DATA_SIZE];
		while(file.good()){
			memset(buffer,0,MAX_DATA_SIZE);
			file.read(buffer,MAX_DATA_SIZE-1);
			streamsize bytes = file.gcount();
			buffer[bytes] = 0;
			packet sendpkt(seq,add(ack.seq,ack.pktsize()),false,true,false,bytes,buffer);
			memset(pkt,0,1024);
			pktsize = encode(sendpkt,pkt);
			n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
			seq = add(seq,pktsize);
			log_send(sendpkt);
			if (n < 0)
				 error("ERROR writing to socket");


			char recvbuf[1024];

			while(true){
				FD_ZERO(&inSet);
				FD_SET(sockfd, &inSet);
				int rv = select(sockfd+1,&inSet,NULL,NULL,&timeout);
				if(rv < 0){
					error("ERROR reading from socket");
				} else if(rv == 0){
					n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
					log_send(sendpkt, true);
					if (n < 0)
						error("ERROR writing to socket");
					
					timeout.tv_sec = 0;
					timeout.tv_usec = TIMEOUT * 1000;
				} else{

					memset(recvbuf,0,1024);
					n = recvfrom(sockfd,(char*)recvbuf,1024,MSG_WAITALL,(struct sockaddr*)&cli_addr,&clilen);
					if(n<0) error("ERROR reading from socket");
					decode(recvbuf,ack);
					log_recv(ack);
					if(ack.ack == seq){
						break;
					}
				}
			}


		}
		file.close();
	}

	// FIN stuff

	packet finpkt(seq,add(ack.seq,ack.pktsize()),false,true,true,0,NULL);
	memset(pkt,0,1024);
	pktsize = encode(finpkt,pkt);
	n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
	seq = add(seq,pktsize);
	log_send(finpkt);
	if (n < 0)
		 error("ERROR writing to socket");

	int retrycount = 0;
	while(true){
		FD_ZERO(&inSet);
		FD_SET(sockfd, &inSet);
		int rv = select(sockfd+1,&inSet,NULL,NULL,&timeout);
		if(rv < 0){
			error("ERROR reading from socket");
		} else if(rv == 0){
			n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
			log_send(finpkt, true);
			if (n < 0)
				error("ERROR writing to socket");
			
			timeout.tv_sec = 0;
			timeout.tv_usec = TIMEOUT * 1000;
			retrycount++;
			if(retrycount == 5){
				break;
			}
		} else{

			
			break;
		}
	}



	close(sockfd);

	return 0;
}
