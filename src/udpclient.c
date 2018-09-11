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
#include <string.h>

#define MESSAGE_LENGTH 1024
#define BUFFER_SIZE 2048

typedef struct
{
    char hour;
    char minute;
    char second;
} MessageTime;

typedef struct
{
    unsigned int number;
    MessageTime first_time;
    MessageTime second_time;
    unsigned long bbb;
    char message[MESSAGE_LENGTH];
    char sended;
} Message;

typedef struct
{
    Message *messages;
    size_t size;
    size_t used;
} MessageArray;

void add_message(MessageArray *ma, Message msg)
{
    if (ma->used == ma->size)
    {
        ma->size *= 2;
        ma->messages = (Message *)realloc(ma->messages, ma->size * sizeof(Message));
    }

    ma->messages[ma->used++] = msg;
}

void load_messages(MessageArray *ma, FILE *f)
{
    char buffer[BUFFER_SIZE];
    unsigned int H1, M1, S1, H2, M2, S2;
    unsigned int bbb;
    char message[MESSAGE_LENGTH];
    unsigned int message_count = 0;
    Message msg;

    while (fgets(buffer, BUFFER_SIZE, f))
    {
        if (sscanf(buffer, "%2u:%2u:%2u %2u:%2u:%2u %u %[^\n]", &H1, &M1, &S1, &H2, &M2, &S2, &bbb, message) <= 0)
        {
            continue;
        }

        if (bbb > 4294967295)
        {
            continue;
        }

        msg.number = htonl(message_count++);
        msg.first_time.hour = (char)H1;
        msg.first_time.minute = (char)M1;
        msg.first_time.second = (char)S1;
        msg.second_time.hour = (char)H2;
        msg.second_time.minute = (char)M2;
        msg.second_time.second = (char)S2;
        msg.bbb = htonl(bbb);
        strcpy(msg.message, message);
        msg.sended = 0;

        add_message(ma, msg);
    }
}

void init_messages(MessageArray *a, size_t initial_size)
{
    a->size = initial_size;
    a->messages = (Message *)malloc(a->size * sizeof(Message));
    a->used = 0;
}

void free_messages(MessageArray *a)
{
    free(a->messages);
    a->messages = NULL;
    a->used = a->size = 0;
}

int serialize_message(Message msg, char *datagram)
{
    datagram[0] = (msg.number >> 24) & 0xFF;
    datagram[1] = (msg.number >> 16) & 0xFF;
    datagram[2] = (msg.number >> 8) & 0xFF;
    datagram[3] = msg.number & 0xFF;
    datagram[4] = msg.first_time.hour;
    datagram[5] = msg.first_time.minute;
    datagram[6] = msg.first_time.second;
    datagram[7] = msg.second_time.hour;
    datagram[8] = msg.second_time.hour;
    datagram[9] = msg.second_time.hour;
    datagram[10] = (msg.bbb >> 24) & 0xFF;
    datagram[11] = (msg.bbb >> 16) & 0xFF;
    datagram[12] = (msg.bbb >> 8) & 0xFF;
    datagram[13] = msg.bbb & 0xFF;

    for (int i = 0; i <= strlen(msg.message); i++)
    {
        datagram[14 + i] = msg.message[i];
    }

    return 14 + strlen(msg.message) + 1;
}

int calc_sended(MessageArray *ma)
{
    int count = 0;
    for (int i = 0; i < ma->used; i++)
    {
        if (ma->messages[i].sended > 0)
        {
            count++;
        }
    }
    return count;
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

long get_number(char *buffer, int i, size_t length)
{
    unsigned long value = 0;

    if (i + 3 < length)
    {
        value |= buffer[i + 0] << 24;
        value |= buffer[i + 1] << 16;
        value |= buffer[i + 2] << 8;
        value |= buffer[i + 3];
        return ntohl(value);
    }
    else
    {
        return -1;
    }
}

int receive(MessageArray *ma, int s)
{
    char datagram[1024];
    struct timeval tv = {0, 100 * 1000}; // 100 msec

    int res;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);

    // Проверка - если в сокете входящие дейтаграммы // (ожидание в течение tv)
    res = select(s + 1, &fds, 0, 0, &tv);
    //printf("checking socket - %d\n", res);
    if (res > 0)
    { // Данные есть, считывание их
        //printf("have some data in the socket\n");
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);

        int received = recvfrom(s, datagram, sizeof(datagram), 0, (struct sockaddr *)&addr, &addrlen);
        if (received <= 0)
        { // Ошибка считывания полученной дейтаграммы
            sock_err("recvfrom", s);
            return 0;
        }
        printf("received %d bytes from server\n", received);

        int i = 0;
        do
        {
            long msg_number = get_number(datagram, i, received);
            if (msg_number < 0)
            {
                break;
            }

            if (msg_number < ma->used)
            {
                printf("message number %ld confirmed\n", msg_number);
                ma->messages[msg_number].sended = 1;
            } else {
                printf("unknown message number - %ld \n", msg_number);
            }
            i++;
        } while (1 == 1);
        return 1;
    }
    else if (res == 0)
    { // Данных в сокете нет, возврат ошибки
        return 0;
    }
    else
    {
        sock_err("select", s);
        return 0;
    }
}

int main(int argc, char *argv[])
{
    MessageArray ma;
    FILE *f;

    char *host, *port, *str_ptr;

    //Check args count
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s host:port filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Check host
    if ((host = strtok(argv[1], ":")) == NULL)
    {
        fprintf(stderr, "error: Cannot find host %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // Check port
    if ((port = strtok(NULL, ":")) == NULL)
    {
        fprintf(stderr, "error: Cannot find port %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // Open file for read
    if ((f = fopen(argv[2], "r")) == NULL)
    {
        fprintf(stderr, "error: Cannot open %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    init_messages(&ma, 1);
    load_messages(&ma, f);

    printf("loaded %lu messages\n", ma.used);

    if (ma.used > 0)
    {
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

        // working
        printf("start loop\n");
        do
        {
            // sending
            int msg_count = 0;
            int i = 0;
            do
            {
                if (ma.messages[i].sended == 0)
                {
                    char datagram[1024];
                    int datagram_size = serialize_message(ma.messages[i], datagram);
                    sendto(s, datagram, datagram_size, 0, (struct sockaddr *)&host_address, sizeof(host_address));
                    printf("send message %d\n", i);
                    msg_count++;
                }
                i++;
            } while (msg_count < 10 && i < ma.used);

            printf("start receiving\n");
            //receiving
            int res = 0;
            do
            {
                res = receive(&ma, s);
            } while (res > 0);

            int send_count = calc_sended(&ma);
            printf("sended %d messages\n", send_count);
            if (send_count >= 20 || send_count == ma.used)
            {
                break;
            }
        } while (1 == 1);

        return 0;
    }
}
