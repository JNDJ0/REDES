#include "common.c"

char location_id;
char sl_id[MAX_MSG_SIZE + 1];
char ss_id[MAX_MSG_SIZE + 1];

/**
 * @brief Conecta ao servidor em um endereço IP e porta especificados.
 * 
 * @param ip O endereço IP do servidor.
 * @param port A porta do servidor.
 * 
 * @return O socket do servidor conectado ou -1 em caso de erro.
 */
int ServerConnect(char *ip, int port) {
    struct sockaddr_in serv_addr;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) error("ERROR opening socket");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR connecting");
    }

    SendMessage(REQ_CONNSEN, &location_id, sock);
    message msg = ReceiveRawMessage(sock);
    if (msg.type == RES_CONNSEN){
        if (port == SL_CLIENT_LISTEN_PORT_DEFAULT) {
            strncpy(sl_id, msg.payload, MAX_MSG_SIZE);
            sl_id[MAX_MSG_SIZE] = '\0';
            printf("SL New ID: %s\n", sl_id);
        }
        else if (port == SS_CLIENT_LISTEN_PORT_DEFAULT) {
            strncpy(ss_id, msg.payload, MAX_MSG_SIZE);
            ss_id[MAX_MSG_SIZE] = '\0';
            printf("SS New ID: %s\n", ss_id);
        }
    }

    return sock;
}

int main(int argc, char **argv) {
    // Definindo seed para localizações
    time_t current_time = time(NULL);
    pid_t current_pid = getpid();
    unsigned int seed = current_time ^ current_pid;
    srand(seed);

    char* ip;
    char buffer[256];;
    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sl_socket, ss_socket, sl_port, ss_port, n;

    // Lendo entradas do terminal
    ip = argv[1];
    sl_port = atoi(argv[2]);
    ss_port = atoi(argv[3]);
    if (sl_port != SL_CLIENT_LISTEN_PORT_DEFAULT && ss_port != SS_CLIENT_LISTEN_PORT_DEFAULT) {
        sl_port = SL_CLIENT_LISTEN_PORT_DEFAULT;
        ss_port = SS_CLIENT_LISTEN_PORT_DEFAULT;
    }

    // Gerando ID de localização aleatório, conectando aos servidores e recebendo ID
    location_id = (rand() % 10) + '0';
    sl_socket = ServerConnect(ip, sl_port);
    ss_socket = ServerConnect(ip, ss_port);

    // Loop de mensagens e comandos do sistema
    fd_set read_fds;
    int max_fd = sl_socket;
    while(1) {
        FD_ZERO(&read_fds);
        // Aguardando novas requisições do servidor de localização
        FD_SET(sl_socket, &read_fds);
        // Aguardando novas requisições do servidor de status
        FD_SET(ss_socket, &read_fds);
        // Aguardando entradas no terminal
        FD_SET(STDIN_FILENO, &read_fds);

        // Trocando prioridade de leitura
        if (sl_socket > max_fd) max_fd = sl_socket;
        if (ss_socket > max_fd) max_fd = ss_socket;
        if (STDIN_FILENO > max_fd) max_fd = STDIN_FILENO;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Erro em select");
            break;
        }

        // Verifica entrada do servidor de status
        if (FD_ISSET(ss_socket, &read_fds)) {
            bzero(buffer, 256);
            n = read(ss_socket, buffer, 255);
            if (n <= 0) {
                printf("SS desconectado.\n");
                break;
            }
            printf("Recebido do SS: %s", buffer);
        }

        // Verifica entrada do servidor de localização
        if (FD_ISSET(sl_socket, &read_fds)) {
            bzero(buffer, 256);
            n = read(sl_socket, buffer, 255);
            if (n <= 0) {
                printf("SL desconectado.\n");
                break;
            }
            printf("Recebido do SL: %s", buffer);
        }

        // Verifica entrada do terminal
        if (FD_ISSET(STDIN_FILENO, &read_fds))  {
            bzero(buffer, 256);
            fgets(buffer, 255, stdin);
            // kill: encerra comunicações com os servidores.
            if (strncmp(buffer, "kill", 4) == 0) {
                SendMessage(REQ_DISCSEN, ss_id, ss_socket);
                SendMessage(REQ_DISCSEN, sl_id, sl_socket);
                message msg = ReceiveRawMessage(ss_socket);
                if (msg.type == OK_CODE){
                    printf("SS Successful disconnect\n");
                    close(ss_socket);
                }
                msg = ReceiveRawMessage(sl_socket);
                if (msg.type == OK_CODE){
                    printf("SL Successful disconnect\n");
                    close(sl_socket);
                }
                break;
            }
            // check failure: checa se há pane elétrica, e onde
            if (strncmp(buffer, "check failure", 12) == 0) {
                SendMessage(REQ_SENSSTATUS, ss_id, ss_socket);
                message msg = ReceiveRawMessage(ss_socket);
                if (msg.type == RES_SENSSTATUS) {
                    if (strncmp(msg.payload, "1", 1) == 0) {
                        printf("Alert received from location: 1 (Norte)\n");
                    } 
                    else if (strncmp(msg.payload, "2", 1) == 0) {
                        printf("Alert received from location: 2 (Sul)\n");
                    } 
                    else if (strncmp(msg.payload, "3", 1) == 0) {
                        printf("Alert received from location: 3 (Leste)\n");
                    } 
                    else if (strncmp(msg.payload, "4", 1) == 0) {
                        printf("Alert received from location: 4 (Oeste)\n");
                    } 
                }
                else if (msg.type == ERROR_CODE) {
                    printf("No sensor found\n");
                }
            }
        }
    }

    close(ss_socket);
    close(sl_socket);   
    return 0;
}
