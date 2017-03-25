#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

void helpOutput(int nmbrOfRgmnts, char* othrRgmnt){
    if(nmbrOfRgmnts < 3)
    {
        printf("httpc is a curl-like application but supports HTTP protocol only.\nUsage:\n\thttpc command [arguments]\nThe commands are:\n\tget executes a HTTP GET request and prints the response.\n\tpost executes a HTTP POST request and prints the response.\n\thelp prints this screen.\nUse \"httpc help [command]\" for more information about a command.\n");
    }
    else if(nmbrOfRgmnts == 3){
        
        for(int i = 0; othrRgmnt[i]; i++){
            othrRgmnt[i] = toupper(othrRgmnt[i]);
        }
        
        if(strcmp(othrRgmnt, "POST") == 0)
        {
            printf("usage: httpc post [-v] [-h key:value] [-d inline-data] [-f file] URL\nPost executes a HTTP POST request for a given URL with inline data or from file.\n    -v\t\t\tPrints the detail of the response such as protocol,status, and headers.\n    -h key:value\tAssociates headers to HTTP Request with the format 'key:value'.\n    -d string\t\tAssociates an inline data to the body HTTP POST request.\n    -f file\t\tAssociates the content of a file to the body HTTP POST request.\nEither [-d] or [-f] can be used but not both.\n");
        }
        else if(strcmp(othrRgmnt, "GET") == 0)
        {
            printf("usage: httpc get [-v] [-h key:value] URL\nGet executes a HTTP GET request for a given URL.\n    -v\t\t\tPrints the detail of the response such as protocol, status, and headers.\n    -h key:value\tAssociates headers to HTTP Request with the format 'key:value'.\n");
        }else{
            printf("Not a recognizable command, try:\n httpc HELP GET \n httpc HELP POST\n");
        }
    }
}

// gets size of file and returns content
int getFileData(char* fileName, char **fileData){
    FILE *file = fopen(fileName, "r");
    char *content;
    size_t n = 0;
    int c;
    
    // file couldn't be opened
    if (file == NULL) {
        printf("Error file couldn't be opened or doesn't exist\n");
        return NULL;
    }
    
    // seek end of file
    fseek(file, 0, SEEK_END);
    
    // get position of cursor
    long f_size = ftell(file);
    // reposition cursor at start of file
    fseek(file, 0, SEEK_SET);
    
    //allocate enough space for the content of the file
    content = malloc(f_size);
    
    
    // copy content over into the char array
    while ((c = fgetc(file)) != EOF) {
        content[n++] = (char)c;
    }
    
    // printf("Content = %s\n", content);
    
    // set termination character
    *fileData = malloc(f_size * sizeof(char));
    sprintf(*fileData, "%s\r\n\r\n", content);
    return 0;
}

void requestLineCreator(char* method, char* URL, char** requestLine, char** address){
    char* serverAddress;
    int addressSize;
    
    char* arguments;
    int argumentsSize;
    
    char* start;
    char* end;
    int firstIndex;
    int secondIndex;
    
    printf("URL = %s\n", URL);
    
    if((strncmp(URL, "https://", 8) == 0) || (strncmp(URL, "http://", 7) == 0)){
        start = strchr(URL, '/');
        start = strchr(start + 1, '/');
        firstIndex = (int)(start + 1 - URL);
        
        if(strchr(start + 1, '/') != NULL){
        
            end = strchr(start + 1, '/');
            secondIndex = (int) (end - URL);
        
            addressSize = secondIndex - firstIndex;
        
            serverAddress = malloc(addressSize);
            memmove(serverAddress, URL + firstIndex, addressSize);
            serverAddress[addressSize] = '\0';
        
            argumentsSize = strlen(URL) - secondIndex + 1;
            arguments = malloc(argumentsSize);
            memmove(arguments, URL + secondIndex, argumentsSize);
            arguments[argumentsSize] = '\0';
        }else{
            addressSize = strlen(URL) - firstIndex;
            serverAddress = malloc(addressSize);
            memmove(serverAddress, URL + firstIndex, addressSize);
            serverAddress[addressSize] = '\0';
            
            arguments = malloc(2);
            arguments = "/";
        }
    }else if(strchr(URL, '/') != NULL){
        end = strchr(URL, '/');
        secondIndex = (int) (end - URL);
        
        addressSize = secondIndex;
        argumentsSize = strlen(URL) - secondIndex + 1;
        
        serverAddress = malloc(addressSize);
        memmove(serverAddress, URL, addressSize);
        serverAddress[addressSize] = '\0';
        
        arguments = malloc(argumentsSize);
        memmove(arguments, URL + secondIndex, argumentsSize);
        arguments[argumentsSize] = '\0';
    }else{
     
        serverAddress = malloc(strlen(URL));
        sprintf(serverAddress, "%s", URL);
        
        arguments = malloc(2);
        arguments = "/";
    }

    *address = malloc(strlen(serverAddress) + 1);
    sprintf(*address, "%s", serverAddress);
    
    *requestLine = malloc(strlen(method) + strlen(arguments) + strlen(" HTTP/1.0\r\n") + 1);
    sprintf(*requestLine, "%s %s HTTP/1.0\r\n", method, arguments);
}

int requestCreator(char **parameters, int nmbrOfPrmtrs, char** request, char** URI){
    int verboseSetter = 1;
    
    char* headerLines;
    int headerAllocationBool = 0;
    
    char* body;
    
    char* address;
    
    char* requestLine;
    
    int DAndFCheck = 0;
    
    if(strcmp(parameters[1], "GET") == 0){
        DAndFCheck = 1;
    }
   
    for( int i = 1; i < nmbrOfPrmtrs; i++){
        if(strcmp(parameters[i], "-v") == 0){   // check for the verbose attribute
            verboseSetter = 0;
        }else if(strcmp(parameters[i], "-h") == 0){ // check for headerlines
            i++;
            if(headerAllocationBool == 0){
                headerAllocationBool = 1;
                headerLines = malloc(strlen(parameters[i]) + strlen("\r\n"));
                strcat(headerLines, parameters[i]);
                strcat(headerLines, "\r\n");
            }else if(headerAllocationBool == 1){
                headerLines = realloc(headerLines, strlen(headerLines) + strlen(parameters[i]) + strlen("\r\n"));
                strcat(headerLines, parameters[i]);
                strcat(headerLines, "\r\n");
            }
        }else if(strcmp(parameters[i], "-d") == 0){
            if(DAndFCheck == 0){
                DAndFCheck = 1;
                i++;
                body = malloc(strlen(parameters[i]) + strlen("\r\n"));
                sprintf(body, "%s\r\n", parameters[i]);
            }else if(DAndFCheck == 1){
                printf("POST request should only have -f or -d but not both at the same time. GET should not have any of the two...\n");
                return 1;
            }
        }else if(strcmp(parameters[i],"-f") == 0){
            if(DAndFCheck == 0){
                DAndFCheck = 1;
                i++;
                // if file is empty just put end sequence in the body
                if(getFileData(parameters[i], &body) == 1){
                    body = malloc(strlen("\r\n"));
                    sprintf(body, "\r\n");
                }
            }else if(DAndFCheck == 1){
                printf("Error: POST request should only have -f or -d but not both at the same time. GET should not have any of the two...\n");
                return 1;
            }
        }else if(i == nmbrOfPrmtrs - 1){
            requestLineCreator(parameters[1], parameters[i], &requestLine, &address);
        }
    }
    
    if(((DAndFCheck == 1) && (strcmp(parameters[1], "GET") == 0))){
        body = malloc(strlen("\r\n"));
        sprintf(body, "\r\n");
    }else if((DAndFCheck == 0) && (strcmp(parameters[1], "POST") == 0)){
        printf("Error: Post request requires -d (inline data) or -f (file to read from)\n");
        return 1;
    }
    
    if(headerAllocationBool == 0){
        *URI = malloc(strlen(address));
        *request = malloc(strlen(requestLine) + 4 + strlen(body));
        sprintf(*request, "%s\r\n%s", requestLine, body);
        sprintf(*URI, "%s", address);
        return 0;
    }

    
    *URI = malloc(strlen(address));
    *request = malloc(strlen(requestLine) + strlen(headerLines) + strlen(body));
    sprintf(*request, "%s%s%s", requestLine, headerLines, body);
    sprintf(*URI, "%s", address);
    return 0;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]){
    char *method = argv[1];
    char *request;
    char* port = "8080";
    char* URI;
    
    if(argc < 2){
        printf("Insufficient arguments\n");
        return 0;
    }
    
    for(int i = 0; method[i]; i++){
        method[i] = toupper(method[i]);
    }
    
    if(strcmp(method, "HELP") == 0)
    {
        helpOutput(argc, argv[2]);
        return 0;
    }
    else if((strcmp(method, "GET") == 0) || (strcmp(method, "POST") == 0))
    {
        if(requestCreator(argv, argc, &request, &URI) == 0){
           if(request == NULL){
               printf("Request couldn't be created, try again...\n");
               return 0;
           }else{
               printf("Request:\n%s\n", request);
               printf("Address:\n%s\n", URI);
           }
        }else{
            return 0;
        }
    }else{
        printf("Command line arguments format is wrong, try again.\n");
        return 0;
    }
    
    int maxdatasize = 5000;
    int sockfd, numbytes;
    char buf[maxdatasize];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if(strcmp(URI, "localhost") == 0){
        hints.ai_flags = AI_PASSIVE;
    }
    
    if ((rv = getaddrinfo(URI, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Client: resolving host");
            continue;
        }
        break;
    }
    
    if (p == NULL) {
        fprintf(stderr, "Client: failed to connect\n");
        return 0;
    }
    
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    printf("Client: succesfully connected to %s\n", s);
    
    freeaddrinfo(servinfo); // all done with this structure
    
    write(sockfd, request, strlen(request));
    
    if ((numbytes = recv(sockfd, buf, maxdatasize-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    
    buf[numbytes] = '\0';
    
    printf("Client: The server response is: %s\n",buf);
    
    close(sockfd);
    
    return 0;
}
