#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <linux/if_ether.h>
#include <net/if.h>


#define error_func(a) do{if(-1 == a){ printf("line:%d\n", __LINE__); \
                                        perror("error"); exit(EXIT_FAILURE);}} while(0)

//подстчет чексуммы
unsigned short my_checksum(struct ip *point_header, int size_head);

int main(void){

    struct sockaddr_ll serv, client;

    //дескриптор сокета
    int fd_serv, fd_client;
    //проверка возвращаемых функций
    int status;
    ssize_t amount_byte;

    //заголовок udp
    struct udphdr *head_udp;

    //заголовок ip
    struct ip *head_ip;

    //заголовок ethernet
    struct ether_header *head_ethernet;

    //для sendto() и recvfrom()
    socklen_t len;
    len = sizeof(client);

    //буфер, для хранения отправленныч и принимаемых пакетов
    char buffer[200];

    //указатель для перемещения по пакету
    char *mover;

    //включенная опция
    int opt = 1;

    //mac источника и назначения
    //МЕНЯТЬ ТУТА
    uint8_t dest_mac[6] = {0x0c, 0xef, 0x1a, 0x45, 0x12, 0xbc};
    uint8_t src_mac[6] = {0x61, 0xf6, 0x12, 0xcc, 0x4a, 0xd5};
    //МЕНЯТЬ ТУТА

    //сырой сокет на уровне драйверов
    fd_serv = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    //адрес отправления
    serv.sll_family = AF_PACKET;
    serv.sll_protocol = htons(ETH_P_ALL);
    serv.sll_ifindex = if_nametoindex("wlx20e6170cbb1f");
    if(0 == serv.sll_ifindex){
        printf("Неправильный интерфейс\n");
        perror("error");
        exit(EXIT_FAILURE);
    }
    serv.sll_hatype = 0;
    serv.sll_pkttype = PACKET_OTHERHOST;
    serv.sll_halen = 6;
    memcpy(serv.sll_addr, dest_mac, ETH_ALEN);

    //указатель на начало данных
    mover = buffer + sizeof(struct udphdr) + sizeof(struct ip) + sizeof(struct ether_header);

    //указатель на заголовок Ethernet
    head_ethernet = (struct ether_header*)buffer;

    //указатель на ip заголовок
    head_ip = (struct ip*)(buffer + sizeof(struct ether_header));

    //указатель на UDP заголовок
    head_udp = (struct udphdr*)(buffer + sizeof(struct ip) + sizeof(struct ether_header));


    while(1){

        sleep(1);
        memset(buffer, 0, sizeof(buffer));

        printf("Напишем сереверу: ");
        fgets(mover, 100, stdin);
        mover[strcspn(mover, "\n")] = '\0';

        //заголовок Ethernet
        memcpy(head_ethernet->ether_dhost, dest_mac, ETH_ALEN);
        memcpy(head_ethernet->ether_shost, src_mac, ETH_ALEN);
        head_ethernet->ether_type = htons(0x0800);

        //ip заголовок
        head_ip->ip_hl = 5;
        head_ip->ip_v = 4;
        head_ip->ip_tos = 1;
        head_ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udphdr) + strlen(mover));
        head_ip->ip_id = htons(1);
        head_ip->ip_off = 0;
        head_ip->ip_ttl = 5;
        head_ip->ip_p = IPPROTO_UDP;
        //МЕНЯТЬ ТУТА
        head_ip->ip_src.s_addr = inet_addr("192.168.0.16");
        head_ip->ip_dst.s_addr = inet_addr("192.168.0.21");
        //МЕНЯТЬ ТУТА
        head_ip->ip_sum = my_checksum(head_ip, 20);

        //udp заголовок
        head_udp->source = ntohs(1456);
        head_udp->dest = ntohs(3457);
        head_udp->len = ntohs(sizeof(struct udphdr) + strlen(mover));
        head_udp->check = 0;

        amount_byte = sendto(fd_serv, (void *)buffer, sizeof(struct ip) + sizeof(struct udphdr) + sizeof(struct ether_header) + strlen(mover), 0, (struct sockaddr*)&serv, len);
            error_func(amount_byte);

        if(!strcmp(mover,"exit")){
            break;
        }

        while(1){

            //ожидание ответа от сервера
            amount_byte = recvfrom(fd_serv, (void *)&buffer, sizeof(buffer), 0, (struct sockaddr*)&serv, &len);
            error_func(amount_byte);
        
            if(0 == amount_byte){
                break;
            } else if (3457 == ntohs(head_udp->source)){
                //проверка ответа от сервера
                break;
            }
            
        }

        printf("Ответ сервера: %s\n", mover);
    }

    close(fd_serv);

    exit(EXIT_SUCCESS);
}

unsigned short my_checksum(struct ip *point_header, int size_head){
    //- будут хранить всю сумм;
    int all_sum = 0;
    //- будет хранить то, что нужно будет просуммировать;
    int dop_sum;
    //- готовый вариант суммы;
    unsigned short checksum;
    //- этим указателем будем ходить по структуре;
    unsigned short *ptr = (short *)point_header;
    //- лдя проверки четности;
    unsigned short last_byte;

    //- складываем по два байта;
    for(int i = 0; i < size_head / 2; i++){
        all_sum += *ptr;
        ptr++;
    }

    //- если размер заголовка нечетный, добавляем оставшийся байт
    if (size_head % 2 != 0) {
        last_byte = *((unsigned char *)ptr);
        all_sum += last_byte;
    }

    //- смотрим есть ли остаток;
    dop_sum = all_sum >> 16;

    while(dop_sum > 0){
        all_sum = (all_sum & 0xFFFF) + dop_sum;
        dop_sum = all_sum >> 16;
    }
    checksum = (unsigned short)~all_sum;

    return checksum;
}
