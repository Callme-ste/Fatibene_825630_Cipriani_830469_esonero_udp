/*
 * main.c (Server)
 *
 * UDP Server - Implementazione completa esonero
 */

#if defined WIN32
#include <winsock.h>
typedef int socklen_t;
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "protocol.h"

#define NO_ERROR 0

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

// Funzioni helper stringhe
int str_case_cmp(const char *s1, const char *s2) {
#if defined WIN32
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

// Logica Business (Range Specifiche)
float get_temperature(void) { return ((float)rand() / RAND_MAX) * 50.0f - 10.0f; }
float get_humidity(void)    { return ((float)rand() / RAND_MAX) * 80.0f + 20.0f; }
float get_wind(void)        { return ((float)rand() / RAND_MAX) * 100.0f; }
float get_pressure(void)    { return ((float)rand() / RAND_MAX) * 100.0f + 950.0f; }

const char* SUPPORTED_CITIES[] = {
    "Bari", "Roma", "Milano", "Napoli", "Torino",
    "Palermo", "Genova", "Bologna", "Firenze", "Venezia", NULL
};

int is_city_supported(const char* city) {
    for (int i = 0; SUPPORTED_CITIES[i] != NULL; i++) {
        if (str_case_cmp(city, SUPPORTED_CITIES[i]) == 0) return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;

    // Parsing argomenti
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
                port = atoi(argv[++i]);
            }
        }
    }

#if defined WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != NO_ERROR) {
        printf("Error at WSAStartup()\n");
        return 0;
    }
#endif

    srand((unsigned int)time(NULL));

    // Creazione Socket UDP
    int my_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (my_socket < 0) {
        printf("Errore socket.\n");
        clearwinsock();
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind
    if (bind(my_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Errore bind. Porta %d occupata?\n", port);
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    printf("Server UDP in ascolto sulla porta %d...\n", port);

    // Ciclo di ricezione infinito
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        char buffer_in[BUFFER_SIZE];

        // Ricezione Datagramma
        int bytes = recvfrom(my_socket, buffer_in, BUFFER_SIZE, 0,
                             (struct sockaddr*)&client_addr, &client_len);

        if (bytes > 0) {
            // Deserializzazione Richiesta
            char req_type = buffer_in[0];
            char req_city[64];
            strncpy(req_city, &buffer_in[1], 63);
            req_city[63] = '\0'; // Sicurezza

            // Logging con DNS Lookup
            struct hostent *client_host;
            client_host = gethostbyaddr((char*)&client_addr.sin_addr, 4, AF_INET);
            char *client_name = (client_host) ? client_host->h_name : inet_ntoa(client_addr.sin_addr);

            printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
                   client_name, inet_ntoa(client_addr.sin_addr), req_type, req_city);

            // Preparazione Risposta
            unsigned int status = STATUS_SUCCESS;
            float value = 0.0f;

            int valid_type = (req_type == 't' || req_type == 'h' || req_type == 'w' || req_type == 'p');

            if (!valid_type) {
                status = STATUS_INVALID;
            } else if (!is_city_supported(req_city)) {
                status = STATUS_CITY_NA;
            } else {
                switch(req_type) {
                    case 't': value = get_temperature(); break;
                    case 'h': value = get_humidity(); break;
                    case 'w': value = get_wind(); break;
                    case 'p': value = get_pressure(); break;
                }
            }

            // Serializzazione Manuale Risposta
            // Buffer: [STATUS(4)] [TYPE(1)] [VALUE(4)]
            char buffer_out[BUFFER_SIZE];
            int offset = 0;

            uint32_t net_status = htonl(status);
            memcpy(buffer_out + offset, &net_status, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            memcpy(buffer_out + offset, &req_type, sizeof(char));
            offset += sizeof(char);

            // Conversione float -> int -> network order
            uint32_t temp_int;
            memcpy(&temp_int, &value, sizeof(float));
            temp_int = htonl(temp_int);
            memcpy(buffer_out + offset, &temp_int, sizeof(uint32_t));
            offset += sizeof(uint32_t); // Totale 9 bytes

            // Invio Risposta
            sendto(my_socket, buffer_out, offset, 0,
                   (struct sockaddr*)&client_addr, client_len);
        }
    }

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
