#ifndef PARSE_H
#define PARSE_H

typedef struct {
    int bf;          // on:1, off:0
    int ns;          // on:1, off:0
    int aec;         // on:1, off:0
    int beamauto;    // on:1, off:0
    int beamnum;     // number
    int nrlevel;     // nr level : 0 - 3
} Settings;

// 설정 파일을 읽어서 Settings 구조체를 채운다
void load_settings(const char* filename, Settings* settings);

// Settings 구조체 값을 출력한다
void print_settings(const Settings* settings);

#endif // PARSE_H