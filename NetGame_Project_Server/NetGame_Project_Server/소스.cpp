#define _WINSOCK_DEPRECATED_NO_WARNINGS // 최신 VC++ 컴파일 시 경고 방지
#pragma comment(lib, "ws2_32")
#pragma comment(lib,"winmm")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include<iostream>
#include <process.h>
#include<windows.h>
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
// 총알의 정보입니다.
struct Bullet {
    bool isFire; // 총알을 발사했습니까?
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

#pragma pack(push,1)
struct Monster {
    float x, y;
    short size;
    bool isActivated;

};
#pragma pack(pop)

#pragma region 오류 출력 부분
// 소켓 함수 오류 출력 후 종료
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
// 소켓 함수 오류 출력
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
SOCKET clientSocks[MAX_CLNT];//클라이언트 소켓 보관용 배열

HANDLE hReadEvent, hOperEvent;
KEY keyInfo;
CHero hero[2];
char buf[BUFSIZE];
HANDLE hThread;
HANDLE hThread2;
Monster monsters[5];

bool leftMove;
bool rightMove;
bool upMove;
bool downMove;
bool attackState;

int MonsterSpawnTick = 0;
int BulletSpawnTick = 0;

CRITICAL_SECTION cs; // 임계영역 변수

void MonsterSpawn(int type) {
    switch (type) {
    case 1:
        monsters[0].x = 14.f;
        monsters[0].y = 0.f;
        monsters[1].x = 88.f;
        monsters[1].y = 0.f;
        monsters[2].x = 162.f;
        monsters[2].y = 0.f;
        monsters[3].x = 236.f;
        monsters[3].y = 0.f;
        monsters[4].x = 310.f;
        monsters[4].y = 0.f;
        break;
    case 2:
        monsters[0].x = 14.f;
        monsters[0].y = -30.f;
        monsters[1].x = 88.f;
        monsters[1].y = 0.f;
        monsters[2].x = 162.f;
        monsters[2].y = -30.f;
        monsters[3].x = 236.f;
        monsters[3].y = 0.f;
        monsters[4].x = 310.f;
        monsters[4].y = -30.f;
        break;
    case 3:
        monsters[0].x = 162.f;
        monsters[0].y = 0.f;
        monsters[1].x = 162.f;
        monsters[1].y = -70.f;
        monsters[2].x = 162.f;
        monsters[2].y = -140.f;
        monsters[3].x = 162.f;
        monsters[3].y = -210.f;
        monsters[4].x = 162.f;
        monsters[4].y = -280.f;

        break;
    }
    for (int i = 0; i < 5; ++i) {
        monsters[i].isActivated = true;
    }
}

int main(int argc, char* argv[])
{
    // 이벤트 생성
    hReadEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (hReadEvent == NULL) return 1;
    hOperEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hOperEvent == NULL) return 1;
    int retval;
    // 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    printf("서버socket()\n");
    // socket()
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) err_quit("socket()");


    printf("서버bind()\n");
    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR) err_quit("bind()");


    printf("서버listen()\n");
    // listen()
    retval = listen(listen_sock, SOMAXCONN);
    if (retval == SOCKET_ERROR) err_quit("listen()");

    // 서버 대기 상태 완료 -----------------------------------------
    printf("서버 대기 상태 완료\n");

    // 임계영역 변수 선언
    InitializeCriticalSection(&cs);


    // 데이터 통신에 사용할 변수
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
        if (clientCount == 0) {
            hero[clientCount] = CHero{ 0,460,true,(short)clientCount,NULL,false };
        }
        else if (clientCount == 1) {
            hero[clientCount] = CHero{ 350,460,true,(short)clientCount,NULL,false };
        }
        for (int i = 0; i < 5; i++) {
            monsters[i] = Monster{ 600.f ,600.f, 60 };
        }

        cout << "접속한 클라 개수 : " << clientCount << endl;
        cout << "hero [" << clientCount << "].id : " << hero[clientCount].id << endl;

        hThread = CreateThread(NULL, 0, Client_Thread, (LPVOID)&client_sock, 0, NULL);//HandleClient 쓰레드 실행, clientSock을 매개변수로 전달
        printf("Connected Client IP : %s\n", inet_ntoa(clientaddr.sin_addr));


        hThread2 = CreateThread(NULL, 0, Operation_Thread, (LPVOID)&client_sock, 0, NULL);

    }

    // 임계역역 삭제
    DeleteCriticalSection(&cs);

    // closesocket()
    closesocket(listen_sock);

    // 윈속 종료
    WSACleanup();
    return 0;
}
short Client_ID{};

DWORD WINAPI Client_Thread(LPVOID arg)
{
    SOCKET clientSock = *((SOCKET*)arg); //매개변수로받은 클라이언트 소켓을 전달

    int retval;

    // hero.id 송신
    send(clientSock, (char*)&hero[clientCount], sizeof(CHero), 0);

    // 처음 몬스터 초기값 송신
    send(clientSock, (char*)&monsters, sizeof(monsters), 0);

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

        //cout << "한번만 실행?" << endl;
        //KeyMessage(&keyInfo.cKey, hero[keyInfo.id]);
        ResetEvent(hReadEvent);
        SetEvent(hOperEvent);

        send(clientSock, (char*)&monsters, sizeof(monsters), 0);
        send(clientSock, (char*)&hero, sizeof(hero), 0);

        //SetEvent(hOperEvent);


    }

    closesocket(clientSock);//소켓을 종료한다.
    return 0;
}

DWORD WINAPI Operation_Thread(LPVOID arg)
{

    DWORD dwStartTime = timeGetTime();
    while (true) {

        WaitForSingleObject(hOperEvent, INFINITE);

        if (rightMove == true)          // 오른 키
        {
            hero[Client_ID].x += 5;
        }
        if (leftMove == true)     // 왼쪽 키
        {
            hero[Client_ID].x -= 5;
        }
        if (upMove == true) {
            hero[Client_ID].y -= 5;
        }
        if (downMove == true) {
            hero[Client_ID].y += 5;
        }
        if (attackState == true)     // 스페이스 키
        {
            ++BulletSpawnTick;
            if (BulletSpawnTick > 10) {
                for (int j = 0; j < 10; j++) {
                    if (hero[Client_ID].BulletArr[j].isFire == false)
                    {
                        hero[Client_ID].BulletArr[j].isFire = true;
                        hero[Client_ID].BulletArr[j].x = hero[Client_ID].x;
                        hero[Client_ID].BulletArr[j].y = hero[Client_ID].y;

                        break;
                    }
                }
                BulletSpawnTick = 0;
            }
        }
        for (int j = 0; j < 10; j++)
        {
            if (hero[Client_ID].BulletArr[j].isFire == true)
            {
                hero[Client_ID].BulletArr[j].y -= 10;

                if (hero[Client_ID].BulletArr[j].y < -64) {
                    hero[Client_ID].BulletArr[j].isFire = false;
                    hero[Client_ID].BulletArr[j].x = 0;
                    hero[Client_ID].BulletArr[j].y = 600;
                }
            }
        }


        {
            //monster spawn
            DWORD dwNowTime = timeGetTime();
            if (dwNowTime - dwStartTime > 4500.0f) {
                MonsterSpawn(rand() % 3 + 1);
                dwStartTime = dwNowTime;
            }

            //monster move
            for (int i = 0; i < 5; ++i) {
                if (monsters[i].isActivated == true) {
                    monsters[i].x = monsters[i].x;
                    monsters[i].y = monsters[i].y + 3.f;
                }
                if (monsters[i].y >= 614.f) {
                    monsters[i].isActivated = false;
                    monsters[i].x = 0;
                    monsters[i].y = -70;
                }
            }
        }
        //cout << "operation 이후에 send 진행?" << endl;

        ResetEvent(hOperEvent);
        SetEvent(hReadEvent);
    }
    return 0;
}

// 사용자 정의 데이터 수신 함수, recvn( 소켓 디스크립터, 수신할 버퍼 포인터 데이터, 버퍼의 바이트 단위, )
int recvn(SOCKET s, char* buf, int len, int flags)
{
    int received;
    char* ptr = buf;
    int left = len;

    while (left > 0) {
        received = recv(s, ptr, left, flags);
        // recv = 실제 읽은 데이터의 크기를 return
        if (received == SOCKET_ERROR) return SOCKET_ERROR;
        else if (received == 0) {
            break;
        }
        left -= received;
        ptr += received;
    }
    return (len - left);
}