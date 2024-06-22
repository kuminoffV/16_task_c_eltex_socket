#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PATH_SOCKET "/path_serv_forever"

#define error_func(a) do{if(-1 == a){ printf("line:%d\n", __LINE__); \
                                        perror("error"); exit(EXIT_FAILURE);}} while(0)

int main(void){

    struct sockaddr_un serv;
    struct sockaddr_un client;

    //дескриптор сокета
    int fd_sock;

    //для проверки правильности завершения программы
    int status;
    ssize_t amount_byte;

    //дескрипторы клиентов
    int fd_clients;
    char buf[100];

    //размер
    socklen_t len;

    //cоздаем сокет
    fd_sock = socket(AF_LOCAL, SOCK_DGRAM, 0);
        error_func(fd_sock);

    //наш сервер
    serv.sun_family = AF_LOCAL;
    strcpy(serv.sun_path, PATH_SOCKET);

    status = bind(fd_sock, (struct sockaddr *)&serv, sizeof(serv));
        error_func(status);
    
    printf("Сервер создан, ждем движухи\n");
      
    while(1){

        memset(&client,0,sizeof(client));

        len = sizeof(client);

        amount_byte = recvfrom(fd_sock, (void *)&buf, sizeof(buf), 0, (struct sockaddr *)&client, &len);
            error_func(amount_byte);

        if(0 == client.sun_family){
            printf("Ошибка передачи\n");
            break;
        }

        if(!strcmp(buf,"exit")){
            break;
        }
        
        printf("Клиент пишет: %s\n", buf);
        printf("Отвечаем ему: окай\n");

        strcpy(buf, "okey");

        amount_byte = sendto(fd_sock, (void *)&buf, sizeof(buf), 0, (struct sockaddr *)&client, len);
            error_func(amount_byte);
    }

    close(fd_sock);

    unlink(PATH_SOCKET);

    exit(EXIT_SUCCESS);
}
