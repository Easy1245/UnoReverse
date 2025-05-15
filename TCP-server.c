#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <netinet/in.h>
#include <curl/curl.h>

#define PORT 2222
#define LOG_FILE "unoreverse.log"
#define PAYLOAD "YOU GOT UNO REVERSED!\n"
#define PAYLOAD_REPEAT 1000

// ========== Helper voor curl-response capture ==========
struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

// ========== Logging ==========
void log_entry(const char *entry) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t now = time(NULL);
    char *ts = ctime(&now);
    ts[strlen(ts)-1] = '\0'; // strip newline

    fprintf(log, "[%s] %s\n", ts, entry);
    fclose(log);
}

// ========== GeoIP via cURL ==========
void get_geolocation(const char *ip) {
    CURL *curl;
    CURLcode res;
    char url[256];

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    snprintf(url, sizeof(url), "http://ip-api.com/json/%s", ip);

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "UnoReverse-C/1.0");

        res = curl_easy_perform(curl);
        if(res == CURLE_OK) {
            char logbuf[1024];
            snprintf(logbuf, sizeof(logbuf), "GeoIP response: %s", chunk.memory);
            log_entry(logbuf);
        } else {
            log_entry("Failed to fetch GeoIP data");
        }
        curl_easy_cleanup(curl);
    }
    free(chunk.memory);
    curl_global_cleanup();
}

// ========== Payload sturen ==========
void send_payload(int client_sock) {
    for (int i = 0; i < PAYLOAD_REPEAT; i++) {
        send(client_sock, PAYLOAD, strlen(PAYLOAD), 0);
    }
    log_entry("Reverse payload verzonden.");
}

// ========== Main server ==========
int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];
    char buffer[4096];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    log_entry("UnoReverse TCP-server gestart");

    while (1) {
        log_entry("Wacht op nieuwe verbinding...");

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        // IP achterhalen
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        char logbuf[256];
        snprintf(logbuf, sizeof(logbuf), "Verbinding van %s:%d", client_ip, ntohs(client_addr.sin_port));
        log_entry(logbuf);

        // Ontvangen data
        memset(buffer, 0, sizeof(buffer));
        int valread = recv(client_fd, buffer, sizeof(buffer)-1, 0);
        if (valread > 0) {
            snprintf(logbuf, sizeof(logbuf), "Ontvangen: %s", buffer);
            log_entry(logbuf);
        }

        // Geolocatie opvragen
        get_geolocation(client_ip);

        // Payload sturen
        send_payload(client_fd);

        close(client_fd);
        log_entry("Verbinding gesloten.\n");
    }

    return 0;
}