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

#ifndef __CUTSCENE_H__
#define __CUTSCENE_H__

#include "intern.h"
#include "graphics.h"

struct Resource;
struct SystemStub;
struct Video;

struct Cutscene {
	typedef void (Cutscene::*OpcodeStub)();

	enum {
		NUM_OPCODES = 15,
		TIMER_SLICE = 15
	};

	static const OpcodeStub _opcodeTable[];
	static const char *_namesTable[];
	static const uint16 _offsetsTable[];
	static const uint16 _cosTable[];
	static const uint16 _sinTable[];
	static const uint8 _creditsData[];
	static const uint16 _creditsCutSeq[];

	Cutscene(Resource *res, SystemStub *stub, Video *vid, Version ver);

	Graphics _gfx;
	Resource *_res;
	SystemStub *_stub;
	Video *_vid;
	Version _ver;

	uint16 _id;
	uint16 _deathCutsceneId;
	bool _interrupted;
	uint8 *_polPtr;
	uint8 *_cmdPtr;
	uint8 *_cmdPtrBak;
	uint32 _tstamp;
	uint8 _frameDelay;
	bool _newPal;
	uint8 _palBuf[0x20 * 2];
	uint16 _startOffset;
	bool _creditsSequence;
	uint32 _rotData[4];
	uint8 _primitiveColor;
	uint8 _clearScreen;
	Point _vertices[0x80];
	bool _hasAlphaColor;
	uint8 _varText;
	uint8 _varKey;
	uint16 _shape_ix;
	uint16 _shape_iy;
	uint16 _shape_ox;
	uint16 _shape_oy;
	uint16 _shape_unk_x;
	uint16 _shape_unk_y;
	uint16 _shape_unk_x2;	
	uint16 _shape_unk_y2;
	uint16 _shape_cur_x;
	uint16 _shape_cur_y;
	uint16 _shape_prev_x;
	uint16 _shape_prev_y;
	uint16 _shape_count;
	uint32 _shape_cur_x16;
	uint32 _shape_cur_y16;
	uint32 _shape_prev_x16;
	uint32 _shape_prev_y16;
	uint8 _textSep[0x14];
	uint8 _textBuf[500];
	const uint8 *_textCurPtr;
	uint8 *_textCurBuf;
	uint8 _textUnk2;
	uint8 _creditsTextPosX;
	uint8 _creditsTextPosY;
	int16 _creditsTextCounter;
	uint8 *_page0, *_page1, *_pageC;
	
	void sync();
	void copyPalette(const uint8 *pal, uint16 num);
	void setPalette0xC();
	void setPalette();
	void initRotationData(uint16 a, uint16 b, uint16 c);
	uint16 findTextSeparators(const uint8 *p);
	void drawText(int16 x, int16 y, const uint8 *p, uint16 color, uint8 *page, uint8 n);
	void swapLayers();
	void drawCreditsText();
	void op_drawShape0Helper(const uint8 *data, int16 x, int16 y);
	void op_drawShape1Helper(const uint8 *data, int16 zoom, int16 b, int16 c, int16 d, int16 e, int16 f, int16 g);
	void op_drawShape2Helper(const uint8 *data, int16 zoom, int16 b, int16 c, int16 d, int16 e, int16 f, int16 g);
	
	void op_markCurPos();
	void op_refreshScreen();
	void op_waitForSync();
	void op_drawShape0();
	void op_setPalette();
	void op_drawStringAtBottom();
	void op_nop();
	void op_skip3();
	void op_refreshAll();
	void op_drawShape1();
	void op_drawShape2();
	void op_drawCreditsText();
	void op_drawStringAtPos();
	void op_handleKeys();
	
	uint8 CMD_fetchByte();
	uint16 CMD_fetchWord();
	void mainLoop(uint16 offset);
	void load(uint16 cutName);
	void prepare();
	void startCredits();
	void play();
};

#endif // __CUTSCENE_H__
