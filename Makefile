# Compilador C
CC = gcc

# Flags de compilação
# -Wall: Habilita a maioria dos warnings
# -g: Adiciona informações de debugging
CFLAGS = -Wall -g

# Arquivos de cabeçalho (se common.h for usado por ambos)
HDRS = common.h

# Alvos default: compila tudo
all: server sensor

# Regra para compilar o server
server: server.c $(HDRS)
	$(CC) $(CFLAGS) -o server server.c

# Regra para compilar o sensor (cliente/equipment)
sensor: sensor.c $(HDRS)
	$(CC) $(CFLAGS) -o sensor sensor.c

# Regra para limpar os arquivos compilados e executáveis
clean:
	rm -f server sensor *.o

# .PHONY: Evita conflitos com arquivos que tenham o mesmo nome dos alvos
.PHONY: all clean