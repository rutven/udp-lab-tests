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
//#include <string.h>
#include <stdlib.h>

#define LINE_SIZE 1048576

int main(int argc, char *argv[])
{
    char *host, *port, *str_ptr;
    FILE *f;

    //Check args count
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s host:port filename\n", argv[0]);
        exit(1);
    }

    // Check host
    if ((host = (char *)strtok_r(argv[1], ":", &str_ptr)) == NULL)
    {
        fprintf(stderr, "error: Cannot find host %s\n", argv[1]);
        exit(1);
    }
    // Check port
    if ((port = (char *)strtok_r(NULL, ":", &str_ptr)) == NULL)
    {
        fprintf(stderr, "error: Cannot find port %s\n", argv[1]);
        exit(1);
    }

    // Open file for read
    if ((f = fopen(argv[2], "r")) == NULL)
    {
        fprintf(stderr, "error: Cannot open %s\n", argv[2]);
        exit(1);
    }

    // Init nerwork
    init();

    // Create UDP socket
    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0)
        return sock_err("socket", s);

    struct sockaddr_in host_address;

    // Заполнение структуры с адресом удаленного узла
    memset(&host_address, 0, sizeof(host_address));
    host_address.sin_family = AF_INET;
    host_address.sin_port = htons(strtol(port, (char **)NULL, 10));
    host_address.sin_addr.s_addr = get_host_ip(host);

	char line_buffer[LINE_SIZE];
    __STRING
    int line_count = 0;

    //Чтение файла с сообщениями
	while (fgets(line_buffer, LINE_SIZE, f)) {
		line_count++;
		if (sscanf(line_buffer,"%u %[^\n]", message) <= 0) {
//			fprintf(stderr, "skip line %d: error format\n",line);
			continue;
		}
//		fprintf(stdout,"%u %02u:%02u:%02u %02u:%02u:%02u %u %s\n",line,H1,M1,S1,H2,M2,S2,bbb,message);
		if ( bbb > 4294967295 ) {
//			fprintf(stderr, "skip line %d: bbb > 4294967295\n",line);
			continue;
		}



    // Установка соединения с удаленным хостом
    int i = 0;
    while (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        i++;
        if (i > 10)
        {
            fprintf(stderr, "cannot connect %s:%s\n", host, port);
            s_close(s);
            return sock_err("connect", s);
        }
        usleep(100000);
    }
#ifdef _WIN32
    int flags = 0;
#else
    int flags = MSG_NOSIGNAL;
#endif

    /* code */
    return 0;
}

int init()
{
#ifdef _WIN32
    // Для Windows следует вызвать WSAStartup перед началом использования сокетов
    WSADATA wsa_data;
    return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
#else
    return 1; // Для других ОС действий не требуется
#endif
}

int sock_err(const char *function, int s)
{
    int err;
#ifdef _WIN32
    err = WSAGetLastError();
#else
    err = errno;
#endif
    fprintf(stderr, "%s: socket error: %d\n", function, err);
    return -1;
}

// Функция определяет IP-адрес узла по его имени.
// Адрес возвращается в сетевом порядке байтов.
unsigned int get_host_ip(const char *name)
{
    struct addrinfo *addr = 0;
    unsigned int ip4addr = 0;

    // Функция возвращает все адреса указанного хоста
    // в виде динамического однонаправленного списка
    if (0 == getaddrinfo(name, 0, 0, &addr))
    {
        struct addrinfo *current_addr = addr;
        while (current_addr)
        {
            // Интересует только IPv4 адрес, если их несколько - то первый
            if (current_addr->ai_family == AF_INET)
            {
                ip4addr = ((struct sockaddr_in *)current_addr->ai_addr)->sin_addr.s_addr;
                break;
            }
            current_addr = current_addr->ai_next;
        }
        freeaddrinfo(addr);
    }
    return ip4addr;
}
