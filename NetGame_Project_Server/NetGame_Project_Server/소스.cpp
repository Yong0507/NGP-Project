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
int recvn(SOCKET s, char* buf, int len, int flags);


#pragma pack(push,1)
struct KEY {
    bool up, left, right, down, space;
    short id;
};
#pragma pack(pop)

#pragma pack(push,1)
// �Ѿ��� �����Դϴ�.
struct Bullet {
    bool isFire; // �Ѿ��� �߻��߽��ϱ�?
    short x, y;
};
#pragma pack(pop)

#pragma pack(push,1)
struct CHero {
    short x, y;
    bool connect;
    short id;
    RECT rc;
    bool IsShoot;
    Bullet BulletArr[10];
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
KEY keyInfo;
CHero hero[2];
char buf[BUFSIZE];
HANDLE hThread;
HANDLE hThread2;


bool leftMove;
bool rightMove;
bool upMove;
bool downMove;
bool attackState;

CRITICAL_SECTION cs; // �Ӱ迵�� ����

int main(int argc, char* argv[])
{
    // �̺�Ʈ ����
    hReadEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (hReadEvent == NULL) return 1;
    hOperEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
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

    // �Ӱ迵�� ���� ����
    InitializeCriticalSection(&cs);


    // ������ ��ſ� ����� ����
    SOCKET client_sock;
    SOCKADDR_IN clientaddr;
    int addrlen;

    while (1) {
        //accept()
        addrlen = sizeof(clientaddr);
        client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            err_display("accept()");
        }

        clientCount++;
        clientSocks[clientCount] = client_sock;
        //hero[clientCount].connect = true;
        //hero[clientCount].id = clientCount;
        hero[clientCount] = CHero{ 0,460,true,(short)clientCount,NULL,false };
        hero[clientCount].id = clientCount;

        cout << "������ Ŭ�� ���� : " << clientCount << endl;
        cout << "hero [" << clientCount << "].id : " << hero[clientCount].id << endl;

        hThread = CreateThread(NULL, 0, Client_Thread, (LPVOID)&client_sock, 0, NULL);//HandleClient ������ ����, clientSock�� �Ű������� ����
        printf("Connected Client IP : %s\n", inet_ntoa(clientaddr.sin_addr));


        hThread2 = CreateThread(NULL, 0, Operation_Thread, (LPVOID)&client_sock, 0, NULL);
    }

    // �Ӱ迪�� ����
    DeleteCriticalSection(&cs);

    // closesocket()
    closesocket(listen_sock);

    // ���� ����
    WSACleanup();
    return 0;
}
short Client_ID{};

DWORD WINAPI Client_Thread(LPVOID arg)
{
    SOCKET clientSock = *((SOCKET*)arg); //�Ű������ι��� Ŭ���̾�Ʈ ������ ����

    int retval;
    int i = 0;
    // hero.id �۽�
    send(clientSocks[clientCount], (char*)&hero[clientCount], sizeof(CHero), 0);

    unsigned int n = 0;

    while (1) {
        WaitForSingleObject(hReadEvent, INFINITE);
        recvn(clientSock, (char*)&keyInfo, sizeof(keyInfo), 0);
        EnterCriticalSection(&cs);
        Client_ID = keyInfo.id;
        leftMove = keyInfo.left;
        rightMove = keyInfo.right;
        upMove = keyInfo.up;
        downMove = keyInfo.down;
        attackState = keyInfo.space;
        LeaveCriticalSection(&cs);

        ResetEvent(hReadEvent);
        SetEvent(hOperEvent);
        //cout << "�ѹ��� ����?" << endl;
        //KeyMessage(&keyInfo.cKey, hero[keyInfo.id]);

        send(clientSock, (char*)&hero, sizeof(hero), 0);


        //SetEvent(hOperEvent);
    }

    closesocket(clientSock);//������ �����Ѵ�.
    return 0;
}

DWORD WINAPI Operation_Thread(LPVOID arg)
{
    int bulletTimer{};

    int time[2];
    int delete_Time[2][10];
    for (int i = 0; i < 2; i++)
    {
        time[i] = 0;

        for (int j = 0; j < 10; j++)
        {
            delete_Time[i][j] = 0;
        }
    }

    while (true) {
        WaitForSingleObject(hOperEvent, INFINITE);

        for (int i = 0; i < MAX_CLNT; i++) {
            if (Client_ID == i)               // ID ����
            {
                if (rightMove == true)          // ���� Ű
                {
                    hero[i].x += 1;
                }
                if (leftMove == true)     // ���� Ű
                {
                    hero[i].x -= 1;
                }
                if (upMove == true) {
                    hero[i].y -= 1;
                }
                if (downMove == true) {
                    hero[i].y += 1;
                }
                if (attackState == true)     // �����̽� Ű
                {
                    if (time[i] < 5)
                    {
                        time[i]++;
                    }

                    for (int j = 0; j < 10; j++)
                    {
                        if (hero[i].BulletArr[j].isFire == false)
                        {
                            hero[i].BulletArr[j].isFire = true;
                            hero[i].BulletArr[j].x = hero[i].x;
                            hero[i].BulletArr[j].y = hero[i].y;

                            break;
                        }
                    }
                    time[i] = 0;
                }
                else
                {
                    time[i] = 0;

                }

                for (int j = 0; j < 10; j++)
                {
                    if (hero[i].BulletArr[j].isFire == true)
                    {
                        hero[i].BulletArr[j].y -= 1;
                        if (hero[i].BulletArr[j].y < -64) {
                            hero[i].BulletArr[j].isFire = false;
                            hero[i].BulletArr[j].y = 600;
                        }
                        delete_Time[i][j]++;
                    }

                    if (delete_Time[i][j] > 30)
                    {
                        hero[i].BulletArr[i].isFire = false;
                        delete_Time[i][j] = 0;
                    }
                }
            }
        }

        //cout << "operation ���Ŀ� send ����?" << endl;

        ResetEvent(hOperEvent);
        SetEvent(hReadEvent);
    }
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