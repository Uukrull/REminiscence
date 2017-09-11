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
#include "file.h"
#include "systemstub.h"
#include "unpack.h"
#include "game.h"


Game::Game(SystemStub *stub, const char *dataPath, const char *savePath, Version ver)
	: _cut(&_res, stub, &_vid, ver), _menu(&_res, stub, &_vid, ver), _mix(stub), 
	_res(dataPath), _vid(&_res, stub), _stub(stub), _ver(ver), _savePath(savePath) {
	switch (_ver) {
	case VER_FR:
		_stringsTable = _stringsTableFR;
		_textsTable = _textsTableFR;
		break;
	case VER_US:
		_stringsTable = _stringsTableEN;
		_textsTable = _textsTableEN;
		break;	
	}
	_stateSlot = 1;
}

void Game::run() {
	_stub->init("REminiscence", Video::GAMESCREEN_W, Video::GAMESCREEN_H);
	_mix.init();
	
	_randSeed = time(0);
	
	_menu._charVar1 = 0;
	_menu._charVar2 = 0;
	_menu._charVar3 = 0;
	_menu._charVar4 = 0;
	_menu._charVar5 = 0;
	
	_vid._drawCharColor1 = 0;
	_vid._drawCharColor2 = 0;
	_vid._drawCharColor3 = 0;
	
	_res.load("FB_TXT", Resource::OT_FNT);

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
	_res.load_FIB("GLOBAL");
	
	_skillLevel = 1;
	_currentLevel = 0;
	
	while (!_stub->_pi.quit && _menu.handleTitleScreen(_skillLevel, _currentLevel)) {
		if (_currentLevel == 7) {
			_vid.fadeOut();
			_vid.setPalette0xE();
			_cut._id = 0x3D;
			_cut.play();
		} else {
			_vid.setPalette0xE();
			_vid.setPalette0xF();
			_stub->setOverscanColor(0xE0);
			mainLoop();
		}
	}
	
	_mix.free();
	_stub->destroy();
}
	
void Game::resetGameState() {
	_animBuffers._states[0] = _animBuffer0State;
	_animBuffers._curPos[0] = 0xFF;
	_animBuffers._states[1] = _animBuffer1State;
	_animBuffers._curPos[1] = 0xFF;
	_animBuffers._states[2] = _animBuffer2State;
	_animBuffers._curPos[2] = 0xFF;
	_animBuffers._states[3] = _animBuffer3State;
	_animBuffers._curPos[3] = 0xFF;
	_currentRoom = _res._pgeInit[0].init_room;
	_cut._deathCutsceneId = 0xFFFF;
	_pge_opTempVar2 = 0xFFFF;
	_deathCutsceneCounter = 0;
	_saveStateCompleted = false;
	_loadMap = true;
	pge_resetGroups();
	_blinkingConradCounter = 0;
	_pge_processOBJ = false;
	_pge_opTempVar1 = 0;
	_textToDisplay = 0xFFFF;
}

void Game::mainLoop() {
	_vid._unkPalSlot1 = 0;
	_vid._unkPalSlot2 = 0;
	_score = 0;
	_firstBankData = _bankData;
	_lastBankData = _bankData + sizeof(_bankData);
	loadLevelData();
	resetGameState();
	while (!_stub->_pi.quit) {
		_cut.play();
		if (_cut._id == 0x3D) {
			showFinalScore();
			break;
		}
		if (_deathCutsceneCounter) {
			--_deathCutsceneCounter;
			if (_deathCutsceneCounter == 0) {
				_cut._id = _cut._deathCutsceneId;
				_cut.play();
				if (!handleContinueAbort()) {
					_cut._id = 0x41;
					_cut.play();					
					break;
				} else {
					if (_validSaveState) {
						loadGameState(0);
					} else {
						loadLevelData();
						resetGameState();
					}
					continue;
				}
				
			}
		}		
		memcpy(_vid._frontLayer, _vid._backLayer, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
		inp_update();
		pge_prepare();
		col_prepareRoomState();
		uint8 oldLevel = _currentLevel;
		for (uint16 i = 0; i < _res._pgeNum; ++i) {
			LivePGE *pge = _pge_liveTable2[i];
			if (pge) {
				_col_currentPiegeGridPosY = (pge->pos_y / 36) & ~1;
				_col_currentPiegeGridPosX = (pge->pos_x + 8) / 16;
				pge_process(pge);
			}
		}
		if (oldLevel != _currentLevel) {
			changeLevel();
			_pge_opTempVar1 = 0;
			continue;
		}
		if (_loadMap) {
			if (_currentRoom == 0xFF) {
				_cut._id = 6;
				_deathCutsceneCounter = 1;
			} else {
				_currentRoom = _pgeLive[0].room_location;
				loadLevelMap();
				_loadMap = false;
				_vid.fullRefresh();
			}
		}
		prepareAnims();
		drawAnims();
		drawCurrentInventoryItem();
		drawLevelTexts();
		printLevelCode();
		if (_blinkingConradCounter != 0) {
			--_blinkingConradCounter;
		}

		_vid.updateScreen();
		static uint32 tstamp = 0;
		if (!(_stub->_pi.dbgMask & PlayerInput::DF_FASTMODE)) {
			int32 delay = _stub->getTimeStamp() - tstamp;
			int32 pause = 30 - delay;
			if (pause > 0) {
				_stub->sleep(pause);
			}
			tstamp = _stub->getTimeStamp();
		}
		
		drawStoryTexts();
		if (_stub->_pi.backspace) {
			_stub->_pi.backspace = false;
			handleInventory();
		}
		if (_stub->_pi.dbgMask & PlayerInput::DF_SETLIFE) {
			_pgeLive[0].life = 0x7FFF;
		}
		if (_stub->_pi.load) {
			loadGameState(_stateSlot);
			_stub->_pi.load = false;
		}
		if (_stub->_pi.save) {
			saveGameState(_stateSlot);
			_stub->_pi.save = false;
		}
		if (_stub->_pi.stateSlot != 0) {
			int8 slot = _stateSlot + _stub->_pi.stateSlot;
			if (slot >= 1 && slot < 100) {
				_stateSlot = slot;
				debug(DBG_INFO, "Current game state slot is %d", _stateSlot);
			}
			_stub->_pi.stateSlot = 0;
		}
	}
}

void Game::drawCurrentInventoryItem() {
	uint16 src = _pgeLive[0].current_inventory_PGE;
	if (src != 0xFF) {
		_currentIcon = _res._pgeInit[src].icon_num;
		drawIcon(_currentIcon, 232, 8, 0xA);
	}	
}

void Game::showFinalScore() {
	_cut._id = 0x49;
	_cut.play();
	char textBuf[50];
	sprintf(textBuf, "SCORE %08lu", _score);
	_vid.drawString(textBuf, (256 - strlen(textBuf) * 8) / 2, 40, 0xE5);
	strcpy(textBuf, _menu._passwords[7][_skillLevel]);
	_vid.drawString(textBuf, (256 - strlen(textBuf) * 8) / 2, 16, 0xE7);
	while (!_stub->_pi.quit) {
		_stub->copyRect(0, 0, Video::GAMESCREEN_W, Video::GAMESCREEN_H, _vid._frontLayer, 256);
		_stub->updateScreen();
		_stub->processEvents();
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			break;
		}
		_stub->sleep(100);
	}
}

bool Game::handleContinueAbort() {
	_cut._id = 0x48;
	_cut.play();
	char textBuf[50];
	int timeout = 100;
	int current_color = 0;
	uint8 colors[] = { 0xE4, 0xE5 };
	uint8 color_inc = 0xFF;
	Color col;
	_stub->getPaletteEntry(0xE4, &col);
	memcpy(_vid._tempLayer, _vid._frontLayer, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
	while (timeout >= 0 && !_stub->_pi.quit) {
		_vid.drawString(_textsTable[0], (256 - strlen(_textsTable[0]) * 8) / 2, 64, 0xE3);
		sprintf(textBuf, "%s : %d", _textsTable[1], timeout / 10);
		_vid.drawString(textBuf, 96, 88, 0xE3);
		_vid.drawString(_textsTable[2], (256 - strlen(_textsTable[2]) * 8) / 2, 104, colors[0]);
		_vid.drawString(_textsTable[3], (256 - strlen(_textsTable[3]) * 8) / 2, 112, colors[1]);		
		sprintf(textBuf, "SCORE  %08lu", _score);
		_vid.drawString(textBuf, 64, 154, 0xE3);
		if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
			if (current_color > 0) {
				SWAP(colors[current_color], colors[current_color - 1]);
				--current_color;
			}
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
			if (current_color < 1) {
				SWAP(colors[current_color], colors[current_color + 1]);
				++current_color;
			}
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			return (current_color == 0);
		}
		_stub->copyRect(0, 0, Video::GAMESCREEN_W, Video::GAMESCREEN_H, _vid._frontLayer, 256);
		_stub->updateScreen();
		if (col.b >= 0x3D) {
			color_inc = 0;
		}
		if (col.b < 2) {
			color_inc = 0xFF;
		}
		if (color_inc == 0xFF) {
			col.b += 2;
			col.g += 2;
		} else {
			col.b -= 2;
			col.g -= 2;
		}		
		_stub->setPaletteEntry(0xE4, &col);
		_stub->processEvents();
		_stub->sleep(100);
		--timeout;
		memcpy(_vid._frontLayer, _vid._tempLayer, Video::GAMESCREEN_W * Video::GAMESCREEN_H);		
	}
	return false;
}

void Game::printLevelCode() {
	if (_printLevelCodeCounter != 0) {
		--_printLevelCodeCounter;
		if (_printLevelCodeCounter != 0) {
			char levelCode[50];
			sprintf(levelCode, "CODE: %s", _menu._passwords[_currentLevel][_skillLevel]);
			_vid.drawString(levelCode, (Video::GAMESCREEN_W - strlen(levelCode) * 8) / 2, 16, 0xE7);
		}
	}
}

void Game::printSaveStateCompleted() {
	if (_saveStateCompleted) {
		_vid.drawString(_textsTable[4], (176 - strlen(_textsTable[4]) * 8) / 2, 34, 0xE6);
	}	
}

void Game::drawLevelTexts() {
	LivePGE *_si = &_pgeLive[0];
	int8 obj = col_findCurrentCollidingObject(_si, 3, 0xFF, 0xFF, &_si);
	if (obj == 0) {
		obj = col_findCurrentCollidingObject(_si, 0xFF, 5, 9, &_si);
	}
	if (obj == 0 || obj < 0) {
		return;
	}	
	_printLevelCodeCounter = 0;
	if (_textToDisplay == 0xFFFF) {
		uint8 icon_num = obj - 1;
		drawIcon(icon_num, 80, 8, 0xA);
		uint8 txt_num = _si->init_PGE->text_num;
		const char *str = (const char *)_res._tbn + READ_LE_UINT16(_res._tbn + txt_num * 2);
		_vid.drawString(str, (176 - strlen(str) * 8) / 2, 26, 0xE6);
		if (icon_num == 2) {
			printSaveStateCompleted();
		} else {
			_saveStateCompleted = false;
		}
	} else {
		_currentInventoryIconNum = obj - 1;
	}
}

void Game::drawStoryTexts() {
	if (_textToDisplay != 0xFFFF) {
		uint16 text_col_mask = 0xE8;
		const uint8 *str = _stringsTable + READ_LE_UINT16(_stringsTable + _textToDisplay * 2);
		memcpy(_vid._tempLayer, _vid._frontLayer, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
		while (!_stub->_pi.quit) {
			drawIcon(_currentInventoryIconNum, 80, 8, 0xA);
			if (*str == 0xFF) {
				text_col_mask = READ_LE_UINT16(str + 1);
				str += 3;
			}			
			int16 text_y_pos = 26;
			while (1) {
				uint16 len = getLineLength(str);
				str = (const uint8 *)_vid.drawString((const char *)str, (176 - len * 8) / 2, text_y_pos, text_col_mask);
				text_y_pos += 8;
				if (*str == 0 || *str == 0xB) {
					break;
				}
				++str;
			}
			_vid.updateScreen();
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
		_textToDisplay = 0xFFFF;
	}
}

void Game::prepareAnims() {
	if (!(_currentRoom & 0x80) && _currentRoom < 0x40) {
		int8 pge_room;
		LivePGE *pge = _pge_liveTable1[_currentRoom];
		while (pge) {
			prepareAnimsHelper(pge, 0, 0);
			pge = pge->next_PGE_in_room;
		}
		pge_room = _res._ctData[0x00 + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if ((pge->init_PGE->object_type != 10 && pge->pos_y > 176) || (pge->init_PGE->object_type == 10 && pge->pos_y > 216)) {
					prepareAnimsHelper(pge, 0, -216);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[0x40 + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if (pge->pos_y < 48) {
					prepareAnimsHelper(pge, 0, 216);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[0xC0 + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {			
				if (pge->pos_x > 224) {
					prepareAnimsHelper(pge, -256, 0);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[0x80 + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {			
				if (pge->pos_x <= 32) {
					prepareAnimsHelper(pge, 256, 0);
				}
				pge = pge->next_PGE_in_room;
			}
		}		
	}
}

void Game::prepareAnimsHelper(LivePGE *pge, int16 dx, int16 dy) {
	debug(DBG_GAME, "Game::prepareAnimsHelper() dx=0x%X dy=0x%X pge_num=%d pge=0x%X pge->flags=0x%X pge->anim_number=0x%X", dx, dy, pge - &_pgeLive[0], pge, pge->flags, pge->anim_number);
	int16 xpos, ypos;
	if (!(pge->flags & 8)) {
		if (pge->index != 0 && loadMonsterSprites(pge) == 0) {
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
			_animBuffers.addState(1, xpos, ypos, dataPtr, pge);
		} else if (pge->flags & 0x10) {
			_animBuffers.addState(2, xpos, ypos, dataPtr, pge);
		} else {
			_animBuffers.addState(0, xpos, ypos, dataPtr, pge);
		}
	} else {
		assert(pge->anim_number < _res._numSpc);
		const uint8 *dataPtr = _res._spc + READ_BE_UINT16(_res._spc + pge->anim_number * 2);
		xpos = dx + pge->pos_x + 8;
		ypos = dy + pge->pos_y + 2;

		if (pge->init_PGE->object_type == 11) {
			_animBuffers.addState(3, xpos, ypos, dataPtr, pge);
		} else if (pge->flags & 0x10) {
			_animBuffers.addState(2, xpos, ypos, dataPtr, pge);
		} else {
			_animBuffers.addState(0, xpos, ypos, dataPtr, pge);
		}
	}
}

void Game::drawAnims() {
	debug(DBG_GAME, "Game::drawAnims()");	
	_eraseBackground = false;
	drawAnimBuffer(2, _animBuffer2State);
	drawAnimBuffer(1, _animBuffer1State);
	drawAnimBuffer(0, _animBuffer0State);
	_eraseBackground = true;
	drawAnimBuffer(3, _animBuffer3State);
}

void Game::drawAnimBuffer(uint8 stateNum, AnimBufferState *state) {
	debug(DBG_GAME, "Game::drawAnimBuffer() state = %d", stateNum);
	assert(stateNum < 4);
	_animBuffers._states[stateNum] = state;
	uint8 lastPos = _animBuffers._curPos[stateNum];
	if (lastPos != 0xFF) {
		uint8 numAnims = lastPos + 1;
		state += lastPos;
		_animBuffers._curPos[stateNum] = 0xFF;
		do {
			LivePGE *pge = state->pge;
			if (!(pge->flags & 8)) {
				if (stateNum == 1 && (_blinkingConradCounter & 1)) {
					break;
				}
				if (!(state->dataPtr[-2] & 0x80)) {
					decodeCharacterFrame(state->dataPtr, _res._memBuf);
					drawCharacter(_res._memBuf, state->x, state->y, state->dataPtr[-1], state->dataPtr[-2], pge->flags);
				} else {
					drawCharacter(state->dataPtr, state->x, state->y, state->dataPtr[-1], state->dataPtr[-2], pge->flags);
				}
			} else {
				drawObject(state->dataPtr, state->x, state->y, pge->flags);
			}
			--state;
		} while (--numAnims != 0);
	}	
}

void Game::drawObject(const uint8 *dataPtr, int16 x, int16 y, uint8 flags) {
	debug(DBG_GAME, "Game::drawObject() dataPtr[] = 0x%X dx = %d dy = %d",  dataPtr[0], (int8)dataPtr[1], (int8)dataPtr[2]);
	assert(dataPtr[0] < 0x4A);
	uint8 slot = _res._rp[dataPtr[0]];
	uint8 *data = findBankData(slot);
	if (data == 0) {
		data = processMBK(slot);
	}
	_bankDataPtrs = data;
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
		drawObjectFrame(dataPtr, posx, posy, flags);
		dataPtr += 4;
	}
}

void Game::drawObjectFrame(const uint8 *dataPtr, int16 x, int16 y, uint8 flags) {
	debug(DBG_GAME, "Game::drawObjectFrame(0x%X, %d, %d, 0x%X)", dataPtr, x, y, flags);
	const uint8 *_si = _bankDataPtrs + dataPtr[0] * 32;
	
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

	if (_eraseBackground) {
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
	_vid.markBlockAsDirty(sprite_x, sprite_y, sprite_clipped_w, sprite_clipped_h);
}

void Game::decodeCharacterFrame(const uint8 *dataPtr, uint8 *dstPtr) {
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


void Game::drawCharacter(const uint8 *dataPtr, int16 pos_x, int16 pos_y, uint8 a, uint8 b, uint8 flags) {
	debug(DBG_GAME, "Game::drawCharacter(0x%X, %d, %d, 0x%X, 0x%X, 0x%X)", dataPtr, pos_x, pos_y, a, b, flags);

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
	_vid.markBlockAsDirty(pos_x, pos_y, sprite_clipped_w, sprite_clipped_h);
}

uint8 *Game::processMBK(uint16 MbkEntryNum) {
	debug(DBG_GAME, "Game::processMBK(%d)", MbkEntryNum);
	MbkEntry *me = &_res._mbk[MbkEntryNum];
	uint16 _cx = _lastBankData - _firstBankData;
	uint16 size = (me->len & 0x7FFF) * 32;
	if (_cx < size) {
		_curBankSlot = &_bankSlots[0];
		_curBankSlot->entryNum = 0xFFFF;
		_curBankSlot->ptr = 0;
		_firstBankData = _bankData;
	}
	_curBankSlot->entryNum = MbkEntryNum;
	_curBankSlot->ptr = _firstBankData;
	++_curBankSlot;
	_curBankSlot->entryNum = 0xFFFF;
	_curBankSlot->ptr = 0;
	const uint8 *data = _res._mbkData + me->offset;
	if (me->len & 0x8000) {
		memcpy(_firstBankData, data, size);
	} else {
		assert(me->offset != 0);
		Unpack unp;
		int decSize;
		bool ret = unp.unpack(_firstBankData, data, 0, decSize);
		assert(ret);
	}
	uint8 *bank_data = _firstBankData;
	_firstBankData += size;
	assert(_firstBankData < _lastBankData);
	return bank_data;
}

int Game::loadMonsterSprites(LivePGE *pge) {
	debug(DBG_GAME, "Game::loadMonsterSprites()");
	InitPGE *init_pge = pge->init_PGE;
	if (init_pge->obj_node_number == 0x49 && init_pge->object_type != 10) {
		return 0;
	}
	if (init_pge->obj_node_number == _curMonsterFrame) {
		return 0xFFFF;
	}
	if (pge->room_location != _currentRoom) {
		return 0;
	}
	
	const uint8 *mList = _monsterListLevels[_currentLevel];
	while (*mList != init_pge->obj_node_number) {
		if (*mList == 0xFF) { // end of list
			return 0;
		}
		mList += 2;
	}
	_curMonsterFrame = mList[0];
	if (_curMonsterNum != mList[1]) {
		_curMonsterNum = mList[1];
		_res.load(_monsterNames[_curMonsterNum], Resource::OT_SPRM);
		_res.load_SPR_OFF(_monsterNames[_curMonsterNum], _res._sprm);
		_vid.setPaletteSlotLE(5, _monsterPals[_curMonsterNum]);
	}
	return 0xFFFF;
}

void Game::loadLevelMap() {
	debug(DBG_GAME, "Game::loadLevelMap() room = %d", _currentRoom);
	_currentIcon = 0xFF;
	_vid.copyLevelMap(_currentRoom);
	_vid.setLevelPalettes();
}

void Game::loadLevelData() {	
	_res.clearLevelRes();
			
	const Level *pl = &_gameLevels[_currentLevel];
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

	_curMonsterNum = 0xFFFF;
	_curMonsterFrame = 0;

	_curBankSlot = &_bankSlots[0];
	_curBankSlot->entryNum = 0xFFFF;
	_curBankSlot->ptr = 0;
	_firstBankData = _bankData;
	_printLevelCodeCounter = 150;
	
	_col_slots2Cur = _col_slots2;
	_col_slots2Next = 0;

	memset(_pge_liveTable2, 0, sizeof(_pge_liveTable2));
	memset(_pge_liveTable1, 0, sizeof(_pge_liveTable1));
	
	_currentRoom = _res._pgeInit[0].init_room;
	uint16 n = _res._pgeNum;
	while (n--) {
		pge_loadForCurrentLevel(n);
	}

	for (uint16 i = 0; i < _res._pgeNum; ++i) {
		if (_res._pgeInit[i].skill <= _skillLevel) {
			LivePGE *pge = &_pgeLive[i];
			pge->next_PGE_in_room = _pge_liveTable1[pge->room_location];
			_pge_liveTable1[pge->room_location] = pge;
		}
	}
	pge_resetGroups();
	_validSaveState = false;
}

uint8 *Game::findBankData(uint16 entryNum) {
	BankSlot *slot = &_bankSlots[0];
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

void Game::drawIcon(uint8 iconNum, int16 x, int16 y, uint8 colMask) {
	uint16 offset = READ_LE_UINT16(_res._icn + iconNum * 2);
	uint8 buf[256];
	uint8 *p = _res._icn + offset + 2;
	for (int i = 0; i < 128; ++i) {
		uint8 col = *p++;
		buf[i * 2 + 0] = (col & 0xF0) >> 4;
		buf[i * 2 + 1] = (col & 0x0F) >> 0;
	}
	_vid.drawSpriteSub1(buf, _vid._frontLayer + x + y * 256, 16, 16, 16, colMask << 4);
	_vid.markBlockAsDirty(x, y, 16, 16);
}

uint16 Game::getRandomNumber() {
	uint32 n = _randSeed * 2;
	if (_randSeed > n) {
		n ^= 0x1D872B41;
	}
	_randSeed = n;
	return n & 0xFFFF;
}

void Game::changeLevel() {
	_vid.fadeOut();
	loadLevelData();
	loadLevelMap();
	_vid.setPalette0xF();
	_vid.setPalette0xE();
}

uint16 Game::getLineLength(const uint8 *str) {
	uint16 len = 0;
	while (*str && *str != 0xB && *str != 0xA) {
		++str;
		++len;
	}
	return len;
}

void Game::handleInventory() {
	LivePGE *selected_pge = 0;
	LivePGE *pge = &_pgeLive[0];
	if (pge->life > 0 && pge->current_inventory_PGE != 0xFF) {
		snd_playSound(66, 0);
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
					drawIcon(icon_num, icon_x, icon_y, 0xF);
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
					drawIcon(items[item_it].icon_num, icon_x_pos, 157, 0xA);
					if (current_item == item_it) {
						drawIcon(76, icon_x_pos, 157, 0xA);
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
					drawIcon(0x4E, 120, 176, 0xA); // down arrow
				}
				if (current_line != num_lines - 1) {
					drawIcon(0x4D, 120, 143, 0xA); // up arrow
				}
			} else {
				char textBuf[50];
				sprintf(textBuf, "SCORE %08lu", _score);
				_vid.drawString(textBuf, (114 - strlen(textBuf) * 8) / 2 + 72, 158, 0xE5);
				sprintf(textBuf, "%s:%s", _textsTable[5], _menu._textOptions[7 + _skillLevel]);
				_vid.drawString(textBuf, (114 - strlen(textBuf) * 8) / 2 + 72, 166, 0xE5);
			}
		
			_vid.updateScreen();
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
		_vid.fullRefresh();
		_stub->_pi.backspace = false;
		if (selected_pge) {
			pge_setCurrentInventoryObject(selected_pge);
		}
		snd_playSound(66, 0);
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

void Game::snd_playSound(uint8 sfxId, uint8 softVol) {
	if (sfxId < _res._numSfx) {
		SoundFx *sfx = &_res._sfxList[sfxId];
		if (sfx->data) {
			MixerChunk mc;
			mc.data = sfx->data;
			mc.len = sfx->len;
			_mix.play(&mc, 6000, 64 >> softVol);
		}
	}
}

void Game::makeGameStateName(uint8 slot, char *buf) {
	sprintf(buf, "rs%d.s%02d", _currentLevel + 1, slot);
}

void Game::saveGameState(uint8 slot) {
	char stateFile[20];
	makeGameStateName(slot, stateFile);
	File f(true);
	if (!f.open(stateFile, _savePath, "wb")) {
		warning("Unable to save state file '%s'", stateFile);
	} else {
		// header
		f.writeUint32BE('FBSV');
		f.writeUint16BE(0);
		char hdrdesc[32];
		memset(hdrdesc, 0, sizeof(hdrdesc));
		sprintf(hdrdesc, "level%d", _currentLevel + 1);
		f.write(hdrdesc, sizeof(hdrdesc));
		// contents
		saveState(&f);
		if (f.ioErr()) {
			warning("I/O error when saving game state");
		} else {
			debug(DBG_INFO, "Saved state to slot %d", slot);
		}
	}
}

void Game::loadGameState(uint8 slot) {
	char stateFile[20];
	makeGameStateName(slot, stateFile);
	File f(true);
	if (!f.open(stateFile, _savePath, "rb")) {
		warning("Unable to open state file '%s'", stateFile);
	} else {
		uint32 id = f.readUint32BE();
		if (id != 'FBSV') {
			warning("Bad save state format");
		} else {
			uint16 ver = f.readUint16BE();
			if (ver != 0) {
				warning("Invalid save state version");				
			} else {
				char hdrdesc[32];
				f.read(hdrdesc, sizeof(hdrdesc));
				// contents
				loadState(&f);
				if (f.ioErr()) {
					warning("I/O error when loading game state");
				} else {
					debug(DBG_INFO, "Loaded state from slot %d", slot);
				}
			}
		}
	}
}

void Game::saveState(File *f) {
	f->writeUint32BE(_col_slots2Cur - &_col_slots2[0]);
	f->writeUint32BE(_col_slots2Next - &_col_slots2[0]);
	f->writeUint32BE(_score);
	for (int i = 0; i < _res._pgeNum; ++i) {
		LivePGE *pge = &_pgeLive[i];
		f->writeUint16BE(pge->obj_type);
		f->writeUint16BE(pge->pos_x);
		f->writeUint16BE(pge->pos_y);
		f->writeByte(pge->anim_seq);
		f->writeByte(pge->room_location);
		f->writeUint16BE(pge->life);
		f->writeUint16BE(pge->counter_value);
		f->writeByte(pge->collision_slot);
		f->writeByte(pge->next_inventory_PGE);
		f->writeByte(pge->current_inventory_PGE);
		f->writeByte(pge->unkF);
		f->writeUint16BE(pge->anim_number);
		f->writeByte(pge->flags);
		f->writeByte(pge->index);
		f->writeUint16BE(pge->first_obj_number);
		f->writeUint32BE(pge->next_PGE_in_room - &_pgeLive[0]);
		f->writeUint32BE(pge->init_PGE - &_res._pgeInit[0]);		
	}
	f->write(&_res._ctData[0x100], 0x1C00);
	for (CollisionSlot2 *cs2 = &_col_slots2[0]; cs2 < _col_slots2Cur; ++cs2) {
		f->writeUint32BE(cs2->next_slot - &_col_slots2[0]);
		f->writeUint32BE(cs2->unk2 - &_res._ctData[0x100]);
		f->writeByte(cs2->data_size);
		f->write(cs2->data_buf, 0x10);
	}
}

void Game::loadState(File *f) {
	uint16 i;
	memset(_pge_liveTable2, 0, sizeof(_pge_liveTable2));
	memset(_pge_liveTable1, 0, sizeof(_pge_liveTable1));
	_col_slots2Cur = &_col_slots2[0] + f->readUint32BE();
	_col_slots2Next = &_col_slots2[0] + f->readUint32BE();
	_score = f->readUint32BE();
	for (i = 0; i < _res._pgeNum; ++i) {
		LivePGE *pge = &_pgeLive[i];
		pge->obj_type = f->readUint16BE();
		pge->pos_x = f->readUint16BE();
		pge->pos_y = f->readUint16BE();
		pge->anim_seq = f->readByte();
		pge->room_location = f->readByte();
		pge->life = f->readUint16BE();
		pge->counter_value = f->readUint16BE();
		pge->collision_slot = f->readByte();
		pge->next_inventory_PGE = f->readByte();
		pge->current_inventory_PGE = f->readByte();
		pge->unkF = f->readByte();
		pge->anim_number = f->readUint16BE();
		pge->flags = f->readByte();
		pge->index = f->readByte();
		pge->first_obj_number = f->readUint16BE();
		pge->next_PGE_in_room = &_pgeLive[0] + f->readUint32BE();
		pge->init_PGE = &_res._pgeInit[0] + f->readUint32BE();
	}
	f->read(&_res._ctData[0x100], 0x1C00);
	for (CollisionSlot2 *cs2 = &_col_slots2[0]; cs2 < _col_slots2Cur; ++cs2) {
		cs2->next_slot = &_col_slots2[0] + f->readUint32BE();
		cs2->unk2 = &_res._ctData[0x100] + f->readUint32BE();
		cs2->data_size = f->readByte();
		f->read(cs2->data_buf, 0x10);
	}
	for (i = 0; i < _res._pgeNum; ++i) {
		if (_res._pgeInit[i].skill <= _skillLevel) {
			LivePGE *pge = &_pgeLive[i];
			if (pge->flags & 4) {
				_pge_liveTable2[pge->index] = pge;
			}
			pge->next_PGE_in_room = _pge_liveTable1[pge->room_location];
			_pge_liveTable1[pge->room_location] = pge;
		}
	}
	resetGameState();
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
