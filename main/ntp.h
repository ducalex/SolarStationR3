#include "sys/time.h"
#include "sys/socket.h"
#include "esp_log.h"
#include "string.h"
#include "netdb.h"

long ntpTimeUpdate(const char *host)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct hostent *server = gethostbyname(host);
    struct sockaddr_in serv_addr = {};
    struct timeval timeout = { 2, 0 };
    struct timeval ntp_time = {0, 0};
    struct timeval cur_time;

    if (server == NULL) {
        ESP_LOGE("NTP", "Unable to resolve NTP server");
        return -1;
    }

    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(123);

    uint32_t ntp_packet[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ((uint8_t*)ntp_packet)[0] = 0x1b; // li, vn, mode.

    int64_t prev_micros = esp_timer_get_time();

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr));
    send(sockfd, &ntp_packet, sizeof(ntp_packet), 0);

    if (recv(sockfd, &ntp_packet, sizeof(ntp_packet), 0) < 0) {
        ESP_LOGE("NTP", "Error receiving NTP packet");
        return -1;
    }
    else {
        int64_t request_time = (esp_timer_get_time() - prev_micros);

        ntp_time.tv_sec = ntohl(ntp_packet[10]) - 2208988800UL; // DIFF_SEC_1900_1970;
        ntp_time.tv_usec = (((int64_t)ntohl(ntp_packet[11]) * 1000000) >> 32) + (request_time / 2);

        gettimeofday(&cur_time, NULL);
        settimeofday(&ntp_time, NULL);

        int64_t prev_millis = ((((int64_t)cur_time.tv_sec * 1000000) + cur_time.tv_usec) / 1000);
        int64_t now_millis = ((int64_t)ntp_time.tv_sec * 1000000 + ntp_time.tv_usec) / 1000;
        long ntp_time_delta = (now_millis - prev_millis);

        ESP_LOGI("NTP", "Received Time: %.24s, we were %ldms %s. Req time: %ldus",
            ctime(&ntp_time.tv_sec), (long)abs(ntp_time_delta), ntp_time_delta < 0 ? "ahead" : "behind", (long)request_time);

        return ntp_time_delta;
    }
}
