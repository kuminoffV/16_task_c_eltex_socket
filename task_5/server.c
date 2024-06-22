#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define error_func(a) do{if(-1 == a){ printf("line:%d\n", __LINE__); \
                                        perror("error"); exit(EXIT_FAILURE);}} while(0)

#define AMOUNT_MATH_SERV 10 
#define SUM 10

//функция, которая будет делить числа на два
void *math_server(void *arg);

//- количетсво потоков поддерживаемые сервером
pthread_t index_pthread[AMOUNT_MATH_SERV];

//идентификатор свободного сервера
int index_free_serv = 0;

//начальный порт потока
int port_serv[10][2] = {{7780 + SUM,0},{7781 + SUM,0},{7782 + SUM,0},{7783 + SUM,0},{7784 + SUM,0},{7785 + SUM,0},{7786 + SUM,0},{7787 + SUM,0},{7788 + SUM,0},{7789 + SUM,0}};
int port_thread = 0;

//мьютекст для обработки клиентов
pthread_mutex_t mutex_clients = PTHREAD_MUTEX_INITIALIZER;

int main(void){

    struct sockaddr_in serv;
    struct sockaddr_in client;

    //дескриптор сокета
    int fd_sock;

    //роверка правильности завершения программы
    int status;
    ssize_t amount_byte;

    //размер фукнции accept()
    socklen_t len;

    //создаем сокет
    fd_sock = socket(AF_INET, SOCK_STREAM, 0);
        error_func(fd_sock);

    //индекс циклов
    int i;

    //дескрипторы клиентов
    int invite_fd;

    //дескриптор обрабатывающего сервера
    int fd_math_serv;

    //наш сервер
    serv.sin_family = AF_INET;
    serv.sin_port = htons(3456);
    serv.sin_addr.s_addr = inet_addr("127.0.0.1");

    status = bind(fd_sock, (struct sockaddr *)&serv, sizeof(serv));
        error_func(status);
    
    printf("ФСБ начинает прослушивать клиентов\n");

    //очередь для подключения клиентов
    status = listen(fd_sock, 5);
        error_func(status);
    
    len = sizeof(client);

    while(1){

        sleep(1);

        //- ждем присоединения клиентов
        invite_fd = accept(fd_sock, (struct sockaddr *)&client, &len);
            error_func(invite_fd);

        printf("Новый клиент\n");

        //обрабатываем взаимодействие данныхс потоками
        pthread_mutex_lock(&mutex_clients);

        fd_math_serv = -1;

        //обрабатывающий сокет
        fd_math_serv = socket(AF_INET, SOCK_STREAM, 0);
            error_func(fd_math_serv);
            
        //обрабатывающий сервер
        serv.sin_family = AF_INET;

        //присваиваем свободный порт
        for(i = 0; i < AMOUNT_MATH_SERV; i++){
            if(0 == port_serv[i][1]){
                serv.sin_port = htons(port_serv[i][0]);
                printf("Порт %d\n",htons(port_serv[i][0]));
                port_serv[i][1] = 1;
                break;
            }
        }
        serv.sin_addr.s_addr = inet_addr("127.0.0.1");

        //новый сервер для обслуживания клиентов
        status = bind(fd_math_serv, (struct sockaddr *)&serv, sizeof(serv));
            error_func(status);

        //очередь для обрабатывающего сервера
        status = listen(fd_math_serv, 1);
            error_func(status);

        //поток для обслуживания клиентов
        pthread_create(&index_pthread[index_free_serv], NULL, math_server, (void *)&fd_math_serv);

        //отправляем клиенту данные об обрабатывающем сервере
        amount_byte = send(invite_fd, (void *)&serv, sizeof(serv), 0);
            error_func(amount_byte);

        //счетчик клиентов
        index_free_serv++;

        pthread_mutex_unlock(&mutex_clients);
    }

    for(i = 0; i < AMOUNT_MATH_SERV; i++){
        pthread_join(index_pthread[index_free_serv], NULL);
    }

    close(fd_sock);

    exit(EXIT_SUCCESS);
}

void *math_server(void *arg){

    //дескриптор обрабатывающего сервера
    int fd_math_serv = *((int *)arg);

    //число, которое нужно будет разделить на два
    int num;

    //индекс для циклов
    int i = 0;

    //проверка ошибок
    ssize_t amount_byte;

    //структуры для сетевого общения
    struct sockaddr_in serv;
    struct sockaddr_in client;

    //фиксирование ошибок
    int status;

    //дескриптор клиента
    int invite_fd;

    // размер фукнции accept()
    socklen_t len;
    //- для высвобождения порта;
    int reuse = 1;

    len = sizeof(client);

    pthread_mutex_lock(&mutex_clients);

    //готов принимать клиентов
    invite_fd = accept(fd_math_serv, (struct sockaddr *)&client, &len);
            error_func(invite_fd);

    port_thread++;

    pthread_mutex_unlock(&mutex_clients);

    while(1){
        amount_byte = recv(invite_fd, (void *)&num, sizeof(num), 0);
            error_func(amount_byte);

        if(-1 == num){
            break;
        }
            
        printf("Клиент прислал: %d\n", num);

        num = num / 2;

        amount_byte = send(invite_fd, (void *)&num, sizeof(num), 0);
        error_func(amount_byte);
    }

    pthread_mutex_lock(&mutex_clients);

    port_serv[i][1] = 0;

    close(invite_fd);
    close(fd_math_serv);
    setsockopt(fd_math_serv, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));


    index_pthread[i] = index_pthread[index_free_serv - 1];
    index_pthread[index_free_serv - 1] = 0;
    
    index_free_serv--;

    pthread_mutex_unlock(&mutex_clients);
}
