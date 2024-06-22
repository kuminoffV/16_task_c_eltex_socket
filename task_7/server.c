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
#include <time.h>


#define error_func(a) do{if(-1 == a){ printf("line:%d\n", __LINE__); \
                                        perror("error"); exit(EXIT_FAILURE);}} while(0)

#define AMOUNT_MATH_SERV 10 
#define NAME_FILE_KEY "key"
#define MAIN_PORT 3451

//функция потока, которая будет делить числа
void *math_server(void *arg);

//переменные для потоков, которые хотят взаимодействовать с клиентом
pthread_cond_t condition[AMOUNT_MATH_SERV] = { PTHREAD_COND_INITIALIZER };  

//структура запроса
struct request{
    int activity; //проверка наличия запроса;

    struct sockaddr_in client_info; //информация о клиентах

    int want; //запрос пользователя;
              //1 - минуты;
              //2 - секунды;
              //3 - часы;
              //4 - все вместе;
};

//количетсво потоков поддерживаемые сервером
pthread_t index_pthread[AMOUNT_MATH_SERV] = { 0 };

//дескрипторы потоков, обрабатывающих клиентов
int fd_math_serv[AMOUNT_MATH_SERV] = { 0 };

//порт
int port_serv[AMOUNT_MATH_SERV] = {5689,5690,5691,5692,5693,5694,5695,5696,5697,5698};

//дескрипторы семафоров и разделяемой памяти
int fd_shmem;
int fd_sem;

//идентификатор ключа System V
key_t key;

//указатель на структуру запроса
struct request *request;

//мьютекст по обработки клиентов
pthread_mutex_t mutex_clients =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_queue[AMOUNT_MATH_SERV] = { PTHREAD_MUTEX_INITIALIZER };

int main(void){
    //структура с информацией о клиенте и сервере
    struct sockaddr_in serv;
    struct sockaddr_in client;

    //- дескриптор сокета;
    int fd_sock;

    //проверка правильности завершения программы
    int status;
    ssize_t amount_byte;

    //размер accept()
    socklen_t len;

    //-индекс циклов
    int i = 0;

    //-запрос запрашиваемый клиентом
    int wtf;

    //ключ для разделяемой памяти и семафоров
    key = ftok("key", 90);
        error_func(key);
    
    //разделяемая память для запросы от клиентов;
    //выделяем под 10 запросов
    fd_shmem = shmget(key, sizeof(struct request) * 10, IPC_CREAT | 0666);
        error_func(fd_shmem);
    request = (struct request*)shmat(fd_shmem, NULL, 0);
    memset(request, 0, sizeof(struct request) * 10);

    //семафор
    fd_sem = semget(key, 1, IPC_CREAT | 0666);
        error_func(fd_sem);
    status = semctl(fd_sem, 0, SETVAL, 15);
            error_func(status);

    //пул серверов
    for(i = 0; i < AMOUNT_MATH_SERV; i++){

        fd_math_serv[i] = socket(AF_INET, SOCK_DGRAM, 0);
            error_func(fd_math_serv[i]);
        
        serv.sin_family = AF_INET;
        serv.sin_port = htons(port_serv[i]);
        serv.sin_addr.s_addr = inet_addr("127.0.0.1");

        //привязываем n-pointer 
        status = bind(fd_math_serv[i], (struct sockaddr *)&serv, sizeof(serv));
            error_func(status);

        //отправляем потоку информацию о дескрипторе сокета
        pthread_create(&index_pthread[i], NULL, math_server, (void *)&(fd_math_serv[i]));

    }

    //создаем сокет
    fd_sock = socket(AF_INET, SOCK_DGRAM, 0);
        error_func(fd_sock);

    //наш сервер
    serv.sin_family = AF_INET;
    serv.sin_port = htons(MAIN_PORT);
    serv.sin_addr.s_addr = inet_addr("127.0.0.1");

    status = bind(fd_sock, (struct sockaddr *)&serv, sizeof(serv));
        error_func(status);
    
    len = sizeof(client);

    while(1){

        sleep(1);

        printf("Ожидание заявки клиента\n");

        //запрос от клиента
        amount_byte = recvfrom(fd_sock, (void *)&wtf, sizeof(wtf), 0, (struct sockaddr *)&client, &len);
            error_func(amount_byte);

        pthread_mutex_lock(&mutex_clients);

        //ищем заявку для обработки
        for(i = 0; i < AMOUNT_MATH_SERV; i++){
            if(0 == (request + i)->activity){
                status = pthread_mutex_trylock(&mutex_queue[i]);
                if(0 == status){
                    (request + i)->activity = 1;
                    memcpy(&((request + i)->client_info), &client, sizeof(struct sockaddr));
                    (request + i)->want = wtf;
                    pthread_mutex_unlock(&mutex_queue[i]);
                    break;
                }
            }
        }

        //сообщаем потокам о новой заявке
        status = semctl(fd_sem, 0, SETVAL, 0);
            error_func(status);

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

    //структура с информацией о клиенте
    struct sockaddr_in client;

    //индекс обрабатывающего потока
    int fd_thread = *((int *)arg);

    //число, которое разделить на два
    int num;
    int i = 0;
    ssize_t amount_byte;
    int fd_client_math_serv;

    //ловля ошибок
    int status;

    //размер accept()
    socklen_t len = sizeof(client);

    //принимаем дескриптор клиента
    int invite_fd;

    //врямя, которое будем передавать клиентам
    time_t current_time;
    struct tm* time_info;

    //желание клиента
    int wtf;

    time(&current_time);
    time_info = localtime(&current_time);

    //обрабатывающий сервер входит в очередь запросов, когда появляется запрос и закрывает за собой вход
    struct sembuf go_to_queue[2] = {{0,0,0},{0,1,0}};

    while(1){

        sleep(1);

        semop(fd_sem, go_to_queue, 2);

        for(i = 0; i < AMOUNT_MATH_SERV; i++){
            if(1 == (request + i)->activity){
                //создадим trylock() чтобы потоки одновременно не забрали запросы
                status = pthread_mutex_trylock(&mutex_queue[i]);
                if(0 == status){
                    printf("Поток %d вошел в разделяему память, чтобы забрать (%d)\n", fd_thread, (request + i)->want);
                    memcpy(&client,&((request + i)->client_info), sizeof(struct sockaddr));
                    wtf = (request + i)->want;
                    (request + i)->activity = 0;
                    pthread_mutex_unlock(&mutex_queue[i]);
                    break;
                }
            }
        }
        
        printf("Значение: %d\n", wtf);

        client.sin_family = AF_INET;
        client.sin_addr.s_addr = inet_addr("127.0.0.1");

        switch(wtf){
            case 1:
                wtf = time_info->tm_min;
                amount_byte = sendto(fd_thread, (void *)&wtf, sizeof(wtf), 0, (struct sockaddr *)&client, len);
                    error_func(amount_byte);
                wtf = 0;
                break;
            case 2:
                wtf = time_info->tm_sec; 
                amount_byte = sendto(fd_thread, (void *)&wtf, sizeof(wtf), 0, (struct sockaddr *)&client, len);
                    error_func(amount_byte);
                    wtf = 0;
                break;

            case 3:
                wtf = time_info->tm_hour;
                amount_byte = sendto(fd_thread, (void *)&wtf, sizeof(wtf), 0, (struct sockaddr *)&client, len);
                    error_func(amount_byte);
                wtf = 0;
                break;

            default:
                wtf = -1;
                amount_byte = sendto(fd_thread, (void *)&wtf, sizeof(wtf), 0, (struct sockaddr *)&client, len);
                    error_func(amount_byte);
        }

        printf("Отправили %ld байт\n", amount_byte);

    }
}
