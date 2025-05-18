#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"  // Verander naar jouw server-IP indien nodig
#define SERVER_PORT 22
#define BUFFER_SIZE 4096

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = 0, total_bytes = 0;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket fout");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Ongeldig adres of niet ondersteund");
        close(sock);
        return 1;
    }

    printf("Verbinding maken met de UnoReverse server op %s:%d...\n", SERVER_IP, SERVER_PORT);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Verbinding mislukt");
        close(sock);
        return 1;
    }

    // Nep SSH-banner sturen
    const char *ssh_banner = "SSH-2.0-OpenSSH_8.2\n";
    send(sock, ssh_banner, strlen(ssh_banner), 0);
    printf("Nep SSH-banner verzonden: %s", ssh_banner);

    // Antwoord ontvangen van de server
    printf("Wachten op reactie van de server...\n\n");
    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer); // Kan vervangen worden met fwrite voor binary data
        total_bytes += bytes_received;
    }

    if (bytes_received < 0) {
        perror("Fout bij ontvangen van data");
    } else {
        printf("\n\nTotale bytes ontvangen: %zd\n", total_bytes);
    }

    close(sock);
    printf("Verbinding gesloten.\n");
    return 0;
}

