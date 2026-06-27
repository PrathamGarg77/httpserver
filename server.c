#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define MYPORT "3490"
#define BACKLOG 10

//handlechild
void sigchld_handler(int s){
    (void)s;
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
    return;
}

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET)return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//main function
int main(){
    //initial setup of variables
    struct sigaction sa;
    int status,sockfd,new_fd;
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    //getting address info
    if ((status = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        exit(1);
    }

    //make a socket
    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1){
        perror("Socket Creation Failed");
        exit(1);
    }

    //making port reusable without timeout on exit
    int yes=1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    // bind it to the port we passed in to getaddrinfo():
    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("Binding Failed");
        exit(1);
    }
    
    // free the linked-list
    freeaddrinfo(servinfo);

    //listening
    if (listen(sockfd, BACKLOG) == -1) {
        perror("Listening Failed");
        exit(1);
    }

    //fork child process handling
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    //infinite loop for accepting and handling requests
    while(1){
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        //printing accepted connection ip
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("\nServer got connection from %s\n", s);

        //forking for handling each request
        pid_t pid = fork();
        //fail
        if (pid < 0) {
            perror("Fork Failed");
            printf("Ignored a request from %s\n", s);
            close(new_fd);
            continue;
        }
        //child
        else if (pid == 0) {
            close(sockfd);
            char request[1000],response[1000]={0},body[500];

            //receiving
            if (recv(new_fd, request, sizeof(request)-1, 0) == -1) {
                perror("recv");
                close(new_fd);
                exit(0);
            }

            //parsing and checking method,path and protocol
            char method[5],path[10],protocol[9];
            char method1[]="GET",method2[]="POST",protocol1[]="HTTP/1.1";
            char path1[]="/",path2[]="/file.txt";
            if(sscanf(request, "%4s %9s %8s", method, path, protocol) != 3){
                printf("Malformed Request.Use HTTP/1.1\nClosing Connection.\n\n");
                sprintf(response,"HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n");
                if (send(new_fd, response, strlen(response), 0) == -1) {
                    perror("send");
                }
                close(new_fd);
                exit(0);
            }
            //showing parsed data on terminal where server is running
            printf("Method: %s\nPath: %s\nProtocol: %s\n", method, path, protocol);

            //checking protocol
            if(strcmp(protocol, protocol1)!=0){//400 protocol
                printf("Malformed Request:Use protocol HTTP/1.1\nClosing Connection.\n\n");
                sprintf(response,"HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n");
                if (send(new_fd, response, strlen(response), 0) == -1) {
                    perror("send");
                }
                close(new_fd);
                exit(0);
            }
            
            //method and path
            //get
            if(strcmp(method, method1)==0){
                //home.txt
                if(strcmp(path, path1)==0){
                    FILE *f=fopen("home.txt", "r");
                    fread(body, sizeof(body)-1, 1, f);
                    sprintf(response,"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s\n\n",strlen(body),body);
                    if (send(new_fd, response, strlen(response), 0) == -1) {
                        perror("send");
                    }
                }
                //file.txt
                else if(strcmp(path, path2)==0){
                    FILE *f=fopen("file.txt", "r");
                    fread(body, sizeof(body)-1, 1, f);
                    sprintf(response,"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s\n\n",strlen(body),body);
                    if (send(new_fd, response, strlen(response), 0) == -1) {
                        perror("send");
                    }
                }
                //not found
                else{
                    printf("Malformed Request:File not found.\nClosing Connection.\n\n");
                    sprintf(response,"HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n");
                    if (send(new_fd, response, strlen(response), 0) == -1) {
                        perror("send");
                    }
                }
            }
            //post not supported
            else if(strcmp(method, method2)==0){
                printf("Incomplete server code:Method not supported.\nClosing Connection.\n\n");
                sprintf(response,"HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n");
                if (send(new_fd, response, strlen(response), 0) == -1) {
                    perror("send");
                }
            }
            //method not supported
            else{
                printf("Malformed Request:Method not supported.\nClosing Connection.\n\n");
                sprintf(response,"HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n");
                if (send(new_fd, response, strlen(response), 0) == -1) {
                    perror("send");
                }
            }

            //ending connection
            close(new_fd);
            exit(0);
        }

        //parent
        close(new_fd);
    }
}