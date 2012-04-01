/*
 * *****************************************************************************
 * file name : color.c
 * author    : Pradeep Padala
 * source    : Hung Ngo copied it from Padala's article in the Linux Journal
 *             http://www.linuxjournal.com/article/8603
 *             Hung Ngo wrote the textnormal() function
 * *****************************************************************************
 */
#include <stdio.h>
#include "color.h"

/*
 * -----------------------------------------------------------------------------
 * change the text background & foreground colors
 * -----------------------------------------------------------------------------
 */
void textcolor(int attr, int fg, int bg) {
  char command[13];
  /* Command is the control command to the terminal */
  sprintf(command, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40);
  printf("%s", command);
}

/*
 * -----------------------------------------------------------------------------
 * reset
 * -----------------------------------------------------------------------------
 */
void textnormal() {
  char command[13];
  sprintf(command, "%c[%dm", 0x1B, 0);
  printf("%s", command);
}
