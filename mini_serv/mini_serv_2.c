#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

int next_id = 0, maxfd = 0;
int ids[65536];
char* msgs[65536];
fd_set readset, writeset, allset;
char bufread[1001], bufwrite[42];

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

void	notify_others(int author, char *str) {
	for (int fd = 0; fd <= maxfd; fd++) {
		if (FD_ISSET(fd, &writeset) && fd != author) {
			send(fd, str, strlen(str), 0);
        }
	}
}

void register_client(int fd) {
    write(2, "1\n", 2);
	maxfd = fd > maxfd ? fd : maxfd;
	ids[fd] = next_id++;
	msgs[fd] = NULL;
    FD_SET(fd, &allset);
    sprintf(bufwrite, "server: client %d just arrived\n", ids[fd]);
    notify_others(fd, bufwrite);
}

void remove_client(int fd) {
    sprintf(bufwrite, "server: client %d just left\n", ids[fd]);
    notify_others(fd, bufwrite);
    free(msgs[fd]);
    FD_CLR(fd, &allset);
    close(fd);
}

void send_msg(int fd) {
    char* msg;

    while (extract_message(&(msgs[fd]), &msg)) {
        sprintf(bufwrite, "server: client %d just left\n", ids[fd]);
        notify_others(fd, bufwrite);
        notify_others(fd, msg);
        free(msg);
    }
}

int create_socket() {
	maxfd = socket(AF_INET, SOCK_STREAM, 0);
    if (maxfd < 0) {
        fatal();
    }
    FD_SET(maxfd, &allset);
    return(maxfd);
}

int main(int ac, char** av) {
    if (ac != 2) {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

    FD_ZERO(&allset);
    int sockfd = create_socket();


	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));

	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        fatal();
    } else if (listen(sockfd, SOMAXCONN)) {
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
                socklen_t socklen = sizeof(servaddr);
                int client_fd = accept(sockfd, (struct sockaddr *)&servaddr, &socklen);

                if (client_fd >= 0) {
                    register_client(client_fd);
                    break;
                }
            } else {
                int read_bytes = recv(fd, bufread, 1000, 0);

                if (read_bytes <= 0) {
                    remove_client(fd);
                    break;
                }
                bufread[read_bytes] = '\0';
                msgs[fd] = str_join(msgs[fd], bufread);
                send_msg(fd);
            }
        }
    }
    return 0;
}
