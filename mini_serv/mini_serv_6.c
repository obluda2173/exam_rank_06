#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

int maxfd = 0, nextid = 0;
char* msgs[65536];
int ids[65536];
fd_set readset, writeset, allset;
char readbuf[1001], writebuf[100];

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

void notify_others(int author, char* msg) {
    for (int fd = 0; fd <= maxfd; fd++) {
        if (FD_ISSET(fd, &writeset) && fd != author) {
            send(fd, msg, strlen(msg), 0);
        }
    }
}

void register_client(int fd) {
    maxfd = maxfd > fd ? maxfd : fd;
    FD_SET(fd, &allset);
    ids[fd] = nextid++;
    msgs[fd] = NULL;
    sprintf(writebuf, "server: client %d just arrived\n", ids[fd]);
    notify_others(fd, writebuf);
}

void remove_client(int fd) { // maybe rearange
    free(msgs[fd]);
    FD_CLR(fd, &allset);
    close(fd);
    sprintf(writebuf, "server: client %d just left\n", ids[fd]);
    notify_others(fd, writebuf);
}

void send_message(int fd) {
    char* msg;

    while (extract_message(&(msgs[fd]), &msg)) {
        sprintf(writebuf, "client %d: %s", ids[fd], msg);
        notify_others(fd, writebuf);
        free(msg);
    }
}

int create_sock() {
    maxfd = socket(AF_INET, SOCK_STREAM, 0);

    if (maxfd < 0) { // not sure
        fatal();
    }
    FD_SET(maxfd, &allset);
    return maxfd;
}

int main(int ac, char** av) {
	if (ac != 2) {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

    FD_ZERO(&allset);
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

    while (1) {
        readset = writeset = allset;
        if (select(maxfd + 1, &readset, &writeset, NULL, NULL) < 0) {
            continue;
        }

        for (int fd = 0; fd <= maxfd; fd++) {
            if (!FD_ISSET(fd, &readset)) {
                continue;
            }

            if (fd == sockfd) {
                socklen_t slen = sizeof(servaddr);
                int newconn = accept(fd, (struct sockaddr *)&servaddr, &slen);

                if (newconn >= 0) {
                    register_client(newconn);
                    break;
                }                
            } else {
                int bytesrecv = recv(fd, &readbuf, sizeof(readbuf) - 1, 0);

                if (bytesrecv <= 0) {
                    remove_client(fd);
                    break;
                }

                readbuf[bytesrecv] = '\0';
                msgs[fd] = str_join(msgs[fd], readbuf);
                send_message(fd);
            }
        }
    }
    return (0);
}
