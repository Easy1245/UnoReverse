#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define PORT 22
#define LOG_FILE "unoreverse.log"
#define PAYLOAD "YOU GOT UNO REVERSED!!\n"
#define PAYLOAD_REPEAT 1000

typedef struct IPNode {
    char ip[INET_ADDRSTRLEN];
    struct IPNode *next;
} IPNode;

IPNode *ip_list_head = NULL;
pthread_mutex_t ip_list_mutex = PTHREAD_MUTEX_INITIALIZER;

int ip_exists(const char *ip) {
    pthread_mutex_lock(&ip_list_mutex);
    IPNode *curr = ip_list_head;
    while (curr) {
        if (strcmp(curr->ip, ip) == 0) {
            pthread_mutex_unlock(&ip_list_mutex);
            return 1;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&ip_list_mutex);
    return 0;
}

void add_ip(const char *ip) {
    pthread_mutex_lock(&ip_list_mutex);
    IPNode *new_node = malloc(sizeof(IPNode));
    if (!new_node) {
        pthread_mutex_unlock(&ip_list_mutex);
        return;
    }
    strncpy(new_node->ip, ip, INET_ADDRSTRLEN);
    new_node->next = ip_list_head;
    ip_list_head = new_node;
    pthread_mutex_unlock(&ip_list_mutex);
}

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

void log_entry(const char *entry) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t now = time(NULL);
    char *ts = ctime(&now);
    ts[strlen(ts) - 1] = '\0';

    fprintf(log, "[%s] %s\n", ts, entry);
    fclose(log);
}

void parse_and_log_geojson(const char *json_str) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        log_entry("JSON parsing failed");
        return;
    }

    const cJSON *country = cJSON_GetObjectItemCaseSensitive(root, "country");
    const cJSON *city = cJSON_GetObjectItemCaseSensitive(root, "city");
    const cJSON *query = cJSON_GetObjectItemCaseSensitive(root, "query");

    char logbuf[512];
    if (cJSON_IsString(country) && cJSON_IsString(city) && cJSON_IsString(query)) {
        snprintf(logbuf, sizeof(logbuf), "Geolocatie voor IP %s: %s, %s", query->valuestring, city->valuestring, country->valuestring);
        log_entry(logbuf);
    } else {
        log_entry("Geolocatie niet volledig beschikbaar in JSON");
    }

    cJSON_Delete(root);
}

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
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "UnoReverse-C/1.0");

        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            parse_and_log_geojson(chunk.memory);
        } else {
            log_entry("Failed to fetch GeoIP data");
        }
        curl_easy_cleanup(curl);
    }
    free(chunk.memory);
    curl_global_cleanup();
}

void send_payload(int client_sock, size_t *bytes_sent) {
    for (int i = 0; i < PAYLOAD_REPEAT; i++) {
        ssize_t sent = send(client_sock, PAYLOAD, strlen(PAYLOAD), 0);
        if (sent > 0) *bytes_sent += sent;
        else break;
    }
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    if (getpeername(client_fd, (struct sockaddr *)&client_addr, &addrlen) == -1) 
    {
        log_entry("Kan client IP niet ophalen");
        close(client_fd);
        return NULL;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    char logbuf[256];

    if (ip_exists(client_ip)) 
    {
        snprintf(logbuf, sizeof(logbuf), "Herhaalde verbinding van IP %s", client_ip);
    } else 
    {
        add_ip(client_ip);
        snprintf(logbuf, sizeof(logbuf), "Nieuwe verbinding van IP %s", client_ip);
    }
    log_entry(logbuf);

    memset(logbuf, 0, sizeof(logbuf));
    snprintf(logbuf, sizeof(logbuf), "Verbinding van %s:%d", client_ip, ntohs(client_addr.sin_port));
    log_entry(logbuf);

    char buffer[4096] = {0};
    int valread = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (valread > 0) 
    {
        snprintf(logbuf, sizeof(logbuf), "Ontvangen: %s", buffer);
        log_entry(logbuf);
    } else if (valread == 0) 
    {
        log_entry("Client heeft verbinding gesloten zonder data te sturen.");
    } else {
        log_entry("Fout bij ontvangen van data van client.");
    }

    get_geolocation(client_ip);

    size_t total_bytes_sent = 0;
    send_payload(client_fd, &total_bytes_sent);

    snprintf(logbuf, sizeof(logbuf), "Aantal bytes verzonden: %zu", total_bytes_sent);
    log_entry(logbuf);

    close(client_fd);
    log_entry("Verbinding gesloten.\n");

    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) 
    {
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

    if (listen(server_fd, 10) < 0) 
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    log_entry("UnoReverse TCP-server gestart");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int *client_fd = malloc(sizeof(int));
        if (!client_fd) {
            log_entry("Memory alloc failed");
            continue;
        }

        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd) != 0) {
            log_entry("Fout bij aanmaken thread");
            close(*client_fd);
            free(client_fd);
        } else {
            pthread_detach(tid);
        }
    }

    return 0;
}
