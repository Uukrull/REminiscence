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

struct SystemStub;
struct Video;

struct Cutscene {
	typedef void (Cutscene::*OpcodeStub)();

	static const OpcodeStub _opcodeTable[];
	static const char *_namesTable[];
	static const uint16 _offsetsTable[];
	static const uint16 _cosTable[];
	static const uint16 _sinTable[];

	Cutscene(SystemStub *stub, Video *vid);

	SystemStub *_stub;
	Video *_vid;

	uint16 _id;
	uint16 _deathCutsceneId;
	bool _interrupted;	

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

	void play();
};

#endif
