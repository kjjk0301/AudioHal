#ifndef _KEYB_H
#define _KEYB_H

void init_keyboard();
void close_keyboard();
int _kbhit();
char _getch();
int _putch(int c);

#endif