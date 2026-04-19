#if RETRO_USE_NETWORKING
#ifndef NETWORKING_H
#define NETWORKING_H
#if RETRO_PLATFORM != RETRO_PS3
#include <thread>
#include <memory>
#endif

#define PACKET_SIZE 512

struct RoomInfo {
    uint code;
    char username[20];
    byte rounds;
    byte itemMode;
    char modName[20];
};

extern char networkHost[64];
extern char networkUsername[20];
extern RoomInfo availableRooms[10];
extern int availableRoomCount;
extern char networkGame[7];
extern int networkPort;
extern bool useHostServer;

extern float lastPing;
extern int dcError;
extern bool waitingForPing;
extern char publicIP[16];

struct MultiplayerData {
    int type;
    int data[(PACKET_SIZE - 16) / sizeof(int) - 1];
};

struct ServerPacket {
    byte header;
    char game[7];
    uint player;
    uint room;

    union {
        unsigned char bytes[PACKET_SIZE - 16];
        MultiplayerData multiData;
    } data;
};

enum ClientHeaders {
    CL_REQUEST_CODE = 0x00,
    CL_JOIN         = 0x01,
    CL_LIST_ROOMS   = 0x02,

    CL_DATA          = 0x10,
    CL_DATA_VERIFIED = 0x11,

    CL_QUERY_VERIFICATION = 0x20,

    CL_LEAVE = 0xFF
};
enum ServerHeaders {
    SV_CODES      = 0x00,
    SV_NEW_PLAYER = 0x01,
    SV_ROOM_LIST  = 0x02,

    SV_DATA          = 0x10,
    SV_DATA_VERIFIED = 0x11,

    SV_RECEIVED     = 0x20,
    SV_VERIFY_CLEAR = 0x21,

    SV_INVALID_HEADER = 0x80,
    SV_NO_ROOM        = 0x81,
    SV_UNKNOWN_PLAYER = 0x82,

    SV_LEAVE = 0xFF
};

class NetworkSession;

#if RETRO_PLATFORM == RETRO_PS3
extern NetworkSession *session;
#else
extern std::shared_ptr<NetworkSession> session;
#endif

void InitNetwork();
void RunNetwork();
void UpdateNetwork();
void SendData(bool verify = false);
void DisconnectNetwork(bool finalClose = false);
void SendServerPacket(ServerPacket &send, bool verify = false);
int GetRoomCode();
void SetRoomCode(int code);
int GetNetworkCode();

void SetNetworkGameName(int *unused, const char *name);

#endif
#endif
