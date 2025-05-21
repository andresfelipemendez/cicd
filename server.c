#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <stdlib.h>

#define BUFFER_SIZE 4096

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = SO_REUSEADDR;

    address.sin_family = AF_INET;
    address.sin_port = htons(8081);
    address.sin_addr.s_addr = INADDR_ANY;
    
    if((server_fd = socket(AF_INET, SOCK_STREAM,0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt SO_REUSEADDR");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if(bind(server_fd,(struct sockaddr*)&address,sizeof(address))<0){
        perror("socket creation failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // check for errors
    if(listen(server_fd,3)<0){
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    };
    
    while(1) {
        int client_fd = accept(server_fd,0,0);
        if(client_fd < 0) {
            perror("Accept failed");
            continue;
        }
        
        char buffer[BUFFER_SIZE] = {0};
        ssize_t bytes_received = recv(client_fd,buffer,BUFFER_SIZE - 1,0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
        }
        else if (bytes_received == 0) {
            close(client_fd);
            continue;
        } else {
            perror("Receive dfailed or connection closed");
            close(client_fd);
            continue;
        }
        
        char* headers_end = strstr(buffer, "\r\n\r\n");
        if(!headers_end) {
            const char* bad_request = "HTTP/1.1 400 Bad Request\r\n\r\n";
            send(client_fd, bad_request, strlen(bad_request),0);
            close(client_fd);
            continue;
        }

        char method[10] = {0};
        char path[256] = {0};
        char body[4096] = {0};
        sscanf(buffer, "%s %s", method, path);

        size_t header_len = (headers_end - buffer) + 4;

        char* content_length_str_start = strstr(buffer, "Content-Length:");
        long content_length = 0;
        if(content_length_str_start) {
            content_length = atol(content_length_str_start + strlen("Content-Length:"));
        }

        size_t body_bytes_received = 0;

        // copy any part of the body already in the buffer
        if(bytes_received > header_len) {
            size_t initial_body_cunk_len = bytes_received - header_len;
            if(initial_body_cunk_len > content_length) {
                initial_body_cunk_len = content_length;
            }
            memcpy(body, headers_end + 4, initial_body_cunk_len);
            body_bytes_received += initial_body_cunk_len;
        }

        // remaining bobdy data if content length > 0
        while(body_bytes_received < content_length && body_bytes_received < sizeof(body) -1) {
            ssize_t current_recv = recv(client_fd, body + body_bytes_received, content_length - body_bytes_received, 0);
            if(current_recv <= 0) {
                if(current_recv < 0) perror("Body recv failed");
                break;
            }
            body_bytes_received += current_recv;
        }
        body[body_bytes_received] = '\0';
        
        printf("Method: %s, Path: %s\nBody: %s\n", method, path, body);
        
        if(strcmp(method, "POST") == 0) {

            pid_t pid = fork();
            if(pid == -1) {
                perror("fork failed");
            } else if(pid ==0) {
                if(setsid() < 0) {
                    perror("setsid failed in child");
                    _exit(1);
                }

                const char *script_path = "/home/andres/development/cicd/buildserver.sh"; 
                int dev_null_fd = open("/dev/null", O_RDWR);
                if (dev_null_fd != -1) {
                    dup2(dev_null_fd, STDIN_FILENO);
                    dup2(dev_null_fd, STDOUT_FILENO);
                    dup2(dev_null_fd, STDERR_FILENO);
                    if (dev_null_fd > STDERR_FILENO) { // Don't close if it's one of 0,1,2 by chance
                        close(dev_null_fd);
                    }
                } else {
                    // Fallback if /dev/null can't be opened: just close them
                    // This is important so the script doesn't hold onto pipes connected to the parent
                    close(STDIN_FILENO);
                    close(STDOUT_FILENO);
                    close(STDERR_FILENO);
                }
                execlp("sh", "sh", "-c", script_path, (char *)NULL);
                perror("execlp failed to run buildserver.sh");
                _exit(127);
            } 
            
            printf("Build script process launched with PID %d.\n", pid);
            const char *response = "HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            ssize_t sent_bytes = send(client_fd, response, strlen(response),0);
            if(sent_bytes < 0) {
                perror("send failed for POST 202 response");
            }
        }
        
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
    close(server_fd);
    return 0;
}
