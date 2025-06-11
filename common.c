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

#define ID_LEN 10
#define MAX_PEERS 2
#define MAX_SENSORS 2
#define MAX_MSG_SIZE 500 
#define SL_CLIENT_LISTEN_PORT_DEFAULT 60000
#define SS_CLIENT_LISTEN_PORT_DEFAULT 61000

#define REQ_CONNPEER 20 // Requisição de conexão com o peer, sem payload
#define RES_CONNPEER 21 // Resposta de conexão com o peer, com o ID do peer conectado
#define REQ_DISCPEER 22 // Requisição de desconexão com o peer, com o ID do peer desconectado
#define REQ_CONNSEN 23 // Requisição de conexão com o sensor, com o ID da localização
#define RES_CONNSEN 24 // Resposta de conexão com o sensor, com o ID do sensor
#define REQ_DISCSEN 25 // Requisição de desconexão com o sensor, com o ID do sensor

#define REQ_CHECKALERT 36 // Requisição de alerta de status, com o ID do sensor
#define RES_CHECKALERT 37 // Requisição de alerta de status, com a localização do sensor
#define REQ_SENSLOC 38 // Requisição de localização do sensor, com o ID do sensor
#define RES_SENSLOC 39 // Resposta de localização do sensor, com o ID da localização
#define REQ_SENSSTATUS 40 // Requisição de informação sobre o status do sensor. 
#define RES_SENSSTATUS 41 // Resposta com o ID da região onde ocorre a falha, se houver.
#define REQ_LOCLIST 42 // Requisição de lista de locais, com o status (palpite) e o ID da localização
#define RES_LOCLIST 43 // Resposta de lista de locais, com todos os IDs de sensores que tiverem o status na req

#define OK_CODE 0 // Código de sucesso, onde o payload pode ser as mensagens de sucesso logo abaixo.
#define OK_SUCCESSFUL_DISCONNECT "01"
#define OK_SUCCESSFUL_CREATE "02"
#define OK_STATUS_0 "03"

#define ERROR_CODE 255 // Código de erro, onde o payload pode ser as mensagens de erro logo abaixo.
#define ERROR_PEER_LIMIT_EXCEEDED "01"
#define ERROR_PEER_NOT_FOUND "02"
#define ERROR_SENSOR_LIMIT_EXCEEDED "09"
#define ERROR_SENSOR_NOT_FOUND "10"
#define ERROR_LOCATION_NOT_FOUND "11"

typedef struct {
    int type;
    char payload[MAX_MSG_SIZE]; 
} message;

typedef struct {
    int status; 
    int location; 
    int socket_fd; 
    char id[ID_LEN + 1]; 
} sensor_info;

/**
 * @brief Exibe uma mensagem de erro e encerra o programa.
 * 
 * @param msg A mensagem de erro a ser exibida.
 */
void error(const char *msg){
    perror(msg);
    exit(1);
}

/**
 * @brief Gera um ID único. 
 * 
 * @param id_buffer O buffer onde o ID gerado será armazenado.
 */
void GenerateRandomID(char *id_buffer) {
    for (int i = 0; i < 10; i++) {
        id_buffer[i] = '0' + rand() % 10;
    }
    id_buffer[10] = '\0';
}

/**
 * @brief Envia uma mensagem para um socket.
 * 
 * @param type O tipo da mensagem a ser enviada.
 * @param payload O payload da mensagem a ser enviada.
 * @param socket_fd O descritor do socket para onde a mensagem será enviada.
 */
void SendMessage(int type, char* payload, int socket_fd) {
    message msg;
    bzero(msg.payload, MAX_MSG_SIZE);
    msg.type = type;
    strncpy(msg.payload, payload, MAX_MSG_SIZE - 1);
    msg.payload[MAX_MSG_SIZE - 1] = '\0'; 
    int n = write(socket_fd, &msg, sizeof(msg));
    if (n < 0) error("ERROR writing to socket");
}

/**
 * @brief Recebe uma mensagem de um socket.
 * 
 * @param socket_fd O descritor do socket de onde a mensagem será recebida.
 *
 * @return A mensagem recebida.
 */
message ReceiveRawMessage(int socket_fd) {
    message msg;
    ssize_t n = read(socket_fd, &msg, sizeof(msg));
    if (n < 0) error("ERROR reading from socket");
    return msg;
}

/**
 * @brief Verifica a localização de um sensor e retorna uma região.
 * 
 * @param location A localização do sensor.
 *
 * @return A região correspondente à localização (1: Norte, 2: Sul, 3: Leste, 4: Oeste).
 */
int CheckLocation(int location){
    // Norte
    if (location >= 1 && location < 3) {
        return 1;
    }
    // Sul
    else if (location == 4 || location == 5) {
        return 2;
    }
    // Leste
    else if (location == 6 || location == 7) {
        return 3;
    }
    // Oeste
    else if (location >= 8 && location <= 10) {
        return 4;
    }
}