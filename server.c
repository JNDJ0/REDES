#define _GNU_SOURCE
#include "common.c"

/*
./server 127.0.0.1 64000 60000
./server 127.0.0.1 64000 61000
./sensor 127.0.0.1 60000 61000
*/

char my_id[ID_LEN + 1];
char peer_id[ID_LEN + 1];
int connected_peers;

sensor_info sensors[MAX_SENSORS];
int connected_sensors;

/**
 * @brief Cria um socket de escuta para conexões P2P.
 * 
 * @param ip O endereço IP para escutar.
 * @param port A porta para escutar.
 * @return int O socket de escuta.
 */
int P2PListener(char* ip, int port){
    int listener_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_socket < 0) {
        error("P2P: ERROR opening listener socket");
    }
    int optval = 1;
    setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(listener_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));  
    struct sockaddr_in my_listen_addr;
    bzero((char *)&my_listen_addr, sizeof(my_listen_addr));
    my_listen_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &my_listen_addr.sin_addr) <= 0) {
        close(listener_socket);
        error("P2P: Invalid peer target IP address");
    }
    my_listen_addr.sin_port = htons(port);
    if (bind(listener_socket, (struct sockaddr *)&my_listen_addr, sizeof(my_listen_addr)) < 0) {
        error("P2P: ERROR on binding listener socket");
    }
    if (listen(listener_socket, 1) < 0) {
        close(listener_socket);
        error("P2P: ERROR on listen");
    }
    return listener_socket;
}

/**
 * @brief Aceita uma conexão de um peer.
 * 
 * @param listener_socket O socket de escuta.
 * @return int O socket do peer conectado ou -1 em caso de erro.
 */
int P2PAccept(int listener_socket){
    // Implementando select para permitir que o servidor digite kill e encerre as tentativas
    fd_set read_fds;
    int max_fd = (listener_socket > STDIN_FILENO) ? listener_socket : STDIN_FILENO;
    while(1){
        FD_ZERO(&read_fds);
        // Escutando por um novo peer
        FD_SET(listener_socket, &read_fds);
        // Escutando por entrada do terminal
        FD_SET(STDIN_FILENO, &read_fds);
        // Trocando prioridade de leitura
        if (listener_socket > max_fd) max_fd = listener_socket;
        if (STDIN_FILENO > max_fd) max_fd = STDIN_FILENO;
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            error("P2P: ERROR in select");
            close(listener_socket);
            return -1;
        }
        // Checa entrada do terminal
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char buffer[256];
            bzero(buffer, 256);
            fgets(buffer, 255, stdin);
            if (strncmp(buffer, "kill", 4) == 0) {
                printf("Successful disconnect\n");
                close(listener_socket);
                return -1; 
            }
        }
        // Checa novas conexões de peers
        if (FD_ISSET(listener_socket, &read_fds)) {
            // Aceitando a conexão feita
            struct sockaddr_in connected_peer_addr;
            socklen_t peer_len = sizeof(connected_peer_addr);
            int peer_socket = accept(listener_socket, (struct sockaddr *)&connected_peer_addr, &peer_len);
            char connected_peer_ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &connected_peer_addr.sin_addr, connected_peer_ip_str, INET_ADDRSTRLEN);
            // Conexão bem sucedida; necessário gerar e enviar o ID ao novo peer.
            if (peer_socket >= 0) {
                connected_peers++;
                message msg = ReceiveRawMessage(peer_socket);
                if (msg.type == REQ_CONNPEER) {
                    // Gerando ID ao peer conectado
                    GenerateRandomID(peer_id);
                    printf("Peer %s connected\n", peer_id);
                    SendMessage(RES_CONNPEER, peer_id, peer_socket);
                    
                    // Recebendo ID dado pelo peer conectado
                    msg = ReceiveRawMessage(peer_socket);
                    if (msg.type == RES_CONNPEER) {
                        char* assigned_id = msg.payload;
                        strncpy(my_id, assigned_id, ID_LEN);
                        my_id[ID_LEN] = '\0';
                        printf("New Peer ID: %s\n", my_id);
                    }
                    return peer_socket; 
                }
            }
            else {
                close(peer_socket);
                error("P2P: ERROR on peer connection");
            }
            break;
        }
    }
}
/**
 * @brief Conecta a um peer em um endereço IP e porta especificados.
 * 
 * @param ip O endereço IP do peer.
 * @param port A porta do peer.
 * 
 * @return O socket do peer conectado; -1 caso dê erro no connect; -2 caso dê erro de limite de peers.
 */
int P2PConnect(char* ip, int port) {
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
            strncpy(my_id, msg.payload, ID_LEN);
            my_id[ID_LEN] = '\0';
            printf("New Peer ID: %s\n", my_id);

            // Dando ID ao peer conectado
            GenerateRandomID(peer_id);
            printf("Peer %s connected\n", peer_id);
            peer_socket = connector_socket;
            SendMessage(RES_CONNPEER, peer_id, peer_socket);
            return peer_socket;
        }
        else if (msg.type == ERROR_CODE && strncmp(msg.payload, ERROR_PEER_LIMIT_EXCEEDED, strlen(ERROR_PEER_LIMIT_EXCEEDED)) == 0) {
            printf("Peer limit exceeded.\n");
            return -2;
        }
    }

    return peer_socket;
}

/**
 * @brief Conecta a múltiplos sensores em um endereço IP e porta especificados.
 * 
 * @param ip O endereço IP do sensor.
 * @param port A porta do sensor.
 * 
 * @return O socket do servidor de sensores ou -1 em caso de erro.
 */
int SensorConnect(char *ip, int port) {
    struct sockaddr_in serv_addr;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) error("CLI: ERROR opening socket");

    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        close(server_socket);
        error("CLI: Invalid peer target IP address");
    }
    serv_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("CLI: ERROR on binding");
    }

    if (listen(server_socket, 15) < 0) error("CLI: ERROR on listen");

    return server_socket;
}

/**
 * @brief Função que lida com entradas do terminal.
 * 
 * @param read_fds O conjunto de descritores de arquivo prontos para leitura.
 * @param peer_socket O socket do peer.
 * 
 * @return 1 se o programa deve encerrar, 0 caso contrário.
 */
int TerminalHandler(fd_set read_fds, int peer_socket, int listener_socket) {
    char buffer[256];
    // Checa entrada do terminal
    if (FD_ISSET(STDIN_FILENO, &read_fds)) {
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);
        // kill: encerra comunicações com o outro peer.
        if (strncmp(buffer, "kill", 4) == 0) {
            SendMessage(REQ_DISCPEER, my_id, peer_socket);
            message msg = ReceiveRawMessage(peer_socket);
            if (msg.type == OK_CODE) {
                printf("Successful disconnect\nPeer %s disconnected.\n", peer_id);
                return 1; 
            }
            else if(msg.type == ERROR_CODE && strncmp(msg.payload, ERROR_PEER_NOT_FOUND, strlen(ERROR_PEER_NOT_FOUND)) == 0) {
                printf("Peer not found.\n");
            }
        }
    }
    return 0;
}

/**
 * @brief Função que lida com as requisições dos peers.
 * 
 * @param read_fds O conjunto de descritores de arquivo prontos para leitura.
 * @param peer_socket O socket do peer.
 * @param role O papel do servidor (0 status, 1 localização).
 * 
 * @return 1 se o socket do peer deve ser trocado, 0 caso contrário.
 */
int PeerHandler(fd_set read_fds, int peer_socket, int role) {
    // Verifica novas requisições dos peers
    if (FD_ISSET(peer_socket, &read_fds)) {
        message msg = ReceiveRawMessage(peer_socket);
            
        switch (msg.type) {
            // Recebendo requisição de desconexão do peer
            case REQ_DISCPEER:
                if (strcmp(msg.payload, peer_id) == 0){
                    printf("Peer %s disconnected.\n", peer_id);
                    SendMessage(OK_CODE, OK_SUCCESSFUL_DISCONNECT, peer_socket);
                    return 1;
                }
                else {
                    SendMessage(ERROR_CODE, ERROR_PEER_NOT_FOUND, peer_socket);
                }
                break;

            // Recebendo requisição de checar localização do sensor
            case REQ_CHECKALERT:
                if (role) {
                    int error_check = 0;
                    printf("REQ_CHECKALERT %s\n", msg.payload);
                    for (int i = 0; i < connected_sensors; i++) {
                        // Caso o sensor esteja na lista, envia o status
                        if (strcmp(sensors[i].id, msg.payload) == 0) {
                            char location_area = CheckLocation(sensors[i].location) + '0';
                            printf("Found location of sensor %s: Location %c\n", sensors[i].id, location_area);
                            printf("Sending RES_CHECKALERT %c to SS\n", location_area);
                            SendMessage(RES_CHECKALERT, &location_area, peer_socket);
                            break;
                        }
                        else error_check++;
                    }
                    if (error_check == connected_sensors){
                        printf("Sending ERROR_CODE %s to SS\n", ERROR_SENSOR_NOT_FOUND);
                        SendMessage(ERROR_CODE, ERROR_SENSOR_NOT_FOUND, peer_socket);
                    }
                }
                break;

            default:
                error("P2P: Mensagem desconhecida recebida");
                break;
        }               
    }

    return 0;
}

/**
 * @brief Função que lida com novas conexões de sensores.
 * 
 * @param read_fds O conjunto de descritores de arquivo prontos para leitura.
 * @param sensor_listener_socket O socket que escuta por conexões de sensores.
 * @param peer_socket O socket do peer.
 * @param role O papel do servidor (0 status, 1 localização).
 */
void SensorConnectionHandler(fd_set read_fds, int sensor_listener_socket, int peer_socket, int role) {
    // Verifica novas conexões de sensores
    if (FD_ISSET(sensor_listener_socket, &read_fds)) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int new_socket = accept(sensor_listener_socket, (struct sockaddr *)&cli_addr, &cli_len);
        message msg = ReceiveRawMessage(new_socket);
            
        switch(msg.type) {
            // Recebendo requisição de conexão do sensor
            case REQ_CONNSEN:
                // Caso tenha espaço, adiciona o sensor 
                if (connected_sensors < MAX_SENSORS) {
                    sensor_info new_sensor;
                    if (role) {
                        strncpy(new_sensor.id, msg.payload, ID_LEN);
                        new_sensor.location = (rand() % 10) + 1;
                        printf("Client %s added (Loc: %d)\n", new_sensor.id, new_sensor.location);
                    }
                    else {
                        strncpy(new_sensor.id, msg.payload, ID_LEN);
                        new_sensor.status = rand() % 2;
                        printf("Client %s added (Status: %d)\n", new_sensor.id, new_sensor.status);
                    }
                    new_sensor.socket_fd = new_socket;
                    sensors[connected_sensors] = new_sensor;
                    SendMessage(RES_CONNSEN, "", new_socket);
                    connected_sensors++;
                } 
                // Caso não tenha espaço, envia erro
                else {
                    printf("Sending ERROR_CODE %s to sensor\n", ERROR_SENSOR_LIMIT_EXCEEDED);
                    SendMessage(ERROR_CODE, ERROR_SENSOR_LIMIT_EXCEEDED, new_socket);
                    close(new_socket);
                }
                break;

            default:
                error("CLI: Mensagem desconhecida recebida");
                break;
        }
    }
}

/**
 * @brief Função que lida com as mensagens recebidas dos sensores.
 * 
 * @param read_fds O conjunto de descritores de arquivo prontos para leitura.
 * @param sensors A lista de sensores conectados.
 * @param peer_socket O socket do peer.
 * @param role O papel do servidor (0 status, 1 localização).
 */
void SensorMessageHandler(fd_set read_fds, sensor_info *sensors, int peer_socket, int role) {
    // Verifica mensagens dos sensores
    for (int i = 0; i < connected_sensors; i++) {
        if (FD_ISSET(sensors[i].socket_fd, &read_fds)) {
            message msg = ReceiveRawMessage(sensors[i].socket_fd);
            switch(msg.type) {
                // Recebendo requisição de desconexão do sensor
                case REQ_DISCSEN:
                    for (int i = 0; i < connected_sensors; i++) {
                        // Caso o sensor esteja na lista, o remove
                        if (strcmp(sensors[i].id, msg.payload) == 0) {
                            if (role) printf("Client %s removed (Loc: %d)\n", sensors[i].id, sensors[i].location);
                            else printf("Client %s removed (Status: %d)\n", sensors[i].id, sensors[i].status);

                            SendMessage(OK_CODE, OK_SUCCESSFUL_DISCONNECT, sensors[i].socket_fd);
                            close(sensors[i].socket_fd);
                            for (int j = i; j < connected_sensors - 1; j++) sensors[j] = sensors[j + 1];
                            connected_sensors--;
                            break;
                        }
                    }
                    break;

                // Recebendo requisição de status do sensor
                case REQ_SENSSTATUS:
                    if (!role) {
                        printf("REQ_SENSSTATUS %s\n", sensors[i].id);
                        int status = sensors[i].status;
                        // Se for positivo, checa a localização do sensor
                        if (status) {
                            printf("Sensor %s status = 1 (failure detected)\n", sensors[i].id);
                            printf("Sending REQ_CHECKALERT %s to SL\n", sensors[i].id);
                            SendMessage(REQ_CHECKALERT, sensors[i].id, peer_socket);
                            message alert_msg = ReceiveRawMessage(peer_socket);
                            if (alert_msg.type == RES_CHECKALERT) {
                                printf("Sending RES_SENSSTATUS %s to sensor\n", sensors[i].id);
                                SendMessage(RES_SENSSTATUS, alert_msg.payload, sensors[i].socket_fd);
                            }
                            else if (alert_msg.type == ERROR_CODE) {
                                printf("Sending ERROR_CODE %s to sensor\n", ERROR_SENSOR_NOT_FOUND);
                                SendMessage(ERROR_CODE, ERROR_SENSOR_NOT_FOUND, sensors[i].socket_fd);
                            }
                        }
                        else {
                            printf("Sensor %s status = 0 (no failure detected)\n", sensors[i].id);
                            printf("Sending OK_CODE %s to sensor\n", OK_STATUS_0);
                            SendMessage(OK_CODE, OK_STATUS_0, sensors[i].socket_fd);
                        }
                    }   
                    break;

                // Recebendo requisição de localização do sensor
                case REQ_SENSLOC:
                    if (role) {
                        printf("REQ_SENSLOC %s\n", msg.payload);
                        int error_check = 0;
                        for (int j = 0; j < connected_sensors; j++) {
                            if (strcmp(sensors[j].id, msg.payload) == 0) {
                                char location = sensors[j].location + '0';
                                printf("REQ_SENSLOC %s\n", sensors[i].id);
                                SendMessage(RES_SENSLOC, &location, sensors[i].socket_fd);
                            }
                            else error_check++;
                        }
                        if (error_check == connected_sensors) {
                            printf("Sending ERROR_CODE %s to sensor\n", ERROR_SENSOR_NOT_FOUND);
                            SendMessage(ERROR_CODE, ERROR_SENSOR_NOT_FOUND, sensors[i].socket_fd);
                        }
                    }
                    break;

                // Recebendo requisição de listar sensores na localização
                case REQ_LOCLIST:
                    if (role) {
                        printf("REQ_LOCLIST %s\n", msg.payload);
                        char answer[MAX_MSG_SIZE] = "";
                        int location = atoi(msg.payload);
                        for (int j = 0; j < connected_sensors; j++) {
                            if (sensors[j].location == location) {    
                                if (strlen(answer) > 0) {
                                    strcat(answer, ", ");
                                }
                                strcat(answer, sensors[j].id);
                            }
                        }
                        if (strlen(answer) > 0) {
                            printf("Sending RES_LOCLIST %s to sensor\n", answer);
                            SendMessage(RES_LOCLIST, answer, sensors[i].socket_fd);
                        } 
                        else {
                            printf("Sending ERROR_CODE %s to sensor\n", ERROR_LOCATION_NOT_FOUND);
                            SendMessage(ERROR_CODE, ERROR_LOCATION_NOT_FOUND, sensors[i].socket_fd);
                        }
                    }
                    break;

                default:
                    error("CLI: Mensagem desconhecida recebida");
                    break;
            }
        }
    }
}

int main(int argc, char **argv) {
    // Definindo seed para IDs
    time_t current_time = time(NULL);
    pid_t current_pid = getpid();
    unsigned int seed = current_time ^ current_pid;
    srand(seed);

    char* ip;
    char buffer[256];
    struct sockaddr_in peer_addr;
    int peer_port, sensor_port, n, sensor_listener_socket, listener_socket, peer_socket, max_fd;
    connected_peers = 0;
    connected_sensors = 0;
    listener_socket = -1;
    
    // Lendo entradas do terminal e atribuindo papel
    ip = argv[1];
    peer_port = atoi(argv[2]);
    sensor_port = atoi(argv[3]);
    int role = sensor_port == SL_CLIENT_LISTEN_PORT_DEFAULT ? 1 : 0;

    // Conectando aos sockets do peer e do sensor
    peer_socket = P2PConnect(ip, peer_port);
    if (peer_socket == -1){
        printf("No peers found, starting to listen...\n");
        listener_socket = P2PListener(ip, peer_port);
        peer_socket = P2PAccept(listener_socket);
        if (peer_socket < 0) return 1;
    }
    // Erro de limite excedido
    else if (peer_socket == -2) {
        return 1;
    }
    sensor_listener_socket = SensorConnect(ip, sensor_port);
    
    // Loop de mensagens e comandos do sistema
    fd_set read_fds;
    while (1) {
        FD_ZERO(&read_fds);
        max_fd = sensor_listener_socket;
        // Aguardando novas requisições dos sensores
        FD_SET(sensor_listener_socket, &read_fds); 
        // Aguardando novas requisições do peer
        FD_SET(peer_socket, &read_fds);
        // Aguardando entradas no terminal 
        FD_SET(STDIN_FILENO, &read_fds);
        // Aguardando listener
        if (listener_socket > 0) FD_SET(listener_socket, &read_fds);
        for (int i = 0; i < connected_sensors; i++) {
            FD_SET(sensors[i].socket_fd, &read_fds);
        }
        
        // Trocando prioridade de leitura
        if (peer_socket > max_fd) max_fd = peer_socket;
        if (STDIN_FILENO > max_fd) max_fd = STDIN_FILENO;
        for (int i = 0; i < connected_sensors; i++) {
            if (sensors[i].socket_fd > max_fd) max_fd = sensors[i].socket_fd;
        }
        if (listener_socket > max_fd) max_fd = listener_socket;
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            error("Error in select");
            break;
        }

        // Checa entrada do terminal
        int flag = TerminalHandler(read_fds, peer_socket, listener_socket);
        if (flag == 1) break;
        
        // Checa requisições do peer
        flag = PeerHandler(read_fds, peer_socket, role);
        if (flag){
            // Peer disconectado, necessário reiniciar conexões
            for (int i = 0; i < connected_sensors; i++) {
                close(sensors[i].socket_fd);
            }
            close(sensor_listener_socket);
            connected_sensors = 0;
            printf("No peers found, starting to listen...\n");
            if (listener_socket < 0) listener_socket = P2PListener(ip, peer_port);
            peer_socket = P2PAccept(listener_socket);
            if (peer_socket < 0) return 1;
            sensor_listener_socket = SensorConnect(ip, sensor_port);
            if (sensor_listener_socket < 0) return 1;
        }

        // Checa novas conexões de sensores
        SensorConnectionHandler(read_fds, sensor_listener_socket, peer_socket, role);

        // Checa mensagens dos sensores
        SensorMessageHandler(read_fds, sensors, peer_socket, role);

        // Caso o listener esteja aberto, rejeita novas conexões
        if (FD_ISSET(listener_socket, &read_fds)) {
            printf("Sending ERROR_CODE %s to server attempting to connect\n", ERROR_PEER_LIMIT_EXCEEDED);
            struct sockaddr_in connected_peer_addr;
            socklen_t peer_len = sizeof(connected_peer_addr);
            int new_socket = accept(listener_socket, (struct sockaddr *)&connected_peer_addr, &peer_len);
            SendMessage(ERROR_CODE, ERROR_PEER_LIMIT_EXCEEDED, new_socket);
            close(new_socket);
        }
    }

    // Encerra conexões
    for (int i = 0; i < connected_sensors; i++) {
        close(sensors[i].socket_fd);
    }
    close(sensor_listener_socket);
    close(peer_socket);
    if (listener_socket > 0) close(listener_socket);
    return 0; 
}