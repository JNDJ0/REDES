#include <time.h>
#include <netdb.h> 
#include <stdio.h>
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

char* peer_id;
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

    printf("P2P: Attempting to connect to eer at %s:%d...\n", ip, port);
    if (connect(connector_socket, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) == 0) {
        peer_socket = connector_socket;
        // Aqui você iniciaria a troca de mensagens REQ_CONNPEER, RES_CONNPEER
        // Ex: send_req_connpeer(peer_socket);
        //     receive_and_process_res_connpeer(peer_socket); // Recebe ID do peer, envia o seu
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
            error("P2P: ERROR on accept");
            close(listener_socket);
            return -1;
        }
        // Conexão aceita! O listener_socket pode ser fechado pois não aceitará mais conexões P2P.
        close(listener_socket);
        char connected_peer_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &connected_peer_addr.sin_addr, connected_peer_ip_str, INET_ADDRSTRLEN);
        printf("P2P: Peer connected from %s:%d.\n", connected_peer_ip_str, ntohs(connected_peer_addr.sin_port));
        // Aqui você esperaria o REQ_CONNPEER do peer que se conectou
        // Ex: receive_and_process_req_connpeer(peer_socket); // Recebe REQ, envia RES com ID, recebe RES com ID do peer
    }

    if (peer_socket > 0) {
        // char p2p_id_assigned_by_me[PEER_ID_MAX_LEN]; // Supondo que você tenha PEER_ID_MAX_LEN
        // GeneratePeerID(p2p_id_assigned_by_me); // Se a geração de ID é feita aqui
        // printf("Peer (logical connection established), ID for them by me: %s\n", p2p_id_assigned_by_me);
        // A troca de IDs (PidS) deve seguir o protocolo do PDF após o socket TCP estar conectado.
    }
    return peer_socket; // Retorna o socket de dados P2P conectado
}
// int P2PConnect(char* ip, int port){
//     printf("Conectando a %s:%d...\n", ip, port);
//     socklen_t peer_len;
//     struct sockaddr_in serv_addr, peer_addr;
//     int connector_socket = socket(AF_INET, SOCK_STREAM, 0), listener_socket, peer_socket;
//     if (connector_socket < 0) error("ERROR opening connector socket");

//     // Tentando estabelecer conexões com algum peer já aberto
//     bzero((char *) &peer_addr, sizeof(peer_addr));
//     peer_addr.sin_family = AF_INET;
//     if (inet_pton(AF_INET, ip, &peer_addr.sin_addr) <= 0) {
//         close(connector_socket);
//         error("ERROR invalid peer target IP address");
//     }
//     peer_addr.sin_port = htons(port);
//     peer_len = sizeof(peer_addr);
//     connector_socket = connect(connector_socket, (struct sockaddr *) &peer_addr, &peer_len);
//     if (connector_socket == 0) peer_socket = connector_socket;
//     // Caso não tenha nenhum aberto, começa a escutar por conexões
//     else if (connector_socket < 0){
//         listener_socket = socket(AF_INET, SOCK_STREAM, 0);
//         int optval = 1;
//         setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
//         if (listener_socket < 0) error("ERROR opening listener socket");
//         printf("No peers found, starting to listen...\n");

//         // Definindo endereço do socket para conexões p2p
//         bzero((char *) &peer_addr, sizeof(peer_addr));
//         peer_addr.sin_family = AF_INET;
//         peer_addr.sin_addr.s_addr = inet_addr(ip);
//         peer_addr.sin_port = htons(port);
//         if (bind(listener_socket, (struct sockaddr *) &peer_addr, sizeof(peer_addr)) < 0){
//             error("ERROR on binding");
//         }

//         listen(listener_socket, 5);
//         peer_len = sizeof(peer_addr);
//         listener_socket = accept(listener_socket, (struct sockaddr *) &peer_addr, &peer_len);
//         if (listener_socket < 0) error("ERROR on accept");
//         peer_socket = listener_socket;
//     }
//     GeneratePeerID(peer_id);
//     printf("Peer %s connected\n", peer_id);
//     return peer_socket;
// }

int SensorConnect(char *ip, int port){
    srand(time(NULL));
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

    // Esperando conexão inicial sensor
    listen(sensor_socket, 5);
    cli_len = sizeof(cli_addr);
    sensor_socket = accept(sensor_socket, (struct sockaddr *) &cli_addr, &cli_len);
    if (sensor_socket < 0) error("ERROR on accept");

    return sensor_socket;
}

int main(int argc, char **argv){
    char* ip;
    char buffer[256];
    struct sockaddr_in peer_addr;
    int peer_port, sensor_port, n, sensor_socket, peer_socket, peer_connections = 0, sensor_connections = 0;

    // Lendo entradas do terminal
    ip = argv[1];
    peer_port = atoi(argv[2]);
    sensor_port = atoi(argv[3]);

    // Conectando aos sockets do peer e do sensor
    peer_socket = P2PConnect(ip, peer_port, peer_port);
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