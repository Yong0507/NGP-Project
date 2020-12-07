#define _WINSOCK_DEPRECATED_NO_WARNINGS // �ֽ� VC++ ������ �� ��� ����
#pragma comment(lib, "ws2_32")
#pragma comment(lib,"winmm")
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <process.h>
#include <windows.h>
#include "protocol.h"
#include "global.h"
using namespace std;

bool IsCollide(RECT rc1, RECT rc2);
void MonsterSpawn(int type);
void BossSpawn();

DWORD WINAPI Client_Thread(LPVOID arg);
DWORD WINAPI Operation_Thread(LPVOID arg);
int recvn(SOCKET s, char* buf, int len, int flags);

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

#define SERVERPORT 9000

#define monsterMax 5
#define bulletMax  10

CRITICAL_SECTION cs;

//HANDLE hReadEvent, hOperEvent;
HANDLE hThread;
HANDLE hThread2;

KEY keyInfo;
CHero hero[2];
Monster monsters[5];
BossMonster boss;

bool leftMove;
bool rightMove;
bool upMove;
bool downMove;
bool attackState;


int main(int argc, char* argv[])
{
    //// �̺�Ʈ ����
    //hReadEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    //if (hReadEvent == NULL) return 1;
    //hOperEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    //if (hOperEvent == NULL) return 1;
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

        //�÷��̾�1 �ʱ�ȭ
        if (clientCount == 0) {
            hero[clientCount] = CHero{ 0,460,true,(short)clientCount,NULL,false ,0 };
            hero[0].rc = RECT{
                            hero[0].x + 10
                            ,hero[0].y + 10
                            ,hero[0].x + 80
                            ,hero[0].y + 80
            };
        }
        // �÷��̾�2 �ʱ�ȭ
        else if (clientCount == 1) {
            hero[clientCount] = CHero{ 350,460,true,(short)clientCount,NULL,false,0 };
            hero[1].rc = RECT{
                            hero[1].x + 10
                            ,hero[1].y + 10
                            ,hero[1].x + 80
                            ,hero[1].y + 80
            };
        }

        // �÷��̾� �Ѿ� �ʱ� ��ǥ
        for (int i = 0; i < bulletMax; ++i) {
            hero[clientCount].BulletArr[i].x = -100;
            hero[clientCount].BulletArr[i].y = 600;
        }

        // ���� �ʱ� ��ǥ
        for (int i = 0; i < monsterMax; i++) {
            monsters[i] = Monster{ 600 ,600, 60 };
        }

        // ���� �Ѿ� �ʱ� ��ǥ
        for (int i = 0; i < 5; i++) {
            boss.BossBulletArr[i].x = -100;
            boss.BossBulletArr[i].y = -100;
        }

        cout << "������ Ŭ�� ���� : " << clientCount << endl;
        cout << "hero [" << clientCount << "].id : " << hero[clientCount].id << endl;
        printf("Connected Client IP : %s\n", inet_ntoa(clientaddr.sin_addr));

        hThread = CreateThread(NULL, 0, Client_Thread, (LPVOID)client_sock, 0, NULL);//HandleClient ������ ����, clientSock�� �Ű������� ����

    }

    // �Ӱ迪�� ����
    DeleteCriticalSection(&cs);

    // closesocket()
    closesocket(listen_sock);

    // ���� ����
    WSACleanup();
    return 0;
}


DWORD WINAPI Client_Thread(LPVOID arg)
{
    SOCKET clientSock = (SOCKET)arg;

    int retval;

    // hero.id �۽�
    send(clientSock, (char*)&hero[clientCount], sizeof(CHero), 0);
    EnterCriticalSection(&cs);
    int StartTime = GetTickCount64();
    int deltaTime;
    LeaveCriticalSection(&cs);
    while (1) {
        int NowTime = GetTickCount64();
        EnterCriticalSection(&cs);
        deltaTime = NowTime - StartTime;
        LeaveCriticalSection(&cs);
        if (deltaTime >= 10) {

            /*WaitForSingleObject(hReadEvent, INFINITE);*/

            /*Sleep(16);*/

            recvn(clientSock, (char*)&keyInfo, sizeof(keyInfo), 0);

            EnterCriticalSection(&cs);
            Client_ID = keyInfo.id;
            leftMove = keyInfo.left;
            rightMove = keyInfo.right;
            upMove = keyInfo.up;
            downMove = keyInfo.down;
            attackState = keyInfo.space;
            LeaveCriticalSection(&cs);

            if (rightMove == true)          // ���� Ű
            {
                if (hero[Client_ID].x <= 360) {
                    hero[Client_ID].x += 5;
                    hero[Client_ID].rc = RECT{
                                    hero[Client_ID].x + 10
                                    ,hero[Client_ID].y + 10
                                    ,hero[Client_ID].x + 80
                                    ,hero[Client_ID].y + 80
                    };
                }
            }

            if (leftMove == true)     // ���� Ű
            {
                if (hero[Client_ID].x >= 0) {
                    hero[Client_ID].x -= 5;
                    hero[Client_ID].rc = RECT{
                                    hero[Client_ID].x + 10
                                    ,hero[Client_ID].y + 10
                                    ,hero[Client_ID].x + 80
                                    ,hero[Client_ID].y + 80
                    };
                }
            }

            if (upMove == true) {
                if (hero[Client_ID].y >= 0) {
                    hero[Client_ID].y -= 5;
                    hero[Client_ID].rc = RECT{
                                    hero[Client_ID].x + 10
                                    ,hero[Client_ID].y + 10
                                    ,hero[Client_ID].x + 80
                                    ,hero[Client_ID].y + 80
                    };
                }
            }

            if (downMove == true) {
                if (hero[Client_ID].y <= 520) {
                    hero[Client_ID].y += 5;
                    hero[Client_ID].rc = RECT{
                             hero[Client_ID].x + 10
                             ,hero[Client_ID].y + 10
                             ,hero[Client_ID].x + 80
                             ,hero[Client_ID].y + 80
                    };
                }
            }

            if (attackState == true)     // �����̽� Ű
            {
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
            }

            for (int j = 0; j < bulletMax; j++)
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

            EnterCriticalSection(&cs);
            ++MonsterSpawnTick;
            //monster spawn
            if (MonsterSpawnTick > 300) {
                if (MonsterWave < 10) {
                    MonsterSpawn(rand() % 3 + 1);
                    MonsterWave += 1;
                    MonsterSpawnTick = 0;
                }
            }
            LeaveCriticalSection(&cs);

            //boss spawn
            if (MonsterWave == 10) {
                if (BossWave == 1 && boss.isActivated == false) {
                    boss.isActivated = true;
                    EnterCriticalSection(&cs);
                    BossWave = 0;
                    LeaveCriticalSection(&cs);
                    BossSpawn();
                }
            }

            //boss move
            if (boss.isActivated == true) {
                if (boss.y < 15) {
                    boss.x = boss.x;
                    boss.y = boss.y + 1;
                    boss.rc = RECT{
                        boss.x
                        ,boss.y
                        ,boss.x + 200
                        ,boss.y + 200
                    };
                }
                else if (boss.y >= 15) {
                    EnterCriticalSection(&cs);
                    ++BossBulletTick;
                    LeaveCriticalSection(&cs);
                    boss.y = 15;
                    boss.rc = RECT{
                        boss.x
                        ,boss.y
                        ,boss.x + 200
                        ,boss.y + 200
                    };
                    // �¿�� ����������
                    if (direct == 0) {
                        boss.x -= 1;
                        boss.rc = RECT{
                        boss.x
                        ,boss.y
                        ,boss.x + 200
                        ,boss.y + 200
                        };
                        if (boss.x <= 3) {
                            EnterCriticalSection(&cs);
                            direct = 1;
                            LeaveCriticalSection(&cs);
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
                            EnterCriticalSection(&cs);
                            direct = 0;
                            LeaveCriticalSection(&cs);
                        }
                    }


                }
            }
            //boss bullet spawn
            if (BossBulletTick > 150) {
                for (int i = 0; i < 5; i++) {
                    if (boss.BossBulletArr[i].isFire == false)
                    {
                        boss.BossBulletArr[i].isFire = true;
                        boss.BossBulletArr[i].x = boss.x + 100;
                        boss.BossBulletArr[i].y = boss.y + 100;
                        boss.BossBulletArr[i].rc = RECT{
                            boss.BossBulletArr[i].x + 100
                            ,boss.BossBulletArr[i].y + 100
                            ,boss.BossBulletArr[i].x + 126
                            ,boss.BossBulletArr[i].y + 126
                        };
                        break;
                    }
                }
                EnterCriticalSection(&cs);
                BossBulletTick = 0;
                LeaveCriticalSection(&cs);
            }


            // boss bullet move
            for (int j = 0; j < 5; j++)
            {
                if (boss.BossBulletArr[j].isFire == true)
                {
                    boss.BossBulletArr[j].y += 2;
                    boss.BossBulletArr[j].rc = RECT{
                                     boss.BossBulletArr[j].x
                                     ,boss.BossBulletArr[j].y
                                     ,boss.BossBulletArr[j].x + 40
                                     ,boss.BossBulletArr[j].y + 40
                    };
                }
                if (boss.BossBulletArr[j].y > 640) {
                    boss.BossBulletArr[j].isFire = false;
                    boss.BossBulletArr[j].x = -100;
                    boss.BossBulletArr[j].y = -100;
                    boss.BossBulletArr[j].rc = RECT{
                                boss.BossBulletArr[j].x
                                ,boss.BossBulletArr[j].y
                                ,boss.BossBulletArr[j].x + 40
                                ,boss.BossBulletArr[j].y + 40
                    };

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
                    monsters[i].x = -70;
                    monsters[i].y = -70;
                    monsters[i].rc = RECT{
                            monsters[i].x
                            ,monsters[i].y
                            ,monsters[i].x + 55
                            ,monsters[i].y + 55
                    };
                }
            }


            // ------------------------------------ //
            // ------------------------------------ //
            // --------------Collide--------------- //
            // ------------------------------------ //
            // ------------------------------------ //
             // �Ѿ˰� ������ �浹
            for (int i = 0; i < monsterMax; ++i) {
                for (int j = 0; j < bulletMax; ++j) {

                    if (IsCollide(hero[Client_ID].BulletArr[j].rc, monsters[i].rc) == true)
                    {
                        hero[Client_ID].point += 100;
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
                        monsters[i].x = -70;
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
            // ���Ϳ� �÷��̾� ���� �浹
            for (int j = 0; j < monsterMax; ++j) {
                if (IsCollide(hero[Client_ID].rc, monsters[j].rc) == true)
                {
                    hero[Client_ID].point -= 500;
                    monsters[j].isActivated = false;
                    monsters[j].x = -70;
                    monsters[j].y = -70;
                    monsters[j].rc = RECT{
                            monsters[j].x
                            ,monsters[j].y
                            ,monsters[j].x + 55
                            ,monsters[j].y + 55
                    };
                }
            }

            // ������ �÷��̾� ���� �浹
            if (IsCollide(hero[Client_ID].rc, boss.rc) == true) {
                hero[Client_ID].point -= 1000;
                if (Client_ID == 0) {
                    hero[Client_ID].x = 0;
                    hero[Client_ID].y = 460;
                    hero[Client_ID].rc = RECT{
                        hero[Client_ID].x + 10
                        ,hero[Client_ID].y + 10
                        ,hero[Client_ID].x + 80
                        ,hero[Client_ID].y + 80
                    };
                }
                else if (Client_ID == 1) {
                    hero[Client_ID].x = 350;
                    hero[Client_ID].y = 460;
                    hero[Client_ID].rc = RECT{
                        hero[Client_ID].x + 10
                        ,hero[Client_ID].y + 10
                        ,hero[Client_ID].x + 80
                        ,hero[Client_ID].y + 80
                    };
                }
            }

            // ������ �÷��̾� �Ѿ� ���� �浹
            for (int j = 0; j < bulletMax; ++j) {
                if (IsCollide(hero[Client_ID].BulletArr[j].rc, boss.rc) == true)
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

                    boss.hp -= 1;
                    if (boss.hp <= 0) {
                        if (hero[0].point > hero[1].point) {
                            hero[0].winlose = true;
                            hero[1].winlose = false;
                        }
                        else if (hero[0].point < hero[1].point) {
                            hero[0].winlose = false;
                            hero[1].winlose = true;
                        }
                        boss.dead = true;
                        BossWave = 0;
                        boss.isActivated = false;
                        boss.x = 120;
                        boss.y = -250;
                        boss.rc = RECT{
                            boss.x
                            ,boss.y
                            ,boss.x + 200
                            ,boss.y + 200
                        };
                    }

                }
            }

            // ���� �Ѿ˰� �÷��̾� ���� �浹
            for (int j = 0; j < 5; ++j) {
                if (IsCollide(hero[Client_ID].rc, boss.BossBulletArr[j].rc) == true)
                {
                    hero[Client_ID].point -= 500;
                    boss.BossBulletArr[j].isFire = false;
                    boss.BossBulletArr[j].x = -100;
                    boss.BossBulletArr[j].y = -100;
                    boss.BossBulletArr[j].rc = RECT{
                                boss.BossBulletArr[j].x
                                ,boss.BossBulletArr[j].y
                                ,boss.BossBulletArr[j].x + 40
                                ,boss.BossBulletArr[j].y + 40
                    };
                }
            }


            send(clientSock, (char*)&monsters, sizeof(monsters), 0);
            send(clientSock, (char*)&hero, sizeof(hero), 0);
            send(clientSock, (char*)&boss, sizeof(boss), 0);

            /*       ResetEvent(hReadEvent);
                   SetEvent(hOperEvent);*/

        }
        EnterCriticalSection(&cs);
        StartTime = NowTime;
        LeaveCriticalSection(&cs);
    }

    closesocket(clientSock);//������ �����Ѵ�.
    return 0;
}

DWORD WINAPI Operation_Thread(LPVOID arg)
{

    while (true) {
        /*WaitForSingleObject(hOperEvent, INFINITE);*/



        /*ResetEvent(hOperEvent);
        SetEvent(hReadEvent);*/
    }
    return 0;
}

bool IsCollide(RECT rc1, RECT rc2)
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
    boss.hp = 50;
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