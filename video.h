/* REminiscence - Flashback interpreter
 * Copyright (C) 2005 Gregory Montoir
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
 
#ifndef __VIDEO_H__
#define __VIDEO_H__

#include "intern.h"

struct Resource;
struct SystemStub;

struct Video {
	enum {
		GAMESCREEN_W = 256,
		GAMESCREEN_H = 224
	};

	static const uint8 _conradPal1[];
	static const uint8 _conradPal2[];
	static const uint8 _palSlot0xE[];
	static const uint8 _palSlot0xF[];
	
	Resource *_res;
	SystemStub *_stub;	

	uint8 *_frontLayer;
	uint8 *_backLayer;
	uint8 *_tempLayer;
	uint8 *_tempLayer2;
	uint8 _unkPalSlot1, _unkPalSlot2;
	uint8 _mapPalSlot1, _mapPalSlot2, _mapPalSlot3, _mapPalSlot4;
	uint8 _drawCharColor1;
	uint8 _drawCharColor2;
	uint8 _drawCharColor3;	
	
	Video(Resource *res, SystemStub *stub);
	~Video();
	
	void fadeOut();
	void setPaletteSlotBE(int palSlot, int palNum);
	void setPaletteSlotLE(int palSlot, const uint8 *palData);
	void setPalette0xE();
	void setPalette0xF();
	void copyLevelMap(uint16 room);
	void decodeLevelMap(uint16 sz, const uint8 *src, uint8 *dst);
	void setLevelPalettes();
	void drawSpriteSub1(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawSpriteSub2(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawSpriteSub3(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawSpriteSub4(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawSpriteSub5(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawSpriteSub6(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawChar(char c, int16 y, int16 x);
	const char *drawString(const char *str, int16 x, int16 y, uint8 col);
};

#endif
