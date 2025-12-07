/*
 * protocol.h
 *
 * Header file condiviso tra Client e Server.
 * Contiene le definizioni del protocollo, le strutture dati e i codici di stato.
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdint.h>

/*
 * ============================================================================
 * PROTOCOL CONSTANTS
 * ============================================================================
 */

// Parametri condivisi
#define SERVER_PORT 56700
#define BUFFER_SIZE 512

// Tipi di richiesta meteo
#define RESP_TYPE_TEMP 't'
#define RESP_TYPE_HUM  'h'
#define RESP_TYPE_WIND 'w'
#define RESP_TYPE_PRES 'p'

// Codici di stato
#define STATUS_SUCCESS 0
#define STATUS_CITY_NA 1
#define STATUS_INVALID 2

/*
 * ============================================================================
 * PROTOCOL DATA STRUCTURES
 * ============================================================================
 */

// NOTA: In UDP non invieremo queste struct direttamente per evitare padding.
// Useremo un buffer manuale, ma manteniamo le definizioni per comodità logica.

typedef struct {
    char type;        // Tipo dati
    char city[64];    // Nome città
} weather_request_t;

typedef struct {
    unsigned int status;
    char type;
    float value;
} weather_response_t;

/*
 * ============================================================================
 * FUNCTION PROTOTYPES
 * ============================================================================
 */

// Prototipi funzioni server (per la logica di business)
float get_temperature(void);
float get_humidity(void);
float get_wind(void);
float get_pressure(void);

#endif /* PROTOCOL_H_ */
