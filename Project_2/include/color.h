/*
 * *****************************************************************************
 * file name : color.h
 * source    : Hung Ngo copied it from an article in the Linux Journal
 *             http://www.linuxjournal.com/article/8603
 *             Hung Ngo wrote the textnormal() function
 * *****************************************************************************
 */

#ifndef _COLOR_H
#define _COLOR_H

// the attributes
#define RESET		0
#define BRIGHT 		1
#define DIM		2
#define UNDERLINE 	3
#define BLINK		4
#define REVERSE		7
#define HIDDEN		8

// the colors
#define BLACK 		0
#define RED		1
#define GREEN		2
#define YELLOW		3
#define BLUE		4
#define MAGENTA		5
#define CYAN		6
#define	WHITE		7

/*
 * -----------------------------------------------------------------------------
 * textcolor() changes  the attribute, foreground and background
 * textnormal() returns to previous terminal setting
 * -----------------------------------------------------------------------------
 */
void textcolor(int attr, int fg, int bg);
void textnormal();

#endif
