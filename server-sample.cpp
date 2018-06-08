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
using namespace std;

#define INIT_SEQ 123
#define MAX_DATA_SIZE 1009

struct packet{
	unsigned int seq; // 4 bytes
	unsigned int ack; // 4 bytes
	bool syn_flag; // 1 byte
	bool ack_flag; // 1 byte
	bool fin_flag; // 1 byte
	unsigned int data_size; // 4 bytes
	char data[MAX_DATA_SIZE]; // 1024-15 = 1009 bytes 
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

packet decode(char* buffer){
	unsigned char* ptr = (unsigned char*)buffer;
	unsigned int seq = (ptr[3]<<24)+(ptr[2]<<16)+(ptr[1]<<8)+(ptr[0]);
	ptr += 4;
	unsigned int ack = (ptr[3]<<24)+(ptr[2]<<16)+(ptr[1]<<8)+(ptr[0]);
	ptr += 4;
	bool syn_flag = ptr[0] != 0;
	ptr += 1;
	bool ack_flag = ptr[0] != 0;
	ptr += 1;
	bool fin_flag = ptr[0] != 0;
	ptr += 1;
	unsigned int data_size = (ptr[3]<<24)+(ptr[2]<<16)+(ptr[1]<<8)+(ptr[0]);
	ptr += 4;
	char data[MAX_DATA_SIZE];
	strcpy(data,(char*)ptr);

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

	int n;
	int seq = INIT_SEQ;

	// receive first SYN

	char pkt[1024];
	memset(pkt,0,1024);
	n = recvfrom(sockfd,(char*)pkt,1024,MSG_WAITALL,(struct sockaddr*)&cli_addr,&clilen);
	if(n<0) error("ERROR reading from socket");
	packet syn = decode(pkt);

	log_packet(syn);

	// send SYN-ACK

	memset(pkt,0,1024);
	packet synack(seq,syn.seq+1,true,true,false,0,NULL);
	encode(synack,pkt);
	n = sendto(sockfd, (const char*)pkt, 1024, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
	seq++;
	if (n < 0)
		 error("ERROR writing to socket");

	// receive ACK with name of requested file

	memset(pkt,0,1024);

	n = recvfrom(sockfd,(char*)pkt,1024,MSG_WAITALL,(struct sockaddr*)&cli_addr,&clilen);
	if(n<0) error("ERROR reading from socket");
	packet ack = decode(pkt);

	log_packet(ack);

	string filename(ack.data);

	// find the file in the directory
	filename = search_directory(filename);
	if(filename == ""){
		// whatever happens if the file doesn't exist :(
	} else{
		ifstream file(filename.c_str(),ios_base::binary);
		char buffer[MAX_DATA_SIZE];
		while(file.good()){
			memset(buffer,0,MAX_DATA_SIZE);
			file.read(buffer,MAX_DATA_SIZE-1);
			buffer[MAX_DATA_SIZE-1] = 0;
			cout << buffer;
			packet sendpkt(seq,ack.seq+1,true,true,false,strlen(buffer),buffer);
			memset(pkt,0,1024);
			encode(sendpkt,pkt);
			n = sendto(sockfd, (const char*)pkt, 1024, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
			seq++;
			if (n < 0)
				 error("ERROR writing to socket");

			memset(pkt,0,1024);

			// n = recvfrom(sockfd,(char*)pkt,1024,MSG_WAITALL,(struct sockaddr*)&cli_addr,&clilen);
			// if(n<0) error("ERROR reading from socket");
			// packet rcvpkt = decode(pkt);

			// log_packet(rcvpkt);

		}
		file.close();

		packet finpkt(seq,ack.seq+1,true,true,true,0,NULL);
		memset(pkt,0,1024);
		encode(finpkt,pkt);
		n = sendto(sockfd, (const char*)pkt, 1024, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
		seq++;
		if (n < 0)
			 error("ERROR writing to socket");
	}


	close(sockfd);

	return 0;
}
