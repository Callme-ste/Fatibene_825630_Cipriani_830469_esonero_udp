/*
 * main.c (Client)
 *
 * UDP Client - Implementazione completa esonero
 */

#if defined WIN32
#include <winsock.h>
// FIX: Definizione necessaria per Windows, dove socklen_t è semplicemente int
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
#include <ctype.h>
#include "protocol.h"

// FIX: Definisce NO_ERROR solo se non esiste già (evita il warning su Windows)
#ifndef NO_ERROR
#define NO_ERROR 0
#endif

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

void print_usage(const char* prog_name) {
    printf("Usage: %s [-s server] [-p port] -r \"type city\"\n", prog_name);
}

void capitalize_city(char* city) {
    if (city && strlen(city) > 0) city[0] = toupper((unsigned char)city[0]);
}

int main(int argc, char *argv[]) {
    char server_host[64] = "localhost";
    int server_port = SERVER_PORT;
    char request_str[128] = "";
    int r_flag = 0;

    // --- 1. Parsing argomenti ---
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) strncpy(server_host, argv[++i], 63);
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) server_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            strncpy(request_str, argv[++i], 127);
            r_flag = 1;
        }
    }

    if (!r_flag) {
        print_usage(argv[0]);
        return 0;
    }

    // --- 2. Parsing e Validazione Richiesta ---
    char req_type;
    char req_city[64] = "";

    // Trova il primo spazio
    char* space_ptr = strchr(request_str, ' ');
    if (space_ptr == NULL) {
        // Nessuno spazio trovato, formato non valido.
        // Nota: non stampiamo errori specifici qui per restare fedeli al comportamento silenzioso su argomenti errati standard,
        // ma potresti aggiungere una print se preferisci debuggare.
        return 0;
    }

    // Verifica token type (deve essere 1 solo carattere prima dello spazio)
    int type_len = space_ptr - request_str;
    if (type_len != 1) {
        // Errore: token tipo troppo lungo (es. "temp") o vuoto
        return 0;
    }

    req_type = request_str[0];

    // Salta gli spazi multipli per trovare la città
    char* city_start = space_ptr;
    while (*city_start == ' ' && *city_start != '\0') city_start++;

    if (strlen(city_start) > 63) {
        printf("Errore: nome città troppo lungo.\n");
        return 0;
    }
    strncpy(req_city, city_start, 63);

#if defined WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != NO_ERROR) {
        printf("Error at WSAStartup()\n");
        return 0;
    }
#endif

    int my_socket = socket(AF_INET, SOCK_DGRAM, 0); // UDP Socket
    if (my_socket < 0) {
        printf("Errore creazione socket.\n");
        clearwinsock();
        return -1;
    }

    // --- 3. Risoluzione indirizzo Server ---
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // Supporto sia IP (127.0.0.1) che Hostname (localhost)
    struct hostent *he = gethostbyname(server_host);
    if (he == NULL) {
        printf("Errore risoluzione host %s\n", server_host);
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);


    // --- 4. Serializzazione Manuale (Buffer) ---
    // Protocollo: [TYPE (1 byte)] [CITY (n bytes...)]
    char buffer_out[BUFFER_SIZE];
    memset(buffer_out, 0, BUFFER_SIZE);

    buffer_out[0] = req_type;
    strcpy(&buffer_out[1], req_city); // Copia stringa dopo il primo byte

    // Calcolo lunghezza esatta: 1 byte type + lunghezza stringa + 1 terminatore nullo
    int msg_len = 1 + strlen(req_city) + 1;

    // Invio UDP
    if (sendto(my_socket, buffer_out, msg_len, 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Errore sendto.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // --- 5. Ricezione Risposta ---
    char buffer_in[BUFFER_SIZE];
    struct sockaddr_in sender_addr;
    // Qui ora usiamo socklen_t che è stato definito correttamente anche per Windows
    socklen_t sender_len = sizeof(sender_addr);

    int n = recvfrom(my_socket, buffer_in, BUFFER_SIZE, 0,
                     (struct sockaddr*)&sender_addr, &sender_len);

    if (n < 0) {
        printf("Errore recvfrom.\n");
    } else {
        // --- 6. Deserializzazione Manuale ---
        // Protocollo: [STATUS (4b)] [TYPE (1b)] [VALUE (4b)]
        unsigned int net_status;
        char resp_type;
        uint32_t net_value_int;
        float final_value;

        int offset = 0;
        memcpy(&net_status, buffer_in + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(&resp_type, buffer_in + offset, sizeof(char));
        offset += sizeof(char);

        memcpy(&net_value_int, buffer_in + offset, sizeof(uint32_t));

        // Conversione Byte Order
        unsigned int status = ntohl(net_status);
        uint32_t host_value_int = ntohl(net_value_int);
        memcpy(&final_value, &host_value_int, sizeof(float));

        // --- 7. Output Formattato e DNS Reverse ---
        struct hostent *remote_host;
        // socklen_t non serve qui, gethostbyaddr vuole int o size_t a seconda del sistema, ma length 4 è standard
        remote_host = gethostbyaddr((char*)&sender_addr.sin_addr, 4, AF_INET);
        char *hostname_str = (remote_host) ? remote_host->h_name : inet_ntoa(sender_addr.sin_addr);

        printf("Ricevuto risultato dal server %s (ip %s). ",
               hostname_str, inet_ntoa(sender_addr.sin_addr));

        if (status != STATUS_SUCCESS) {
            if (status == STATUS_CITY_NA) printf("Città non disponibile\n");
            else if (status == STATUS_INVALID) printf("Richiesta non valida\n");
            else printf("Errore sconosciuto\n");
        } else {
            capitalize_city(req_city);
            switch (resp_type) {
                case 't': printf("%s: Temperatura = %.1f°C\n", req_city, final_value); break;
                case 'h': printf("%s: Umidità = %.1f%%\n", req_city, final_value); break;
                case 'w': printf("%s: Vento = %.1f km/h\n", req_city, final_value); break;
                case 'p': printf("%s: Pressione = %.1f hPa\n", req_city, final_value); break;
                default: printf("Tipo dati sconosciuto\n");
            }
        }
    }

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
