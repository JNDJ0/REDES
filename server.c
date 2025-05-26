#include "common.c"

/*
./server 127.0.0.1 64000 60000
./server 127.0.0.1 64000 61000
./sensor 127.0.0.1 60000
./sensor 127.0.0.1 61000
*/

char my_id[10];

// 1 -> localização / 0 -> status
int role;
char peer_id[10];
int connected_peers;

char* sensors_id[15];
int connected_sensors;
int sensors_sockets[MAX_SENSORS]; 
int sensors_status[MAX_SENSORS];
int sensors_locations[MAX_SENSORS];

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
        message msg = ReceiveRawMessage(connector_socket);
        if (msg.type == RES_CONNPEER) {
            // Recebendo ID dado pelo peer
            strncpy(my_id, msg.payload, sizeof(my_id) - 1);
            my_id[sizeof(my_id) - 1] = '\0';
            printf("New Peer ID: %s\n", my_id);

            // Dando ID ao peer conectado
            GeneratePeerID(peer_id);
            printf("Peer %s connected\n", peer_id);
            peer_socket = connector_socket;
            SendMessage(RES_CONNPEER, peer_id, peer_socket);
            return peer_socket;
        }
    } 
    else {
        // Nenhum peer aberto; começar a ouvir por possíveis conexões
        close(connector_socket); 
        printf("No peers found, starting to listen...\n");

        // Instancia socket para ouvir conexões
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

        // Aceitando a conexão feita
        struct sockaddr_in connected_peer_addr;
        socklen_t peer_len = sizeof(connected_peer_addr);
        peer_socket = accept(listener_socket, (struct sockaddr *)&connected_peer_addr, &peer_len);
        close(listener_socket);
        char connected_peer_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &connected_peer_addr.sin_addr, connected_peer_ip_str, INET_ADDRSTRLEN);
        
        // Conexão bem sucedida: receber REQ e enviar RES
        if (peer_socket >= 0) {
            connected_peers++;
            message msg = ReceiveRawMessage(peer_socket);
            if (msg.type == REQ_CONNPEER) {
                // Gerando ID ao peer conectado
                GeneratePeerID(peer_id);
                printf("Peer %s connected\n", peer_id);
                SendMessage(RES_CONNPEER, peer_id, peer_socket);

                // Recebendo ID dado pelo peer conectado
                msg = ReceiveRawMessage(peer_socket);
                if (msg.type == RES_CONNPEER) {
                    char* assigned_id = msg.payload;
                    strncpy(my_id, assigned_id, sizeof(my_id) - 1);
                    my_id[sizeof(my_id) - 1] = '\0';
                    printf("New Peer ID: %s\n", my_id);
                }
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

int SensorConnectMulti(char *ip, int port) {
    struct sockaddr_in serv_addr;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) error("ERROR opening socket");

    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }

    if (listen(server_socket, 5) < 0) error("ERROR on listen");

    return server_socket;
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
    int peer_port, sensor_port, n, sensor_listener_socket, peer_socket, max_fd;
    connected_peers = 0;
    connected_sensors = 0;
    
    // Lendo entradas do terminal e atribuindo papel
    ip = argv[1];
    peer_port = atoi(argv[2]);
    sensor_port = atoi(argv[3]);
    role = sensor_port == SL_CLIENT_LISTEN_PORT_DEFAULT ? 1 : 0;

    // Conectando aos sockets do peer e do sensor
    peer_socket = P2PConnect(ip, peer_port);
    sensor_listener_socket = SensorConnectMulti(ip, sensor_port);
    fd_set read_fds;
    
    // Loop de mensagens e comandos do sistema
    while (1) {
        FD_ZERO(&read_fds);
        max_fd = sensor_listener_socket;
        // Aguardando novas requisições dos sensores
        FD_SET(sensor_listener_socket, &read_fds); 
        // Aguardando novas requisições do peer
        FD_SET(peer_socket, &read_fds);
        // Aguardando entradas no terminal 
        FD_SET(STDIN_FILENO, &read_fds);
        for (int i = 0; i < connected_sensors; i++) {
            FD_SET(sensors_sockets[i], &read_fds);
        }
        
        // Trocando prioridade de leitura
        if (peer_socket > max_fd) max_fd = peer_socket;
        if (STDIN_FILENO > max_fd) max_fd = STDIN_FILENO;
        for (int i = 0; i < connected_sensors; i++) {
            if (sensors_sockets[i] > max_fd) max_fd = sensors_sockets[i];
        }
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Erro em select");
            break;
        }

        // Checa entrada do terminal
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            bzero(buffer, 256);
            fgets(buffer, 255, stdin);
            // kill: encerra comunicações com o outro peer.
            if (strncmp(buffer, "kill", 4) == 0) {
                SendMessage(REQ_DISCPEER, my_id, peer_socket);
                message msg = ReceiveRawMessage(peer_socket);
                if (msg.type == OK_CODE){
                    printf("Peer %s disconnected.\n", peer_id);
                    close(peer_socket);
                }
                break;
            }
            // Default: Broadcast para sensores
            for (int i = 0; i < connected_sensors; i++) {
                write(sensors_sockets[i], buffer, strlen(buffer));
            }
        }

        // Verifica novas requisições dos peers
        if (FD_ISSET(peer_socket, &read_fds)) {
            message msg = ReceiveRawMessage(peer_socket);
            
            switch (msg.type) {
                case REQ_DISCPEER:
                    printf("Peer %s disconnected.\n", peer_id);
                    SendMessage(OK_CODE, OK_SUCCESSFUL_DISCONNECT, peer_socket);
                    peer_socket = P2PConnect(ip, peer_port);
                    break;
            
                case REQ_CONNSEN:
                    if (connected_sensors < MAX_SENSORS) {
                        sensors_id[connected_sensors] = strdup(msg.payload);
                        sensors_status[connected_sensors] = 0;
                        printf("Client %s added (Status: %d)\n", sensors_id[connected_sensors], sensors_status[connected_sensors]);
                        connected_sensors++;
                    } else {
                        SendMessage(ERROR_CODE, ERROR_SENSOR_LIMIT_EXCEEDED, peer_socket);
                        close(peer_socket);
                    }
                    break;
            
                default:
                    fprintf(stderr, "1. Mensagem desconhecida recebida: tipo %d\n", msg.type);
                    break;
            }            
        }

        // Verifica novas conexões de sensores
        if (FD_ISSET(sensor_listener_socket, &read_fds)) {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int new_socket = accept(sensor_listener_socket, (struct sockaddr *)&cli_addr, &cli_len);
            message msg = ReceiveRawMessage(new_socket);
            
            switch(msg.type){
                case REQ_CONNSEN:
                    if (connected_sensors < MAX_SENSORS) {
                        sensors_locations[connected_sensors] = atoi(msg.payload);
                        sensors_sockets[connected_sensors] = new_socket;
    
                        sensors_id[connected_sensors] = malloc(10 * sizeof(char));
                        GenerateSensorID(sensors_id[connected_sensors]);
                        printf("Client %s added (Loc: %d)\n", sensors_id[connected_sensors], sensors_locations[connected_sensors]);
                        SendMessage(RES_CONNSEN, sensors_id[connected_sensors], new_socket);
                        SendMessage(REQ_CONNSEN, sensors_id[connected_sensors], peer_socket);
                        connected_sensors++;
                    } 
                    else {
                        SendMessage(ERROR_CODE, ERROR_SENSOR_LIMIT_EXCEEDED, new_socket);
                        close(new_socket);
                    }
                    break;

                default:
                    fprintf(stderr, "2. Mensagem desconhecida recebida: tipo %d\n", msg.type);
                    break;
            }
        }

        // Verifica mensagens dos sensores
        for (int i = 0; i < connected_sensors; i++) {
            if (FD_ISSET(sensors_sockets[i], &read_fds)) {
                message msg = ReceiveRawMessage(sensors_sockets[i]);
                switch(msg.type){
                    // Caso o sensor se desconecte, remove-o da lista de sensores conectados
                    case REQ_DISCSEN:
                        int error = 0;
                        for (int i = 0; i < connected_sensors; i++) {
                            if (strcmp(sensors_id[i], msg.payload) == 0) {
                                printf("Client %s removed (Loc %d).\n", sensors_id[i], sensors_locations[i]);
                                SendMessage(OK_CODE, OK_SUCCESSFUL_DISCONNECT, sensors_sockets[i]);
                                close(sensors_sockets[i]);
                                free(sensors_id[i]);
                                // Remove sensor
                                for (int j = i; j < connected_sensors - 1; j++) {
                                    sensors_sockets[j] = sensors_sockets[j + 1];
                                    sensors_id[j] = sensors_id[j + 1];
                                    sensors_locations[j] = sensors_locations[j + 1];
                                }
                                connected_sensors--;
                                break;
                            }
                            else error++;
                        }
                        // if (error == connected_sensors) {
                        //     printf("ta perdido pai?\n");
                        //     // SendMessage(ERROR_CODE, ERROR_SENSOR_NOT_FOUND, new_socket);
                        //     // close(new_socket);
                        // }
                        break;

                    default:
                        fprintf(stderr, "3. Mensagem desconhecida recebida: tipo %d\n", msg.type);
                        break;
                }
            }
        }
    }

    // Encerra conexões
    for (int i = 0; i < connected_sensors; i++) {
        close(sensors_sockets[i]);
    }
    close(sensor_listener_socket);
    close(peer_socket);
    return 0; 
}