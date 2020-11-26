#define _WINSOCK_DEPRECATED_NO_WARNINGS // �ֽ� VC++ ������ �� ��� ����
#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include<iostream>
#include <process.h>
#include<thread>
using namespace std;

#define SERVERPORT 9000
#define BUFSIZE 1024

#define KEY_NULL   '0'
#define KEY_DOWN   '1'
#define KEY_LEFT   '2'
#define KEY_RIGHT  '3'
#define KEY_UP     '4'
#define KEY_SPACE  '5'
#define KEY_SPACE_NULL '6'

#define MAX_CLNT 2
DWORD WINAPI Client_Thread(LPVOID arg);
DWORD WINAPI Operation_Thread(LPVOID arg);
DWORD WINAPI Timer_Thread(LPVOID arg);
int recvn(SOCKET s, char* buf, int len, int flags);

class CStopwatch
{
public:
    CStopwatch();
    ~CStopwatch();

    void Start();

    __int64 Now() const;

    __int64 NowInMicro() const;

private:
    LARGE_INTEGER m_liPerfFreq;
    LARGE_INTEGER m_liPerfStart;
};

CStopwatch::CStopwatch() { QueryPerformanceFrequency(&m_liPerfFreq); Start(); }
CStopwatch::~CStopwatch() {};

void CStopwatch::Start() { QueryPerformanceCounter(&m_liPerfStart); }

__int64 CStopwatch::Now() const {
    LARGE_INTEGER liPerfNow;
    QueryPerformanceCounter(&liPerfNow);
    return (((liPerfNow.QuadPart - m_liPerfStart.QuadPart) * 1000)
        / m_liPerfFreq.QuadPart);
}

__int64 CStopwatch::NowInMicro() const {
    LARGE_INTEGER liPerfNow;
    QueryPerformanceCounter(&liPerfNow);
    return (((liPerfNow.QuadPart - m_liPerfStart.QuadPart) * 100000)
        / m_liPerfFreq.QuadPart);
}

float timer = 0.f;              // �ð�
CStopwatch stopwatch;

#pragma pack(push,1)
struct KEY {
    char cKey;
    short id;
};
#pragma pack(pop)

#pragma pack(push,1)
struct CHero {
    short x, y;
    bool connect;
    short id;
    RECT rc;
    bool IsShoot;
};
#pragma pack(pop)


#pragma region ���� ��� �κ�
// ���� �Լ� ���� ��� �� ����
void err_quit(char* msg)
{
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
    LocalFree(lpMsgBuf);
    exit(1);
}
// ���� �Լ� ���� ���
void err_display(char* msg)
{
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    printf("[%s] %s", msg, (char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}
#pragma endregion

int clientCount = -1;
SOCKET clientSocks[MAX_CLNT];//Ŭ���̾�Ʈ ���� ������ �迭

HANDLE hReadEvent, hOperEvent;
KEY keyInfo{ KEY_NULL };
CHero hero[2];
char buf[BUFSIZE];
HANDLE hThread;
HANDLE hThread2;
HANDLE hTimer;

void KeyMessage(const char* key, CHero& hero)
{
    if (KEY_RIGHT == *key)
    {
        cout << hero.x << endl;
        hero.x += 5;
    }
    else if (KEY_LEFT == *key)
    {
        cout << hero.x << endl;
        hero.x -= 5;
    }

}

int main(int argc, char* argv[])
{
    // �̺�Ʈ ����
    hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
    if (hReadEvent == NULL) return 1;
    hOperEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hOperEvent == NULL) return 1;

    int retval;
    // ���� �ʱ�ȭ
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    printf("����socket()\n");
    // socket()
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) err_quit("socket()");


    printf("����bind()\n");
    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR) err_quit("bind()");


    printf("����listen()\n");
    // listen()
    retval = listen(listen_sock, SOMAXCONN);
    if (retval == SOCKET_ERROR) err_quit("listen()");

    // ���� ��� ���� �Ϸ� -----------------------------------------
    printf("���� ��� ���� �Ϸ�\n");


    // ������ ��ſ� ����� ����
    SOCKET client_sock;
    SOCKADDR_IN clientaddr;
    int addrlen;

    // Timer ������ ����
    hTimer = CreateThread(NULL, 0, Timer_Thread, 0, 0, NULL);
    if (hTimer != NULL)
    {
        CloseHandle(hTimer);
    }

    while (1) {
        //accept()
        addrlen = sizeof(clientaddr);
        client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            err_display("accept()");
        }

        // ������� (Nagle �˰��� ����)
//	int opt_val = TRUE; // FALSE : Nagle �˰��� ����
//	setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&opt_val, sizeof(opt_val));

        clientCount++;
        clientSocks[clientCount] = client_sock;
        //hero[clientCount].connect = true;
        //hero[clientCount].id = clientCount;
        hero[clientCount] = CHero{ 0,400,true,(short)clientCount,NULL,false };
        hero[clientCount].id = clientCount;

        cout << "������ Ŭ�� ���� : " << clientCount << endl;
        cout << "hero [" << clientCount << "].id : " << hero[clientCount].id << endl;

        hThread = CreateThread(NULL, 0, Client_Thread, (LPVOID)&client_sock, 0, NULL);//HandleClient ������ ����, clientSock�� �Ű������� ����
        printf("Connected Client IP : %s\n", inet_ntoa(clientaddr.sin_addr));
        timer = 0.f;

        hThread2 = CreateThread(NULL, 0, Operation_Thread, (LPVOID)&client_sock, 0, NULL);
    }

    // closesocket()
    closesocket(listen_sock);

    // ���� ����
    WSACleanup();
    return 0;
}

DWORD WINAPI Client_Thread(LPVOID arg)
{
    SOCKET clientSock = *((SOCKET*)arg); //�Ű������ι��� Ŭ���̾�Ʈ ������ ����

    int retval;
    int i = 0;
    // hero.id �۽�
    send(clientSocks[clientCount], (char*)&hero[clientCount], sizeof(CHero), 0);

    unsigned int n = 0;

    while (1) {

        // 1�ʿ� 30���� �����Ѵ�.
        if (timer > (1.f / 30.f) * n)
        {
            cout << timer << endl;
            // timer ����
            send(clientSock, (char*)&timer, sizeof(timer), 0);

            recvn(clientSock, (char*)&keyInfo, sizeof(KEY), 0);
            
            KeyMessage(&keyInfo.cKey, hero[keyInfo.id]);

            send(clientSock, (char*)&hero, sizeof(hero), 0);
            n++;
        }

    }

    closesocket(clientSock);//������ �����Ѵ�.
    return 0;
}

DWORD WINAPI Operation_Thread(LPVOID arg)
{
    SOCKET clientSock = *((SOCKET*)arg); //�Ű������ι��� Ŭ���̾�Ʈ ������ ����
    WaitForSingleObject(hOperEvent, INFINITE);

    KeyMessage(&keyInfo.cKey, hero[keyInfo.id]);

    send(clientSock, (char*)&hero, sizeof(hero), 0);

    SetEvent(hReadEvent);
    return 0;
}

// ����� ���� ������ ���� �Լ�, recvn( ���� ��ũ����, ������ ���� ������ ������, ������ ����Ʈ ����, )
int recvn(SOCKET s, char* buf, int len, int flags)
{
    int received;
    char* ptr = buf;
    int left = len;

    while (left > 0) {
        received = recv(s, ptr, left, flags);
        // recv = ���� ���� �������� ũ�⸦ return
        if (received == SOCKET_ERROR) return SOCKET_ERROR;
        else if (received == 0) {
            break;
        }
        left -= received;
        ptr += received;
    }
    return (len - left);
}

// Timer ���� ������
DWORD WINAPI Timer_Thread(LPVOID arg)
{
    int retval;


    __int64 past_time = -1;
    while (1) {
        // �ð� ���� �Լ�
        if (past_time == -1) {
            past_time = stopwatch.Now();
        }

        __int64 now_time = stopwatch.Now();

        __int64 ElapsedTime = now_time - past_time;


        past_time = now_time;

        float etime = float(ElapsedTime) / 1000.f;
        timer += etime;
    }
    return 0;
}
