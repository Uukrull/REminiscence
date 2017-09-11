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

#include <ctime>
#include "systemstub.h"
#include "unpack.h"
#include "game.h"


Game::Game(Version ver, const char *dataPath, SystemStub *stub)
	: _cut(stub, &_vid), _menu(&_res, stub, &_vid), _res(dataPath), _vid(&_res, stub), _stub(stub), _ver(ver) {
	switch (_ver) {
	case VER_FR:
		_menu._textOptions = Menu::_textOptionsFR;
		_stringsTable = _stringsTableFR;
		break;
	case VER_EN:
		_menu._textOptions = Menu::_textOptionsEN;
		_stringsTable = _stringsTableEN;
		break;	
	}
}

void Game::run() {
	_stub->init("REminiscence", Video::GAMESCREEN_W, Video::GAMESCREEN_H);
	
	_game_randSeed = time(0);
	
	_menu._charVar1 = 0;
	_menu._charVar2 = 0;
	_menu._charVar3 = 0;
	_menu._charVar4 = 0;
	_menu._charVar5 = 0;
	
	_vid._drawCharColor1 = 0;
	_vid._drawCharColor2 = 0;
	_vid._drawCharColor3 = 0;
	
	_res.load("fb_txt.fnt", Resource::OT_FNT);	
	
	_cut._interrupted = false;
	_cut._id = 0x40;
	_cut.play();
	_cut._id = 0x0D;
	_cut.play();
	if (!_cut._interrupted) {
		_cut._id = 0x4A;
		_cut.play();
	}
	
	_res.load("GLOBAL", Resource::OT_ICN);
	_res.load("PERSO", Resource::OT_SPR);
	_res.load_SPR_OFF("PERSO", _res._spr1);
	
	_game_skillLevel = 1;
	_game_currentLevel = 0;
	
	while (!_stub->_pi.quit && _menu.handleTitleScreen(_game_skillLevel, _game_currentLevel)) {
		if (_game_currentLevel == 7) {
			_vid.fadeOut();
			_vid.setPalette0xE();
			_cut._id = 0x3D;
			_cut.play();
		} else {
			_vid.setPalette0xE();
			_vid.setPalette0xF();
			_stub->setOverscanColor(0xE0);
			game_mainLoop();
		}
	}
	
	_stub->destroy();
}

void Game::game_mainLoop() {
	_vid._unkPalSlot1 = 0;
	_vid._unkPalSlot2 = 0;
	_game_prevScore = _game_score = 0;
	_game_firstBankData = _game_bankData;
	_game_lastBankData = _game_bankData + sizeof(_game_bankData);
	game_loadLevelData();
	_game_animBuffers._states[0] = _game_animBuffer0State;
	_game_animBuffers._curPos[0] = 0xFF;
	_game_animBuffers._states[1] = _game_animBuffer1State;
	_game_animBuffers._curPos[1] = 0xFF;
	_game_animBuffers._states[2] = _game_animBuffer2State;
	_game_animBuffers._curPos[2] = 0xFF;
	_game_animBuffers._states[3] = _game_animBuffer3State;
	_game_animBuffers._curPos[3] = 0xFF;
	_cut._deathCutsceneId = 0xFFFF;
	_pge_opTempVar2 = 0xFFFF;
	_cut._id = 0xFFFF;
	_game_deathCutsceneCounter = 0;
	_game_saveStateCompleted = false;
	_game_loadMap = true;
	pge_resetGroups();
	_game_blinkingConradCounter = 0;
	_pge_processOBJ = false;
	_pge_opTempVar1 = 0;
	_game_textToDisplay = 0xFFFF;
	while (!_stub->_pi.quit) {
		_cut.play();
		if (_cut._id == 0x3D) {
			game_showScore();
			break;
		}
		if (_game_deathCutsceneCounter) {
			--_game_deathCutsceneCounter;
			if (_game_deathCutsceneCounter == 0) {
				_cut._id = _cut._deathCutsceneId;
				_cut.play();
				warning("Unhandled death case");
				break; // XXX
			}
		}		
		memcpy(_vid._frontLayer, _vid._backLayer, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
		inp_update();
		pge_prepare();
		col_prepareRoomState();
		uint8 oldLevel = _game_currentLevel;
		for (uint16 i = 0; i < _res._pgeNum; ++i) {
			LivePGE *pge = _pge_liveTable2[i];
			if (pge) {
				_col_currentPiegeGridPosY = (pge->pos_y / 36) & ~1;
				_col_currentPiegeGridPosX = (pge->pos_x + 8) / 16;
				pge_process(pge);
			}
		}
		if (oldLevel != _game_currentLevel) {
			_game_prevScore = _game_score;
			game_changeLevel();
			_pge_opTempVar1 = 0;
			continue;
		}
		if (_game_loadMap) {
			if (_game_currentRoom == 0xFF) {
				_cut._id = 6;
				_game_deathCutsceneCounter = 1;
			} else {
				_game_currentRoom = _pgeLive[0].room_location;
				game_loadLevelMap();
				_game_loadMap = false;
			}
		}
		game_prepareAnims();
		game_drawAnims();
		game_drawCurrentInventoryItem();
		game_drawLevelTexts();
		game_printLevelCode();
		game_drawStoryTexts();
		if (_game_blinkingConradCounter != 0) {
			--_game_blinkingConradCounter;
		}

		_stub->copyRect(0, 0, Video::GAMESCREEN_W, Video::GAMESCREEN_H, _vid._frontLayer, 256);
		static uint32 tstamp = 0;
		if (!_stub->_pi.fastMode) {
			int32 delay = _stub->getTimeStamp() - tstamp;
			int32 pause = 30 - delay;
			if (pause > 0) {
				_stub->sleep(pause);
			}
			tstamp = _stub->getTimeStamp();
		}
		
		if (_stub->_pi.backspace) {
			_stub->_pi.backspace = false;
			game_handleInventory();
		}
	}
}

void Game::game_drawCurrentInventoryItem() {
	uint16 src = _pgeLive[0].current_inventory_PGE;
	if (src != 0xFF) {
		_game_currentIcon = _res._pgeInit[src].icon_num;
		game_drawIcon(_game_currentIcon, 232, 8, 0xA);
	}	
}

void Game::game_showScore() {
	_cut._id = 0x49;
	_cut.play();
	char textBuf[50];
	sprintf(textBuf, "SCORE %08lu", _game_score);
	_vid.drawString(textBuf, (256 - strlen(textBuf) * 8) / 2, 40, 0xE5);
	strcpy(textBuf, _menu._passwords[7][_game_skillLevel]);
	_vid.drawString(textBuf, (256 - strlen(textBuf) * 8) / 2, 16, 0xE7);
	// XXX
}

void Game::game_printLevelCode() {
	if (_game_printLevelCodeCounter != 0) {
		--_game_printLevelCodeCounter;
		if (_game_printLevelCodeCounter != 0) {
			char levelCode[50];
			sprintf(levelCode, "CODE: %s", _menu._passwords[_game_currentLevel][_game_skillLevel]);
			_vid.drawString(levelCode, (Video::GAMESCREEN_W - strlen(levelCode) * 8) / 2, 16, 0xE7);
		}
	}
}

void Game::game_printSaveStateCompleted() {
	if (_game_saveStateCompleted) {
		const char *completed = 0;
		switch (_ver) {
		case VER_FR:
			completed = "TERMINEE";
			break;
		case VER_EN:
			completed = "COMPLETED";
			break;
		}
		_vid.drawString(completed, (176 - strlen(completed) * 8) / 2, 34, 0xE6);
	}	
}

void Game::game_drawLevelTexts() {
	LivePGE *_si = &_pgeLive[0];
	int8 obj = col_findCurrentCollidingObject(_si, 3, 0xFF, 0xFF, &_si);
	if (obj == 0) {
		obj = col_findCurrentCollidingObject(_si, 0xFF, 5, 9, &_si);
	}
	if (obj == 0 || obj < 0) {
		return;
	}	
	_game_printLevelCodeCounter = 0;
	if (_game_textToDisplay == 0xFFFF) {
		uint8 icon_num = obj - 1;
		game_drawIcon(icon_num, 80, 8, 0xA);
		uint8 txt_num = _si->init_PGE->text_num;
		const char *str = (const char *)_res._tbn + READ_LE_UINT16(_res._tbn + txt_num * 2);
		_vid.drawString(str, (176 - strlen(str) * 8) / 2, 26, 0xE6);
		if (icon_num == 3) {
			game_printSaveStateCompleted();
		} else {
			_game_saveStateCompleted = false;
		}
	} else {
		_game_currentInventoryIconNum = obj - 1;
	}
}

void Game::game_drawStoryTexts() {
	if (_game_textToDisplay != 0xFFFF) {
		uint16 text_col_mask = 0xE8;
		const uint8 *str = _stringsTable + READ_LE_UINT16(_stringsTable + _game_textToDisplay * 2);
		memcpy(_vid._tempLayer, _vid._frontLayer, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
		while (!_stub->_pi.quit) {
			game_drawIcon(_game_currentInventoryIconNum, 80, 8, 0xA);
			if (*str == 0xFF) {
				text_col_mask = READ_LE_UINT16(str + 1);
				str += 3;
			}			
			int16 text_y_pos = 26;
			while (1) {
				uint16 len = game_getLineLength(str);
				str = (const uint8 *)_vid.drawString((const char *)str, (176 - len * 8) / 2, text_y_pos, text_col_mask);
				text_y_pos += 8;
				if (*str == 0 || *str == 0xB) {
					break;
				}
				++str;
			}
			_stub->copyRect(0, 0, Video::GAMESCREEN_W, Video::GAMESCREEN_H, _vid._frontLayer, 256);
			while (!_stub->_pi.backspace && !_stub->_pi.quit) {
				_stub->processEvents();
				_stub->sleep(80);
			}
			_stub->_pi.backspace = false;
			if (*str == 0) {
				break;
			}
			++str;
			memcpy(_vid._frontLayer, _vid._tempLayer, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
		}
		_game_textToDisplay = 0xFFFF;
	}
}

void Game::game_prepareAnims() {
	if (!(_game_currentRoom & 0x80) && _game_currentRoom < 0x40) {
		int8 pge_room;
		LivePGE *pge = _pge_liveTable1[_game_currentRoom];
		while (pge) {
			game_prepareAnimsHelper(pge, 0, 0);
			pge = pge->next_PGE_in_room;
		}
		pge_room = _res._ctData[0x00 + _game_currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if ((pge->init_PGE->object_type != 10 && pge->pos_y > 176) || (pge->init_PGE->object_type == 10 && pge->pos_y > 216)) {
					game_prepareAnimsHelper(pge, 0, -216);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[0x40 + _game_currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if (pge->pos_y < 48) {
					game_prepareAnimsHelper(pge, 0, 216);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[0xC0 + _game_currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {			
				if (pge->pos_x > 224) {
					game_prepareAnimsHelper(pge, -256, 0);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[0x80 + _game_currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {			
				if (pge->pos_x <= 32) {
					game_prepareAnimsHelper(pge, 256, 0);
				}
				pge = pge->next_PGE_in_room;
			}
		}		
	}
}

void Game::game_prepareAnimsHelper(LivePGE *pge, int16 dx, int16 dy) {
	debug(DBG_GAME, "Game::game_prepareAnimsHelper() dx=0x%X dy=0x%X pge_num=%d pge=0x%X pge->flags=0x%X pge->anim_number=0x%X", dx, dy, pge - &_pgeLive[0], pge, pge->flags, pge->anim_number);
	int16 xpos, ypos;
	if (!(pge->flags & 8)) {
		if (pge->index != 0 && game_loadMonsterSprites(pge) == 0) {
			return;
		}
		assert(pge->anim_number < 1287);
		const uint8 *dataPtr = _res._spr_off[pge->anim_number];
		if (dataPtr == 0) {
			return;
		}

		if (pge->flags & 2) {
			xpos = (int8)dataPtr[0] + dx + pge->pos_x;
			uint8 _cl = dataPtr[2];
			if (_cl & 0x40) {
				_cl = dataPtr[3];
			} else {
				_cl &= 0x3F;
			}
			xpos -= _cl;
		} else {
			xpos = dx + pge->pos_x - (int8)dataPtr[0];
		}
		
		ypos = dy + pge->pos_y - (int8)dataPtr[1] + 2;
		if (xpos <= -32 || xpos >= 256 || ypos < -48 || ypos >= 224) {
			return;
		}
		xpos += 8;
		dataPtr += 4;
		if (pge == &_pgeLive[0]) {
			_game_animBuffers.addState(1, xpos, ypos, dataPtr, pge);
		} else if (pge->flags & 0x10) {
			_game_animBuffers.addState(2, xpos, ypos, dataPtr, pge);
		} else {
			_game_animBuffers.addState(0, xpos, ypos, dataPtr, pge);
		}
	} else {
		assert(pge->anim_number < _res._numSpc);
		const uint8 *dataPtr = _res._spc + READ_BE_UINT16(_res._spc + pge->anim_number * 2);
		xpos = dx + pge->pos_x + 8;
		ypos = dy + pge->pos_y + 2;

		if (pge->init_PGE->object_type == 11) {
			_game_animBuffers.addState(3, xpos, ypos, dataPtr, pge);
		} else if (pge->flags & 0x10) {
			_game_animBuffers.addState(2, xpos, ypos, dataPtr, pge);
		} else {
			_game_animBuffers.addState(0, xpos, ypos, dataPtr, pge);
		}
	}
}

void Game::game_drawAnims() {
	debug(DBG_GAME, "Game::game_drawAnims()");	
	_game_eraseBackground = false;
	game_drawAnimBuffer(2, _game_animBuffer2State);
	game_drawAnimBuffer(1, _game_animBuffer1State);
	game_drawAnimBuffer(0, _game_animBuffer0State);
	_game_eraseBackground = true;
	game_drawAnimBuffer(3, _game_animBuffer3State);
}

void Game::game_drawAnimBuffer(uint8 stateNum, AnimBufferState *state) {
	debug(DBG_GAME, "Game::game_drawAnimBuffer() state = %d", stateNum);
	assert(stateNum < 4);
	_game_animBuffers._states[stateNum] = state;
	uint8 lastPos = _game_animBuffers._curPos[stateNum];
	if (lastPos != 0xFF) {
		uint8 numAnims = lastPos + 1;
		state += lastPos;
		_game_animBuffers._curPos[stateNum] = 0xFF;
		do {
			LivePGE *pge = state->pge;
			if (!(pge->flags & 8)) {
				if (stateNum == 1 && (_game_blinkingConradCounter & 1)) {
					break;
				}
				if (!(state->dataPtr[-2] & 0x80)) {
					game_decodeCharacterFrame(state->dataPtr, _res._memBuf);
					game_drawCharacter(_res._memBuf, state->x, state->y, state->dataPtr[-1], state->dataPtr[-2], pge->flags);
				} else {
					game_drawCharacter(state->dataPtr, state->x, state->y, state->dataPtr[-1], state->dataPtr[-2], pge->flags);
				}
			} else {
				game_drawObject(state->dataPtr, state->x, state->y, pge->flags);
			}
			--state;
		} while (--numAnims != 0);
	}	
}

void Game::game_drawObject(const uint8 *dataPtr, int16 x, int16 y, uint8 flags) {
	debug(DBG_GAME, "Game::game_drawObject() dataPtr[] = 0x%X dx = %d dy = %d",  dataPtr[0], (int8)dataPtr[1], (int8)dataPtr[2]);
	assert(dataPtr[0] < 0x4A);
	uint8 slot = _res._rp[dataPtr[0]];
	uint8 *data = game_findBankData(slot);
	if (data == 0) {
		data = game_processMBK(slot);
	}
	_game_bankDataPtrs = data;
	int16 posy = y - (int8)dataPtr[2];
	int16 posx = x;
	if (flags & 2) {
		posx += (int8)dataPtr[1];
	} else {
		posx -= (int8)dataPtr[1];
	}
	int i = dataPtr[5];
	dataPtr += 6;
	while (i--) {
		game_drawObjectFrame(dataPtr, posx, posy, flags);
		dataPtr += 4;
	}
}

void Game::game_drawObjectFrame(const uint8 *dataPtr, int16 x, int16 y, uint8 flags) {
	debug(DBG_GAME, "Game::game_drawObjectFrame(0x%X, %d, %d, 0x%X)", dataPtr, x, y, flags);
	const uint8 *_si = _game_bankDataPtrs + dataPtr[0] * 32;
	
	int16 sprite_y = y + dataPtr[2];
	int16 sprite_x;
	if (flags & 2) {
		sprite_x = x - dataPtr[1] - (((dataPtr[3] & 0xC) + 4) * 2);
	} else {
		sprite_x = x + dataPtr[1];
	}
	
	uint8 sprite_flags = dataPtr[3];
	if (flags & 2) {
		sprite_flags ^= 0x10;
	}
	
	uint8 sprite_h = (((sprite_flags >> 0) & 3) + 1) * 8;
	uint8 sprite_w = (((sprite_flags >> 2) & 3) + 1) * 8;
	
	int size = sprite_w * sprite_h / 2;
	for (int i = 0; i < size; ++i) {
		uint8 col = *_si++;
		_res._memBuf[i * 2 + 0] = (col & 0xF0) >> 4;
		_res._memBuf[i * 2 + 1] = (col & 0x0F) >> 0;
	}
		
	_si = _res._memBuf;
	bool var14 = false;
	int16 _cx = sprite_x;
	if (_cx >= 0) {
		_cx += sprite_w;
		if (_cx < 256) {
			_cx = sprite_w;
		} else {
			_cx = 256 - sprite_x;
			if (sprite_flags & 0x10) {
				var14 = true;
				_si += sprite_w - 1;
			}
		}
	} else {
		_cx += sprite_w;
		if (!(sprite_flags & 0x10)) {
			_si -= sprite_x;
			sprite_x = 0;
		} else {
			var14 = true;
			_si += sprite_x + sprite_w - 1;
			sprite_x = 0;
		}
	}
	
	if (_cx <= 0) {
		return;
	}
	uint16 sprite_clipped_w = _cx;
	
	int16 _dx = sprite_y;
	if (_dx >= 0) {
		_cx = 224 - sprite_h;
		if (_dx < _cx) {
			_cx = sprite_h;
		} else {
			_cx = 224 - _dx;
		}
	} else {
		_cx = sprite_h + _dx;
		sprite_y = 0;
		_si -= sprite_w * _dx;		
	}
	
	if (_cx <= 0) {
		return;
	}
	uint16 sprite_clipped_h = _cx;

	if (!var14 && (sprite_flags & 0x10)) {
		_si += sprite_w - 1;
	}
	
	uint32 var2 = 256 * sprite_y + sprite_x;
	uint8 sprite_col_mask = (flags & 0x60) >> 1;

	if (_game_eraseBackground) {
		if (!(sprite_flags & 0x10)) {
			_vid.drawSpriteSub1(_si, _vid._frontLayer + var2, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		} else {
			_vid.drawSpriteSub2(_si, _vid._frontLayer + var2, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		}
	} else {
		if (!(sprite_flags & 0x10)) {
			_vid.drawSpriteSub3(_si, _vid._frontLayer + var2, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		} else {
			_vid.drawSpriteSub4(_si, _vid._frontLayer + var2, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		}
	}
}

void Game::game_decodeCharacterFrame(const uint8 *dataPtr, uint8 *dstPtr) {
	int n = READ_BE_UINT16(dataPtr); dataPtr += 2;
	uint16 len = n * 2;
	uint8 *dst = dstPtr + 0x400;
	while (n--) {
		uint8 c = *dataPtr++;
		dst[0] = (c & 0xF0) >> 4;
		dst[1] = (c & 0x0F) >> 0;
		dst += 2;
	}
	dst = dstPtr;
	const uint8 *src = dstPtr + 0x400;
	do {
		uint8 c1 = *src++;
		if (c1 == 0xF) {
			uint8 c2 = *src++;
			uint16 c3 = *src++;
			if (c2 == 0xF) {
				c1 = *src++;
				c2 = *src++;
				c3 = (c3 << 4) | c1;
				len -= 2;
			}
			memset(dst, c2, c3 + 4);
			dst += c3 + 4;
			len -= 3;
		} else {
			*dst++ = c1;
			--len;
		}
	} while (len != 0);
}


void Game::game_drawCharacter(const uint8 *dataPtr, int16 pos_x, int16 pos_y, uint8 a, uint8 b, uint8 flags) {
	debug(DBG_GAME, "Game::game_drawCharacter(0x%X, %d, %d, 0x%X, 0x%X, 0x%X)", dataPtr, pos_x, pos_y, a, b, flags);

	bool var16 = false; // sprite_mirror_y
	if (b & 0x40) {
		b &= 0xBF;
		SWAP(a, b);
		var16 = true;
	}
	uint16 sprite_h = a;
	uint16 sprite_w = b;
	
	const uint8 *src = dataPtr;
	bool var14 = false;
	
	int16 sprite_clipped_w;
	if (pos_x >= 0) {
		if (pos_x + sprite_w < 256) {
			sprite_clipped_w = sprite_w;
		} else {
			sprite_clipped_w = 256 - pos_x;
			if (flags & 2) {
				var14 = true;
				if (var16) {
					src += (sprite_w - 1) * sprite_h;
				} else {
					src += sprite_w - 1;
				}	
			}
		}
	} else {
		sprite_clipped_w = pos_x + sprite_w;
		if (!(flags & 2)) {
			if (var16) {
				src -= sprite_h * pos_x;
				pos_x = 0;
			} else {
				src -= pos_x;
				pos_x = 0;
			}
		} else {
			var14 = true;
			if (var16) {
				src += sprite_h * (pos_x + sprite_w - 1);
				pos_x = 0;
			} else {
				src += pos_x + sprite_w - 1;
				var14 = true;
				pos_x = 0;
			}
		}
	}
	if (sprite_clipped_w <= 0) {
		return;
	}
		
	int16 sprite_clipped_h;
	if (pos_y >= 0) {
		if (pos_y < 224 - sprite_h) {
			sprite_clipped_h = sprite_h;
		} else {
			sprite_clipped_h = 224 - pos_y;
		}
	} else {
		sprite_clipped_h = sprite_h + pos_y;
		if (var16) {
			src -= pos_y;
		} else {
			src -= sprite_w * pos_y;
		}
		pos_y = 0;
	}
	if (sprite_clipped_h <= 0) {
		return;
	}
	
	if (!var14 && (flags & 2)) {
		if (var16) {
			src += sprite_h * (sprite_w - 1);
		} else {
			src += sprite_w - 1;
		}
	}
	
	uint32 dst_offset = 256 * pos_y + pos_x;
	uint8 sprite_col_mask = ((flags & 0x60) == 0x60) ? 0x50 : 0x40;

	debug(DBG_GAME, "dst_offset = 0x%X src_offset = 0x%X", dst_offset, src - dataPtr);

	if (!(flags & 2)) {
		if (var16) {
			_vid.drawSpriteSub5(src, _vid._frontLayer + dst_offset, sprite_h, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		} else {
			_vid.drawSpriteSub3(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		}
	} else {
		if (var16) {
			_vid.drawSpriteSub6(src, _vid._frontLayer + dst_offset, sprite_h, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		} else {
			_vid.drawSpriteSub4(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		}
	}
}

uint8 *Game::game_processMBK(uint16 MbkEntryNum) {
	debug(DBG_GAME, "Game::game_processMBK(%d)", MbkEntryNum);
	MbkEntry *me = &_res._mbk[MbkEntryNum];
	uint16 _cx = _game_lastBankData - _game_firstBankData;
	uint16 size = (me->len & 0x7FFF) * 32;
	if (_cx < size) {
		_game_curBankSlot = &_game_bankSlots[0];
		_game_curBankSlot->entryNum = 0xFFFF;
		_game_curBankSlot->ptr = 0;
		_game_firstBankData = _game_bankData;
	}
	_game_curBankSlot->entryNum = MbkEntryNum;
	_game_curBankSlot->ptr = _game_firstBankData;
	++_game_curBankSlot;
	_game_curBankSlot->entryNum = 0xFFFF;
	_game_curBankSlot->ptr = 0;
	const uint8 *data = _res._mbkData + me->offset;
	if (me->len & 0x8000) {
		memcpy(_game_firstBankData, data, size);
	} else {
		assert(me->offset != 0);
		Unpack unp;
		int decSize;
		bool ret = unp.unpack(_game_firstBankData, data, 0, decSize);
		assert(ret);
	}
	uint8 *bank_data = _game_firstBankData;
	_game_firstBankData += size;
	assert(_game_firstBankData < _game_lastBankData);
	return bank_data;
}

int Game::game_loadMonsterSprites(LivePGE *pge) {
	debug(DBG_GAME, "Game::game_loadMonsterSprites()");
	InitPGE *init_pge = pge->init_PGE;
	if (init_pge->obj_node_number == 0x49 && init_pge->object_type != 10) {
		return 0;
	}
	if (init_pge->obj_node_number == _game_curMonsterFrame) {
		return 0xFFFF;
	}
	if (pge->room_location != _game_currentRoom) {
		return 0;
	}
	
	const uint8 *mList = _monsterListLevels[_game_currentLevel];
	while (*mList != init_pge->obj_node_number) {
		if (*mList == 0xFF) { // end of list
			return 0;
		}
		mList += 2;
	}
	_game_curMonsterFrame = mList[0];
	if (_game_curMonsterNum != mList[1]) {
		_game_curMonsterNum = mList[1];
		_res.load(_monsterNames[_game_curMonsterNum], Resource::OT_SPRM);
		_res.load_SPR_OFF(_monsterNames[_game_curMonsterNum], _res._sprm);
		_vid.setPaletteSlotLE(5, _monsterPals[_game_curMonsterNum]);
	}
	return 0xFFFF;
}

void Game::game_loadLevelMap() {
	debug(DBG_GAME, "Game::game_loadLevelMap() room = %d", _game_currentRoom);
	_game_currentIcon = 0xFF;
	_vid.copyLevelMap(_game_currentRoom);
	_vid.setLevelPalettes();
	memcpy(_vid._backLayer, _vid._frontLayer, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
}

void Game::game_loadLevelData() {	
	_res.clearLevelRes();
	
	_game_curMonsterNum = 0xFFFF;
	_game_curMonsterFrame = 0;
		
	const Level *pl = &_gameLevels[_game_currentLevel];
	_res.load(pl->name, Resource::OT_SPC);
	_res.load(pl->name, Resource::OT_MBK);
	_res.load(pl->name, Resource::OT_RP);
	_res.load(pl->name, Resource::OT_CT);
	_res.load(pl->name, Resource::OT_MAP);
	_res.load(pl->name, Resource::OT_PAL);
	
	_res.load(pl->name2, Resource::OT_PGE);
	_res.load(pl->name2, Resource::OT_OBJ);
	_res.load(pl->name2, Resource::OT_ANI);
	_res.load(pl->name2, Resource::OT_TBN);

	_cut._id = pl->cutscene_id;
	_game_curBankSlot = &_game_bankSlots[0];
	_game_curBankSlot->entryNum = 0xFFFF;
	_game_curBankSlot->ptr = 0;
	_game_firstBankData = _game_bankData;
	_game_printLevelCodeCounter = 150;
	
	_col_slots2Cur = _col_slots2;
	_col_slots2Next = 0;

	memset(_pge_liveTable2, 0, sizeof(_pge_liveTable2));
	memset(_pge_liveTable1, 0, sizeof(_pge_liveTable1));

	_game_currentRoom = _res._pgeInit[0].init_room;
	uint16 n = _res._pgeNum;
	while (n--) {
		pge_loadForCurrentLevel(n);
	}

	for (uint16 i = 0; i < _res._pgeNum; ++i) {
		if (_res._pgeInit[i].skill <= _game_skillLevel) {
			LivePGE *pge = &_pgeLive[i];
			pge->next_PGE_in_room = _pge_liveTable1[pge->room_location];
			_pge_liveTable1[pge->room_location] = pge;
		}
	}
	pge_resetGroups();
}

uint8 *Game::game_findBankData(uint16 entryNum) {
	BankSlot *slot = &_game_bankSlots[0];
	while (1) {
		if (slot->entryNum == entryNum) {
			return slot->ptr;
		}
		if (slot->entryNum == 0xFFFF) {
			break;
		}
		++slot;
	}
	return 0;
}

void Game::game_drawIcon(uint8 iconNum, int16 x, int16 y, uint8 colMask) {
	uint16 offset = READ_LE_UINT16(_res._icn + iconNum * 2);
	uint8 buf[256];
	uint8 *p = _res._icn + offset + 2;
	for (int i = 0; i < 128; ++i) {
		uint8 col = *p++;
		buf[i * 2 + 0] = (col & 0xF0) >> 4;
		buf[i * 2 + 1] = (col & 0x0F) >> 0;
	}
	_vid.drawSpriteSub1(buf, _vid._frontLayer + x + y * 256, 16, 16, 16, colMask << 4);
}

uint16 Game::game_getRandomNumber() {
	uint32 n = _game_randSeed * 2;
	if (_game_randSeed > n) {
		n ^= 0x1D872B41;
	}
	_game_randSeed = n;
	return n & 0xFFFF;
}

void Game::game_changeLevel() {
	_vid.fadeOut();
	game_loadLevelData();
	game_loadLevelMap();
	_vid.setPalette0xF();
	_vid.setPalette0xE();
}

uint16 Game::game_getLineLength(const uint8 *str) {
	uint16 len = 0;
	while (*str && *str != 0xB && *str != 0xA) {
		++str;
		++len;
	}
	return len;
}

void Game::game_handleInventory() {
	LivePGE *selected_pge = 0;
	LivePGE *pge = &_pgeLive[0];
	if (pge->life > 0 && pge->current_inventory_PGE != 0xFF) {
		snd_playSound(66);
		InventoryItem items[24];
		int num_items = 0;
		uint8 inv_pge = pge->current_inventory_PGE;
		while (inv_pge != 0xFF) {
			items[num_items].icon_num = _res._pgeInit[inv_pge].icon_num;
			items[num_items].init_pge = &_res._pgeInit[inv_pge];
			items[num_items].live_pge = &_pgeLive[inv_pge];
			inv_pge = _pgeLive[inv_pge].next_inventory_PGE;
			++num_items;
		}
		items[num_items].icon_num = 0xFF;
		int current_item = 0;
		int num_lines = (num_items - 1) / 4 + 1;
		int current_line = 0;		
		bool display_score = false;
		while (!_stub->_pi.backspace && !_stub->_pi.quit) {			
			// draw inventory background
			int icon_h = 5;
			int icon_y = 140;
			int icon_num = 31;
			do {
				int icon_x = 56;
				int icon_w = 9;
				do {
					game_drawIcon(icon_num, icon_x, icon_y, 0xF);
					++icon_num;
					icon_x += 16;
				} while (--icon_w);
				icon_y += 16;
			} while (--icon_h);
			
			if (!display_score) {
				int icon_x_pos = 72;
				for (int i = 0; i < 4; ++i) {
					int item_it = current_line * 4 + i;
					if (items[item_it].icon_num == 0xFF) {
						break;
					}
					game_drawIcon(items[item_it].icon_num, icon_x_pos, 157, 0xA);
					if (current_item == item_it) {
						game_drawIcon(76, icon_x_pos, 157, 0xA);
						selected_pge = items[item_it].live_pge;
						uint8 txt_num = items[item_it].init_pge->text_num;
						const char *str = (const char *)_res._tbn + READ_LE_UINT16(_res._tbn + txt_num * 2);
						_vid.drawString(str, (256 - strlen(str) * 8) / 2, 189, 0xED);
						if (items[item_it].init_pge->init_flags & 4) {
							char counterValue[10];
							sprintf(counterValue, "%d", selected_pge->life);
							_vid.drawString(counterValue, (256 - strlen(counterValue) * 8) / 2, 197, 0xED);			
						}
					}
					icon_x_pos += 32;
				}
				if (current_line != 0) {
					game_drawIcon(0x4E, 120, 176, 0xA); // down arrow
				}
				if (current_line != num_lines - 1) {
					game_drawIcon(0x4D, 120, 143, 0xA); // up arrow
				}
			} else {
				char textBuf[50];
				sprintf(textBuf, "SCORE %08lu", _game_score);
				_vid.drawString(textBuf, (114 - strlen(textBuf) * 8) / 2 + 72, 158, 0xE5);
				switch (_ver) {
				case VER_FR:
					sprintf(textBuf, "NIVEAU:%s", _menu._textOptions[7 + _game_skillLevel]);
					break;
				case VER_EN:
					sprintf(textBuf, "LEVEL:%s", _menu._textOptions[7 + _game_skillLevel]);
					break;
				}
				_vid.drawString(textBuf, (114 - strlen(textBuf) * 8) / 2 + 72, 166, 0xE5);
			}
		
			_stub->copyRect(0, 0, Video::GAMESCREEN_W, Video::GAMESCREEN_H, _vid._frontLayer, 256);
			_stub->sleep(80);
			_stub->processEvents();

			if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;				
				if (current_line < num_lines - 1) {
					++current_line;
					current_item = current_line * 4;
				}
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				if (current_line > 0) {
					--current_line;
					current_item = current_line * 4;
				}
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_LEFT) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				if (current_item > 0) {
					int item_num = current_item % 4;
					if (item_num > 0) {
						--current_item;
					}
				}				
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_RIGHT) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				if (current_item < num_items - 1) {
					int item_num = current_item % 4;
					if (item_num < 3) {
						++current_item;
					}
				}
			}
			if (_stub->_pi.enter) {
				_stub->_pi.enter = false;
				display_score = !display_score;
			}
		}
		_stub->_pi.backspace = false;
		if (selected_pge) {
			pge_setCurrentInventoryObject(selected_pge);
		}
		snd_playSound(66);
	}
}

void Game::inp_update() {
	_stub->processEvents();
	_pge_inpKeysMask = _stub->_pi.dirMask;
	if (_stub->_pi.enter) {
		_pge_inpKeysMask |= 0x10;
	}
	if (_stub->_pi.space) {
		_pge_inpKeysMask |= 0x20;
	}
	if (_stub->_pi.shift) {
		_pge_inpKeysMask |= 0x40;
	}
}

void Game::snd_playSound(uint16 a) {
//	warning("playing sound 0x%X not yet implemented", a);
}

void AnimBuffers::addState(uint8 stateNum, int16 x, int16 y, const uint8 *dataPtr, LivePGE *pge) {
	debug(DBG_GAME, "AnimBuffers::addState() stateNum=%d x=%d y=%d dataPtr=0x%X pge=0x%X", stateNum, x, y, dataPtr, pge);
	assert(stateNum < 4);
	AnimBufferState *abstate = _states[stateNum];
	abstate->x = x;
	abstate->y = y;
	abstate->dataPtr = dataPtr;
	abstate->pge = pge;
	++_curPos[stateNum];
	++_states[stateNum];
}
