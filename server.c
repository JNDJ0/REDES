#include "common.c"

/*
./server 127.0.0.1 64000 60000
./server 127.0.0.1 64000 61000
./sensor 127.0.0.1 60000
./sensor 127.0.0.1 61000
*/

char peer_id[10];
char my_id[10];
char* sensors_id[15];
int connected_peers;
int connected_sensors;

int P2PConnect(const char* ip, int port) {
    int peer_socket = -1;
    struct sockaddr_in peer_addr;

    int connector_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (connector_socket < 0) {
        error("P2P: ERROR opening connector socket");
    }

    // Tentativa de conexão a um peer aberto
    bzero((char *)&peer_addr, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &peer_addr.sin_addr) <= 0) {
        close(connector_socket);
        error("P2P: Invalid peer target IP address");
    }
    peer_addr.sin_port = htons(port);
    // Caso o peer já esteja aberto, conectar
    if (connect(connector_socket, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) == 0) {
        connected_peers++;
        SendMessage(REQ_CONNPEER, "", connector_socket);
        char* validation = ReceiveMessage(RES_CONNPEER, connector_socket);
        if (validation != NULL) {
            strncpy(my_id, validation, sizeof(my_id) - 1);
            my_id[sizeof(my_id) - 1] = '\0';
            printf("New Peer ID: %s\n", my_id);

            GeneratePeerID(peer_id);
            printf("Peer %s connected\n", peer_id);
            peer_socket = connector_socket;
            SendMessage(RES_CONNPEER, peer_id, peer_socket);
            free(validation);
            return peer_socket;
        }
    } 
    else {
        // Nenhum peer aberto; começar a ouvir por possíveis conexões
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
        close(listener_socket);
        char connected_peer_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &connected_peer_addr.sin_addr, connected_peer_ip_str, INET_ADDRSTRLEN);
        // Conexão bem sucedida: receber REQ e enviar RES
        if (peer_socket >= 0) {
            connected_peers++;
            char* validation = ReceiveMessage(REQ_CONNPEER, peer_socket);
            if (validation != NULL) {
                GeneratePeerID(peer_id);
                printf("Peer %s connected\n", peer_id);
                SendMessage(RES_CONNPEER, peer_id, peer_socket);
                // Recebendo ID dado pelo peer conectado
                char* assigned_id = ReceiveMessage(RES_CONNPEER, peer_socket);
                if (assigned_id != NULL) {
                    strncpy(my_id, assigned_id, sizeof(my_id) - 1);
                    my_id[sizeof(my_id) - 1] = '\0';
                    printf("New Peer ID: %s\n", my_id);
                }
                free(validation);
                free(assigned_id);
                return peer_socket; 
            }
        }
        else {
            close(peer_socket);
            error("P2P: ERROR on peer connection");
        }
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

int main(int argc, char **argv){
    // Definindo seed para IDs
    time_t current_time = time(NULL);
    pid_t current_pid = getpid();
    unsigned int seed = current_time ^ current_pid;
    srand(seed);

    char* ip;
    char buffer[256];
    struct sockaddr_in peer_addr;
    int peer_port, sensor_port, n, sensor_socket, peer_socket;
    connected_peers = 0;
    connected_sensors = 0;

    // Lendo entradas do terminal
    ip = argv[1];
    peer_port = atoi(argv[2]);
    sensor_port = atoi(argv[3]);

    // Conectando aos sockets do peer e do sensor
    peer_socket = P2PConnect(ip, peer_port);
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