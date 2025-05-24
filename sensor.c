#include <netdb.h> 
#include <stdio.h>
// #include "common.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

void error(const char *msg){
    perror(msg);
    exit(0);
}

int main(int argc, char **argv){
    char buffer[256];
    struct hostent *server;
    struct sockaddr_in serv_addr;
    int server_socket, server_port, n;

    // Lendo entradas do terminal
    server_port = atoi(argv[2]);
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)  error("ERROR opening socket");

    // Pegando dados do servidor e conectando
    server = gethostbyname(argv[1]);
    if (server == NULL) error("ERROR, no such host\n");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(server_port);
    if (connect(server_socket,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    // Fluxo de troca de mensagens entre sensor e servidor
    while(1){
        // Enviando mensagem para o servidor
        printf("Escreva um comando: ");
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);
        n = write(server_socket, buffer, strlen(buffer));
        // Caso seja kill, encerra conexão
        if (strcmp(buffer, "kill\n") == 0){
            printf("Conexão encerrada pelo sensor.\n");
            close(server_socket);
            break;
        }
        if (n < 0) error("ERROR writing to socket");
        bzero(buffer, 256);

        // Lendo mensagem recebida e exibindo, se for o caso
        n = read(server_socket, buffer, 255);
        if (n < 0) error("ERROR reading from socket");
        if (strcmp(buffer, "kill\n") == 0){
            printf("Conexão encerrada pelo servidor.\n");
            close(server_socket);
            break;
        }
        printf("Recebido do servidor: %s\n",buffer);
    }
    return 0;
}
