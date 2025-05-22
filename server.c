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

int main(int argc, char **argv){
    char* ip;
    char buffer[256];
    socklen_t clilen;
    struct sockaddr_in p2p_addr, serv_addr, cli_addr;
    int p2p_port, sensor_port, n, client_socket, new_socket;
    // Lendo entradas do terminal
    ip = argv[1];
    p2p_port = atoi(argv[2]);
    sensor_port = atoi(argv[3]);
    printf("Esperando conexão de sensores em %s:%d...\n", ip, sensor_port);
    printf("Esperando conexão de peers em %s:%d...\n", ip, p2p_port);

    // Criando socket 
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) error("ERROR opening socket");
    
    // Definindo endereço do socket para conexões cliente-servidor
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(sensor_port);
    if (bind(client_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        error("ERROR on binding");
    }

    // Esperando conexão inicial sensor
    listen(client_socket, 5);
    clilen = sizeof(cli_addr);
    client_socket = accept(client_socket, (struct sockaddr *) &cli_addr, &clilen);
    if (client_socket < 0) error("ERROR on accept");
    
    // Fluxo de troca de mensagens entre servidor e sensor
    while(1){
        // Lendo mensagem recebida e exibindo, se for o caso
        bzero(buffer, 256);
        n = read(client_socket, buffer, 255);
        if (n < 0) error("ERROR reading from socket");
        if (strcmp(buffer, "kill\n") == 0){
            printf("Conexão encerrada pelo sensor.\n");
            close(client_socket);
            break;
        }
        printf("Recebido do sensor: %s\n", buffer);

        // Escrevendo uma mensagem para o sensor
        printf("Escreva uma mensagem (kill para sair): ");
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);
        n = write(client_socket, buffer, strlen(buffer));
        // Caso seja kill, encerra conexão
        if (strcmp(buffer, "kill\n") == 0){
            printf("Conexão encerrada pelo servidor.\n");
            close(client_socket);
            break;
        }
        if (n < 0) error("ERROR writing to socket");
        bzero(buffer, 256);
    }
    return 0; 
}