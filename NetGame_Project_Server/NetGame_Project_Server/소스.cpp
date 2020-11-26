#define _WINSOCK_DEPRECATED_NO_WARNINGS // 최신 VC++ 컴파일 시 경고 방지
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

float timer = 0.f;              // 시간
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
    // 이벤트 생성
    hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
    if (hReadEvent == NULL) return 1;
    hOperEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
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


    // 데이터 통신에 사용할 변수
    SOCKET client_sock;
    SOCKADDR_IN clientaddr;
    int addrlen;

    // Timer 쓰레드 생성
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

        // 즉시전송 (Nagle 알고리즘 해제)
//	int opt_val = TRUE; // FALSE : Nagle 알고리즘 설정
//	setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&opt_val, sizeof(opt_val));

        clientCount++;
        clientSocks[clientCount] = client_sock;
        //hero[clientCount].connect = true;
        //hero[clientCount].id = clientCount;
        hero[clientCount] = CHero{ 0,400,true,(short)clientCount,NULL,false };
        hero[clientCount].id = clientCount;

        cout << "접속한 클라 개수 : " << clientCount << endl;
        cout << "hero [" << clientCount << "].id : " << hero[clientCount].id << endl;

        hThread = CreateThread(NULL, 0, Client_Thread, (LPVOID)&client_sock, 0, NULL);//HandleClient 쓰레드 실행, clientSock을 매개변수로 전달
        printf("Connected Client IP : %s\n", inet_ntoa(clientaddr.sin_addr));
        timer = 0.f;

        hThread2 = CreateThread(NULL, 0, Operation_Thread, (LPVOID)&client_sock, 0, NULL);
    }

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
    int i = 0;
    // hero.id 송신
    send(clientSocks[clientCount], (char*)&hero[clientCount], sizeof(CHero), 0);

    unsigned int n = 0;

    while (1) {

        // 1초에 30번씩 전송한다.
        if (timer > (1.f / 30.f) * n)
        {
            cout << timer << endl;
            // timer 전송
            send(clientSock, (char*)&timer, sizeof(timer), 0);

            recvn(clientSock, (char*)&keyInfo, sizeof(KEY), 0);
            
            KeyMessage(&keyInfo.cKey, hero[keyInfo.id]);

            send(clientSock, (char*)&hero, sizeof(hero), 0);
            n++;
        }

    }

    closesocket(clientSock);//소켓을 종료한다.
    return 0;
}

DWORD WINAPI Operation_Thread(LPVOID arg)
{
    SOCKET clientSock = *((SOCKET*)arg); //매개변수로받은 클라이언트 소켓을 전달
    WaitForSingleObject(hOperEvent, INFINITE);

    KeyMessage(&keyInfo.cKey, hero[keyInfo.id]);

    send(clientSock, (char*)&hero, sizeof(hero), 0);

    SetEvent(hReadEvent);
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

// Timer 변경 쓰레드
DWORD WINAPI Timer_Thread(LPVOID arg)
{
    int retval;


    __int64 past_time = -1;
    while (1) {
        // 시간 측정 함수
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
