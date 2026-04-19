#include "RetroEngine.hpp"
#include "UPnP.hpp"
#if RETRO_USE_NETWORKING

#include <cstdlib>
#include <deque>
#include <iostream>
#if RETRO_PLATFORM != RETRO_PS3
#include <functional>
#include <thread>
#include <chrono>
#endif
#if RETRO_PLATFORM == RETRO_ANDROID
#define ASIO_NO_TYPEID
#endif
#if RETRO_PLATFORM == RETRO_PS3
#include <netex/net.h>
#include <netex/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#ifndef FIONBIO
#define FIONBIO 0x8004667e
#endif

#else
#include <asio.hpp>
#endif

char networkHost[64];
char networkUsername[20] = "Player";
RoomInfo availableRooms[10];
int availableRoomCount = 0;
char networkGame[7] = "SONIC2";
int networkPort     = 30000;
bool useHostServer  = false;
int dcError         = 0;
float lastPing      = 0;

bool waitingForPing = false;
char publicIP[16]   = "0.0.0.0";

uint64_t lastTime = 0;

#if RETRO_PLATFORM != RETRO_PS3
using asio::ip::udp;
#endif


typedef std::deque<ServerPacket> DataQueue;

#if RETRO_IS_BIG_ENDIAN
void SwapEntityEndian(Entity *e)
{
    // Use local variables to handle alignment safely on PS3
    int temp;
#define SWAP_ENTITY_FIELD(field) \
    memcpy(&temp, &e->field, sizeof(int)); \
    SWAP_ENDIAN(temp); \
    memcpy(&e->field, &temp, sizeof(int));

    SWAP_ENTITY_FIELD(xpos);
    SWAP_ENTITY_FIELD(ypos);
    SWAP_ENTITY_FIELD(xvel);
    SWAP_ENTITY_FIELD(yvel);
    SWAP_ENTITY_FIELD(speed);
    for (int i = 0; i < 48; ++i) {
        memcpy(&temp, &e->values[i], sizeof(int));
        SWAP_ENDIAN(temp);
        memcpy(&e->values[i], &temp, sizeof(int));
    }
    SWAP_ENTITY_FIELD(state);
    SWAP_ENTITY_FIELD(angle);
    SWAP_ENTITY_FIELD(scale);
    SWAP_ENTITY_FIELD(rotation);
    SWAP_ENTITY_FIELD(alpha);
    SWAP_ENTITY_FIELD(animationTimer);
    SWAP_ENTITY_FIELD(animationSpeed);
    SWAP_ENTITY_FIELD(lookPosX);
    SWAP_ENTITY_FIELD(lookPosY);
    
    ushort tempShort;
    memcpy(&tempShort, &e->groupID, sizeof(ushort));
    SwapEndian(&tempShort, sizeof(ushort));
    memcpy(&e->groupID, &tempShort, sizeof(ushort));
#undef SWAP_ENTITY_FIELD
}

void SwapPacketEndian(ServerPacket *p, bool receiving)
{
    uint l_player, l_room;
    memcpy(&l_player, &p->player, sizeof(uint));
    memcpy(&l_room, &p->room, sizeof(uint));
    SWAP_ENDIAN(l_player);
    SWAP_ENDIAN(l_room);
    memcpy(&p->player, &l_player, sizeof(uint));
    memcpy(&p->room, &l_room, sizeof(uint));

    int type;
    memcpy(&type, &p->data.multiData.type, sizeof(int));
    if (receiving) {
        // LE (from packet) -> BE (Host)
        SwapEndian(&type, sizeof(int));
        // type is now Host-endian
        int hostType = type;
        // Decided to keep the packet Host-endian internally after reception for easier handling
        memcpy(&p->data.multiData.type, &type, sizeof(int));
        
        if (p->header == SV_ROOM_LIST || p->header == CL_REQUEST_CODE || p->header == CL_JOIN) {
            // These packets contain strings or handled specifically
            // We still need to swap room and player IDs (done above)
            // and we might need to handle the 'type' field which is an int
            return;
        }

        if (hostType == 1 && (p->header == SV_DATA || p->header == SV_DATA_VERIFIED || p->header == CL_DATA || p->header == CL_DATA_VERIFIED)) {
            SwapEntityEndian((Entity *)p->data.multiData.data);
        }
        else {
            int *data = (int *)p->data.multiData.data;
            int count = (PACKET_SIZE - 16 - sizeof(int)) / sizeof(int);
            for (int i = 0; i < count; ++i) {
                int val;
                memcpy(&val, &data[i], sizeof(int));
                SWAP_ENDIAN(val);
                memcpy(&data[i], &val, sizeof(int));
            }
        }
    }
    else {
        // BE (Host) -> LE (to packet)
        int hostType = type;

        if (p->header == CL_REQUEST_CODE || p->header == CL_JOIN) {
            // Strings at multiData.data[1] (offset 20)
            // settings at multiData.data[0] (offset 16)
            int settings;
            memcpy(&settings, &p->data.multiData.data[0], sizeof(int));
            SWAP_ENDIAN(settings);
            memcpy(&p->data.multiData.data[0], &settings, sizeof(int));
            
            // Swap type last
            SwapEndian(&type, sizeof(int));
            memcpy(&p->data.multiData.type, &type, sizeof(int));
            return;
        }

        if (hostType == 1) {
            SwapEntityEndian((Entity *)p->data.multiData.data);
        }
        else {
            int *data = (int *)p->data.multiData.data;
            int count = (PACKET_SIZE - 16 - sizeof(int)) / sizeof(int);
            for (int i = 0; i < count; ++i) {
                int val;
                memcpy(&val, &data[i], sizeof(int));
                SWAP_ENDIAN(val);
                memcpy(&data[i], &val, sizeof(int));
            }
        }
        // Swap the type last so hostType check works
        SwapEndian(&type, sizeof(int));
        memcpy(&p->data.multiData.type, &type, sizeof(int));
    }
}
#endif

#if RETRO_PLATFORM == RETRO_PS3
class NetworkSession
{
public:
    bool running;
    int sessionSocket;
    struct sockaddr_in addr;

    uint code;
    uint partner;
    uint room;

    bool awaitingReceive;
    bool verifyReceived;

    bool retried;
    uint retries;
    uint64_t timer;

    ServerPacket repeat;
    ServerPacket read_msg_;
    DataQueue write_msgs_;
    sys_lwmutex_t writeMutex __attribute__((aligned(16)));

    NetworkSession(const char *host, int port)
    {
        sys_lwmutex_attribute_t attr;
        sys_lwmutex_attribute_initialize(attr);
        if (sys_lwmutex_create(&writeMutex, &attr) != CELL_OK) {
             // PrintLog("NetworkSession - Failed to create mutex");
        }

        running         = false;
        code            = 0;
        partner         = 0;
        room            = 0;
        awaitingReceive = false;
        verifyReceived  = false;
        retried         = true;
        retries         = 0;
        timer           = 0;

        sessionSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sessionSocket < 0) {
             // PrintLog("NetworkSession - Failed to create socket: %d", sessionSocket);
            running = false;
            return;
        }

        memset(&addr, 0, sizeof(addr));
#if RETRO_PLATFORM == RETRO_PS3
        addr.sin_len    = sizeof(addr);
#endif
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        
        struct hostent *he = gethostbyname(host);
        if (he) {
             // PrintLog("NetworkSession - Resolved %s to %s", host, inet_ntoa(*(struct in_addr *)he->h_addr));
            memcpy(&addr.sin_addr, he->h_addr, he->h_length);
        } else {
            addr.sin_addr.s_addr = inet_addr(host);
             // PrintLog("NetworkSession - Using IP %s", host);
        }

        int opt = 1;
        setsockopt(sessionSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        
        // Non-blocking
        int on = 1;
        setsockopt(sessionSocket, SOL_SOCKET, SO_NBIO, (const char*)&on, sizeof(on));
    }

    ~NetworkSession()
    {
        close();
        sys_lwmutex_destroy(&writeMutex);
    }

    void write(const ServerPacket &msg, bool repeat = false)
    {
        if (!running) return;

        // Ignore game data packets if we are not connected yet
        if (!code && msg.header != CL_REQUEST_CODE && msg.header != CL_JOIN && msg.header != CL_LIST_ROOMS)
            return;

        ServerPacket sent = msg;
        sent.player = code;
        
        if (sent.header == CL_REQUEST_CODE) {
            // Ensure request code always has the magic room ID
            sent.room = 0x1F2F3F4F;
        }
        else if (sent.header == CL_JOIN) {
            // Keep the room ID provided for joining
        }
        else {
            sent.room = room;
        }
        
        sys_lwmutex_lock(&writeMutex, 0);
        write_msgs_.push_back(sent);
        sys_lwmutex_unlock(&writeMutex);

        if (repeat) {
            this->repeat = sent;
            this->retries = 0;
            this->timer = sys_time_get_system_time();
        }
    }

    void start()
    {
         // PrintLog("NetworkSession::start()");
        sys_lwmutex_lock(&writeMutex, 0);
        write_msgs_.clear();
        sys_lwmutex_unlock(&writeMutex);

        running       = true;

        ServerPacket send;
        memset(&send, 0, sizeof(ServerPacket));
        send.header = CL_REQUEST_CODE;
        send.room   = 0x1F2F3F4F;
        StrCopy((char *)&send.data.multiData.data[6], networkUsername);
        write(send, true);

        code          = 0;
        room          = 0;
        partner       = 0;
        retries       = 0;
        timer         = sys_time_get_system_time();
        lastTime      = timer;
        
        repeat.header = 0x80;
        retried       = true;
    }

    void close()
    {
        if (running)
            running = false;
        if (sessionSocket >= 0) {
#if RETRO_PLATFORM == RETRO_PS3
            shutdown(sessionSocket, SHUT_RDWR);
            socketclose(sessionSocket);
#else
            close(sessionSocket);
#endif
            sessionSocket = -1;
        }
    }

    void run()
    {
        if (sessionSocket < 0) return;

        while (true) {
            sys_lwmutex_lock(&writeMutex, 0);
            if (write_msgs_.empty()) {
                sys_lwmutex_unlock(&writeMutex);
                break;
            }
            ServerPacket sentMsg = write_msgs_.front();
            write_msgs_.pop_front();
            sys_lwmutex_unlock(&writeMutex);

            ServerPacket *send = &sentMsg;
            StrCopy(send->game, networkGame);

            ServerPacket packet = *send;
#if RETRO_IS_BIG_ENDIAN
            SwapPacketEndian(&packet, false);
#endif
            int res = sendto(sessionSocket, (const char*)&packet, sizeof(ServerPacket), 0, (struct sockaddr *)&addr, sizeof(addr));
            if (res < 0) {
                 // PrintLog("NetworkSession::run() - sendto failed: %d, errno: %d", res, errno);
            }
            
            if (send->header == 0xFF) {
                 // PrintLog("NetworkSession::run() - CL_LEAVE sent, stopping session");
                running = false;
            }
        }

        do_read();

        uint64_t now = sys_time_get_system_time();
        if (repeat.header != 0x80) {
            if (now >= timer + 1000000) { // 1 second
                handle_timer();
                timer = now;
            }
        }
        else {
            retries = 0;
            timer = now;
        }

        if (retries > 30) {
            switch (repeat.header) {
                case CL_JOIN:
                case CL_REQUEST_CODE: {
                    dcError          = 4;
                    vsPlaying        = false;
                    running          = false;
                    break;
                }
            }
        }
    }

    void leave()
    {
        ServerPacket send;
        memset(&send, 0, sizeof(ServerPacket));
        send.header = CL_LEAVE;
        vsPlaying   = false;
        write(send);
    }

private:
    void do_read()
    {
        if (sessionSocket < 0) return;

        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        // Drain the socket buffer to process all pending packets in a single frame
        while (true) {
            int res = recvfrom(sessionSocket, (char*)&read_msg_, sizeof(ServerPacket), 0, (struct sockaddr *)&from, &fromlen);
            if (res > 0) {
                handle_read(res);
            }
            else {
                break;
            }
        }
    }

    void handle_timer()
    {
        if (sessionSocket < 0) return;

        if (repeat.header != CL_REQUEST_CODE && repeat.header != CL_JOIN)
            repeat.room = room;
            
        StrCopy(repeat.game, networkGame);

         // PrintLog("NetworkSession::handle_timer() - Retrying packet: header=0x%02X, retries=%u", repeat.header, retries);
        
        ServerPacket packet = repeat;
#if RETRO_IS_BIG_ENDIAN
        SwapPacketEndian(&packet, false);
#endif
        int res = sendto(sessionSocket, (const char*)&packet, sizeof(ServerPacket), 0, (struct sockaddr *)&addr, sizeof(addr));
        if (res < 0) {
             // PrintLog("NetworkSession::handle_timer() - sendto failed: %d, errno: %d", res, errno);
        }
        
        retries++;
    }

    void handle_read(size_t bytes)
    {
#if RETRO_IS_BIG_ENDIAN
        SwapPacketEndian(&read_msg_, true);
#endif

        uint64_t now   = sys_time_get_system_time();
        lastPing       = (float)((now - lastTime) / 1000.0);
        lastTime       = now;
        
        waitingForPing = false;
        if (!code) {
            if (read_msg_.header == SV_CODES && read_msg_.player) {
                code = read_msg_.player;
                room = read_msg_.room;
                 // PrintLog("NetworkSession::handle_read() - Assigned player code: %u, room: 0x%08X", code, room);
                repeat.header = 0x80;
            }
            else {
                return;
            }
        }

        switch (read_msg_.header) {
            case SV_CODES: {
                if (vsPlaying)
                    return;
                room = read_msg_.room;
                 // PrintLog("NetworkSession::handle_read() - SV_CODES: room=0x%08X, type=%d", room, read_msg_.data.multiData.type);
                if (read_msg_.data.multiData.type == 1 && room != 0) {
                    repeat.header = 0x80;
                }

                if (read_msg_.data.multiData.type > 2) {
                     // PrintLog("NetworkSession::handle_read() - Room full");
                    dcError = 3;
                    leave();
                    return;
                }

                if (read_msg_.data.multiData.type - 1) {
                    partner = *read_msg_.data.multiData.data;
                     // PrintLog("NetworkSession::handle_read() - Match joined, partner=%u", partner);
                    repeat.header = 0x80;
                    Receive2PVSMatchCode(room);
                    return;
                }
                break;
            }
            case SV_NEW_PLAYER: {
                if (partner)
                    return;
                repeat.header = 0x80;
                vsPlayerID    = 0;
                partner       = read_msg_.player;
                 // PrintLog("NetworkSession::handle_read() - SV_NEW_PLAYER: partner=%u", partner);
                Receive2PVSMatchCode(room);
                return;
            }
            case SV_DATA_VERIFIED: {
                ServerPacket send;
                memset(&send, 0, sizeof(ServerPacket));
                send.header = SV_RECEIVED;
                SendServerPacket(send);
                Receive2PVSData(&read_msg_.data.multiData);
                return;
            }
            case SV_DATA: {
                Receive2PVSData(&read_msg_.data.multiData);
                return;
            }
            case SV_RECEIVED: {
                if (repeat.header == CL_DATA_VERIFIED) {
                    repeat.header = CL_QUERY_VERIFICATION;
                }
                else {
                    ServerPacket send;
                    memset(&send, 0, sizeof(ServerPacket));
                    send.header = SV_VERIFY_CLEAR;
                    SendServerPacket(send);
                }
                return;
            }
            case SV_VERIFY_CLEAR: {
                repeat.header = 0x80;
                return;
            }
            case SV_ROOM_LIST: {
                int count = read_msg_.data.multiData.type;
                availableRoomCount = count > 10 ? 10 : count;
                byte *ptr = (byte*)read_msg_.data.multiData.data;
                for (int i = 0; i < availableRoomCount; ++i) {
                    memcpy(&availableRooms[i].code, ptr, 4);
#if RETRO_IS_BIG_ENDIAN
                    SWAP_ENDIAN(availableRooms[i].code);
#endif
                    ptr += 4;
                    StrCopy(availableRooms[i].username, (char*)ptr);
                    ptr += 20;
                    availableRooms[i].rounds   = *ptr++;
                    availableRooms[i].itemMode = *ptr++;
                    StrCopy(availableRooms[i].modName, (char*)ptr);
                    ptr += 20;
                }
                return;
            }
            case SV_UNKNOWN_PLAYER: {
                code    = 0;
                room    = 0;
                partner = 0;
                ServerPacket send;
                memset(&send, 0, sizeof(ServerPacket));
                send.header = CL_REQUEST_CODE;
                send.room   = 0x1F2F3F4F;
                StrCopy((char *)&send.data.multiData.data[6], networkUsername);
                write(send, true);
                return;
            }
            case SV_NO_ROOM: {
                leave();
                dcError = 5;
                return;
            }
            case SV_LEAVE: {
                if (read_msg_.player != partner)
                    return;
                leave();
                dcError = 1;
                return;
            }
        }
    }
};
#else
class NetworkSession
{
public:
    bool running = false;
    NetworkSession(asio::io_context &io_context, const udp::endpoint &endpoint)
        : io_context(io_context), socket(io_context), endpoint(endpoint), timer(io_context)
    {
        socket.open(udp::v4());
    }

    ~NetworkSession() {}

    void write(const ServerPacket &msg, bool repeat = false)
    {
        if (!running) return;

        // Ignore game data packets if we are not connected yet
        if (!code && msg.header != CL_REQUEST_CODE && msg.header != CL_JOIN && msg.header != CL_LIST_ROOMS)
            return;

        ServerPacket sent(msg);
        sent.player = code;
        
        if (sent.header == CL_REQUEST_CODE) {
            // Ensure request code always has the magic room ID
            sent.room = 0x1F2F3F4F;
        }
        else if (sent.header == CL_JOIN) {
            // Keep the room ID provided for joining
        }
        else {
            sent.room = room;
        }

        write_msgs_.push_back(sent);
        if (repeat) {
            this->repeat = sent;
            this->retries = 0;
            this->retried = true;
        }
    }

    void start()
    {
         // PrintLog("NetworkSession::start()");
        memset(&repeat, 0, sizeof(ServerPacket));
        repeat.header = 0x80;

        running       = true;

        ServerPacket send;
        memset(&send, 0, sizeof(ServerPacket));
        send.header = CL_REQUEST_CODE;
        send.room   = 0x1F2F3F4F;
        StrCopy((char*)send.data.bytes, networkUsername);
        write(send, true);
    }

    void close()
    {
        if (running)
            running = false;
        if (socket.is_open())
            socket.close();
    }

    NetworkSession &operator=(const NetworkSession &s)
    {
        close();
        memcpy(this, &s, sizeof(NetworkSession));
        return *this;
    }

    uint code    = 0;
    uint partner = 0;
    uint room    = 0;

    bool awaitingReceive = false;
    bool verifyReceived  = false;

    bool retried = true;
    uint retries = 0;

    ServerPacket repeat;

    void run()
    {
        // do we write anything
        while (!write_msgs_.empty()) {
            ServerPacket *send = &write_msgs_.front();
            StrCopy(send->game, networkGame);

            ServerPacket packet = *send;
#if RETRO_IS_BIG_ENDIAN
            SwapPacketEndian(&packet, false);
            socket.send_to(asio::buffer(&packet, sizeof(ServerPacket)), endpoint);
#else
            socket.send_to(asio::buffer(&packet, sizeof(ServerPacket)), endpoint);
#endif
            write_msgs_.pop_front();
            if (send->header == 0xFF) {
                 // PrintLog("NetworkSession::run() - CL_LEAVE sent, stopping session");
                running = false;
            }
        }
        if (!awaitingReceive)
            do_read();
        if (repeat.header != 0x80 && retried) {
            retried = false;
            timer.expires_from_now(asio::chrono::seconds(1));
            timer.async_wait(std::bind(&NetworkSession::handle_timer, this, std::placeholders::_1));
        }
        else if (repeat.header == 0x80)
            retries = 0;
            
        if (retries > 30) {
            switch (repeat.header) {
                case CL_JOIN:
                case CL_REQUEST_CODE: {
                    dcError          = 4;
                    vsPlaying        = false;
                    running          = false;
                    break;
                }
            }
        }

        io_context.poll();
        io_context.restart();
    }

    void leave()
    {
        ServerPacket send;
        memset(&send, 0, sizeof(ServerPacket));
        send.header = CL_LEAVE;
        vsPlaying   = false;
        write(send);
    }

private:
    void do_read()
    {
        if (awaitingReceive)
            return;
        awaitingReceive = true;
        socket.async_receive(asio::buffer(&read_msg_, sizeof(ServerPacket)), std::bind(&NetworkSession::handle_read, this, std::placeholders::_1, std::placeholders::_2));
    }

    asio::io_context &io_context;
    asio::steady_timer timer;
    udp::socket socket;
    udp::endpoint endpoint;
    ServerPacket read_msg_;
    DataQueue write_msgs_;

    void handle_timer(const asio::error_code &ec)
    {
        if (ec || !running)
            return;
        retried = true;
        if (repeat.header != CL_REQUEST_CODE && repeat.header != CL_JOIN)
            repeat.room = room;
        StrCopy(repeat.game, networkGame);
         // PrintLog("NetworkSession::handle_timer() - Retrying packet: header=0x%02X, retries=%u", repeat.header, retries);

        if (retries < 30) {
            ServerPacket packet = repeat;
#if RETRO_IS_BIG_ENDIAN
            SwapPacketEndian(&packet, false);
#endif
            socket.send_to(asio::buffer(&packet, sizeof(ServerPacket)), endpoint);
            retries++;
        }
    }

    void handle_read(const asio::error_code &ec, size_t bytes)
    {
        awaitingReceive = false; // async, not threaded. this is safe
        if (ec || !running)
            return;

#if RETRO_IS_BIG_ENDIAN
        SwapPacketEndian(&read_msg_, true);
#endif

        // it's ok to use preformace counter; we're in a different thread and slowdown is safe
        lastPing       = ((SDL_GetPerformanceCounter() - lastTime) * 1000.0 / SDL_GetPerformanceFrequency());
        lastTime       = SDL_GetPerformanceCounter();

        waitingForPing = false;
        if (!code) {
            if (read_msg_.header == SV_CODES && read_msg_.player) {
                code = read_msg_.player;
                room = read_msg_.room;
                 // PrintLog("NetworkSession::handle_read() - Assigned player code: %u", code);
                repeat.header = 0x80;
            }
            else {
                return;
            }
        }

        switch (read_msg_.header) {
            case SV_CODES: {
                if (vsPlaying)
                    return;
                room = read_msg_.room;
                 // PrintLog("NetworkSession::handle_read() - SV_CODES: room=0x%08X, type=%d", room, read_msg_.data.multiData.type);
                if (read_msg_.data.multiData.type > 2) {
                     // PrintLog("NetworkSession::handle_read() - Room full");
                    dcError = 3;
                    leave();
                    return;
                }

                if (read_msg_.data.multiData.type - 1) {
                    partner = *read_msg_.data.multiData.data;
                     // PrintLog("NetworkSession::handle_read() - Match joined, partner=%u", partner);
                    repeat.header = 0x80;
                    Receive2PVSMatchCode(room);
                    return;
                }
                break;
            }
            case SV_NEW_PLAYER: {
                if (partner)
                    return;
                repeat.header = 0x80;
                vsPlayerID    = 0;
                partner       = read_msg_.player;
                 // PrintLog("NetworkSession::handle_read() - SV_NEW_PLAYER: partner=%u", partner);
                Receive2PVSMatchCode(room);
                return;
            }
            case SV_DATA_VERIFIED: {
                ServerPacket send;
                memset(&send, 0, sizeof(ServerPacket));
                send.header = SV_RECEIVED;
                SendServerPacket(send);
                Receive2PVSData(&read_msg_.data.multiData);
                return;
            }
            case SV_DATA: {
                Receive2PVSData(&read_msg_.data.multiData);
                return;
            }
            case SV_RECEIVED: {
                if (repeat.header == CL_DATA_VERIFIED) {
                    repeat.header = CL_QUERY_VERIFICATION;
                }
                else {
                    ServerPacket send;
                    memset(&send, 0, sizeof(ServerPacket));
                    send.header = SV_VERIFY_CLEAR;
                    SendServerPacket(send);
                }
                return;
            }
            case SV_VERIFY_CLEAR: {
                repeat.header = 0x80;
                return;
            }
            case SV_ROOM_LIST: {
                int count          = read_msg_.data.multiData.type;
                availableRoomCount = count > 10 ? 10 : count;
                byte *ptr          = (byte *)read_msg_.data.multiData.data;
                for (int i = 0; i < availableRoomCount; ++i) {
                    memcpy(&availableRooms[i].code, ptr, 4);
#if RETRO_IS_BIG_ENDIAN
                    SWAP_ENDIAN(availableRooms[i].code);
#endif
                    ptr += 4;
                    StrCopy(availableRooms[i].username, (char *)ptr);
                    ptr += 20;
                    availableRooms[i].rounds   = *ptr++;
                    availableRooms[i].itemMode = *ptr++;
                    StrCopy(availableRooms[i].modName, (char *)ptr);
                    ptr += 20;
                }
                return;
            }
            case SV_UNKNOWN_PLAYER: {
                code    = 0;
                room    = 0;
                partner = 0;
                ServerPacket send;
                memset(&send, 0, sizeof(ServerPacket));
                send.header = CL_REQUEST_CODE;
                send.room   = 0x1F2F3F4F;
                StrCopy((char *)&send.data.multiData.data[6], networkUsername);
                write(send, true);
                return;
            }
            case SV_NO_ROOM: {
                leave();
                dcError = 5;
                return;
            }
            case SV_LEAVE: {
                if (read_msg_.player != partner)
                    return;
                leave();
                dcError = 1;
                return;
            }
        }
    }
};
#endif

#if RETRO_PLATFORM == RETRO_PS3
struct RelayRoom {
    uint roomCode;
    struct sockaddr_in player1_addr;
    struct sockaddr_in player2_addr;
    uint player1_id;
    uint player2_id;
    bool player1_active;
    bool player2_active;
};

#define MAX_RELAY_ROOMS 32
static RelayRoom relayRooms[MAX_RELAY_ROOMS];
static int relaySocket = -1;
static bool relayThreadRunning = false;

static void relayLoop(uint64_t arg)
{
    relaySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (relaySocket < 0) {
         // PrintLog("RelayServer - Failed to create socket: %d", relaySocket);
        relayThreadRunning = false;
        sys_ppu_thread_exit(0);
        return;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
#if RETRO_PLATFORM == RETRO_PS3
    serverAddr.sin_len    = sizeof(serverAddr);
#endif
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(networkPort);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(relaySocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
         // PrintLog("RelayServer - Failed to bind to port %d", networkPort);
        socketclose(relaySocket);
        relaySocket = -1;
        relayThreadRunning = false;
        sys_ppu_thread_exit(0);
        return;
    }

    // Non-blocking
    int on = 1;
    setsockopt(relaySocket, SOL_SOCKET, SO_NBIO, (const char*)&on, sizeof(on));

     // PrintLog("RelayServer - Started on port %d", networkPort);
    srand((unsigned int)sys_time_get_system_time());
    memset(relayRooms, 0, sizeof(relayRooms));

    ServerPacket packet;
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    while (relayThreadRunning) {
        int res = recvfrom(relaySocket, (char*)&packet, sizeof(ServerPacket), 0, (struct sockaddr *)&clientAddr, &addrLen);
        if (res > 0) {
            uint room_le;
            memcpy(&room_le, &packet.room, sizeof(uint));
            uint room_host = room_le;
            SWAP_ENDIAN(room_host);
            
            uint player_le;
            memcpy(&player_le, &packet.player, sizeof(uint));
            uint player_host = player_le;
            SWAP_ENDIAN(player_host);

            if (packet.header == CL_REQUEST_CODE) {
                 // PrintLog("RelayServer - Received CL_REQUEST_CODE from %s", inet_ntoa(clientAddr.sin_addr));
                int roomIdx = -1;
                for (int i = 0; i < MAX_RELAY_ROOMS; ++i) {
                    if (!relayRooms[i].roomCode) {
                        roomIdx = i;
                        break;
                    }
                }

                if (roomIdx >= 0) {
                    // RSDKv4 2P Match Code format:
                    // Bits 4-11: Room/Seed/Settings data (passed in from host)
                    uint settings = (uint)packet.data.multiData.data[0];
                    SWAP_ENDIAN(settings); // settings were passed in multiData.data[0] as host-endian

                    if (!(settings & 0xFF0))
                        settings = 0x40; // Default: 4 matches

                    relayRooms[roomIdx].roomCode     = ((uint)rand() & ~0x00000FF0) | (settings & 0x00000FF0);
                    relayRooms[roomIdx].player1_addr = clientAddr;
                    relayRooms[roomIdx].player1_id = (uint)rand() % 1000 + 1;
                    relayRooms[roomIdx].player1_active = true;
                    relayRooms[roomIdx].player2_active = false;
                    
                    ServerPacket response;
                    memset(&response, 0, sizeof(ServerPacket));
                    response.header = SV_CODES;
                    StrCopy(response.game, packet.game);
                    
                    uint r_room = relayRooms[roomIdx].roomCode;
                    uint r_player = relayRooms[roomIdx].player1_id;
                    int r_type = 1; // Host type
                    SWAP_ENDIAN(r_room);
                    SWAP_ENDIAN(r_player);
                    SWAP_ENDIAN(r_type);
                    memcpy(&response.room, &r_room, sizeof(uint));
                    memcpy(&response.player, &r_player, sizeof(uint));
                    memcpy(&response.data.multiData.type, &r_type, sizeof(int));

                    sendto(relaySocket, (const char*)&response, sizeof(ServerPacket), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
                     // PrintLog("RelayServer - Created room 0x%08X for player %u", relayRooms[roomIdx].roomCode, relayRooms[roomIdx].player1_id);
                }
            }
            else if (packet.header == CL_JOIN) {
                 // PrintLog("RelayServer - Received CL_JOIN for room 0x%08X", room_host);
                if (room_host == 0) {
                     // PrintLog("RelayServer - Ignoring JOIN for room 0");
                    continue;
                }
                int roomIdx = -1;
                for (int i = 0; i < MAX_RELAY_ROOMS; ++i) {
                    if (relayRooms[i].roomCode == room_host) {
                        roomIdx = i;
                        break;
                    }
                }

                if (roomIdx >= 0 && !relayRooms[roomIdx].player2_active) {
                    relayRooms[roomIdx].player2_addr = clientAddr;
                    relayRooms[roomIdx].player2_id = (uint)rand() % 1000 + 1001;
                    relayRooms[roomIdx].player2_active = true;

                    ServerPacket response;
                    memset(&response, 0, sizeof(ServerPacket));
                    response.header = SV_CODES;
                    StrCopy(response.game, packet.game);
                    
                    uint r_room = relayRooms[roomIdx].roomCode;
                    uint r_player = relayRooms[roomIdx].player2_id;
                    int r_type = 2; // Join type
                    SWAP_ENDIAN(r_room);
                    SWAP_ENDIAN(r_player);
                    SWAP_ENDIAN(r_type);
                    memcpy(&response.room, &r_room, sizeof(uint));
                    memcpy(&response.player, &r_player, sizeof(uint));
                    memcpy(&response.data.multiData.type, &r_type, sizeof(int));
                    
                    uint partner = relayRooms[roomIdx].player1_id;
                    SWAP_ENDIAN(partner);
                    memcpy(response.data.multiData.data, &partner, sizeof(uint));

                    sendto(relaySocket, (const char*)&response, sizeof(ServerPacket), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
                    
                    // Notify Host
                    memset(&response, 0, sizeof(ServerPacket));
                    response.header = SV_NEW_PLAYER;
                    StrCopy(response.game, packet.game);
                    memcpy(&response.room, &r_room, sizeof(uint));
                    uint r_player2 = relayRooms[roomIdx].player2_id;
                    SWAP_ENDIAN(r_player2);
                    memcpy(&response.player, &r_player2, sizeof(uint));
                    
                    sendto(relaySocket, (const char*)&response, sizeof(ServerPacket), 0, (struct sockaddr *)&relayRooms[roomIdx].player1_addr, sizeof(relayRooms[roomIdx].player1_addr));
                     // PrintLog("RelayServer - Player %u joined room 0x%08X", relayRooms[roomIdx].player2_id, room_host);
                } else {
                    ServerPacket response;
                    memset(&response, 0, sizeof(ServerPacket));
                    response.header = SV_NO_ROOM;
                    sendto(relaySocket, (const char*)&response, sizeof(ServerPacket), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
                }
            }
            else {
                // Relay data between players
                int roomIdx = -1;
                for (int i = 0; i < MAX_RELAY_ROOMS; ++i) {
                    if (relayRooms[i].roomCode == room_host) {
                        roomIdx = i;
                        break;
                    }
                }

                if (roomIdx >= 0) {
                    struct sockaddr_in *target = NULL;
                    if (player_host == relayRooms[roomIdx].player1_id) {
                        target = &relayRooms[roomIdx].player2_addr;
                    } else if (player_host == relayRooms[roomIdx].player2_id) {
                        target = &relayRooms[roomIdx].player1_addr;
                    }

                    if (target && (packet.header == CL_LEAVE)) {
                        ServerPacket response;
                        memset(&response, 0, sizeof(ServerPacket));
                        response.header = SV_LEAVE;
                        uint r_player = player_host;
                        SWAP_ENDIAN(r_player);
                        memcpy(&response.player, &r_player, sizeof(uint));
                        sendto(relaySocket, (const char*)&response, sizeof(ServerPacket), 0, (struct sockaddr *)target, sizeof(struct sockaddr_in));
                        relayRooms[roomIdx].roomCode = 0; // Close room
                         // PrintLog("RelayServer - Room 0x%08X closed", room_host);
                    }
                    else if (target) {
                        sendto(relaySocket, (const char*)&packet, sizeof(ServerPacket), 0, (struct sockaddr *)target, sizeof(struct sockaddr_in));
                    }
                }
            }
        }
        sys_timer_usleep(1000);
    }

    socketclose(relaySocket);
    relaySocket = -1;
    relayThreadRunning = false;
    sys_ppu_thread_exit(0);
}

NetworkSession *session;
#else
std::shared_ptr<NetworkSession> session;
asio::io_context io_context;
#endif
#if RETRO_PLATFORM == RETRO_PS3
sys_ppu_thread_t netThread;
bool netThreadRunning = false;

sys_ppu_thread_t relayThread;

sys_ppu_thread_t upnpThread;
bool upnpThreadRunning = false;

sys_ppu_thread_t ipThread;
bool ipThreadRunning = false;

static void upnpLoop(uint64_t arg)
{
    UPnP_AddPortMapping(networkPort);
    upnpThreadRunning = false;
    sys_ppu_thread_exit(0);
}

static void fetchIPLoop(uint64_t arg)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        ipThreadRunning = false;
        sys_ppu_thread_exit(0);
        return;
    }

    struct hostent *server = gethostbyname("api.ipify.org");
    if (!server) {
        socketclose(sockfd);
        ipThreadRunning = false;
        sys_ppu_thread_exit(0);
        return;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
#if RETRO_PLATFORM == RETRO_PS3
    serv_addr.sin_len    = sizeof(serv_addr);
#endif
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(80);

    // Timeout
    struct timeval tv;
    tv.tv_sec  = 5;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        socketclose(sockfd);
        ipThreadRunning = false;
        sys_ppu_thread_exit(0);
        return;
    }

    const char *request = "GET / HTTP/1.1\r\nHost: api.ipify.org\r\nConnection: close\r\n\r\n";
    send(sockfd, request, strlen(request), 0);

    char buffer[1024];
    int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = 0;
        char *ipStart = strstr(buffer, "\r\n\r\n");
        if (ipStart) {
            ipStart += 4;
            char *ipEnd = ipStart;
            while (*ipEnd && *ipEnd != '\r' && *ipEnd != '\n' && (ipEnd - ipStart) < 15) {
                ipEnd++;
            }
            int len = ipEnd - ipStart;
            if (len > 0) {
                memcpy(publicIP, ipStart, len);
                publicIP[len] = 0;
            }
        }
    }

    socketclose(sockfd);
    ipThreadRunning = false;
    sys_ppu_thread_exit(0);
}
#else
std::thread loopThread;
#endif

void InitNetwork()
{
#if RETRO_PLATFORM == RETRO_PS3
    static bool netInitialised = false;
    if (!netInitialised) {
        InitPS3Network();
        netInitialised = true;
    }

    if (useHostServer && !relayThreadRunning) {
#if RETRO_PLATFORM == RETRO_PS3
        if (!upnpThreadRunning) {
            upnpThreadRunning = true;
            sys_ppu_thread_create(&upnpThread, upnpLoop, 0, 100, 16384, SYS_PPU_THREAD_CREATE_JOINABLE, "UPnPThread");
        }
#endif

        if (!ipThreadRunning && strcmp(publicIP, "0.0.0.0") == 0) {
            ipThreadRunning = true;
            sys_ppu_thread_create(&ipThread, fetchIPLoop, 0, 100, 16384, SYS_PPU_THREAD_CREATE_JOINABLE, "IPThread");
        }

        relayThreadRunning = true;
         // PrintLog("InitNetwork() - Starting Relay Server thread...");
        int res = sys_ppu_thread_create(&relayThread, relayLoop, 0, 400, 32768, SYS_PPU_THREAD_CREATE_JOINABLE, "RelayThread");
        if (res < 0) {
             // PrintLog("InitNetwork() - Failed to start Relay thread: %d", res);
            relayThreadRunning = false;
        } else {
             // PrintLog("InitNetwork() - Relay thread started successfully.");
        }
        
        // When acting as host server, we connect to ourselves
        StrCopy(networkHost, "34.171.163.243");
    }

    if (session)
        delete session;
    session = new NetworkSession(networkHost, networkPort);
    StrCopy(publicIP, networkHost);
#else
    udp::resolver resolver(io_context);
    asio::error_code ec;
    udp::endpoint endpoint = *resolver.resolve(udp::v4(), networkHost, std::to_string(networkPort), ec).begin();
    session.reset();
    std::shared_ptr<NetworkSession> newsession = std::make_shared<NetworkSession>(io_context, endpoint);
    session.swap(newsession);
#endif
}

void networkLoop(uint64_t arg)
{
    if (session) {
        session->start();
        while (session->running) {
            session->run();
            sys_timer_usleep(1000);
        }
        session->close();
    }
    netThreadRunning = false;
    sys_ppu_thread_exit(0);
}
#else
void networkLoop()
{
    session->start();
    while (session->running) session->run();
    session->close();
}
#endif
void RunNetwork()
{
#if RETRO_PLATFORM == RETRO_PS3
    DisconnectNetwork();
    InitNetwork();

    netThreadRunning = true;
    sys_ppu_thread_create(&netThread, networkLoop, 0, 500, 65536, SYS_PPU_THREAD_CREATE_JOINABLE, "NetworkingThread");
#else
    DisconnectNetwork();
    InitNetwork();
    loopThread = std::thread(networkLoop);
#endif
}

void UpdateNetwork()
{
    if (session)
        session->run();
}

void SendData(bool verify)
{
    if (!session)
        return;
    ServerPacket send;
    memset(&send, 0, sizeof(ServerPacket));
    send.header         = CL_DATA + verify;
    send.data.multiData = multiplayerDataOUT;
    session->write(send, verify);
}

void DisconnectNetwork(bool finalClose)
{
    if (session) {
        if (session->running)
            session->leave();
        session->running = false; // Ensure it stops even if leave packet wasn't processed
    }

#if RETRO_PLATFORM == RETRO_PS3
    if (upnpThreadRunning) {
        uint64_t exit_code;
        sys_ppu_thread_join(upnpThread, &exit_code);
        upnpThreadRunning = false;
    }
    if (finalClose && useHostServer) {
        UPnP_DeletePortMapping(networkPort);
    }

    if (netThreadRunning) {
        uint64_t exit_code;
        sys_ppu_thread_join(netThread, &exit_code);
        netThreadRunning = false;
    }

    if (finalClose && relayThreadRunning) {
        relayThreadRunning = false;
        uint64_t exit_code;
        sys_ppu_thread_join(relayThread, &exit_code);
    }

    if (finalClose && ipThreadRunning) {
        // Can't really stop it easily, but we can wait
        uint64_t exit_code;
        sys_ppu_thread_join(ipThread, &exit_code);
        ipThreadRunning = false;
    }
#else
    if (loopThread.joinable())
        loopThread.join();
#endif

    if (finalClose) {
#if RETRO_PLATFORM == RETRO_PS3
        if (session) {
            delete session;
            session = NULL;
        }
#else
        if (session)
            session.reset();
#endif
    }
}

void SendServerPacket(ServerPacket &send, bool repeat)
{
    if (session)
        session->write(send, repeat);
}
int GetRoomCode()
{
    if (session)
        return session->room;
    return 0;
}
void SetRoomCode(int code)
{
    if (session)
        session->room = code;
}
int GetNetworkCode()
{
    if (session)
        return session->code;
    return 0;
}

void SetNetworkGameName(int *a1, const char *name) { StrCopy(networkGame, name); }