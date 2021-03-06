#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>

#define SPORT  45677
#define PPORT  45676
#define MAXLINE 5000
#define TO        29 //seconds

static volatile sig_atomic_t keep_running=1;
static void intHandler(int _) {
    keep_running=0;
}

int main(int argc, char* argv[]) {
    int clntfd,servfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, clntaddr, peeraddr;
    struct ip_mreq mreq;

    struct sigaction act;
    act.sa_handler = intHandler;
    act.sa_flags = SA_RESTART;
    sigaction(SIGINT, &act, NULL); //sign off after ^C

    // Creating socket file descriptor
    if ( (servfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("server socket creation failed");
        exit(EXIT_FAILURE);
    }
    if ( (clntfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("client socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&clntaddr, 0, sizeof(clntaddr));
    memset(&peeraddr, 0, sizeof(peeraddr));

    // Filling server information
    servaddr.sin_family    = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(SPORT);
    // Filling client information
    clntaddr.sin_family    = AF_INET; // IPv4
    clntaddr.sin_addr.s_addr = INADDR_ANY;
    // Filling peer information
    peeraddr.sin_family    = AF_INET; // IPv4
    peeraddr.sin_addr.s_addr = inet_addr(argv[1]);
    peeraddr.sin_port = htons(PPORT);

    int one = 1;
    setsockopt(servfd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
    // Bind the socket with the server address
    if ( bind(servfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) {
        perror("server bind failed"); exit(EXIT_FAILURE);
    }
    if ( bind(clntfd, (const struct sockaddr *)&clntaddr, sizeof(clntaddr)) < 0 ) {
        perror("client bind failed"); exit(EXIT_FAILURE);
    }

    struct timeval tv;
    fd_set rset;
    ssize_t n=0;
    int i,nready,timeout;
    int locked=0;
    unsigned int len;
    time_t timeold=0;
    char on[1],off[1];
    on[0]=TO*3+3;off[0]=0;
    tv.tv_sec = 0;  tv.tv_usec = 250000; // 1/4 second
    FD_ZERO(&rset); // clear the descriptor set
    
//in case of using dest=mcast see https://stackoverflow.com/questions/5281409/get-destination-address-of-a-received-udp-packet
    while(keep_running){
        peeraddr.sin_addr.s_addr = inet_addr(argv[1]); //need to repeat cause we can receive a trigger from other devices 
        if ( (time(NULL)-timeold)>=TO ) timeout=1;
        if (  timeout  ) {
            timeold=time(NULL);
            sendto(clntfd, (const char*)on, sizeof(on), 0, (struct sockaddr*)&peeraddr, sizeof(peeraddr));
            sendto(clntfd, (const char*)on, sizeof(on), 0, (struct sockaddr*)&peeraddr, sizeof(peeraddr));
//             printf("presence sent\n");
        }
        timeout=0;
        
        FD_SET(servfd, &rset); FD_SET(clntfd, &rset); // set servfd and clntfd in readset
        nready = select((servfd>clntfd?servfd:clntfd)+1, &rset, NULL, NULL, &tv); // select the ready descriptor
        if (nready>0) {
            if (FD_ISSET(servfd, &rset)) {
                len = sizeof(peeraddr);
                n = recvfrom(servfd, (char *)buffer, MAXLINE, 0, ( struct sockaddr *) &peeraddr, &len);
                if (!strcmp(inet_ntoa(peeraddr.sin_addr),argv[1])) {
                    timeout=1; locked=1;
                    printf("Received trigger from %s:%d\n",inet_ntoa(peeraddr.sin_addr),ntohs(peeraddr.sin_port));
                } else if (!locked) {
                    printf("Trigger from %s:%d not for %s\n",inet_ntoa(peeraddr.sin_addr),ntohs(peeraddr.sin_port),argv[1]);
                }
            }
            if (FD_ISSET(clntfd, &rset)) {
                len = sizeof(peeraddr);
                n = recvfrom(clntfd, (char *)buffer, MAXLINE, 0, ( struct sockaddr *) &peeraddr, &len);
                for (i=0;i<n;i++) printf("%c", buffer[i]);
                fflush(stdout);
            }
        }
    }
    sendto(clntfd, (const char*)off, sizeof(off), 0, (struct sockaddr*)&peeraddr, sizeof(peeraddr));
    sendto(clntfd, (const char*)off, sizeof(off), 0, (struct sockaddr*)&peeraddr, sizeof(peeraddr));
    sendto(clntfd, (const char*)off, sizeof(off), 0, (struct sockaddr*)&peeraddr, sizeof(peeraddr));
    printf("signed off\n");
}
