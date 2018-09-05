#ifdef _WIN32 
#define WIN32_LEAN_AND_MEAN 
#include <windows.h> 
#include <winsock2.h> 
#pragma comment(lib, "ws2_32.lib") 
#else // LINUX 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/time.h> 
#include <sys/select.h> 
#include <netdb.h> 
#include <errno.h> 
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MESSAGE_LENGTH 1024

typedef struct {
    char hour;
    char minute;
    char second;
} MessageTime;

typedef struct {
    unsigned int number;
    MessageTime first_time;
    MessageTime second_time;
    unsigned long bbb;
    char message[MESSAGE_LENGTH];
    char sended;
} Message;

typedef struct {
    unsigned int ip;
    unsigned short int port;
} ClientId;

typedef struct {
	ClientId clientId;
	unsigned int numbers[20];
} ClientData;

typedef struct {
	ClientData *clientData;
	size_t used;
	size_t size;
} ClientArray;

typedef struct {
	int *sockets;
	int maxSocket;
} SocketData;

int init() {
#ifdef _WIN32
	// Для Windows следует вызвать WSAStartup перед началом использования сокетов
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
#else
	return 1; // Для других ОС действий не требуется
#endif
}

void init_sockets(unsigned short int start_port, unsigned short int finish_port, SocketData *socketData) {

	int ports_count = finish_port - start_port;

	printf("port count - %d", ports_count);

	socketData->maxSocket = 0;
	socketData->sockets = malloc(ports_count * sizeof socketData->sockets);

	//init network
	init();

	for (int i = 0; i < ports_count; i++) {

		unsigned short int current_port = start_port + i;

		int s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0) {
			fprintf(stderr, "Socket init error for port %d!", current_port);
			exit(EXIT_FAILURE);			
		} else { 
			if (socketData->maxSocket < s) {
				socketData->maxSocket = s;
			}

			struct sockaddr_in addr;
			// Заполнение структуры с адресом прослушивания узла 
			memset(&addr, 0, sizeof(addr)); 
			addr.sin_family = AF_INET; 
			addr.sin_port = htons(current_port); // Порт для прослушивания 
			addr.sin_addr.s_addr = htonl(INADDR_ANY);
			// Связь адреса и сокета, чтобы он мог принимать входящие дейтаграммы 
			if (bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0)  {
				fprintf(stderr, "Socket bind error for port %d!", current_port);
				exit(EXIT_FAILURE);			
			} else {
				socketData->sockets[i] = s;
			}
		}
	}
}

int sock_err(const char *function, int s) {
    int err;
#ifdef _WIN32
    err = WSAGetLastError();
#else
    err = errno;
#endif
    fprintf(stderr, "%s: socket error: %d\n", function, err);
    return -1;
}

ClientData * add_client(ClientArray *clientArray, ClientData newClient) {
	if (clientArray->used == clientArray->size) {
		size_t new_size = clientArray->size * 2;
		ClientArray *newArray = realloc(clientArray, new_size * sizeof *clientArray);
		if(!newArray) {
			fprintf(stderr, "Error to resize ClientArray to %zu", new_size);
			exit(EXIT_FAILURE);
		}
		clientArray = newArray;
		clientArray->size = new_size;
	}

	clientArray->clientData[clientArray->used++] = newClient;
	
	return &newClient;
}

ClientData * get_client(ClientArray *clientArray, unsigned int ip, unsigned short int port) {
	for (int i = 0; i < clientArray->used; i++) {
		if(clientArray->clientData[i].clientId.ip == ip && clientArray->clientData[i].clientId.port == port) {
			return &(clientArray->clientData[i]);
		}
	}

	ClientData newClient;
	newClient.clientId.ip = ip;
	newClient.clientId.port = port;

	return add_client(&clientArray, newClient);
}

int receive(int s, unsigned short int port, ClientArray *clientArray, FILE *f) {
	char datagram[1024];

	printf("receiving data from port %d\n", port);

	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	int received = recvfrom(s, datagram, sizeof(datagram), 0, (struct sockaddr *) &addr, &addrlen);
	if (received <= 0) { // Ошибка считывания полученной дейтаграммы
		sock_err("recvfrom", s);
		return 0;
	}
	
	unsigned int ip = ntohl(addr.sin_addr.s_addr);

	printf("received %d bytes from client %d\n", received, ip);

	ClientData *current_client = get_client(&clientArray, ip, port);

	//TODO process messages
}

void init_client_array(ClientArray *ca) {
	ca->size = 5;
	ca->used = 0;
	ca->clientData = malloc(ca->size * sizeof ca->clientData);
}

void start_receiving(unsigned short int start_port, unsigned short int finish_port, FILE *f) {
	int port_count = finish_port - start_port;

	SocketData socketData;
	init_sockets(start_port, finish_port, &socketData);

	ClientArray clientArray;
	init_client_array(&clientArray);

	int stop_flag = 0;
	struct timeval tv = { 1, 0 };

	do {
		// prepare data for select
		fd_set rds;
		FD_ZERO(&rds);
		for (int i = 0; i < port_count; i++) {
			FD_SET(socketData.sockets[i], &rds);
		}

		// check sockets
		if (select(socketData.maxSocket + 1, &rds, 0, 0, &tv) > 0) {
			for (int i = 0; i < port_count; i++) {
				if (FD_ISSET(socketData.sockets[i], &rds)) {
					int res = receive(socketData.sockets[i], start_port + i, &clientArray, f);
					if (res > 0) {
						stop_flag = 1;
					}
				}
			}
		}
	} while (stop_flag == 0);

}

int main(int argc, char const *argv[]) {
	unsigned short int start_port, finish_port;
	FILE *f;

	static const char FILE_NAME[] = "msg.txt";

	//Check args count
	if (argc != 3) {
		fprintf(stderr, "Usage: %s start_port finish_port\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Check start port
	if ((start_port = strtoul(argv[1], NULL, 10)) == 0) {
		fprintf(stderr, "error: Cannot find start_port %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	// Check port
	if ((finish_port = strtoul(argv[2], NULL, 10)) == 0) {
		fprintf(stderr, "error: Cannot find finish_port %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	if (start_port >= finish_port) {
		fprintf(stderr,
				"error: Start port must be less than finish port!, %hu - %hu\n",
				start_port, finish_port);
		exit(EXIT_FAILURE);
	}

	//open output file
	if ((f = fopen(FILE_NAME, "w")) == NULL) {
		fprintf(stderr, "error: Cannot open %s\n", FILE_NAME);
		exit(EXIT_FAILURE);
	}

	start_receiving(start_port, finish_port, f);

	return EXIT_SUCCESS;
}
