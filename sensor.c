#include "common.c"

char my_id[ID_LEN + 1];

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
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        close(sock);
        error("ERROR invalid IP address");
    }
    serv_addr.sin_port = htons(port);

    // Conectando ao socket do servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR connecting");
    }

    // Estabelecendo conexão com os servidores
    SendMessage(REQ_CONNSEN, my_id, sock);
    message msg = ReceiveRawMessage(sock);
    if (msg.type == RES_CONNSEN){
        if (port == SL_CLIENT_LISTEN_PORT_DEFAULT) {
            printf("SL New ID: %s\n", my_id);
        }
        else if (port == SS_CLIENT_LISTEN_PORT_DEFAULT) {
            printf("SS New ID: %s\n", my_id);
        }
    }
    else if (msg.type == ERROR_CODE){
        printf("Received error %s from server\n", msg.payload);
        return -1;
    }

    return sock;
}

/**
 * @brief Função que lida com a entrada do terminal.
 * 
 * @param read_fds O conjunto de descritores de arquivo prontos para leitura.
 * @param sl_socket O socket do servidor de localização.
 * @param ss_socket O socket do servidor de status.
 *
 * @return 1 se deve parar a execução, 0 caso contrário.
 */
int TerminalHandler(fd_set read_fds, int sl_socket, int ss_socket) {
    // Verifica entrada do terminal
    char buffer[256];
    if (FD_ISSET(STDIN_FILENO, &read_fds))  {
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);
        
        // kill: encerra comunicações com os servidores.
        if (strncmp(buffer, "kill", 4) == 0) {
            SendMessage(REQ_DISCSEN, my_id, ss_socket);
            SendMessage(REQ_DISCSEN, my_id, sl_socket);
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
            return 1;
        }
        
        // check failure: checa se há pane elétrica, e onde
        if (strncmp(buffer, "check failure", 12) == 0) {
            printf("Sending REQ_SENSSTATUS %s\n", my_id);
            SendMessage(REQ_SENSSTATUS, my_id, ss_socket);
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
                printf("Sensor not found\n");
            }
            else if (msg.type == OK_CODE && strncmp(msg.payload, OK_STATUS_0, strlen(OK_STATUS_0)) == 0) {
                printf("OK(3)\n");
            }
        }
        
        // locate: checa onde o sensor está
        if (strncmp(buffer, "locate", 6) == 0) {
            printf("Sending REQ_SENSLOC %s\n", my_id);
            SendMessage(REQ_SENSLOC, my_id, sl_socket);
            message msg = ReceiveRawMessage(sl_socket);
            if (msg.type == RES_SENSLOC) {
                printf("Current sensor location: %s\n", msg.payload);
            }
            else if (msg.type == ERROR_CODE) {
                printf("Sensor not found\n");
            }
        }
        
        // diagnose: lista de quais sensores estão na localização informada
        if (strncmp(buffer, "diagnose", 8) == 0){
            char *token = strtok(buffer, " ");
            token = strtok(NULL, " ");  
            
            if (token != NULL) {
                token[strcspn(token, "\n")] = '\0';
                printf("Sending REC_LOCLIST %s\n", token);
                SendMessage(REQ_LOCLIST, token, sl_socket);
                message msg = ReceiveRawMessage(sl_socket);
                if (msg.type == RES_LOCLIST) {
                    printf("Sensors at location %s: %s\n", token, msg.payload);
                }
                else if (msg.type == ERROR_CODE) {
                    printf("Location not found\n");
                }
            }
        }
    }

    return 0;
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
    GenerateRandomID(my_id);
    sl_socket = ServerConnect(ip, sl_port);
    ss_socket = ServerConnect(ip, ss_port);
    printf("OK(2)\n");
    if (sl_socket < 0 || ss_socket < 0) return 1;

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
            error("CLI ERROR in select");
            break;
        }

        // Verifica entrada do servidor de status
        if (FD_ISSET(ss_socket, &read_fds)) {
            memset(buffer, 0, 256);
            n = read(ss_socket, buffer, 255);
            if (n <= 0) {
                printf("SS disconnected.\n");
                break;
            }
        }

        // Verifica entrada do servidor de localização
        if (FD_ISSET(sl_socket, &read_fds)) {
            memset(buffer, 0, 256);
            n = read(sl_socket, buffer, 255);
            if (n <= 0) {
                printf("SL disconnected.\n");
                break;
            }
        }

        // Verifica entrada do terminal
        int flag = TerminalHandler(read_fds, sl_socket, ss_socket);
        if (flag) break;
    }

    close(ss_socket);
    close(sl_socket);   
    return 0;
}