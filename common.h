#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#define MAX_MSG_SIZE 500 // Em bytes
#define SENSOR_ID_LEN 10
#define MAX_CLIENTS 15
#define MAX_PEERS 2
#define P2P_PORT_DEFAULT 64000
#define SL_CLIENT_LISTEN_PORT_DEFAULT 60000
#define SS_CLIENT_LISTEN_PORT_DEFAULT 61000

#define REQ_CONNPEER 20 // Requisição de conexão com o peer, sem payload
#define RES_CONNPEER 21 // Resposta de conexão com o peer, com o ID do peer conectado
#define REQ_DISCPEER 22 // Requisição de desconexão com o peer, com o ID do peer desconectado
#define REQ_CONNSEN 23 // Requisição de conexão com o sensor, com o ID da localização
#define RES_CONNSEN 24 // Resposta de conexão com o sensor, com o ID do sensor
#define REQ_DISCSEN 25 // Requisição de desconexão com o sensor, com o ID do sensor

#define REQ_STATALERT 36 // Requisição de alerta de status, com o ID do sensor
#define RES_STATALERT 37 // Requisição de alerta de status, com a localização do sensor
#define REQ_SENSLOC 38 // Requisição de localização do sensor, com o ID do sensor
#define RES_SENSLOC 39 // Resposta de localização do sensor, com o ID da localização
#define REQ_LOCLIST 40 // Requisição de lista de locais, com o status (palpite) e o ID da localização
#define RES_LOCLIST 41 // Resposta de lista de locais, com todos os IDs de sensores que tiverem o status na req
#define REQ_AREALOG 42 // Requisição de sensores ativos em uma área, com o ID da área


#define OK_CODE 0 // Código de susceso, onde o payload pode ser as mensagens de sucesso logo abaixo.
#define OK_SUCCESSFUL_DISCONNECT "01"
#define OK_SUCCESSFUL_CREATE "02"
#define OK_SUCCESSFUL_UPDATE "03"

#define ERROR_CODE 255 // Código de erro, onde o payload pode ser as mensagens de erro logo abaixo.
#define ERROR_PEER_LIMIT_EXCEEDED "01"
#define ERROR_PEER_NOT_FOUND "02"
#define ERROR_SENSOR_LIMIT_EXCEEDED "09"
#define ERROR_SENSOR_NOT_FOUND "10"


typedef enum {
    ROLE_SL,
    ROLE_SS,
    ROLE_UNKNOWN
} server_role_t;

typedef struct {
    char id; 
    int loc_id;
    int socket_fd;
    int risk_status; 
    time_t last_seen; 
    server_role_t connected_to_role; 
} client_info_t;

typedef struct {
    int socket_fd;
    char peer_id_assigned_by_me;
    char peer_id_assigned_to_me;
    int is_connected;
} peer_conn_t;

typedef struct {
    int type;
    char payload[MAX_MSG_SIZE];
} message;

// // Função que retorna o nome da área com base no ID de localização
// static inline const char* get_area_name(int loc_id) {
//     if (loc_id >= 1 && loc_id <= 3) return "Norte";
//     if (loc_id >= 4 && loc_id <= 5) return "Sul";
//     if (loc_id >= 6 && loc_id <= 7) return "Leste";
//     if (loc_id >= 8 && loc_id <= 10) return "Oeste";
//     if (loc_id == -1) return "Sem Area Definida";
//     return "Desconhecida";
// }

// // Função que retorna o ID do grupo de área com base no ID de localização
// static inline int get_area_group_id(int loc_id) {
//     if (loc_id >= 1 && loc_id <= 3) return 1; // Norte
//     if (loc_id >= 4 && loc_id <= 5) return 2; // Sul
//     if (loc_id >= 6 && loc_id <= 7) return 3; // Leste
//     if (loc_id >= 8 && loc_id <= 10) return 4; // Oeste
//     return 0; // Sem Area Definida or Desconhecida
// }

// // Função que envia uma mensagem
// static inline int send_message(int sockfd, const char *message) {
//     char buffer; // +1 for newline, +1 for null terminator
//     strncpy(buffer, message, MAX_MSG_SIZE);
//     buffer = '\0'; // Ensure null termination if message is too long
//     strcat(buffer, "\n");
//     if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
//         perror("send");
//         return -1;
//     }
//     return 0;
// }

// // Função que recebe uma mensagem
// static inline int receive_message(int sockfd, char *buffer, int buffer_size) {
//     memset(buffer, 0, buffer_size);
//     int total_bytes_received = 0;
//     char ch;
//     while (total_bytes_received < buffer_size - 1) {
//         int bytes_received = recv(sockfd, &ch, 1, 0);
//         if (bytes_received <= 0) {
//             if (bytes_received == 0) {
//                 return 0;
//             }
//             perror("recv");
//             return -1; 
//         }
//         if (ch == '\n') {
//             break; 
//         }
//         buffer[total_bytes_received++] = ch;
//     }
//     buffer[total_bytes_received] = '\0'; 
//     if (total_bytes_received == buffer_size -1 && ch!= '\n') {
//         fprintf(stderr, "Warning: Received message might be truncated or missing newline.\n");
//     }
//     return total_bytes_received;
// }

// // Generate a unique 10-digit sensor ID (simple version for this context)
// static inline void generate_sensor_id(char* id_buffer) {
//     // For simplicity, using timestamp and a random number part
//     // This is not guaranteed to be unique in a distributed system without coordination
//     // but sufficient for this project's scope.
//     long ts_part = time(NULL) % 100000; // Last 5 digits of timestamp
//     int rand_part = rand() % 100000;    // Random 5 digits
//     sprintf(id_buffer, "%05ld%05d", ts_part, rand_part);
//     id_buffer = '\0';
// }


#endif // COMMON_H