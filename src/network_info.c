/**
 * kiddyblaster/src/network_info.c
 *
 * @author Johannes Braun <johannes.braun@hannenz.de>
 * @package kiddyblaster
 * @version 2020-06-07
 */

#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "network_info.h"


int get_wifi_info(wifi_info_t *wifi) {

    FILE *fp;
    static const char* cmd = "iwconfig wlan0 | grep -i quality";
    char line[200], msgbuf[200];
    int i, reti;
    regex_t regex;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        return -1;
    }

    if (fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "Failed to read from stream\n");
        return -1;
    }

    // Compile regex
    reti = regcomp(&regex, "[[:digit:]]+/[[:digit:]]+", REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Regex compilation failed\n");
        return -1;
    }

    // Execute regex (multiple times), e.g. "match_all" behavior
    size_t nmatch = 1; // nr. of matches to be expected
    regmatch_t pmatch[nmatch]; // holds match info (start / end offset)
    char *ptr = &line[0];
    char *matches[3];
    i = 0;
    while ((reti = regexec(&regex, ptr, nmatch, pmatch, 0)) != REG_NOMATCH) {
        if (reti == 0) {
            int len = pmatch[0].rm_eo - pmatch[0].rm_so;
            matches[i] = malloc(len);
            strncpy(matches[i], ptr + pmatch[0].rm_so, len);
            matches[i][len] = '\0';
            ptr += pmatch[0].rm_eo;
            if (i++ > 3) {
                break;
            }
        }
        else {
            regerror(reti, &regex, msgbuf, sizeof(msgbuf));
            fprintf(stderr, "Regex match failed: %s\n", msgbuf);
            return -1;
        }
    }

    if (i != 3) {
        fprintf(stderr, "Wrong number of matches\n");
        return -1;
    }

    regfree(&regex);
    pclose(fp);

    wifi->link_quality = atoi(matches[0]);
    wifi->signal_level = atoi(matches[1]);
    wifi->noise_level = atoi(matches[2]);

    return 0;
}


/**
 * Returns the IP Addr. of a given interface (e.g. "eth0", "wlan0" ...)
 * The string must be freed, when not used anymore
 *
 * @param char* interface
 * @return char*
 */
char *get_ip_address(char* interface) {
    struct ifaddrs *addrs, *p;
    char *ip;

    if (getifaddrs(&addrs) < 0) {
        return NULL;
    }

    p = addrs;

    while (p != NULL) {

        if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET) {
            if (strncmp(p->ifa_name, interface, strlen(interface)) == 0) {
                struct sockaddr_in *p_addr = (struct sockaddr_in*)p->ifa_addr;
                char *addr = inet_ntoa(p_addr->sin_addr);
                int len = strlen(addr);
                ip = malloc(len + 1);
                strncpy(ip, addr, len);
                ip[len] = '\0';
                break;
            }
        }

        p = p->ifa_next;

    }
    freeifaddrs(addrs);
    return ip;
}

