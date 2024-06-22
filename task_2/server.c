#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define error_func(a) do{if(-1 == a){ printf("line:%d\n", __LINE__); \
                                        perror("error"); exit(EXIT_FAILURE);}} while(0)

int main(void){

    struct sockaddr_in serv;
    struct sockaddr_in client;

    //дескриптор сокета
    int fd_sock;

    //проверка правильности завершения программы
    int status;
    ssize_t amount_byte;

    //дескрипторы клиентов
    int fd_clients;
    char buf[100] = {0};

    //размер
    socklen_t len;

    //создаем сокет
    fd_sock = socket(AF_INET, SOCK_STREAM, 0);
        error_func(fd_sock);

    //наш сервер;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(3457);
    serv.sin_addr.s_addr = inet_addr("127.0.0.1");

    status = bind(fd_sock, (struct sockaddr *)&serv, sizeof(serv));
        error_func(status);
    
    printf("ФСБ начинает прослушивать клиентов\n");

    status = listen(fd_sock, 5);
        error_func(status);
    
    len = sizeof(client);
    fd_clients = accept(fd_sock, (struct sockaddr *)&client, &len);
        error_func(fd_clients);

    printf("Клиент присоединился к серверу\n");

    while(1){

        amount_byte = recv(fd_clients, (void *)&buf, sizeof(buf), 0);
            error_func(amount_byte);

        if(!strcmp(buf,"exit")){
            break;
        }
        
        printf("Клиент пишет: %s", buf);
        printf("Отвечаем ему: окай\n");

        strcpy(buf, "okey");

        amount_byte = send(fd_clients, (void *)buf, sizeof(buf), 0);
            error_func(amount_byte);
    }

    close(fd_clients);
    close(fd_sock);

    exit(EXIT_SUCCESS);
}
