#include <time.h>
#include <netdb.h> 
#include <stdio.h>
#include "common.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>


/*
./server 127.0.0.1 64000 60000
./server 127.0.0.1 64000 61000
./sensor 127.0.0.1 60000
./sensor 127.0.0.1 61000
*/
void error(const char *msg){
    perror(msg);
    exit(1);
}

char peer_id[10];
char my_id[10];
char* sensors_id[15];

void GeneratePeerID(char *id_buffer) {
    int random_part = rand() % 100000; 
    sprintf(id_buffer, "P%05d", random_part);
}

int P2PConnect(const char* ip, int port) {
    int peer_socket = -1;
    struct sockaddr_in peer_addr;

    int connector_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (connector_socket < 0) {
        error("P2P: ERROR opening connector socket");
    }

    bzero((char *)&peer_addr, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &peer_addr.sin_addr) <= 0) {
        close(connector_socket);
        error("P2P: Invalid peer target IP address");
    }
    peer_addr.sin_port = htons(port);

    if (connect(connector_socket, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) == 0) {
        GeneratePeerID(peer_id);
        printf("Peer %s connected\n", peer_id);
        peer_socket = connector_socket;
        return peer_socket;
    } 
    else {
        close(connector_socket); 

        printf("No peers found, starting to listen...\n");
        int listener_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (listener_socket < 0) {
            error("P2P: ERROR opening listener socket");
            return -1;
        }

        int optval = 1;
        setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        struct sockaddr_in my_listen_addr;
        bzero((char *)&my_listen_addr, sizeof(my_listen_addr));
        my_listen_addr.sin_family = AF_INET;
        my_listen_addr.sin_addr.s_addr = INADDR_ANY; 
        my_listen_addr.sin_port = htons(port);

        if (bind(listener_socket, (struct sockaddr *)&my_listen_addr, sizeof(my_listen_addr)) < 0) {
            error("P2P: ERROR on binding listener socket"); 
        }

        if (listen(listener_socket, 1) < 0) { 
            close(listener_socket);
            error("P2P: ERROR on listen");
        }

        struct sockaddr_in connected_peer_addr;
        socklen_t peer_len = sizeof(connected_peer_addr);
        peer_socket = accept(listener_socket, (struct sockaddr *)&connected_peer_addr, &peer_len);

        if (peer_socket < 0) {
            close(listener_socket);
            error("P2P: ERROR on accept");
        }
        close(listener_socket);
        char connected_peer_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &connected_peer_addr.sin_addr, connected_peer_ip_str, INET_ADDRSTRLEN);
        // Aqui você esperaria o REQ_CONNPEER do peer que se conectou
        // Ex: receive_and_process_req_connpeer(peer_socket); // Recebe REQ, envia RES com ID, recebe RES com ID do peer
    }

    if (peer_socket > 0) {
        // printf("setei ID\n");
        GeneratePeerID(peer_id);
        printf("Peer %s connected\n", peer_id);
        // A troca de IDs (PidS) deve seguir o protocolo do PDF após o socket TCP estar conectado.
        return peer_socket; 
    }
    else {
        close(peer_socket);
        error("P2P: ERROR on peer connection");
    }
    return -1;
}

int SensorConnect(char *ip, int port){
    srand(time(NULL) ^ getpid());
    socklen_t cli_len;
    struct sockaddr_in serv_addr, cli_addr;
    printf("Esperando conexão de sensores em %s:%d...\n", ip, port);

    // Criando socket 
    int sensor_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (sensor_socket < 0) error("ERROR opening socket");
    
    // Definindo endereço do socket para conexões cliente-servidor
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);
    if (bind(sensor_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        error("ERROR on binding");
    }
    int optval = 1;
    setsockopt(sensor_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // Esperando conexão inicial sensor
    listen(sensor_socket, 5);
    cli_len = sizeof(cli_addr);
    sensor_socket = accept(sensor_socket, (struct sockaddr *) &cli_addr, &cli_len);
    if (sensor_socket < 0) error("ERROR on accept");

    return sensor_socket;
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

int main(int argc, char **argv){
    // Definindo seed para IDs
    time_t current_time = time(NULL);
    pid_t current_pid = getpid();
    unsigned int seed = current_time ^ current_pid;
    srand(seed);

    char* ip;
    char buffer[256];
    struct sockaddr_in peer_addr;
    int peer_port, sensor_port, n, sensor_socket, peer_socket, peer_connections = 0, sensor_connections = 0;

    // Lendo entradas do terminal
    ip = argv[1];
    peer_port = atoi(argv[2]);
    sensor_port = atoi(argv[3]);

    // Conectando aos sockets do peer e recebendo ID
    peer_socket = P2PConnect(ip, peer_port);
    SendMessage(RES_CONNPEER, peer_id, peer_socket);
    message response;
    n = read(peer_socket, &response, sizeof(response));
    if (n < 0) error("ERROR reading from peer socket");
    strncpy(my_id, response.payload, sizeof(my_id) - 1);
    printf("New Peer ID: %s\n", my_id);
    
    // Conectando ao socket do sensor
    sensor_socket = SensorConnect(ip, sensor_port);
    
    // // Fluxo de troca de mensagens entre servidor e sensor
    // while(1){
    //     // Lendo mensagem recebida e exibindo, se for o caso
    //     bzero(buffer, 256);
    //     n = read(sensor_socket, buffer, 255);
    //     if (n < 0) error("ERROR reading from socket");
    //     if (strcmp(buffer, "kill\n") == 0){
    //         printf("Conexão encerrada pelo sensor.\n");
    //         close(sensor_socket);
    //         break;
    //     }
    //     printf("Recebido do sensor: %s\n", buffer);

    //     // Escrevendo uma mensagem para o sensor
    //     printf("Escreva um comando: ");
    //     bzero(buffer, 256);
    //     fgets(buffer, 255, stdin);
    //     n = write(sensor_socket, buffer, strlen(buffer));
    //     // Caso seja kill, encerra conexão
    //     if (strcmp(buffer, "kill\n") == 0){
    //         printf("Conexão encerrada pelo servidor.\n");
    //         close(sensor_socket);
    //         break;
    //     }
    //     if (n < 0) error("ERROR writing to socket");
    //     bzero(buffer, 256);
    // }
    return 0; 
}