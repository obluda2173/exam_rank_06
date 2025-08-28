#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

int maxfd = 0, nextid = 0;
int ids[65536];
char* msgs[65536];
fd_set rset, wset, aset;
char rbuf[1024], wbuf[64];

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void fatal() {
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    exit(1);
}

void broadcast(int self, char* msg) {
    for (int fd = 0; fd <= maxfd; fd++) {
        if (FD_ISSET(fd, &wset) && fd != self) {
            send(fd, msg, strlen(msg), 0);
        }
    }
}

void regitster_client(int fd) {
    maxfd = maxfd > fd ? maxfd : fd;
    FD_SET(fd, &aset);
    ids[fd] = nextid++;
    msgs[fd] = NULL;
    sprintf(wbuf, "server: client %d just arrived\n", ids[fd]);
    broadcast(fd, wbuf);
}

void remove_client(int fd) {
    free(msgs[fd]);
    close(fd);
    FD_CLR(fd, &aset);
    sprintf(wbuf, "server: client %d just left\n", ids[fd]);
    broadcast(fd, wbuf);
}

void send_msg(int fd) {
    char* msg;

    while (extract_message(&(msgs[fd]), &msg)) {
        sprintf(wbuf, "clinet %d: %s", ids[fd], msg);
        broadcast(fd, wbuf);
        free(msg);
    }
}

int create_sock() {
    maxfd = socket(AF_INET, SOCK_STREAM, 0);

    if (maxfd == -1) {
        fatal();
    }
    FD_SET(maxfd, &aset);
    return maxfd;
}

int main(int ac, char** av) {
    if (ac != 2) {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

    FD_ZERO(&aset);
    int sockfd = create_sock();
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));

    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        fatal();
	}
	if (listen(sockfd, SOMAXCONN) != 0) {
		fatal();
	}

    while(1) {
        rset = wset = aset;

        if (select(maxfd + 1, &rset, &wset, NULL, NULL) <= 0) {
            continue;
        }

        for (int fd = 0; fd <= maxfd; fd++) {
            if (!FD_ISSET(fd, &rset)) {
                continue;
            }

            if (fd == sockfd) {
                socklen_t slen = sizeof(servaddr);
                int newconn = accept(fd, (struct sockaddr *)&servaddr, &slen);

                if (newconn >= 0) {
                    regitster_client(newconn);
                    break;
                }
            } else {
                int bytesrecv = recv(fd, &rbuf, sizeof(rbuf) - 1, 0);

                if (bytesrecv <= 0) {
                    remove_client(fd);
                    break;
                }

                rbuf[bytesrecv] = '\0';
                msgs[fd] = str_join(msgs[fd], rbuf);
                send_msg(fd);
            }
        }
    }
    return 0;
}
