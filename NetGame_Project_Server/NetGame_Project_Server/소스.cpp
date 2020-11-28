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
    RECT rc;
};
#pragma pack(pop)

#pragma pack(push,1)
struct CHero {
    short x, y;
    bool connect;
    short id;
    RECT rc;
    Bullet BulletArr[10];
    short point;
};
#pragma pack(pop)

#pragma pack(push,1)
struct Monster {
    short x, y;
    short size;
    bool isActivated;
    RECT rc;
};
#pragma pack(pop)

#pragma pack(push,1)
struct BossMonster {
    short x, y;
    bool isActivated;
    short hp;
    RECT rc;
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
BossMonster boss;

bool leftMove;
bool rightMove;
bool upMove;
bool downMove;
bool attackState;

int BulletSpawnTick = 0;
int MonsterSpawnTick = 100;
int BossSpawnTick = 0;
short MonsterWave = 0;
short BossWave = 1;

short direct = 0;

CRITICAL_SECTION cs; // 임계영역 변수

bool CollideTest(RECT rc1, RECT rc2)
{
    RECT temp;
    return IntersectRect(&temp, &rc1, &rc2);
}

void MonsterSpawn(int type) {
    switch (type) {
    case 1:
        monsters[0].x = 14;
        monsters[0].y = 0;
        monsters[1].x = 106;
        monsters[1].y = 0;
        monsters[2].x = 198;
        monsters[2].y = 0;
        monsters[3].x = 290;
        monsters[3].y = 0;
        monsters[4].x = 382;
        monsters[4].y = 0;
        break;
    case 2:
        monsters[0].x = 14;
        monsters[0].y = -30;
        monsters[1].x = 106;
        monsters[1].y = 0;
        monsters[2].x = 198;
        monsters[2].y = -30;
        monsters[3].x = 290;
        monsters[3].y = 0;
        monsters[4].x = 382;
        monsters[4].y = -30;
        break;
    case 3:
        monsters[0].x = 196;
        monsters[0].y = 0;
        monsters[1].x = 196;
        monsters[1].y = -70;
        monsters[2].x = 196;
        monsters[2].y = -140;
        monsters[3].x = 196;
        monsters[3].y = -210;
        monsters[4].x = 196;
        monsters[4].y = -280;

        break;
    }
    for (int i = 0; i < 5; ++i) {
        monsters[i].isActivated = true;
    }
}

void BossSpawn()
{
    boss.x = 120;
    boss.y = -200;
    boss.rc = RECT{
                            boss.x
                            ,boss.y
                            ,boss.x + 200
                            ,boss.y + 200
    };
    boss.hp = 100;
}

short Client_ID{};
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
            hero[clientCount] = CHero{ 0,460,true,(short)clientCount,NULL,false ,0 };
            hero[0].rc = RECT{
                            hero[0].x + 10
                            ,hero[0].y + 10
                            ,hero[0].x + 80
                            ,hero[0].y + 80
            };
        }
        else if (clientCount == 1) {
            hero[clientCount] = CHero{ 350,460,true,(short)clientCount,NULL,false,0 };
            hero[1].rc = RECT{
                            hero[1].x + 10
                            ,hero[1].y + 10
                            ,hero[1].x + 80
                            ,hero[1].y + 80
            };
        }
        for (int i = 0; i < 10; ++i) {
            hero[clientCount].BulletArr[i].x = -100;
            hero[clientCount].BulletArr[i].y = 600;
        }
        for (int i = 0; i < 5; i++) {
            monsters[i] = Monster{ 600 ,600, 60 };
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


DWORD WINAPI Client_Thread(LPVOID arg)
{
    SOCKET clientSock = *((SOCKET*)arg); //매개변수로받은 클라이언트 소켓을 전달

    int retval;

    // hero.id 송신
    send(clientSock, (char*)&hero[clientCount], sizeof(CHero), 0);

    // 처음 몬스터 초기값 송신
    send(clientSock, (char*)&monsters, sizeof(monsters), 0);

    DWORD dwStartTime = timeGetTime();
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
        send(clientSock, (char*)&boss, sizeof(boss), 0);

        //SetEvent(hOperEvent);    
    }

    closesocket(clientSock);//소켓을 종료한다.
    return 0;
}

DWORD WINAPI Operation_Thread(LPVOID arg)
{


    while (true) {
        WaitForSingleObject(hOperEvent, INFINITE);
        if (rightMove == true)          // 오른 키
        {
            hero[Client_ID].x += 5;
            hero[Client_ID].rc = RECT{
                            hero[Client_ID].x + 10
                            ,hero[Client_ID].y + 10
                            ,hero[Client_ID].x + 80
                            ,hero[Client_ID].y + 80
            };
        }
        if (leftMove == true)     // 왼쪽 키
        {
            hero[Client_ID].x -= 5;
            hero[Client_ID].rc = RECT{
                            hero[Client_ID].x + 10
                            ,hero[Client_ID].y + 10
                            ,hero[Client_ID].x + 80
                            ,hero[Client_ID].y + 80
            };
        }
        if (upMove == true) {
            hero[Client_ID].y -= 5;
            hero[Client_ID].rc = RECT{
                            hero[Client_ID].x + 10
                            ,hero[Client_ID].y + 10
                            ,hero[Client_ID].x + 80
                            ,hero[Client_ID].y + 80
            };
        }
        if (downMove == true) {
            hero[Client_ID].y += 5;
            hero[Client_ID].rc = RECT{
                     hero[Client_ID].x + 10
                     ,hero[Client_ID].y + 10
                     ,hero[Client_ID].x + 80
                     ,hero[Client_ID].y + 80
            };
        }
        if (attackState == true)     // 스페이스 키
        {
            EnterCriticalSection(&cs);
            ++BulletSpawnTick;
            if (BulletSpawnTick > 10) {
                for (int j = 0; j < 10; j++) {
                    if (hero[Client_ID].BulletArr[j].isFire == false)
                    {
                        hero[Client_ID].BulletArr[j].isFire = true;
                        hero[Client_ID].BulletArr[j].x = hero[Client_ID].x;
                        hero[Client_ID].BulletArr[j].y = hero[Client_ID].y;
                        hero[Client_ID].BulletArr[j].rc = RECT{
                            hero[Client_ID].BulletArr[j].x + 30
                            ,hero[Client_ID].BulletArr[j].y
                            ,hero[Client_ID].BulletArr[j].x + 60
                            ,hero[Client_ID].BulletArr[j].y + 60
                        };

                        break;
                    }
                }
                BulletSpawnTick = 0;
            }
            LeaveCriticalSection(&cs);

        }
        for (int j = 0; j < 10; j++)
        {
            if (hero[Client_ID].BulletArr[j].isFire == true)
            {
                hero[Client_ID].BulletArr[j].y -= 10;
                hero[Client_ID].BulletArr[j].rc = RECT{
                            hero[Client_ID].BulletArr[j].x + 30
                            ,hero[Client_ID].BulletArr[j].y
                            ,hero[Client_ID].BulletArr[j].x + 60
                            ,hero[Client_ID].BulletArr[j].y + 60
                };

                if (hero[Client_ID].BulletArr[j].y < -70) {
                    hero[Client_ID].BulletArr[j].isFire = false;
                    hero[Client_ID].BulletArr[j].x = -100;
                    hero[Client_ID].BulletArr[j].y = 600;
                    hero[Client_ID].BulletArr[j].rc = RECT{
                            hero[Client_ID].BulletArr[j].x + 30
                            ,hero[Client_ID].BulletArr[j].y
                            ,hero[Client_ID].BulletArr[j].x + 60
                            ,hero[Client_ID].BulletArr[j].y + 60
                    };

                }
            }
        }
        {
            EnterCriticalSection(&cs);
            ++MonsterSpawnTick;
            //monster spawn
            if (MonsterSpawnTick > 300) {
                if (MonsterWave < 3) {
                    MonsterSpawn(rand() % 3 + 1);
                    MonsterWave += 1;
                    MonsterSpawnTick = 0;
                }
            }
            LeaveCriticalSection(&cs);
            //boss spawn
            if (MonsterWave == 3) {
                if (BossWave == 1 && boss.isActivated == false) {
                    boss.isActivated = true;
                    BossWave = 0;
                    BossSpawn();
                }
            }

            //boss move
            if (boss.isActivated == true) {
                if (boss.y < 15) {
                    boss.x = boss.x;
                    boss.y = boss.y + 3;
                    boss.rc = RECT{
                        boss.x
                        ,boss.y
                        ,boss.x + 200
                        ,boss.y + 200
                    };
                }
                else if (boss.y >= 15) {
                    boss.y = 15;
                    boss.rc = RECT{
                        boss.x
                        ,boss.y
                        ,boss.x + 200
                        ,boss.y + 200
                    };
                    // 좌우로 움직여보자
                    if (direct == 0) {
                        boss.x -= 1;
                        boss.rc = RECT{
                        boss.x
                        ,boss.y
                        ,boss.x + 200
                        ,boss.y + 200
                        };
                        if (boss.x <= 3) {
                            direct = 1;
                        }
                    }
                    else if (direct == 1) {
                        boss.x += 1;
                        boss.rc = RECT{
                        boss.x
                        ,boss.y
                        ,boss.x + 200
                        ,boss.y + 200
                        };
                        if (boss.x >= 250) {
                            direct = 0;
                        }
                    }
                }


            }


            //monster move
            for (int i = 0; i < 5; ++i) {
                if (monsters[i].isActivated == true) {
                    monsters[i].x = monsters[i].x;
                    monsters[i].y = monsters[i].y + 3;
                    monsters[i].rc = RECT{
                            monsters[i].x
                            ,monsters[i].y
                            ,monsters[i].x + 55
                            ,monsters[i].y + 55
                    };
                }
                if (monsters[i].y >= 614) {
                    monsters[i].isActivated = false;
                    monsters[i].x = 0;
                    monsters[i].y = -70;
                    monsters[i].rc = RECT{
                            monsters[i].x
                            ,monsters[i].y
                            ,monsters[i].x + 55
                            ,monsters[i].y + 55
                    };
                }
            }
        }

        // ------------------------------------ //
        // ------------------------------------ //
        // --------------Collide--------------- //
        // ------------------------------------ //
        // ------------------------------------ //
         // 총알과 몬스터의 충돌
        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 10; ++j) {

                if (CollideTest(hero[Client_ID].BulletArr[j].rc, monsters[i].rc) == true)
                {
                    hero[Client_ID].point += 500;
                    hero[Client_ID].BulletArr[j].isFire = false;

                    hero[Client_ID].BulletArr[j].x = -100;
                    hero[Client_ID].BulletArr[j].y = 600;
                    hero[Client_ID].BulletArr[j].rc = RECT{
                           hero[Client_ID].BulletArr[j].x + 15
                           ,hero[Client_ID].BulletArr[j].y
                           ,hero[Client_ID].BulletArr[j].x + 60
                           ,hero[Client_ID].BulletArr[j].y + 60
                    };

                    monsters[i].isActivated = false;
                    monsters[i].x = 0;
                    monsters[i].y = -70;
                    monsters[i].rc = RECT{
                            monsters[i].x
                            ,monsters[i].y
                            ,monsters[i].x + 55
                            ,monsters[i].y + 55
                    };
                }
            }
        }
        // 몬스터와 플레이어 간의 충돌
        for (int j = 0; j < 5; ++j) {
            if (CollideTest(hero[Client_ID].rc, monsters[j].rc) == true)
            {
                hero[Client_ID].point -= 500;
                monsters[j].isActivated = false;
                monsters[j].x = 0;
                monsters[j].y = -70;
                monsters[j].rc = RECT{
                        monsters[j].x
                        ,monsters[j].y
                        ,monsters[j].x + 55
                        ,monsters[j].y + 55
                };
            }
        }

        // 보스와 플레이어 총알 간의 충돌
        for (int j = 0; j < 10; ++j) {
            if (CollideTest(hero[Client_ID].BulletArr[j].rc, boss.rc) == true)
            {
                hero[Client_ID].point += 1000;
                hero[Client_ID].BulletArr[j].isFire = false;

                hero[Client_ID].BulletArr[j].x = -100;
                hero[Client_ID].BulletArr[j].y = 600;
                hero[Client_ID].BulletArr[j].rc = RECT{
                       hero[Client_ID].BulletArr[j].x + 15
                       ,hero[Client_ID].BulletArr[j].y
                       ,hero[Client_ID].BulletArr[j].x + 60
                       ,hero[Client_ID].BulletArr[j].y + 60
                };

                boss.hp -= 1;
                if (boss.hp <= 0) {
                    BossWave = 0;
                    boss.isActivated = false;
                    boss.x = 120;
                    boss.y = -200;
                    boss.rc = RECT{
                        boss.x
                        ,boss.y
                        ,boss.x + 200
                        ,boss.y + 200
                    };
                }
            }
        }

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