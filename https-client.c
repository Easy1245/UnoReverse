#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"  // Of IP van de server
#define SERVER_PORT 2222

int main() 

    int sock;
    struct sockaddr_in server_addr;
    char buffer[4096];

    // Socket maken
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket error");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Verbind met server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect error");
        return 1;
    }

    // Stuur een bericht
    char *message = "Hallo vanaf de client!";
    send(sock, message, strlen(message), 0);

    // Ontvang reverse payload
    int valread;
    while ((valread = recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[valread] = '\0';
        printf("%s", buffer);
    }

    close(sock);
    return 0;
}