#include <time.h>
#include <errno.h>
#include <netdb.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#define MAX_PEERS 2
#define MAX_SENSORS 2
#define MAX_MSG_SIZE 500 // Em bytes
#define SENSOR_ID_LEN 10
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

#define OK_CODE 0 // Código de sucesso, onde o payload pode ser as mensagens de sucesso logo abaixo.
#define OK_SUCCESSFUL_DISCONNECT "01"
#define OK_SUCCESSFUL_CREATE "02"
#define OK_SUCCESSFUL_UPDATE "03"

#define ERROR_CODE 255 // Código de erro, onde o payload pode ser as mensagens de erro logo abaixo.
#define ERROR_PEER_LIMIT_EXCEEDED "01"
#define ERROR_PEER_NOT_FOUND "02"
#define ERROR_SENSOR_LIMIT_EXCEEDED "09"
#define ERROR_SENSOR_NOT_FOUND "10"

typedef struct {
    int type;
    char payload[MAX_MSG_SIZE];
} message;

void error(const char *msg){
    perror(msg);
    exit(1);
}

void GeneratePeerID(char *id_buffer) {
    int random_part = rand() % 100000; 
    sprintf(id_buffer, "P%05d", random_part);
}

void GenerateSensorID(char *id_buffer) {
    int random_part = rand() % 100000; 
    sprintf(id_buffer, "S%05d", random_part);
}

void SendMessage(int type, char* payload, int socket_fd) {
    message msg;
    bzero(msg.payload, MAX_MSG_SIZE);
    msg.type = type;
    strncpy(msg.payload, payload, MAX_MSG_SIZE - 1);
    msg.payload[MAX_MSG_SIZE - 1] = '\0'; 
    int n = write(socket_fd, &msg, sizeof(msg));
    if (n < 0) error("ERROR writing to socket");
}

char* ReceiveMessage(int expected_type, int socket_fd) {
    message msg;
    char* return_payload = NULL; 

    ssize_t n = read(socket_fd, &msg, sizeof(msg));
    // Validando tipo da mensagem recebida com o esperado
    if (msg.type != expected_type) {
        fprintf(stderr, "ReceiveMessage: Incorrect message type. Expected %d, got %d.\n", expected_type, msg.type);
        return NULL;
    }
    return_payload = (char*)malloc(sizeof(msg.payload) + 1);
    if (return_payload == NULL) {
        perror("ReceiveMessage: ERROR allocating memory for payload");
        return NULL;
    }
    // Copiando o payload da mensagem recebida para o buffer de retorno
    strncpy(return_payload, msg.payload, sizeof(msg.payload) - 1);
    return_payload[sizeof(msg.payload) - 1] = '\0';
    return return_payload;
}
