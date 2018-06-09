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

#define INIT_SEQ 0
#define HEADER_SIZE 15
#define MAX_DATA_SIZE 1009
#define CWND 5120
#define MAX_SEQ 30720


#include <iterator>
#include <vector>
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
	fakecpy(data,(char*)ptr,data_size);

	packet p(seq,ack,syn_flag,ack_flag,fin_flag,data_size,data);
	return p;
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

	int n;
	unsigned int seq = INIT_SEQ;

	// receive first SYN

	char pkt[1024];
	memset(pkt,0,1024);
	n = recvfrom(sockfd,(char*)pkt,1024,MSG_WAITALL,(struct sockaddr*)&cli_addr,&clilen);
	if(n<0) error("ERROR reading from socket");
	packet syn = decode(pkt);

	log_recv(syn);

	// send SYN-ACK

	memset(pkt,0,1024);
	packet synack(seq,syn.seq+syn.pktsize(),true,true,false,0,NULL);
	unsigned int pktsize = encode(synack,pkt);
	n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
	seq = add(seq,pktsize);
	log_send(synack);
	if (n < 0)
		 error("ERROR writing to socket");

	// receive ACK with name of requested file

	memset(pkt,0,1024);

	n = recvfrom(sockfd,(char*)pkt,1024,MSG_WAITALL,(struct sockaddr*)&cli_addr,&clilen);
	// TODO: clients responsibility to retransmit file request packet if timeout
	
	if(n<0) error("ERROR reading from socket");
	packet ack = decode(pkt);

	log_recv(ack);

	string filename(ack.data);

	// find the file in the directory
	filename = search_directory(filename);

	//	map<unsigned int, packet> packetMap;
        vector<pair<unsigned int, packet>> packetVector;
	if(filename == ""){
		// whatever happens if the file doesn't exist :(
	} else{
	  /////////////////////////////////////////////////
	  // Read file into packets, 
	  /////////////////////////////////////////////////
	  // we store each packet into a pair, 1st == seq number, 2nd == packet struct
	  // if we need to retransmit a packet we can search our vector of formatted
	  // packets instead of attempting to read the file again
	  ifstream file(filename.c_str(),ios_base::binary);
	  char buffer[MAX_DATA_SIZE];
	  while(file.good()){		  
	    memset(buffer,0,MAX_DATA_SIZE);
	    file.read(buffer,MAX_DATA_SIZE-1);
	    streamsize bytes = file.gcount();
	    buffer[bytes] = 0;
	    packet sendpkt(seq,ack.seq+ack.pktsize(),false,true,false,bytes,buffer);
	    
	    packetVector.push_back(pair<unsigned int, packet>(seq, sendpkt));
	    seq = add(seq,pktsize);
	    
	  }
	  file.close();


	  ////////////////////////////////////////////////////////////
	  // here we
	  for (long unsigned int i = 0; i < packetVector.size(); ++i) {
	    //	    for (long unsigned int i = 0; i < packetVector.size(); ++i) {
	    // TODO ^^ increase limit by 1, if i == .size(), we busy wait for a FIN pkt
	    // if we get a SYN with a retransmission request and a seq number
	    // we search for the seq number and reset the index of the iterator to
	    // the matching packet
	    memset(pkt, 0, 1024);
	    pktsize = encode(packetVector[i].second,pkt);
	    n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
	    log_send(packetVector[i].second);
	    if (n < 0)
	      error("ERROR writing to socket");
	    
	    memset(pkt,0,1024);
	    
	  }
	  packet finpkt(seq,ack.seq+ack.pktsize(),false,true,true,0,NULL);
	  memset(pkt,0,1024);
	  pktsize = encode(finpkt,pkt);
	  n = sendto(sockfd, (const char*)pkt, pktsize, MSG_CONFIRM,(const struct sockaddr*)&cli_addr,clilen);  // write to the socket
	  seq = add(seq,pktsize);
	  if (n < 0)
	    error("ERROR writing to socket");
	}
	
	
	close(sockfd);

	return 0;
}
