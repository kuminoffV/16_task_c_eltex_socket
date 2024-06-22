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
#include <sys/select.h>
#include <signal.h>

#define error_func(a) do{if(-1 == a){ printf("line:%d\n", __LINE__); \
                                        perror("error"); exit(EXIT_FAILURE);}} while(0)

#define AMOUNT_MATH_SERV 10 
#define MAIN_HOST 3454
#define TIMEOUT 3
#define SUM 10

//функция потока, которая будет делить числа
void *math_server_tcp(void *arg);
void *math_server_udp(void *arg);

//количетсво потоков поддерживаемые сервером
pthread_t index_pthread[AMOUNT_MATH_SERV];

//идентификатор свободного сервера
int index_free_serv = 0;

//порт
int port_serv[10][2] = {{7780 + SUM,0},{7781 + SUM,0},{7782 + SUM,0},{7783 + SUM,0},{7784 + SUM,0},{7785 + SUM,0},{7786 + SUM,0},{7787 + SUM,0},{7788 + SUM,0},{7789 + SUM,0}};
int port_thread = 0;

//мьютекст по обработки клиентов
pthread_mutex_t mutex_clients = PTHREAD_MUTEX_INITIALIZER;

int main(void){

    struct sockaddr_in serv_udp;
    struct sockaddr_in serv_tcp;
    struct sockaddr_in client;

    //дескриптор сокета
    int fd_sock_udp, fd_sock_tcp;

    //проверка правильности завершения программы
    int status;
    ssize_t amount_byte;

    //размер accept()
    socklen_t len;

    //создадим структуру данных набора дескрипторов сокетов
    fd_set read_sock;

    //переменная по работе с сигналами
    sigset_t sigmask;

    //максимальный дескриптор в наборе
    int max_fd;

    //ждеми 5 секунд
    struct timespec deadline = {5,0};

    //- инициализируем структуру дескрипторов;
    FD_ZERO(&read_sock);

    //инициализация пустого набора сигналов
    sigemptyset(&sigmask);

    //добавление сигнала SIGALRM
    sigaddset(&sigmask, SIGINT);

    //сокет
    fd_sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
        error_func(fd_sock_tcp);
    fd_sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
        error_func(fd_sock_udp);

    //индекс циклов
    int i;

    //дескрипторы клиентов
    int invite_fd;

    //дескриптор обрабатывающего сервера
    int fd_math_serv;

    //наш сервер tcp
    serv_tcp.sin_family = AF_INET;
    serv_tcp.sin_port = htons(MAIN_HOST);
    serv_tcp.sin_addr.s_addr = inet_addr("127.0.0.1");

    status = bind(fd_sock_tcp, (struct sockaddr *)&serv_tcp, sizeof(serv_tcp));
        error_func(status);

    //очередь для подключения клиентов
    status = listen(fd_sock_tcp, 5);
        error_func(status);

    //наш сервер udp
    serv_udp.sin_family = AF_INET;
    serv_udp.sin_port = htons(MAIN_HOST - 1);
    serv_udp.sin_addr.s_addr = inet_addr("127.0.0.1");

    status = bind(fd_sock_udp, (struct sockaddr *)&serv_udp, sizeof(serv_udp));
        error_func(status);
    
    //добавляем дескрипторы UDP и TCP
    FD_SET(fd_sock_udp,&read_sock);
    FD_SET(fd_sock_tcp,&read_sock);
    
    len = sizeof(client);
    max_fd = (fd_sock_udp > fd_sock_tcp) ? fd_sock_udp : fd_sock_tcp;

    while(1){

        sleep(1);
        
        printf("Начинаем прослушивать клиентов\n");

        //ждем подсоединений
        status = pselect(max_fd + 1, &read_sock, NULL, NULL, &deadline, &sigmask);
            error_func (status);

        //проверяем на подключение
        if(FD_ISSET(fd_sock_udp, &read_sock)){

            recvfrom(fd_sock_udp, NULL, 0, 0, (void *)&client, &len);

            printf("Новый клиент присоединился к серверу udp\n");

            //обрабатываем общения данных с потоками
            pthread_mutex_lock(&mutex_clients);

            //обрабатывающий сокет
            fd_math_serv = socket(AF_INET, SOCK_DGRAM, 0);
                error_func(fd_math_serv);
                
            //обрабатывающий сервер
            serv_udp.sin_family = AF_INET;

            //присваиваем  порт
            for(i = 0; i < AMOUNT_MATH_SERV; i++){
                if(0 == port_serv[i][1]){
                    serv_udp.sin_port = htons(port_serv[i][0]);
                    printf("Порт %d\n",htons(port_serv[i][0]));
                    port_serv[i][1] = 1;
                    break;
                }
            }
            serv_udp.sin_addr.s_addr = inet_addr("127.0.0.1");

            //сервер, который будет обслуживать клиентов
            status = bind(fd_math_serv, (struct sockaddr *)&serv_udp, sizeof(serv_udp));
                error_func(status);
            
            //поток, который будет обрабатывать клиентов
            pthread_create(&index_pthread[index_free_serv], NULL, math_server_udp, (void *)&fd_math_serv);

            //отправляем клиенту данные об обрабатывающем сервере;
            amount_byte = sendto(fd_math_serv, (void *)&serv_udp, sizeof(serv_udp), 0, (void *)&client, len);
                error_func(amount_byte);

            //счетчик клиентов
            index_free_serv++;

            pthread_mutex_unlock(&mutex_clients);

        }else if(FD_ISSET(fd_sock_tcp, &read_sock)){

            //ждем присоединения клиентов
            invite_fd = accept(fd_sock_tcp, (struct sockaddr *)&client, &len);
                error_func(invite_fd);

            printf("Новый клиент присоединился к серверу tcp\n");
            //обрабатываем взаимодействие данных с потоками
            pthread_mutex_lock(&mutex_clients);

            //обрабатывающий сокет
            fd_math_serv = socket(AF_INET, SOCK_STREAM, 0);
                error_func(fd_math_serv);
                
            //обрабатывающий сервер
            serv_tcp.sin_family = AF_INET;

            //присваиваем порт
            for(i = 0; i < AMOUNT_MATH_SERV; i++){
                if(0 == port_serv[i][1]){
                    serv_tcp.sin_port = htons(port_serv[i][0]);
                    printf("Порт %d\n",htons(port_serv[i][0]));
                    port_serv[i][1] = 1;
                    break;
                }
            }
            serv_tcp.sin_addr.s_addr = inet_addr("127.0.0.1");

            //новый сервер, который будет обслуживать клиента
            status = bind(fd_math_serv, (struct sockaddr *)&serv_tcp, sizeof(serv_tcp));
                error_func(status);

            //очередь для подключения клиентов
            status = listen(fd_math_serv, 1);
                error_func(status);

            //поток, который будет обрабатывать клиента
            pthread_create(&index_pthread[index_free_serv], NULL, math_server_tcp, (void *)&fd_math_serv);

            //отправляем клиенту данные об обрабатывающем сервере;
            amount_byte = send(invite_fd, (void *)&serv_tcp, sizeof(serv_tcp), 0);
                error_func(amount_byte);

            //счетчик клиентов
            index_free_serv++;

            pthread_mutex_unlock(&mutex_clients);

        }

        //добавляем дескрипторы UDP и TCP в структуру мульплексированного ввода/вывода из-за того, что pselect() удалила;
        FD_ZERO(&read_sock);
        FD_SET(fd_sock_udp,&read_sock);
        FD_SET(fd_sock_tcp,&read_sock);
    }

    for (i = 0; i < AMOUNT_MATH_SERV; i++) {
        pthread_join(index_pthread[i], NULL);
    }


    close(fd_sock_tcp);
    close(fd_sock_udp);

    exit(EXIT_SUCCESS);
}

void *math_server_tcp(void *arg){

    //дескриптор  сервера
    int fd_math_serv = *((int *)arg);

    //-число, которое нужно будет разделить
    int num;

    //индекс циклов
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

    /размер accept()
    socklen_t len;

    //высвобождение порта
    int reuse = 1;

    len = sizeof(client);

    pthread_mutex_lock(&mutex_clients);

    //готовность принимать клиентов
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

void *math_server_udp(void *arg){

    //дескриптор обрабатывающего сервера
    int fd_math_serv = *((int *)arg);

    //число, которое нужно будет разделить
    int num;

    //индекс циклов
    int i = 0;

    //проверка ошибок
    ssize_t amount_byte;

    //структуры для сетевого общения
    struct sockaddr_in serv;
    struct sockaddr_in client;

    //фиксирование ошибок
    int status;

    //размер accept()
    socklen_t len;

    //высвобождение порта
    int reuse = 1;

    len = sizeof(client);

    pthread_mutex_lock(&mutex_clients);

    port_thread++;

    pthread_mutex_unlock(&mutex_clients);

    while(1){
        amount_byte = recvfrom(fd_math_serv, (void *)&num, sizeof(num), 0, (void *)&client, &len);
            error_func(amount_byte);

        if(-1 == num){
            break;
        }
            
        printf("Клиент прислал: %d\n", num);

        num = num / 2;

        amount_byte = sendto(fd_math_serv, (void *)&num, sizeof(num), 0, (void *)&client, len);
            error_func(amount_byte);
    }

    pthread_mutex_lock(&mutex_clients);

    port_serv[i][1] = 0;

    close(fd_math_serv);
    //сообщаем о возможном использованрии порта
    setsockopt(fd_math_serv, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));


    index_pthread[i] = index_pthread[index_free_serv - 1];
    index_pthread[index_free_serv - 1] = 0;
    
    index_free_serv--;

    pthread_mutex_unlock(&mutex_clients);
}
