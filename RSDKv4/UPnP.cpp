#include "RetroEngine.hpp"
#include "UPnP.hpp"

#if RETRO_PLATFORM == RETRO_PS3
#include <netex/net.h>
#include <netex/errno.h>
#include <netex/libnetctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define SSDP_MULTICAST_ADDR "239.255.255.250"
#define SSDP_MULTICAST_PORT 1900

static char controlURL[256];
static char serviceURN[128];
static char routerIP[64];
static int routerPort;
static char baseDir[256];
static bool isMapped = false;
static uint16_t mappedPort = 0;

bool UPnP_IsMapped() { return isMapped; }
uint16_t UPnP_GetMappedPort() { return mappedPort; }

static void ParseLocation(const char *location, char *ip, int *port, char *path)
{
    if (strncmp(location, "http://", 7) == 0) {
        const char *p = location + 7;
        const char *colon = strchr(p, ':');
        const char *slash = strchr(p, '/');
        
        if (colon && slash && colon < slash) {
            int len = colon - p;
            if (len >= 64) len = 63;
            memcpy(ip, p, len);
            ip[len] = '\0';
            *port = atoi(colon + 1);
            strcpy(path, slash);
        } else if (slash) {
            int len = slash - p;
            if (len >= 64) len = 63;
            memcpy(ip, p, len);
            ip[len] = '\0';
            *port = 80;
            strcpy(path, slash);
        }

        // Extract base directory
        strcpy(baseDir, path);
        char *lastSlash = strrchr(baseDir, '/');
        if (lastSlash) {
            *(lastSlash + 1) = '\0';
        } else {
            strcpy(baseDir, "/");
        }
    }
}

static bool SendSOAPRequest(const char *ip, int port, const char *path, const char *action, const char *body, const char *urn)
{
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
         // PrintLog("UPnP - SOAP: Failed to create socket.");
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
#if RETRO_PLATFORM == RETRO_PS3
    addr.sin_len    = sizeof(addr);
#endif
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    // Timeout
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
         // PrintLog("UPnP - SOAP: Failed to connect to %s:%d", ip, port);
        socketclose(sock);
        return false;
    }

    char *request = (char *)malloc(4096);
    if (!request) {
        socketclose(sock);
        return false;
    }

    sprintf(request,
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Length: %d\r\n"
        "Content-Type: text/xml; charset=\"utf-8\"\r\n"
        "SOAPACTION: \"%s#%s\"\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, ip, port, (int)strlen(body), urn, action, body);

    send(sock, request, strlen(request), 0);
    free(request);
    
    char *response = (char *)malloc(2048);
    if (!response) {
        socketclose(sock);
        return false;
    }

    int bytes = recv(sock, response, 2047, 0);
    socketclose(sock);

    if (bytes > 0) {
        response[bytes] = 0;
        bool success = strstr(response, "200 OK") != NULL;
        if (!success) {
             // PrintLog("UPnP - SOAP: Request failed. Response: %.300s...", response);
        }
        free(response);
        return success;
    }
    
     // PrintLog("UPnP - SOAP: No response received.");
    free(response);
    return false;
}

void UPnP_AddPortMapping(uint16_t port)
{
     // PrintLog("UPnP - Starting discovery...");
    controlURL[0] = '\0';
    serviceURN[0] = '\0';

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
#if RETRO_PLATFORM == RETRO_PS3
    addr.sin_len    = sizeof(addr);
#endif
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SSDP_MULTICAST_PORT);
    addr.sin_addr.s_addr = inet_addr(SSDP_MULTICAST_ADDR);

    const char *msg = 
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 2\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
        "\r\n";

    int bytes = -1;
    char buffer[2048];
    for (int retry = 0; retry < 3; ++retry) {
        sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(addr));

        struct timeval tv_ssdp;
        tv_ssdp.tv_sec = 3;
        tv_ssdp.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv_ssdp, sizeof(struct timeval));

        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        bytes = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&from, &fromlen);
        if (bytes > 0) break;
         // PrintLog("UPnP - SSDP retry %d...", retry + 1);
    }
    socketclose(sock);

    if (bytes <= 0) {
         // PrintLog("UPnP - No response from router after retries.");
        return;
    }

    buffer[bytes] = 0;
    char *loc = strstr(buffer, "LOCATION:");
    if (!loc) loc = strstr(buffer, "location:");
    if (!loc) loc = strstr(buffer, "Location:");
    if (!loc) {
         // PrintLog("UPnP - Location header not found.");
        return;
    }

    loc += 9;
    while (*loc && (*loc == ' ' || *loc == '\t')) loc++;

    char *end = strchr(loc, '\r');
    if (!end) end = strchr(loc, '\n');
    if (!end) return;

    char location[256];
    int locLen = end - loc;
    if (locLen >= 256) locLen = 255;
    memcpy(location, loc, locLen);
    location[locLen] = '\0';

    char path[256];
    ParseLocation(location, routerIP, &routerPort, path);
     // PrintLog("UPnP - Found router at %s:%d, desc: %s", routerIP, routerPort, path);

    int xmlSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (xmlSock < 0) return;
    
    struct sockaddr_in xmlAddr;
    memset(&xmlAddr, 0, sizeof(xmlAddr));
#if RETRO_PLATFORM == RETRO_PS3
    xmlAddr.sin_len    = sizeof(xmlAddr);
#endif
    xmlAddr.sin_family = AF_INET;
    xmlAddr.sin_port = htons(routerPort);
    xmlAddr.sin_addr.s_addr = inet_addr(routerIP);
    
    // Timeout
    struct timeval tv_xml;
    tv_xml.tv_sec = 3;
    tv_xml.tv_usec = 0;
    setsockopt(xmlSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv_xml, sizeof(struct timeval));

    if (connect(xmlSock, (struct sockaddr *)&xmlAddr, sizeof(xmlAddr)) >= 0) {
        char xmlReq[512];
        sprintf(xmlReq, "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n", path, routerIP, routerPort);
        send(xmlSock, xmlReq, strlen(xmlReq), 0);
        
        char *xmlBuf = (char *)malloc(32768);
        if (xmlBuf) {
            int totalBytes = 0;
            int r;
            while (totalBytes < 32767 && (r = recv(xmlSock, xmlBuf + totalBytes, 32767 - totalBytes, 0)) > 0) {
                totalBytes += r;
            }
            xmlBuf[totalBytes] = 0;

            // Check for URLBase
            char *urlBaseTag = strstr(xmlBuf, "<URLBase>");
            if (urlBaseTag) {
                urlBaseTag += 9;
                char *urlBaseEnd = strstr(urlBaseTag, "</URLBase>");
                if (urlBaseEnd) {
                    char urlBase[256];
                    int baseLen = urlBaseEnd - urlBaseTag;
                    if (baseLen >= 256) baseLen = 255;
                    memcpy(urlBase, urlBaseTag, baseLen);
                    urlBase[baseLen] = '\0';
                    ParseLocation(urlBase, routerIP, &routerPort, path);
                }
            }

            const char *urns[] = { "urn:schemas-upnp-org:service:WANIPConnection:1", "urn:schemas-upnp-org:service:WANPPPConnection:1" };
            for (int i = 0; i < 2; ++i) {
                char *serv = strstr(xmlBuf, urns[i]);
                if (serv) {
                    strcpy(serviceURN, urns[i]);
                    char *ctl = strstr(serv, "<controlURL>");
                    if (ctl) {
                        ctl += 12;
                        char *ctlEnd = strstr(ctl, "</controlURL>");
                        if (ctlEnd) {
                            int ctlLen = ctlEnd - ctl;
                            if (ctlLen >= 256) ctlLen = 255;
                            memcpy(controlURL, ctl, ctlLen);
                            controlURL[ctlLen] = '\0';
                            break;
                        }
                    }
                }
            }
            free(xmlBuf);
        }
    } else {
         // PrintLog("UPnP - Failed to connect to router to fetch XML.");
    }
    socketclose(xmlSock);

    if (controlURL[0] == '\0') {
         // PrintLog("UPnP - Could not find controlURL in XML.");
        return;
    }

    if (strncmp(controlURL, "http", 4) != 0) {
        if (controlURL[0] == '/') {
            // Absolute path from host root, keep as is (most routers do this)
        } else {
            // Relative to baseDir
            char temp[256];
            sprintf(temp, "%s%s", baseDir, controlURL);
            strcpy(controlURL, temp);
        }
    }

     // PrintLog("UPnP - Control URL: %s (URN: %s)", controlURL, serviceURN);

    char localIP[16];
    CellNetCtlInfo netInfo;
    if (cellNetCtlGetInfo(CELL_NET_CTL_INFO_IP_ADDRESS, &netInfo) >= 0) {
        strcpy(localIP, netInfo.ip_address);
    } else {
        return;
    }

    char soapBody[1024];
    sprintf(soapBody,
        "<?xml version=\"1.0\"?>\r\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
        "<s:Body>\r\n"
        "<u:AddPortMapping xmlns:u=\"%s\">\r\n"
        "<NewRemoteHost></NewRemoteHost>\r\n"
        "<NewExternalPort>%d</NewExternalPort>\r\n"
        "<NewProtocol>UDP</NewProtocol>\r\n"
        "<NewInternalPort>%d</NewInternalPort>\r\n"
        "<NewInternalClient>%s</NewInternalClient>\r\n"
        "<NewEnabled>1</NewEnabled>\r\n"
        "<NewPortMappingDescription>RSDKV4PS3</NewPortMappingDescription>\r\n"
        "<NewLeaseDuration>0</NewLeaseDuration>\r\n"
        "</u:AddPortMapping>\r\n"
        "</s:Body>\r\n"
        "</s:Envelope>",
        serviceURN, port, port, localIP);

    if (SendSOAPRequest(routerIP, routerPort, controlURL, "AddPortMapping", soapBody, serviceURN)) {
         // PrintLog("UPnP - Port %d (UDP) mapped successfully to %s", port, localIP);
        isMapped = true;
        mappedPort = port;
    } else {
         // PrintLog("UPnP - Failed to map port %d.", port);
        isMapped = false;
        mappedPort = 0;
    }
}

void UPnP_DeletePortMapping(uint16_t port)
{
    if (controlURL[0] == '\0') return;

    char soapBody[1024];
    sprintf(soapBody,
        "<?xml version=\"1.0\"?>\r\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
        "<s:Body>\r\n"
        "<u:DeletePortMapping xmlns:u=\"%s\">\r\n"
        "<NewRemoteHost></NewRemoteHost>\r\n"
        "<NewExternalPort>%d</NewExternalPort>\r\n"
        "<NewProtocol>UDP</NewProtocol>\r\n"
        "</u:DeletePortMapping>\r\n"
        "</s:Body>\r\n"
        "</s:Envelope>",
        serviceURN, port);

    if (SendSOAPRequest(routerIP, routerPort, controlURL, "DeletePortMapping", soapBody, serviceURN)) {
         // PrintLog("UPnP - Port %d (UDP) unmapped successfully.", port);
        isMapped = false;
        mappedPort = 0;
    } else {
         // PrintLog("UPnP - Failed to unmap port %d.", port);
    }
}

#endif
