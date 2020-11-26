#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <conio.h>
#include <atlimage.h>   // CImage 클래스가 정의된 헤더

using namespace std;
#pragma comment (lib,"WS2_32.LIB")

#define SERVERPORT 9000
#define SERVERIP "127.0.0.1"
#define BUFSIZE 1024

#define KEY_NULL   '0'
#define KEY_DOWN   '1'
#define KEY_LEFT   '2'
#define KEY_RIGHT  '3'
#define KEY_UP     '4'
#define KEY_SPACE  '5'
#define KEY_SPACE_NULL '6'

#define bulletMax 10

int Window_Size_X = 460;
int Window_Size_Y = 614;
float timer = 0.f;              // 시간
HINSTANCE g_hInst;
LPCTSTR lpszClass = "Window Class Name";
LPCTSTR lpszWindowName = "NGP";
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
DWORD WINAPI Server_Thread(LPVOID arg);
int recvn(SOCKET s, char* buf, int len, int flags);

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
        Window_Size_Y,
        NULL,
        (HMENU)NULL,
        hInstance,
        NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 소켓 통신 스레드 생성
    CreateThread(NULL, 0, Server_Thread, NULL, 0, NULL);

    while (GetMessage(&Message, 0, 0, 0)) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }
    return Message.wParam;
}

#pragma pack(push,1)
struct KEY {
    short cKey;
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

CHero hero[2];

static KEY keyInfo{ KEY_NULL };    // 입력된 키 정보 구조체
static bool SockConnect = false;
static bool MyRect = false;
CImage imgBackGround;
CImage imgBackBuff;
CImage heroimg;
CImage heroimg2;

static char messageBuffer[BUFSIZE];

static SOCKET sock;

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

    // Hero img load
    heroimg.Load(TEXT("hero.png"));
    heroimg2.Load(TEXT("hero2.png"));

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

    //BG
    imgBackGround.Draw(memDC, 0, 0, 460, 614);

    // hero draw
    if (true == MyRect)
    {
        for (int i = 0; i < 2; ++i)
        {
            if (true == hero[i].connect)
            {
                if (keyInfo.id == i)
                {
                    heroimg.Draw(memDC, hero[i].x, 460, 90, 90);
                }

                else
                {
                    heroimg2.Draw(memDC, hero[i].x, 460, 90, 90);
                }
            }
        }
    }

    InvalidateRect(hWnd, NULL, FALSE);


    SetBkMode(memDC, TRANSPARENT);

    imgBackBuff.Draw(hdc, 0, 0);
    imgBackBuff.ReleaseDC();
    EndPaint(hWnd, &ps);


}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC hdc;
    PAINTSTRUCT ps;


    switch (message)
    {
#pragma region 초기화

    case WM_CREATE:
    {
        // 이미지 로드
        ImgLoad();

        //// 백 버퍼 생성
        imgBackBuff.Create(Window_Size_X, Window_Size_Y, 24);
        break;
    }
#pragma endregion

#pragma region PAINT
    case WM_PAINT:
        OnDraw(hWnd);
        InvalidateRect(hWnd, NULL, FALSE);

        break;
#pragma endregion

#pragma region 타이머
    case WM_TIMER:
        InvalidateRect(hWnd, NULL, FALSE);
        break;

#pragma endregion

#pragma region 키입력
    case WM_KEYUP:
        if (wParam == VK_RIGHT) {
            keyInfo.cKey = 4;
        }
        else if (wParam == VK_LEFT) {
            keyInfo.cKey = 4;
        }
        else if (wParam == VK_SPACE) {
            keyInfo.cKey = 5;
        }
        break;

    case WM_KEYDOWN:

        if (wParam == VK_RIGHT)
        {
            keyInfo.cKey = 1;
            //send(sock, (char*)&keyInfo, sizeof(KEY), 0);
        }
        else if (wParam == VK_LEFT)
        {
            keyInfo.cKey = 2;
            //send(sock, (char*)&keyInfo, sizeof(KEY), 0);
        }
        else if (wParam == VK_SPACE)
        {
            keyInfo.cKey = 3;
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
    recvn(sock, messageBuffer, sizeof(CHero), 0);
    hero[0] = *(CHero*)messageBuffer;
    keyInfo.id = hero[0].id;

    MyRect = true;

    while (1) {
        recvn(sock, (char*)&timer, sizeof(timer), 0);

        send(sock, (char*)&keyInfo, sizeof(KEY), 0);

        recvn(sock, (char*)&hero, sizeof(hero), 0);
    }
    closesocket(sock);
    exit(1);
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