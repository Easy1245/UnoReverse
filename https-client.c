#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"   // Gebruik eventueel je eigen IP
#define SERVER_PORT 22

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[4096];
    int valread;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket maken mislukt");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connectie mislukt");
        return 1;
    }

    char *message = "Hallo vanaf de client!";
    send(sock, message, strlen(message), 0);

    // Ontvang reverse payload
    while ((valread = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[valread] = '\0';
        printf("%s", buffer);
    }

    close(sock);
    return 0;
}
