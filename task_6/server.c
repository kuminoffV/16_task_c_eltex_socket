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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>


#define error_func(a) do{if(-1 == a){ printf("line:%d\n", __LINE__); \
                                        perror("error"); exit(EXIT_FAILURE);}} while(0)

#define AMOUNT_MATH_SERV 10 
#define NAME_FILE_KEY "key_sem"
#define MAIN_PORT 3452

//функция, которая будет делить числа на два
void *math_server(void *arg);

//переменные для потоков, которые хотят взаимодействовать с клиентом
pthread_cond_t condition[AMOUNT_MATH_SERV] = {
                                                PTHREAD_COND_INITIALIZER,
                                                PTHREAD_COND_INITIALIZER,
                                                PTHREAD_COND_INITIALIZER,
                                                PTHREAD_COND_INITIALIZER,
                                                PTHREAD_COND_INITIALIZER,
                                                PTHREAD_COND_INITIALIZER,
                                                PTHREAD_COND_INITIALIZER,
                                                PTHREAD_COND_INITIALIZER,
                                                PTHREAD_COND_INITIALIZER,
                                                PTHREAD_COND_INITIALIZER
};  

//дескрипторы клиентов
int fd_clients[AMOUNT_MATH_SERV] = { 0 };

//количетсво потоков поддерживаемые сервером
pthread_t index_pthread[AMOUNT_MATH_SERV] = { 0 };

//дескрипторы потоков, обрабатывающие клиентов
int fd_math_serv[AMOUNT_MATH_SERV] = { 0 };

//идентификатор свободного сервера
int index_free_serv = 0;

//начальный порт
int port_serv[AMOUNT_MATH_SERV][2] = {{5689,0},{5690,0},{5691,},{5692,0},{5693,0},{5694,0},{5695,0},{5696,0},{5697,0},{5698,0}};

//мьютекст по обработки клиентов
pthread_mutex_t mutex_clients =  PTHREAD_MUTEX_INITIALIZER;

int main(void){

    struct sockaddr_in serv;
    struct sockaddr_in client;

    //дескриптор сокета
    int fd_sock;

    //проверка правильности завершения программы
    int status;
    ssize_t amount_byte;

    //размер accept();
    socklen_t len;

    //индекс циклов
    int i = 0;

    //дескриптор клиента
    int invite_fd;

    //создадим пул серверов
    for(i = 0; i < AMOUNT_MATH_SERV; i++){

        fd_math_serv[i] = socket(AF_INET, SOCK_STREAM, 0);
            error_func(fd_math_serv[i]);
        
        serv.sin_family = AF_INET;
        serv.sin_port = htons(port_serv[i][0]);
        serv.sin_addr.s_addr = inet_addr("127.0.0.1");

        status = bind(fd_math_serv[i], (struct sockaddr *)&serv, sizeof(serv));
            error_func(status);

        status = listen(fd_math_serv[i], 1);
            error_func(status);

        //передаем дескриптор сокера сервера потоку
        pthread_create(&index_pthread[i], NULL, math_server, (void *)&(fd_math_serv[i]));
    }

    //создаем сокет
    fd_sock = socket(AF_INET, SOCK_STREAM, 0);
        error_func(fd_sock);

    //наш сервер
    serv.sin_family = AF_INET;
    serv.sin_port = htons(MAIN_PORT);
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

        invite_fd = accept(fd_sock, (struct sockaddr *)&client, &len);
            error_func(invite_fd);

        pthread_mutex_lock(&mutex_clients);

        //ищем свободжный сервер
        for(i = 0; i < AMOUNT_MATH_SERV; i++){
            if(0 == port_serv[i][1]){
                port_serv[i][1] = 1;
                break;
            }
        }

        //обновляем данные перед отправкой пользователю
        serv.sin_port = htons(port_serv[i][0]);

        //отправляем потоку информацию о дескрипторе сокета
        amount_byte = send(invite_fd, (void *)&serv, sizeof(serv), 0);
            error_func(amount_byte);

        pthread_mutex_unlock(&mutex_clients);

        printf("\nНовый клиент\n\n");
    }

    for(i = 0; i < AMOUNT_MATH_SERV; i++){
        pthread_join(index_pthread[i], NULL);
    }

    close(fd_sock);

    exit(EXIT_SUCCESS);
}

void *math_server(void *arg){

    //индекс обрабатывающего потока
    int fd_thread = *((int *)arg);

    //число, которое нужно будет разделить
    int num;
    int i = 0;
    ssize_t amount_byte;
    int fd_client_math_serv;

    //поиск ошибок
    int status;

    //принятие данных от клиента
    struct sockaddr_in client;

    //размер accept();
    socklen_t len = sizeof(client);

    //принимаем дескриптор клиента
    int invite_fd;

    while(1){

        sleep(1);

        printf("Поток %d ожидает клиента\n", fd_thread);
        invite_fd = accept(fd_thread, (struct sockaddr *)&client, &len);
            error_func(invite_fd);
        printf("Поток %d дождался клиента\n", fd_thread);

        while(1){
            
            sleep(1);

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

        close(invite_fd);
        close(fd_thread);
        pthread_mutex_unlock(&mutex_clients);
    }
}
