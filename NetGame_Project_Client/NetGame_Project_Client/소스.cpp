#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <conio.h>
#include <atlimage.h>   // CImage 클래스가 정의된 헤더
#include "protocol.h"
using namespace std;

#pragma comment (lib,"WS2_32.LIB")
#pragma comment(lib,"winmm")

#define SERVERIP "127.0.0.1"
#define SERVERPORT 9000

#define userMax    2 
#define monsterMax 5
#define bulletMax  10

int Window_Size_X = 460;
int Window_Size_Y = 614;

HINSTANCE g_hInst;
LPCTSTR lpszClass = "Window Class Name";
LPCTSTR lpszWindowName = "NGP";
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
DWORD WINAPI Server_Thread(LPVOID arg);
int recvn(SOCKET s, char* buf, int len, int flags);


SOCKET sock;

CHero hero[2];
HeroBullet hbullet[2];
BossMonster boss;
Monster monster[5];

KEY keyInfo;    // 입력된 키 정보 구조체
bool MyDragon = false;

CImage imgBackGround;
CImage imgBackBuff;
CImage heroimg;
CImage heroimg2;
CImage HBullet[10];
CImage monsterimg[5];
CImage bossimg;
CImage bossbullet[5];
CImage waitingimg;
CImage winimg;
CImage loseimg;

TCHAR str[20];
TCHAR str2[20];

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
    HWND hWnd;
    MSG Message;
    WNDCLASSEX WndClass;
    g_hInst = hInstance;

    WndClass.cbSize = sizeof(WndClass);
    WndClass.style = CS_HREDRAW | CS_VREDRAW;
    WndClass.lpfnWndProc = (WNDPROC)WndProc;
    WndClass.cbClsExtra = 0;
    WndClass.cbWndExtra = 0;
    WndClass.hInstance = hInstance;
    WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    WndClass.lpszMenuName = NULL;
    WndClass.lpszClassName = lpszClass;
    WndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassEx(&WndClass);

    hWnd = CreateWindow

    (lpszClass,
        lpszWindowName,
        (WS_OVERLAPPEDWINDOW),
        0,
        0,
        Window_Size_X,
        Window_Size_Y + 40,
        NULL,
        (HMENU)NULL,
        hInstance,
        NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    CreateThread(NULL, 0, Server_Thread, NULL, 0, NULL);

    while (GetMessage(&Message, 0, 0, 0)) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }
    return Message.wParam;
}


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
#pragma endregion 오류 출력 부분

void ImgLoad() {
    // BG img load
    imgBackGround.Load(TEXT("BG.png"));
    waitingimg.Load(TEXT("wait.png"));
    winimg.Load(TEXT("win.png"));
    loseimg.Load(TEXT("lose.png"));

    // Hero img load
    heroimg.Load(TEXT("hero.png"));
    heroimg2.Load(TEXT("hero2.png"));

    for (int i = 0; i < bulletMax; ++i) {
        HBullet[i].Load(TEXT("bullet.png"));
    }

    for (int i = 0; i < monsterMax; ++i) {
        monsterimg[i].Load(TEXT("monster.png"));
    }

    bossimg.Load(TEXT("boss.png"));

    for (int i = 0; i < 5; ++i) {
        bossbullet[i].Load(TEXT("BossBullet.png"));
    }
}

void OnDraw(HWND hWnd)
{
    // 윈도우 전체 크기
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    HDC memDC = imgBackBuff.GetDC();

    // 초기화
    FillRect(memDC, &rcClient, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    if (boss.dead == true) {
        for (int i = 0; i < userMax; ++i)
        {
            if (true == hero[i].connect)
            {
                if (keyInfo.id == i) {
                    if (hero[i].winlose == true) {
                        winimg.Draw(memDC, 0, 0, 460, 614);
                    }
                    else if (hero[i].winlose == false) {
                        loseimg.Draw(memDC, 0, 0, 460, 614);
                    }
                }
            }
        }
    }

    else
    {
        if (hero[0].connect == true && hero[1].connect == true) {
            //BG
            imgBackGround.Draw(memDC, 0, 0, 460, 614);

            if (hero[0].connect == true && hero[1].connect == true) {
                for (int i = 0; i < monsterMax; ++i) {
                    if (monster[i].isActivated == true) {
                        monsterimg[i].Draw(memDC, monster[i].x, monster[i].y, monster[i].size, monster[i].size);
                    }
                }
            }

            if (hero[0].connect == true && hero[1].connect == true) {
                if (boss.isActivated == true) {
                    bossimg.Draw(memDC, boss.x, boss.y, 200, 200);
                    for (int i = 0; i < 5; ++i) {
                        if (boss.BossBulletArr[i].isFire == true) {
                            bossbullet[i].Draw(memDC, boss.BossBulletArr[i].x, boss.BossBulletArr[i].y, 40, 40);
                        }
                    }
                }

            }

            if (hero[0].connect == true) {
                sprintf(str, "1P : %d", hero[0].point);
                TextOut(memDC, 10, 10, str, lstrlen(str));
            }
            if (hero[1].connect == true) {
                sprintf(str, "2P : %d", hero[1].point);
                TextOut(memDC, 10, 30, str, lstrlen(str));
            }

            // hero draw
            if (true == MyDragon)
            {
                for (int i = 0; i < userMax; ++i)
                {
                    if (true == hero[i].connect)
                    {
                        if (keyInfo.id == i)
                        {
                            heroimg.Draw(memDC, hero[i].x, hero[i].y, 90, 90);
                            // 총알
                            for (int j = 0; j < 10; j++)
                            {
                                if (hero[i].BulletArr[j].isFire == true)
                                {
                                    HBullet[j].Draw(memDC, hero[i].BulletArr[j].x + 15, hero[i].BulletArr[j].y, 64, 64);
                                }
                            }
                        }

                        else
                        {
                            heroimg2.Draw(memDC, hero[i].x, hero[i].y, 90, 90);
                            // 총알
                            for (int j = 0; j < bulletMax; j++)
                            {
                                if (hero[i].BulletArr[j].isFire == true)
                                {
                                    HBullet[j].Draw(memDC, hero[i].BulletArr[j].x + 15, hero[i].BulletArr[j].y, 64, 64);
                                }
                            }
                        }
                    }
                }
            }
        }
        else {
            waitingimg.Draw(memDC, 0, 0, 460, 614);
        }
    }


    SetBkMode(memDC, TRANSPARENT);

    imgBackBuff.Draw(hdc, 0, 0);
    imgBackBuff.ReleaseDC();
    EndPaint(hWnd, &ps);
}

DWORD WINAPI Server_Thread(LPVOID arg)
{
    // send, recv 함수 출력값 저장용
    int retval;

    // 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    // socket()
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) err_quit("socket()");

    // connect()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = connect(sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR) err_quit("connect()");


    //hero.id 수신
    recvn(sock, (char*)&hero, sizeof(CHero), 0);
    keyInfo.id = hero[0].id;


    MyDragon = true;

    while (1) {

        recvn(sock, (char*)&monster, sizeof(monster), 0);
        recvn(sock, (char*)&hero, sizeof(hero), 0);
        recvn(sock, (char*)&boss, sizeof(boss), 0);

    }
    closesocket(sock);
    exit(1);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC hdc;
    PAINTSTRUCT ps;


    switch (message)
    {
#pragma region 초기화

    case WM_CREATE:

        // 이미지 로드
        ImgLoad();

        //// 백 버퍼 생성
        imgBackBuff.Create(Window_Size_X, Window_Size_Y, 24);
        SetTimer(hWnd, 0, 30, NULL);

        break;

#pragma endregion

    case WM_TIMER:

        send(sock, (char*)&keyInfo, sizeof(keyInfo), 0);
        InvalidateRect(hWnd, NULL, FALSE);
        break;

#pragma region PAINT
    case WM_PAINT:

        OnDraw(hWnd);

        break;

#pragma endregion


#pragma region 키입력
    case WM_KEYUP:
        if (wParam == VK_RIGHT) {
            keyInfo.right = false;
        }
        else if (wParam == VK_LEFT) {
            keyInfo.left = false;
        }
        else if (wParam == VK_SPACE) {
            keyInfo.space = false;
        }
        else if (wParam == VK_UP) {
            keyInfo.up = false;
        }
        else if (wParam == VK_DOWN) {
            keyInfo.down = false;
        }
        break;

    case WM_KEYDOWN:

        if (wParam == VK_RIGHT)
        {
            keyInfo.right = true;
        }
        else if (wParam == VK_LEFT)
        {
            keyInfo.left = true;
        }
        else if (wParam == VK_UP) {
            keyInfo.up = true;
        }
        else if (wParam == VK_DOWN) {
            keyInfo.down = true;
        }
        else if (wParam == VK_SPACE) {
            keyInfo.space = true;
        }

        InvalidateRect(hWnd, NULL, FALSE); // FALSE로 하면 이어짐  
        break;

#pragma endregion

    case WM_DESTROY:
        // CImage는 해제해도 되고 안 해도 됨
        WSACleanup();
        imgBackBuff.Destroy();
        PostQuitMessage(0);
        break;

    default: return DefWindowProc(hWnd, message, wParam, lParam);
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