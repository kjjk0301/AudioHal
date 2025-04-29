#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parsesetting.h"

static void trim_newline(char* str) {
    size_t len = strlen(str);
    if (len > 0 && str[len-1] == '\n') {
        str[len-1] = '\0';
    }
}

static int parse_on_off(const char* value, const char* key) {
    if (strcmp(value, "on") == 0) return 1;
    else if (strcmp(value, "off") == 0) return 0;
    else {
        printf("Warning: Invalid value '%s' for key '%s'. Expected 'on' or 'off'.\n", value, key);
        return -1;
    }
}

void load_settings(const char* filename, Settings* settings) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open settings file");
        return;
    }

    // 파일 읽기 전에 기본값 적용
    settings->bf = 1;
    settings->ns = 1;
    settings->aec = 1;
    settings->beamauto = 1;
    settings->beamnum = 2;
    settings->nrlevel = 2;

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        trim_newline(line);

        if (line[0] == '#' || line[0] == '\0') continue;  // 주석이나 빈줄 스킵

        char* equal_pos = strchr(line, '=');
        if (!equal_pos) {
            printf("Warning: Invalid line '%s'\n", line);
            continue;
        }

        *equal_pos = '\0';
        char* key = line;
        char* value = equal_pos + 1;

        if (strcmp(key, "bf") == 0) {
            int parsed = parse_on_off(value, key);
            if (parsed != -1) settings->bf = parsed;
        } else if (strcmp(key, "ns") == 0) {
            int parsed = parse_on_off(value, key);
            if (parsed != -1) settings->ns = parsed;
        } else if (strcmp(key, "aec") == 0) {
            int parsed = parse_on_off(value, key);
            if (parsed != -1) settings->aec = parsed;
        } else if (strcmp(key, "beamauto") == 0) {
            int parsed = parse_on_off(value, key);
            if (parsed != -1) settings->beamauto = parsed;
        } else if (strcmp(key, "beamnum") == 0) {
            int num = atoi(value);
            settings->beamnum = num;
        } else if (strcmp(key, "nrlevel") == 0) {
            int num = atoi(value);
            settings->nrlevel = num;			
        } else {
            printf("Warning: Unknown key '%s'\n", key);
        }
    }

    fclose(file);
}

void print_settings(const Settings* settings) {
    printf("Settings:\n");
    printf("  bf        : %s\n", settings->bf ? "on" : "off");
    printf("  ns        : %s\n", settings->ns ? "on" : "off");
    printf("  aec       : %s\n", settings->aec ? "on" : "off");
    printf("  beamauto  : %s\n", settings->beamauto ? "on" : "off");
    printf("  beamnum   : %d\n", settings->beamnum);
    printf("  nrlevel   : %d\n", settings->nrlevel);
}