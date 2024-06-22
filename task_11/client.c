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

struct head_udp {
    short src_port;
    short dest_port;
    short length_sms;
    short checksum;
};


int main(void){

    struct sockaddr_in serv, client;

    //дескриптор сокета
    int fd_serv, fd_client;

    //проверка возвращаемых функций
    int status;
    ssize_t amount_byte;

    //заголовок udp с данными
    struct head_udp *head_udp;

    //для sendto() и recvfrom()
    socklen_t len;
    len = sizeof(client);

    //буфер, в котором храняться отправленные и принимаемые пакеты
    char buffer[200];

    //указатель, для перемещения по пакету
    char *mover;

    fd_serv = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);

    //адрес отправления
    serv.sin_family = AF_INET;
    serv.sin_port = htons(3457);
    serv.sin_addr.s_addr = inet_addr("127.0.0.1");

    while(1){

        sleep(1);
        memset(buffer, 0, sizeof(buffer));

        //указатель на заголовок
        head_udp = (struct head_udp*)buffer;

        //переместимся к данным за заголовком
        mover = buffer + sizeof(head_udp);

        printf("Напишем серверу: ");
        fgets(mover, 100, stdin);
        mover[strcspn(mover, "\n")] = '\0';

        head_udp->src_port = ntohs(1456);
        head_udp->dest_port = ntohs(3457);
        head_udp->length_sms = ntohs(sizeof(head_udp) + strlen(mover));
        head_udp->checksum = 0;

        amount_byte = sendto(fd_serv, (void *)buffer, sizeof(head_udp) + strlen(mover), 0, (struct sockaddr*)&serv, len);
            error_func(amount_byte);

        if(!strcmp(mover,"exit")){
            break;
        }

        //указатель на заголовок III уровня (не забываем о том, что на IV добавляесят заголовок)
        head_udp = (struct head_udp*)(buffer + 20);

        //указатель на данные
        //переместимся к данным за заголовок
        mover = (buffer + 20) + sizeof(head_udp);

        while(1){
            //ждем ответ от сервера
            amount_byte = recvfrom(fd_serv, (void *)&buffer, sizeof(buffer), 0, (struct sockaddr*)&serv, &len);
            error_func(amount_byte);
        
            if(0 == amount_byte){
                break;
            } else if (3457 == ntohs(head_udp->src_port)){
                //проверка ответа от сервера
                break;
            }
        }

        printf("Ответ сервера: %s\n", mover);
    }

    close(fd_serv);

    exit(EXIT_SUCCESS);
}
