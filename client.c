/* Лабораторная работа №1. Вариант 7.
   TCP-клиент: запрос прогноза погоды на выбранный день недели.
   Windows: cl /utf-8 client.c ws2_32.lib   или   gcc client.c -o client -lws2_32 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int SOCKET;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR   (-1)
  #define closesocket close
#endif

#define PORT     1280
#define BUF_SIZE 512

int main(int argc, char *argv[])
{
    /* адрес сервера: по умолчанию клиент и сервер на одном компьютере */
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Ошибка инициализации WinSock\n");
        return 1;
    }
#endif

    struct sockaddr_in peer;
    memset(&peer, 0, sizeof(peer));
    peer.sin_family      = AF_INET;
    peer.sin_port        = htons(PORT);
    peer.sin_addr.s_addr = inet_addr(server_ip);

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        printf("Ошибка создания сокета\n");
        return 1;
    }

    if (connect(s, (struct sockaddr *)&peer, sizeof(peer)) == SOCKET_ERROR) {
        printf("Не удалось подключиться к серверу %s:%d\n", server_ip, PORT);
        closesocket(s);
        return 1;
    }
    printf("Подключено к серверу %s:%d\n", server_ip, PORT);

    char buf[BUF_SIZE], answer[BUF_SIZE];
    while (1) {
        printf("\nВведите день недели (или exit для выхода): ");
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;
        buf[strcspn(buf, "\r\n")] = '\0';           /* убираем перевод строки */

        if (strcmp(buf, "exit") == 0 || strcmp(buf, "выход") == 0)
            break;
        if (buf[0] == '\0')
            continue;

        if (send(s, buf, (int)strlen(buf) + 1, 0) == SOCKET_ERROR) {
            printf("Ошибка отправки\n");
            break;
        }

        int n = recv(s, answer, sizeof(answer) - 1, 0);
        if (n <= 0) {
            printf("Соединение с сервером закрыто\n");
            break;
        }
        answer[n] = '\0';
        printf("%s\n", answer);
    }

    closesocket(s);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
