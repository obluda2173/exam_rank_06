#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

typedef struct s_client {
    int fd; // -1 if not used
    int id; // assigned client id once on connect
    char buf[4096]; // accumulation buffer
    int len; // number of bytes burrently stored in buf
} t_client;

t_client clients[FD_SETSIZE];

void fatal() {
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    exit(1);
}

int main(int ac, char **av) {
    int sockfd, cfd, maxfd;
    struct sockaddr_in serv;
    socklen_t slen = sizeof(serv);
    fd_set readset, writeset, curset;

    if (ac != 2) {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) {
       fatal();
   }

   memset(&serv, 0, slen);
   serv.sin_family = AF_INET;
   serv.sin_addr.s_addr = htonl(2130706433);
   serv.sin_port = htons(atoi(av[1]));


   if ((bind(sockfd, (const struct sockaddr *)&serv, slen)) < 0) {
       fatal();
   }
   if (listen(sockfd, 10) < 0) {
       fatal();
   }

   // init clients
   for (int i = 0; i < FD_SETSIZE; i++) {
       clients[i].fd = -1;
       clients[i].len = 0;
   }

   FD_ZERO(&curset);
   FD_SET(sockfd, &curset);
   maxfd = sockfd;
   int next_id = 0;

   // main loop
   while (1) {
       readset = writeset = curset;
       if (select(maxfd + 1, &readset, &writeset, NULL, NULL) < 0) {
           continue;
       }

       // new clients
       if (FD_ISSET(sockfd, &readset)) {
           cfd = accept(sockfd, (struct sockaddr *)&serv, &slen); // connection file descriptor

           if (cfd >= 0) {
               if (cfd < FD_SETSIZE) {
                   clients[cfd].fd = cfd;
                   clients[cfd].id = next_id++;
                   clients[cfd].len = 0;
                   FD_SET(cfd, &curset);

                   if (cfd > maxfd) {
                       maxfd = cfd;
                   }

                   char msg[64];
                   int m = sprintf(msg, "server: client %d just arrived\n", clients[cfd].id);
                   for (int i = 0; i <= maxfd; i++) {
                       if (i != cfd && i != sockfd && clients[i].fd != -1 && FD_ISSET(i, &writeset)) {
                           send(i, msg, m, 0);
                       }
                   }

               } else {
                   close(cfd);
               }
           }
       }

       // existing clients

       for (int fd = 0; fd <= maxfd; fd++) {
           if (fd == sockfd) {
               continue;
           } else if (clients[fd].fd == -1) {
               continue;
           } else if (!FD_ISSET(fd, &readset)) {
               continue;
           }

           char tmp[1024];
           int r = recv(fd, tmp, sizeof(tmp), 0);
           if (r <= 0) {
               // disconnect
               int id = clients[fd].id;
               close(fd);
               FD_CLR(fd, &curset);
               clients[fd].fd = -1;
               clients[fd].len = 0;

               char msg[64];
               int m = sprintf(msg, "server: client %d just left\n", id);

               for (int i = 0; i <= maxfd; i++) {
                   if (i != fd && i != sockfd && clients[i].fd != -1 && FD_ISSET(i, &writeset)) {
                       send(i, msg, m, 0);
                   }
               }
               continue;
           }

           for (int i = 0; i < r; i++) {
               if (clients[fd].len < (int)sizeof(clients[fd].buf) - 1) {
                   clients[fd].buf[clients[fd].len++] = tmp[i];
               }

               if (tmp[i] == '\n') {
                   clients[fd].buf[clients[fd].len - 1] = '\0';
                   char msg[4096];
                   int m = sprintf(msg, "client %d: %s\n", clients[fd].id, clients[fd].buf);

                   for (int j = 0; j <= maxfd; j++) {
                       if (j != fd && j != sockfd && clients[j].fd != -1 && FD_ISSET(j, &writeset)) {
                           if (send(j, msg, m, 0) <= 0) {
                               close(j);
                               FD_CLR(j, &curset);
                               clients[j].fd = -1;
                               clients[j].len = 0;
                           }
                       }
                   }
                   clients[fd].len = 0;
               }
           }
       }
   }
   return 0;
}
