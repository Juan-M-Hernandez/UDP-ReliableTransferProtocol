/* A simple server in the internet domain using TCP
   The port number is passed as an argument
   This version runs forever, forking off a separate
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>  /* signal name macros, and the kill() prototype */
#include <ctype.h>
void error(char *msg)
{
    perror(msg);
    exit(1);
}

//////////////////////////////////////////////////
// global constants
//////////////////////////////////////////////////
char* HTTP_response_header = "HTTP/1.1 200 OK";
char* HTTP_connection_header = "Connection: close";
char* HTTP_contentTypeTextHtml_header = "Content-Type: text/html";
char* HTTP_contentTypeJpeg_header = "Content-Type: image/jpeg";
char* HTTP_contentTypePng_header = "Content-Type: image/png";
char* HTTP_contentTypeGif_header = "Content-Type: image/gif";


char* HTTP_contentLength_header = "Content-Length: ";  // requires parsing int to string

char HTTP_lineEnd[3];

char* pageNotFoundMsg = "<html><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";

//120 MB in Bytes
#define MB_to_bytes_120 125829120
char responseMsg[MB_to_bytes_120];

//////////////////////////////////////////////////
// parse first header of an HTTP GET request
//////////////////////////////////////////////////

char* getPathName(char* request){
  if (request[0] != 'G')
    error("Invalid HTTP Request");
  int index = 1;
  while(request[index] != '/') {
    index++;
  }
  index++; // TODO: this skips the leading slash, for this
  // assignment we only deal with files in the current dir
  // and not subdirectories, we will call open directly
  // to expand support for subdirectories 1) get cwd
  // 2) remove this line, append this now relative path to
  // the cwd (.. in a path is invalid) and use that file
  int endIndex = index;
  while (request[endIndex] != ' '){
    endIndex++;
  }
  int sizeOfPathNameString = endIndex - index + 1;
  char* retVal = malloc(sizeof(char) * sizeOfPathNameString);
  retVal[sizeOfPathNameString - 1] = '\0';
  
  int indexIterator = index;
  while (indexIterator != endIndex) {
    retVal[indexIterator - index] = request[indexIterator];
    indexIterator++;
  }
  return retVal;
}


char* getFileType(char* fileName){
  int index = 0;
  int len = strlen(fileName);
  while(fileName[index] != '.'){
    index++;
    if (index == len){
      return HTTP_contentTypeTextHtml_header;
    }
  }
    
  int sizeOfExtension = len - index + 1;
  char extension[100]; // TODO: fix buffer overflow
  extension[sizeOfExtension - 1] = '\0';
  int indexIterator = index;
  while(indexIterator != len){
    extension[indexIterator - index] = fileName[indexIterator];
    indexIterator++;
  }
  if (strcmp(extension, ".html") == 0 ||
      strcmp(extension, ".htm") == 0 ||
      strcmp(extension, ".txt") == 0) {
    return HTTP_contentTypeTextHtml_header;
  }
  else if (strcmp(extension, ".jpg") == 0 ||
	   strcmp(extension, ".jpeg") == 0){
    return HTTP_contentTypeJpeg_header;
  }

  else if (strcmp(extension, ".png") == 0) {
    return HTTP_contentTypePng_header;
  }
  else if (strcmp(extension, ".gif") == 0){
    return HTTP_contentTypeGif_header;
  }
  else // error or send as text?
    return HTTP_contentTypeTextHtml_header;
}

// must be valid c string
char* sanitizeFileName(char* fileName){
  size_t fileNameLength = strlen(fileName);
  size_t iter;
  for(iter = 0; iter < fileNameLength; iter++){
    fileName[iter] = tolower(fileName[iter]);
  }

  for(iter = 0; iter < fileNameLength; iter++){
    if (fileName[iter] == '%' && fileName[iter + 1] == '2' &&
	fileName[iter + 2] == '0'){
      fileName[iter] = ' ';
      size_t iter2 = 0;
      while(fileName[iter + iter2 + 3] != '\0'){
	fileName[iter + iter2 + 1] = fileName[iter + iter2 + 3];
	iter2++;
      }
      fileName[iter + iter2 + 1] = '\0';
      fileNameLength -= 2;
      iter--;
    }
  }
  return fileName;
}
//////////////////////////////////////////////////
// Tools for creating HTTP response to a GET request
//////////////////////////////////////////////////
char singleDigitToChar(int digit){
  char zero = '0';
  return digit + zero;
}
char* convertIntToString(int total){
  int charsNeededToRepresentValue = 2;//1 digit plus null byte
  int totalIterator = total;
  // finds the number of characters that satisfy the creation of a string
  while (totalIterator / 10 != 0){
    charsNeededToRepresentValue++;
    totalIterator = totalIterator / 10;
  }
  char* retVal = malloc(sizeof(char) * (charsNeededToRepresentValue));

  charsNeededToRepresentValue--;
  retVal[charsNeededToRepresentValue] = '\0';
  while (charsNeededToRepresentValue != 0){
    charsNeededToRepresentValue--;
    retVal[charsNeededToRepresentValue] = singleDigitToChar(total % 10);
    total /= 10;
  }
  return retVal;
}


void init_HTTP_lineEnd(){
  HTTP_lineEnd[0] = '\r';
  HTTP_lineEnd[1] = '\n';
  HTTP_lineEnd[2] = '\0';

}

void formatResponseMsg(){
  init_HTTP_lineEnd();
  responseMsg[0] = '\0';
  strcat(responseMsg, HTTP_response_header);
  strcat(responseMsg, HTTP_lineEnd);
  strcat(responseMsg, HTTP_connection_header);
  strcat(responseMsg, HTTP_lineEnd);
  strcat(responseMsg, HTTP_contentLength_header);
}


void finalizeResponseMsg(long length, char* data){
  int length_msg = strlen(responseMsg);
  int iterator = 0;
  while (iterator != length) {
    responseMsg[length_msg + iterator] = data[iterator];
    iterator++;
  }
}

// Interface entry point
char* getFinalResponseMsg(long length, char* data, char* fileName, int* responseLen){
  formatResponseMsg();
  char* len_data = convertIntToString(length);
  strcat(responseMsg, len_data);
  strcat(responseMsg, HTTP_lineEnd);
  char* contentType = getFileType(fileName);
  strcat(responseMsg, contentType);
  strcat(responseMsg, HTTP_lineEnd);
  strcat(responseMsg, HTTP_lineEnd);

  *responseLen = strlen(responseMsg);
  finalizeResponseMsg(length, data);
  return responseMsg;
}

char* get404ResponseMsg(){
  init_HTTP_lineEnd();
  responseMsg[0] = '\0';
  strcat(responseMsg, "HTTP/1.1 404 Not Found");
  strcat(responseMsg, HTTP_lineEnd);
  strcat(responseMsg, HTTP_connection_header);
  strcat(responseMsg, HTTP_lineEnd);
  strcat(responseMsg, HTTP_contentLength_header);

  int length404 = strlen(pageNotFoundMsg);
  char* len_data = convertIntToString(length404);
  strcat(responseMsg, len_data);
  strcat(responseMsg, HTTP_lineEnd);
  strcat(responseMsg, HTTP_contentTypeTextHtml_header);
  strcat(responseMsg, HTTP_lineEnd);
  strcat(responseMsg, HTTP_lineEnd);
  finalizeResponseMsg(strlen(pageNotFoundMsg), pageNotFoundMsg);
  return responseMsg;
}

int main(int argc, char *argv[])
{
  //////////////////////////////////////////////////
  // Socket Creation + API calls
  //////////////////////////////////////////////////
  int sockfd, newsockfd, portno;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  
  if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);  // create socket
    if (sockfd < 0)
        error("ERROR opening socket");
    memset((char *) &serv_addr, 0, sizeof(serv_addr));  // reset memory

    // fill in address info
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(sockfd, 5);  // 5 simultaneous connection at most

    //accept connections
    while (1){
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    if (newsockfd < 0)
     error("ERROR on accept");

    //////////////////////////////////////////////////
    // Parse an HTTP Get request
    //////////////////////////////////////////////////
    int sizeOfGetRequestMsg;
    char buffer[2560];

    memset(buffer, 0, 2560);  // reset memory
    //read client's message
    sizeOfGetRequestMsg = read(newsockfd, buffer, 2559);
    if (sizeOfGetRequestMsg < 0) error("ERROR reading from socket");

    buffer[sizeOfGetRequestMsg] = '\0';

    char* fileName = getPathName(buffer);
    // TODO: sanitize filename function (capital letters spaces)
    fileName = sanitizeFileName(fileName);


    FILE* requestedFile = fopen(fileName, "r");
    if (requestedFile == NULL) {
      char* output = get404ResponseMsg();
      write(newsockfd, output, strlen(output));
      return 1;
    }
    fseek(requestedFile, 0, SEEK_END);
    long fileSize = ftell(requestedFile);
    fseek(requestedFile, 0, SEEK_SET);

    char* fileStream = malloc(fileSize + 1);
    fread(fileStream, fileSize, 1, requestedFile);
    fclose(requestedFile);
    fileStream[fileSize] = '\0';

    printf("%s", buffer);

    int httpResponseLength;
    char* response = getFinalResponseMsg(fileSize, fileStream, fileName, &httpResponseLength);
    
    

    //reply to client
    int n = write(newsockfd, response, fileSize + httpResponseLength);
    if (n < 0) error("ERROR writing to socket");

    buffer[0] = '\0';
    responseMsg[0] = '\0';
    }
    // HERE
    //printf("%s", response);

    //    printf("-----------------\n%")
    close(newsockfd);  // close connection
    close(sockfd);

    return 0;
}

// TODO case sensitivity + space

