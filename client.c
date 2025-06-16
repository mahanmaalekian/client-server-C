#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    struct sockaddr_in address;
    int sock_fd;
    char buf[1024];

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 aka localhost
    address.sin_port = htons(7878);
    // ^ to avoid conflicts with others, change the port number
    // to something else. Reflect that change in server.c

    if (-1 == connect(sock_fd, (struct sockaddr *)&address, sizeof(address))) {
        perror("connect");
        return 1;
    }

    // make stdin nonblocking:
    fcntl(0, F_SETFL, O_NONBLOCK);



    // make the socket nonblocking:
    fcntl(sock_fd, F_SETFL, O_NONBLOCK);





    FILE *server = fdopen(sock_fd, "r+");

    while (1) {
        // - Complete the below polling loop:
        //      - try to read from stdin, and forward across the socket
        //      - try to read from the socket, and forward to stdout

        if (NULL != fgets(buf, sizeof(buf), stdin)){
		if (fprintf(server, "%s", buf) < 0) {
			perror("fprintf");
			fclose(server);
			return 1;
		}
		fflush(server);        
        }

        if(NULL != fgets(buf, sizeof(buf), server)){
            printf("%s", buf);
        }
        

        usleep(100 * 1000); // wait 100ms before checking again
    

    }

    return 0;

}
