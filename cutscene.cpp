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

#include "systemstub.h"
#include "video.h"
#include "cutscene.h"


Cutscene::Cutscene(SystemStub *stub, Video *vid)
	: _stub(stub), _vid(vid) {
}

void Cutscene::op_markCurPos() {
}

void Cutscene::op_refreshScreen() {
}

void Cutscene::op_waitForSync() {
}

void Cutscene::op_drawShape0() {
}

void Cutscene::op_setPalette() {
}

void Cutscene::op_drawStringAtBottom() {
}

void Cutscene::op_nop() {
}

void Cutscene::op_skip3() {
}

void Cutscene::op_refreshAll() {
}

void Cutscene::op_drawShape1() {
}

void Cutscene::op_drawShape2() {
}

void Cutscene::op_drawCreditsText() {
}

void Cutscene::op_drawStringAtPos() {
}

void Cutscene::op_handleKeys() {
}

void Cutscene::play() {
	if (_id != 0xFFFF) {
		warning("playing cutscene 0x%02X not yet implemented", _id);
		_id = 0xFFFF;
	}
}
