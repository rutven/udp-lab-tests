#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/time.h> 
#include <netdb.h> 
#include <errno.h> 
#include <fcntl.h> 
#include <sys/epoll.h> 
#include <string.h> 
#include <stdio.h>

#define MAXEVENTS_PER_CALL (16)
#define MAX_CLIENTS (256)

struct client_ctx { 
    int socket; // Дескриптор сокета 
    unsigned char in_buf[512]; // Буфер принятых данных 
    int received; // Принято данных 
    unsigned char out_buf[512]; // Буфер отправляемых данных 
    int out_total; // Размер отправляемых данных 
    int out_sent; // Данных отправлено 
};

// Массив структур, хранящий информацию о подлкюченных клиентах 
struct client_ctx g_ctxs[1 + MAX_CLIENTS];

int set_non_block_mode(int s) { 
    #ifdef _WIN32 
    unsigned long mode = 1; 
    return ioctlsocket(s, FIONBIO, &mode); 
    #else 
    int fl = fcntl(s, F_GETFL, 0); 
    return fcntl(s, F_SETFL, fl | O_NONBLOCK); 
    #endif 
}


int create_listening_socket() { 
    struct sockaddr_in addr;
    
    int s = socket(AF_INET, SOCK_STREAM, 0); 
    if (s <= 0) 
        return s;

    set_non_block_mode(s);
    memset(&addr, 0, sizeof(addr)); 
    addr.sin_family = AF_INET; 
    addr.sin_port = htons(9000);
    if (bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0 || listen(s, 1) < 0) { 
        printf("error bind() or listen()\n"); 
        return -1; 
    }

    printf("Listening: %hu\n", ntohs(addr.sin_port));
    return s; 
}

void epoll_serv() { 
    int s; 
    int epfd; 
    struct epoll_event ev; 
    struct epoll_event events[MAXEVENTS_PER_CALL];

    memset(&g_ctxs, 0, sizeof(g_ctxs)); // Создание прослушивающего сокета 
    s = create_listening_socket(); 
    if (s <= 0) { 
        printf("socket create error: %d\n", errno); 
        return; 
    }
    // Создание очереди событий 
    epfd = epoll_create(1); 
    if (epfd <= 0) return;
    
    // Добавление прослушивающего сокета в очередь событий 
    ev.events = EPOLLIN; 
    ev.data.fd = 0; // В data будет храниться 0 для прослушивающего сокета. 
    
    // Для других сокетов - индекс в массиве ctxs 
    if ( 0 != epoll_ctl(epfd, EPOLL_CTL_ADD, s, &ev)) { 
        printf("epoll_ctl(s) error: %d\n", errno); 
        return; 
    }

    // Бесконечный цикл обработки событий из очереди epfd 
    while(1) { 
        int i, events_cnt;
        // Получение поступивших событий из очереди в теч. 1 секунды 
        events_cnt = epoll_wait(epfd, events, MAXEVENTS_PER_CALL, 1000);
        if (events == 0) { 
            // Никаких событий нет, программа может выполнить другие операции // ... 
        } 
        
        for(i = 0; i < events_cnt; i++) { 
            struct epoll_event* e = &events[i]; 
            if (e->data.fd == 0 && (ev.events & EPOLLIN)) { // Поступило подключение на прослушивающий сокет, принять его 
            struct sockaddr_in addr; 
            int socklen = sizeof(addr); 
            int as = accept(s, (struct sockaddr*) &addr, &socklen);
            if (as > 0) { 
                int j; 
                for(j = 1; j < MAX_CLIENTS; j++) { 
                    if (g_ctxs[j].socket == 0) { // Слот свободен, можно занять сокетом 
                        memset(&g_ctxs[j], 0, sizeof(g_ctxs[j])); 
                        g_ctxs[j].socket = as; 
                        break; 
                    }
                }
                     
                if (j != MAX_CLIENTS) { // Регистрация сокета клиента в общей очереди событий 
                    unsigned int ip = ntohl(addr.sin_addr.s_addr); 
                    set_non_block_mode(as); 
                    ev.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP;
                    ev.data.fd = j; 
                    epoll_ctl(epfd, EPOLL_CTL_ADD, as, &ev);
                    printf(" New client connected: %u.%u.%u.%u: %d\n", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, (ip) & 0xff, j); 
                } else { // Нет свободных слотов, отключить клиента 
                    close(as); 
                } 
            } 
        }

        if (e->data.fd > 0) { 
            int idx = e->data.fd; 
            if ( (e->events & EPOLLHUP) || (e->events & EPOLLRDHUP) || (e->events & EPOLLERR) ) { // Клиент отключился (или иная ошибка) => закрыть сокет, освободить запись о клиенте 
                close(g_ctxs[idx].socket); 
                memset(&g_ctxs[idx], 0, sizeof(g_ctxs[idx])); 
                printf(" Client disconnect: %d (err or hup)\n", idx); 
            } else if ((e->events & EPOLLIN) && (g_ctxs[idx].out_total == 0) && (g_ctxs[idx].received < sizeof(g_ctxs[idx].in_buf)) ) { // Пришли новые данные от клиента => если еще нет результата - то принять данные, найти результат и отправить его 
                int r = recv(g_ctxs[idx].socket, g_ctxs[idx].in_buf + g_ctxs[idx].received, sizeof(g_ctxs[idx].in_buf) - g_ctxs[idx].received, 0 ); 
                if (r > 0) { // Данные приняты, будет проведен анализ 
                    int k; 
                    int len = -1; 
                    g_ctxs[idx].received += r; 
                    for(k = 0; k < g_ctxs[idx].received; k++) { 
                        if (g_ctxs[idx].in_buf[k] == '\n') { 
                            len = k; 
                            break; 
                        } 
                    }
                    
                    if (len == -1 && k == sizeof(g_ctxs[idx].in_buf)) len = k;
                    if (len != -1) { // Строка получена, форматирование ответа и отправка 
                        sprintf(g_ctxs[idx].out_buf, "Your string length: %d\n", len); 
                        g_ctxs[idx].out_total = strlen(g_ctxs[idx].out_buf); 
                        g_ctxs[idx].out_sent = 0; 
                        r = send(g_ctxs[idx].socket, g_ctxs[idx].out_buf, g_ctxs[idx].out_total, MSG_NOSIGNAL); 
                        if (r > 0) g_ctxs[idx].out_sent += r;
                    } // Иначе - продолжаем ждать данные от клиента 
                } else { // Клиент отключился, либо возникла иная ошибка, закрыть соединение 
                    close(g_ctxs[idx].socket); 
                    memset(&g_ctxs[idx], 0, sizeof(g_ctxs[idx])); 
                    printf(" Client disconnect: %d (read error)\n", idx); 
                } 
            } else if ((e->events & EPOLLOUT) && (g_ctxs[idx].out_total > 0)) { // Сокет стал готов к отправке данных => если не все данные переданы - передать. Если все данные были переданы, сокет можно закрыть, клиента отключить 
                if (g_ctxs[idx].out_sent < g_ctxs[idx].out_total) { 
                    int r = send(g_ctxs[idx].socket, g_ctxs[idx].out_buf + g_ctxs[idx].out_sent, g_ctxs[idx].out_total - g_ctxs[idx].out_sent, MSG_NOSIGNAL); 
                    if (r > 0) g_ctxs[idx].out_sent += r; 
                }
                if (g_ctxs[idx].out_sent >= g_ctxs[idx].out_total) { 
                    printf(" Response has been sent: %d\n", idx); 
                    close(g_ctxs[idx].socket); 
                    memset(&g_ctxs[idx], 0, sizeof(g_ctxs[idx])); 
                    printf(" Client disconnect: %d (all data sent)\n", idx); 
                } 
            } 
        } 
    } 
} 
}


int main() { 
    epoll_serv(); 
    return 0; 
}