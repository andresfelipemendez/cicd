#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <sys/stat.h> 

int main() {
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    int s = socket(AF_INET, SOCK_STREAM,0);
    if(bind(s,(struct sockaddr*)&addr,sizeof(addr))<0){
        perror("socket creation failed");
        return 1;
    }

    // check for errors
    listen(s,10);

    while(1) {
        int client_fd = accept(s,0,0);
        if(client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        char buffer[256] = {0};
        recv(client_fd,buffer,256,0);

        char* f = buffer + 5;
        char* end = strchr(f, ' ');
        if(end) *end = 0;

        printf("Request for file: %s\n", f);

        int open_fd = open(f, O_RDONLY);
        if(open_fd < 0) {
            const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 14\r\n\r\nFile not found";
            send(client_fd, not_found, strlen(not_found),0);
        } else {
            struct stat file_stat;
            fstat(open_fd, &file_stat);
            off_t file_size = file_stat.st_size;

            char header[128];
            sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n", file_size);
            send(client_fd, header, strlen(header),0);

            sendfile(client_fd,open_fd,0,file_size);
            close(open_fd);
        }
        close(client_fd);
    }    
    close(s);
    return 0;
}