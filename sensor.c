#include "common.c"

char location_id;
char my_id[10];

int main(int argc, char **argv){
    char buffer[256];
    struct hostent *server;
    struct sockaddr_in serv_addr;
    int server_socket, server_port, n;

    // Lendo entradas do terminal
    server_port = atoi(argv[2]);
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) error("ERROR opening socket");

    // Pegando dados do servidor e conectando
    server = gethostbyname(argv[1]);
    if (server == NULL) error("ERROR, no such host\n");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(server_port);
    if (connect(server_socket,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    location_id = (rand() % 5) + '0';
    printf("%c\n", location_id);
    SendMessage(REQ_CONNSEN, &location_id, server_socket);
    char* validation = ReceiveMessage(RES_CONNSEN, server_socket);
    printf("ID: %s\n", validation);
    if (validation != NULL) {
        strncpy(my_id, validation, sizeof(my_id) - 1);
        my_id[sizeof(my_id) - 1] = '\0';
        printf("SS New ID: %s\n", my_id);
        printf("SL New ID: %s\n", my_id);
        free(validation);
    }
    fd_set read_fds;
    int max_fd = server_socket > STDIN_FILENO ? server_socket : STDIN_FILENO;

    // Loop de mensagens e comandos do sistema
    while(1){
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Erro em select");
            break;
        }

        // Verifica entrada do servidor
        if (FD_ISSET(server_socket, &read_fds)) {
            bzero(buffer, 256);
            n = read(server_socket, buffer, 255);
            if (n <= 0) {
                printf("Servidor desconectado.\n");
                break;
            }
            if (strcmp(buffer, "kill\n") == 0) {
                printf("Conexão encerrada pelo servidor.\n");
                break;
            }
            printf("Recebido do servidor: %s", buffer);
        }

        // Verifica entrada do terminal
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            bzero(buffer, 256);
            fgets(buffer, 255, stdin);
            n = write(server_socket, buffer, strlen(buffer));
            if (n < 0) error("ERROR writing to socket");

            if (strcmp(buffer, "kill\n") == 0) {
                printf("Conexão encerrada pelo sensor.\n");
                break;
            }
        }
    }

    close(server_socket);
    return 0;
}
