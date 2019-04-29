/* $Id$ */
//
// ������ HF Dream Receiver (�� ������� �����)
// ����� ���� ����������� mgs2001@mail.ru
// UA1ARN
//
/* ��������� 176*132 � ������������ Epson L2F50126 */

#ifndef L2F50_H_INCLUDED
#define L2F50_H_INCLUDED

#if LCDMODE_L2F50

#include "hardware.h"	/* ��������� �� ���������� ������� ������ � ������� */

#define DISPLAYMODES_FPS 5	/* ���������� ���������� ����������� ������� ������ */
#define DISPLAY_FPS	10	/* ���������� ��������� ������� �� ������� */
#define DISPLAYSWR_FPS 5	/* ���������� ���������� SWR �� ������� */

// 132 x 176 pixel
/*
	Active Display Area 31.284 x 41.712 mm� (W x H)
	Blackmatrix area 34,2 x 44,9 mm� (W x H)
	Outline Dimensions panel 35.9 x 52,0 mm� (W x H)
	Design Viewing Direction 6 o�clock display
	Controller: 12 o�clock of the display module
*/

typedef uint_fast16_t COLOR_T;
typedef uint16_t PACKEDCOLOR_T;

// LPH8836-4
// RRRR.RGGG.GGGB.BBBB
#define TFTRGB(red, green, blue) \
	(  (uint_fast16_t) \
		(	\
			(((uint_fast16_t) (red) << 8) & 0xf800) | \
			(((uint_fast16_t) (green) << 3) & 0x07e0) | \
			(((uint_fast16_t) (blue) >> 3) & 0x001f) \
		) \
	)


#endif /* LCDMODE_L2F50 */
#endif /* L2F50_H_INCLUDED */

