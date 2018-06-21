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

        msg.number = htonl(++message_count);
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

void print_message(Message msg)
{
    printf("Message - %u %u:%u:%u %u:%u:%u %u %s\n", ntohl(msg.number),
           msg.first_time.hour, msg.first_time.minute, msg.first_time.second,
           msg.second_time.hour, msg.second_time.minute, msg.second_time.second,
           ntohl(msg.bbb), msg.message);
}

int main(int argc, char *argv[])
{
    MessageArray ma;
    FILE *f;

    // Open file for read
    if ((f = fopen(argv[1], "r")) == NULL)
    {
        fprintf(stderr, "error: Cannot open %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    init_messages(&ma, 1);

    load_messages(&ma, f);

    printf("Loaded %lu messages\n", ma.used);

    for (int i = 0; i < ma.used; i++)
    {
        print_message(ma.messages[i]);
    }

    fclose(f);
    free_messages(&ma);
    exit(EXIT_SUCCESS);
}
