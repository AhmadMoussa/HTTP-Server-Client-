#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>

#define LIST 1
#define GET 2
#define POST 3

// Set how many connections can be pending at a time beforehand
#define BACKLOG 10

int verbose = 1;

// using the dirent header, that is basically a directory file stream,
// we retrieve the files and directories that are contained in the
// path that is passed as an argument
int getListOfFiles(char** response, char* currentDirectory){
    char* list;

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (currentDirectory)) != NULL) {
        /* print all the files and directories within directory */
        if((ent = readdir (dir)) != NULL) {
            list = malloc(strlen(ent->d_name));
            sprintf(list, "%s\n", ent->d_name);
            
            while ((ent = readdir (dir)) != NULL) {
                list = realloc(list, strlen(list) + strlen(ent->d_name));
                sprintf (list,"%s%s\n", list, ent->d_name);
            }
        }
        closedir (dir);
    } else {
        /* could not open directory */

        perror ("getListOfFiles:");
        return 1;
    }
    
    if(list == NULL){
        list = malloc(strlen("Directory Empty"));
        sprintf(list, "Directory Empty");
    }
    
    *response = malloc(strlen(list));
    sprintf(*response, "200 OK:\n%s\n", list);
    return 0;
}

// function that extracts the name of the file from the message sent by the client
// this is a bit clumsy and risky in c
char* getFileName(char* clientMessage){
    char *firstWhiteSpace;
    char *secondWhiteSpace;
    int firstIndex;
    int secondIndex;
    
    firstWhiteSpace = strchr(clientMessage, '/');
    secondWhiteSpace = strchr(firstWhiteSpace, ' ');
    
    // plus one since we want to exclude the slash
    firstIndex = (int)(firstWhiteSpace - clientMessage) + 1;
    
    // minus one since we don't want a whitespace at the end of the string
    secondIndex = (int)(secondWhiteSpace - clientMessage);
    
    int fileNameSize = (secondIndex - firstIndex);
    char* fileName = malloc(fileNameSize);
    fileName = malloc(fileNameSize);
    memmove(fileName, clientMessage + firstIndex, fileNameSize);
    
    // append the termination character manually because memmove doesn't acount for that
    fileName[fileNameSize] = '\0';
    return fileName;
}

int sendFile(int remoteSocketFileDescriptor, char* clientMessage, char** response){
    char* fileName = getFileName(clientMessage);
    FILE *file = fopen(fileName, "rb");
    
    if(file == NULL)
    {
        printf("Error: file couldn't be opened or doesn't exist\n");
        return 1;
    }
    
    char* content;
    size_t n = 0;
    int c;
    printf("Sending file, this may take a few seconds...\n");
    
    fseek(file, 0, SEEK_END);
    long f_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    content = malloc(f_size);
    
    // copy content over into the char array
    while ((c = fgetc(file)) != EOF) {
        content[n++] = (char)c;
    }
    
    *response = malloc(f_size * sizeof(char));
    sprintf(*response, "%s\r\n\r\n", content);
    return 0;
}

// Determines what kind of response we should send back
int selectResponse(char* clientMessage){
    if(strncmp(clientMessage, "GET", 3) == 0){
        if(strncmp(clientMessage, "GET / ", 6) == 0){
           // printf("%s", strstr(clientMessage, " / "));
            return LIST;
        }else{
            return GET;
        }
    }else if(strncmp(clientMessage, "POST /", 5) == 0){
        return POST;
    }
    return 0; // returning 0 means that the request wasn't recognized
}

void getBody(char* clientMessage, char** body){
    char* position = strstr(clientMessage, "\r\n");
    if(position != NULL){
        position = strstr(position + 1, "\r\n");
        if(position != NULL){
            int index = (int)(position - clientMessage + 2);
            int size = strlen(clientMessage) - index;
            *body = malloc(size);
            memmove(*body, clientMessage + index, size);
        }
    }
}

int postRequestHandler(char* clientMessage){
    // get filename
    char* fileName = getFileName(clientMessage);
    
    // try to open file
    FILE *f = fopen(fileName, "w");
    if (f == NULL)
    {
        printf("Error opening file!\n");
        return 1;
    }
    
    // get content to be writtent to file
    char* body;
    getBody(clientMessage, &body);
    fprintf(f, "%s", body);

    // close file
    fclose(f);
    return 0;
}

// Function that handles creeating the appropriate response
void serverResponse(int remoteSocketFileDescriptor, char* clientMessage, char** response, char* currentDirectory){
    int responseType = selectResponse(clientMessage);
    
    if(responseType == 0){
        *response = malloc(strlen("Error 400, bad request\n"));
        sprintf(*response, "%s", "Error 400, bad request\n");
    }else if(responseType == 1){
        getListOfFiles(response, currentDirectory);
        printf("Server: Sent back a list of files in the directory\n");
    }else if(responseType == 2){
        if(sendFile(remoteSocketFileDescriptor, clientMessage, response) == 1){
            *response = "Error 404, file doesn't exist (or couldn't be opened)\n";
        }else{
            printf("Success sending file\n");
        }
    }else if(responseType == 3){
        if(postRequestHandler(clientMessage) == 1){
            *response = "Error posting file";
        }else{
            printf("201 Created, resource has been created");
            *response = "201 Created, resource has been created";
        }
    }
}

int listenForConnections(int verbose, char* portNumber, char* path){
    char currentDirectory[1024];
    if(strcmp(path, "defaultPath") == 0){
        if (getcwd(currentDirectory, sizeof(currentDirectory)) != NULL)
            fprintf(stdout, "Current working dir: %s\n", currentDirectory);
        else
            perror("getcwd() error");
    }else{
        sprintf(currentDirectory, "%s", path);
    }
    
    int socketFileDescriptor;
    
    int yes = 1;
    int status;
    
    struct addrinfo hints, *servinfo, *p;
    
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;    // bind port on own IP address
    
    // Attempt to get the addresses, throw error if we can't get them
    if ((status = getaddrinfo(NULL, portNumber, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    
    // servinfo now points to a linked list of 1 or more struct addrinfosaa
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        // First, not a useful socket descriptor, hence continue looping
        // Don't enter clause if socket is usable
        if ((socketFileDescriptor = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        
        // Set socket options such that at socket level we are allowing addresses to be reused
        if (setsockopt(socketFileDescriptor, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        
        // Thirdly, bind socket to the port that we specified earlier
        if (bind(socketFileDescriptor, p->ai_addr, p->ai_addrlen) == -1) {
            close(socketFileDescriptor);
            perror("server: bind");
            continue;
        }
        
        // Break loop if a good socket has been found and we bound succesfully
        break;
    }
    
    freeaddrinfo(servinfo); // free the servinfo linked-list, since we used what we needed
    
    // If we couldn't bind then we should stop
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    
    // Attempt to listen to the socket
    if (listen(socketFileDescriptor, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    
    printf("server: waiting for connections...\n");
    
    // Size of incoming request
    socklen_t size;
    int remoteSocketFileDescriptor; // Incoming connection request
    char s[INET6_ADDRSTRLEN]; // Buffer containing address of the client
    char clientRequest[3000]; // Buffer that we will write the message of the client into
    char *response;
    
    const struct sockaddr_storage request_addr;
    
    // Now we need to loop to continuously accept connection requests
    while(1){
        size = sizeof request_addr;
        remoteSocketFileDescriptor = accept(socketFileDescriptor, (struct sockaddr *)&request_addr, &size);
        
        if (remoteSocketFileDescriptor == -1) {
            perror("accept");
            continue;
        }
        
        int receivedBytes = recv(remoteSocketFileDescriptor, clientRequest, sizeof clientRequest, 0);
        
        if(receivedBytes == -1){
            perror("Something went wrong receiving");
        }
        
        printf("The received message is: \n%s\n", clientRequest);
        serverResponse(remoteSocketFileDescriptor, clientRequest, &response, currentDirectory);
        
        int sentBytes = sendto(remoteSocketFileDescriptor, (void *)response, strlen(response), 0,
                   (struct sockaddr*)&request_addr, size);
        
        if(sentBytes == -1){
            perror("Something went wrong sending");
        }
        
        memset(clientRequest, 0, 3000);
       
        printf("Server: waiting for connections...\n");
    }
    
    return 0;
}

int main(int argc, char *argv[]){
    char* directoryPath;
    int directoryPathSet = 1;
    
    char* portNumber;
    int portNumberSet = 1;
    
    for(int i = 0; i < argc; i++){
        if(strcmp(argv[i], "-v") == 0){
            verbose = 0;
        }else if(strcmp(argv[i], "-p") == 0){
            i++;
            portNumber = malloc(strlen(argv[i]));
            sprintf(portNumber, "%s", argv[i]);
            portNumberSet = 0;
        }else if(strcmp(argv[i], "-d") == 0){
            i++;
            directoryPath = malloc(strlen(argv[i]));
            sprintf(directoryPath, "%s", argv[i]);
            directoryPathSet = 0;
        }
    }
        
    if(portNumberSet == 1){
        portNumber = malloc(strlen("8080"));
        sprintf(portNumber, "%s", "8080");
    }
    
    if(directoryPathSet == 1){
        directoryPath = malloc(strlen("defaultPath"));
        sprintf(directoryPath, "%s", "defaultPath");
    }
        
    listenForConnections(verbose, portNumber, directoryPath);
    return 0;
}
