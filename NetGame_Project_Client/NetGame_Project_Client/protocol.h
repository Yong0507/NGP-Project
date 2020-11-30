#pragma once

#define KEY_DOWN        '1'
#define KEY_LEFT        '2'
#define KEY_RIGHT       '3'
#define KEY_UP          '4'
#define KEY_SPACE       '5'

#define SERVERPORT 9000


#pragma pack(push,1)

struct KEY {
    bool up, left, right, down, space;
    short id;
};

struct HeroBullet {
    bool isFire;
    short x, y;
    RECT rc;
};

struct CHero {
    short x, y;
    bool connect;
    short id;
    RECT rc;
    HeroBullet BulletArr[10];
    int point;
    bool winlose;
};

struct BossBullet {
    bool isFire;
    short x, y;
    RECT rc;
};

struct Monster {
    short x, y;
    short size;
    bool isActivated;
    RECT rc;
};

struct BossMonster {
    short x, y;
    bool isActivated;
    short hp;
    RECT rc;
    BossBullet BossBulletArr[5];
    bool dead = false;
};

#pragma pack(pop)