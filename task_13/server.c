#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
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
    char buf[200];

    //размер
    socklen_t len;

    //сокет
    fd_sock = socket(AF_INET, SOCK_DGRAM, 0);
        error_func(fd_sock);

    //наш сервер
    serv.sin_family = AF_INET;
    serv.sin_port = htons(3457);
    serv.sin_addr.s_addr = inet_addr("127.0.0.1");

    status = bind(fd_sock, (struct sockaddr *)&serv, sizeof(serv));
        error_func(status);
    
    printf("Создали сервер, ждем движения\n");

    while(1){

        memset(&client,0,sizeof(client));
        memset(&buf,0,sizeof(buf));

        len = sizeof(client);

        amount_byte = recvfrom(fd_sock, (void *)&buf, sizeof(buf), 0, (struct sockaddr *)&client, &len);
            error_func(amount_byte);

        if(0 == client.sin_family){
            printf("Ошибка передачи\n");
            break;
        }

        if(!strcmp(buf,"exit\f")){
            break;
        }
        
        printf("Клиент прислал: %s\n", buf);
        printf("Отвечаем ему: окай\n");

        strcpy(buf, "okey");

        amount_byte = sendto(fd_sock, (void *)&buf, sizeof(buf), 0, (struct sockaddr *)&client, len);
            error_func(amount_byte);
    }

    close(fd_sock);

    exit(EXIT_SUCCESS);
}
