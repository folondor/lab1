/* Лабораторная работа №1. Вариант 7.
   Последовательный TCP-сервер: прогноз погоды на неделю.
   Windows: cl /utf-8 server.c ws2_32.lib   или   gcc server.c -o server -lws2_32 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int sock_len;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int SOCKET;
  typedef socklen_t sock_len;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR   (-1)
  #define closesocket close
#endif

#define PORT      1280
#define BUF_SIZE  512
#define DAYS      7
#define SKY_COUNT 7

typedef struct {
    int t_day;          /* дневная температура, °C */
    int t_night;        /* ночная температура, °C */
    int sky;            /* индекс в массиве SKY */
    int wind;           /* скорость ветра, м/с */
    int precip;         /* вероятность осадков, % */
} Forecast;

static const char *DAY_RU[DAYS] = {
    "понедельник", "вторник", "среда", "четверг",
    "пятница", "суббота", "воскресенье"
};
static const char *DAY_ACC[DAYS] = {          /* винительный падеж: «на ...» */
    "понедельник", "вторник", "среду", "четверг",
    "пятницу", "субботу", "воскресенье"
};
static const char *DAY_EN[DAYS] = {
    "monday", "tuesday", "wednesday", "thursday",
    "friday", "saturday", "sunday"
};
static const char *SKY[SKY_COUNT] = {
    "ясно", "малооблачно", "облачно", "пасмурно",
    "небольшой дождь", "дождь", "гроза"
};
static const int SKY_PRECIP[SKY_COUNT] = { 0, 5, 20, 40, 60, 85, 90 };

static Forecast week[DAYS];

/* Формирование прогноза на неделю (случайные, но правдоподобные значения) */
static void generate_forecast(void)
{
    int base = 8 + rand() % 15;                 /* средняя температура недели */
    for (int i = 0; i < DAYS; i++) {
        week[i].t_day   = base + rand() % 7 - 3;
        week[i].t_night = week[i].t_day - (3 + rand() % 5);
        week[i].sky     = rand() % SKY_COUNT;
        week[i].wind    = 1 + rand() % 12;
        int p = SKY_PRECIP[week[i].sky] + rand() % 11 - 5;
        week[i].precip  = p < 0 ? 0 : (p > 100 ? 100 : p);
    }
}

/* Перевод строки UTF-8 в нижний регистр (латиница и кириллица) */
static void utf8_lower(char *s)
{
    for (size_t i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {
            s[i] = (char)tolower(c);
        } else if (c == 0xD0 && s[i + 1]) {
            unsigned char d = (unsigned char)s[i + 1];
            if (d >= 0x90 && d <= 0x9F)      s[i + 1] = (char)(d + 0x20);
            else if (d >= 0xA0 && d <= 0xAF) { s[i] = (char)0xD1; s[i + 1] = (char)(d - 0x20); }
            else if (d == 0x81)              { s[i] = (char)0xD1; s[i + 1] = (char)0x91; }
            i++;
        } else if (c >= 0xC0 && s[i + 1]) {
            i++;                                /* пропуск второго байта символа */
        }
    }
}

/* Определение дня недели по запросу клиента: возвращает 0..6 или -1 */
static int find_day(const char *request)
{
    char tmp[BUF_SIZE];
    strncpy(tmp, request, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    /* удаляем пробелы и перевод строки по краям */
    char *p = tmp;
    while (*p == ' ' || *p == '\t') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t' ||
                       p[len - 1] == '\r' || p[len - 1] == '\n'))
        p[--len] = '\0';

    utf8_lower(p);

    if (len == 1 && p[0] >= '1' && p[0] <= '7')
        return p[0] - '1';
    for (int i = 0; i < DAYS; i++)
        if (strcmp(p, DAY_RU[i]) == 0 || strcmp(p, DAY_EN[i]) == 0)
            return i;
    return -1;
}

/* Формирование текстового ответа для выбранного дня */
static void format_forecast(int i, char *out, size_t size)
{
    snprintf(out, size,
             "Прогноз на %s: днём %+d°C, ночью %+d°C, %s, ветер %d м/с, "
             "вероятность осадков %d%%",
             DAY_ACC[i], week[i].t_day, week[i].t_night,
             SKY[week[i].sky], week[i].wind, week[i].precip);
}

int main(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Ошибка инициализации WinSock\n");
        return 1;
    }
#endif
    srand((unsigned)time(NULL));
    generate_forecast();

    printf("Прогноз погоды на неделю сформирован:\n");
    for (int i = 0; i < DAYS; i++) {
        char line[BUF_SIZE];
        format_forecast(i, line, sizeof(line));
        printf("  %s\n", line);
    }

    /* 1. Создание сокета */
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        printf("Ошибка создания сокета\n");
        return 1;
    }
#ifndef _WIN32
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
#endif

    /* 2. Привязка к локальному адресу */
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family      = AF_INET;
    local.sin_port        = htons(PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR) {
        printf("Ошибка bind (порт %d занят?)\n", PORT);
        return 1;
    }

    /* 3. Переход в режим прослушивания */
    if (listen(s, 5) == SOCKET_ERROR) {
        printf("Ошибка listen\n");
        return 1;
    }
    printf("\nСервер запущен, порт %d. Ожидание клиентов...\n", PORT);

    /* 4. Последовательная обработка клиентов */
    while (1) {
        struct sockaddr_in remote_addr;
        sock_len size = sizeof(remote_addr);
        SOCKET s2 = accept(s, (struct sockaddr *)&remote_addr, &size);
        if (s2 == INVALID_SOCKET) {
            printf("Ошибка accept\n");
            continue;
        }
        printf("Клиент подключен: %s:%d\n",
               inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port));

        char buf[BUF_SIZE], reply[BUF_SIZE];
        int n;
        while ((n = recv(s2, buf, sizeof(buf) - 1, 0)) > 0) {
            buf[n] = '\0';
            printf("Запрос: \"%s\"\n", buf);

            int day = find_day(buf);
            if (day >= 0)
                format_forecast(day, reply, sizeof(reply));
            else
                snprintf(reply, sizeof(reply),
                         "Ошибка: неизвестный день недели. Введите название дня "
                         "(например, среда) или номер от 1 до 7");

            send(s2, reply, (int)strlen(reply) + 1, 0);
            printf("Ответ отправлен\n");
        }

        printf("Клиент отключился\n\n");
        closesocket(s2);
    }

    closesocket(s);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
