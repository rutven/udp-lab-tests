#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include < windows.h >
#include < winsock2.h >
// Директива линковщику: использовать библиотеку сокетов
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
    struct in_addr ip;
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



int init() {
#ifdef _WIN32
	// Для Windows следует вызвать WSAStartup перед началом использования сокетов
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
#else
	return 1; // Для других ОС действий не требуется
#endif
}

int * init_sockets(unsigned short int start_port, unsigned short int finish_port) {

	int ports_count = finish_port - start_port;

	printf("port count - %d", ports_count);

	int *sockets = malloc(ports_count * sizeof *sockets);

	//init network
	init();

	for (int i = 0; i < ports_count; i++) {
		int s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0) {
			fprintf(stderr, "Socket init error for port %d!", start_port + i);
			exit(EXIT_FAILURE);
		} else {
			sockets[i] = s;
		}
	}

	return sockets;
}

int receive(int s, ClientArray *clientArray, FILE *f) {
	char datagram[1024];
	struct timeval tv = { 0, 10 * 1000 }; // 10 msec

	int res;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(s, &fds);

	// Проверка - если в сокете входящие дейтаграммы // (ожидание в течение tv)
	res = select(s + 1, &fds, 0, 0, &tv);
	//printf("checking socket - %d\n", res);
	if (res > 0) { // Данные есть, считывание их
				   //printf("have some data in the socket\n");
		struct sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);

		int received = recvfrom(s, datagram, sizeof(datagram), 0,
				(struct sockaddr *) &addr, &addrlen);
		if (received <= 0) { // Ошибка считывания полученной дейтаграммы
			sock_err("recvfrom", s);
			return 0;
		}
		printf("received %d bytes from server\n", received);

		int i = 0;
		do {
			long msg_number = get_number(datagram, i, received);
			if (msg_number < 0) {
				break;
			}

			if (msg_number < ma->used) {
				printf("message number %ld confirmed\n", msg_number);
				ma->messages[msg_number].sended = 1;
			}
			i++;
		} while (1 == 1);
		return 1;
	} else if (res == 0) { // Данных в сокете нет, возврат ошибки
		return 0;
	} else {
		sock_err("select", s);
		return 0;
	}
}

void start_receiving(unsigned short int start_port, unsigned short int finish_port, FILE *f) {
	int port_count = finish_port - start_port;

	int *sockets = init_sockets(start_port, finish_port);

	ClientArray clientsData;
	int stop_flag = 0;

	while (stop_flag == 0) {
		for (int i = 0; i < port_count; i++  ) {
			int result = receive(sockets[i], &clientsData, &f);
			if (result == 1) {
				stop_flag = 1;
			}
		}
	}

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

	start_receiving(start_port, finish_port, &f);

	return EXIT_SUCCESS;
}
