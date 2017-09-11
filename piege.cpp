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

#include "game.h"
#include "resource.h"
#include "systemstub.h"


void Game::pge_resetGroups() {
	memset(_pge_groupsTable, 0, sizeof(_pge_groupsTable));
	GroupPGE *le = &_pge_groups[0];	
	_pge_nextFreeGroup = le;
	int n = 0xFF;
	while (n--) {
		le->next_entry = le + 1;
		le->index = 0;
		le->group_id = 0;
		++le;
	}
	le->next_entry = 0;
	le->index = 0;
	le->group_id = 0;
}

void Game::pge_removeFromGroup(int index) {
	GroupPGE *le = _pge_groupsTable[index];
	if (le) {
		_pge_groupsTable[index] = 0;
		GroupPGE *next = _pge_nextFreeGroup;
		while (le) {
			GroupPGE *cur = le->next_entry;
			le->next_entry = next;
			le->index = 0;
			le->group_id = 0;
			next = le;
			le = cur;
		}
		_pge_nextFreeGroup = next;
	}
}

int Game::pge_isInGroup(LivePGE *pge_dst, uint16 group_id, uint16 counter) {
	assert(counter >= 1 && counter <= 4);
	uint16 c = pge_dst->init_PGE->counter_values[counter - 1];
	GroupPGE *le = _pge_groupsTable[pge_dst->index];
	while (le) {		
		if (le->group_id == group_id && le->index == c)
			return 1;
		le = le->next_entry;
	}
	return 0;
}

void Game::pge_loadForCurrentLevel(int index) {
	debug(DBG_PGE, "Game::pge_loadForCurrentLevel() index = %d", index);

	LivePGE *live_pge = &_pgeLive[index];
	InitPGE *init_pge = &_res._pgeInit[index];

	live_pge->init_PGE = init_pge;
	live_pge->obj_type = init_pge->type;
	live_pge->pos_x = init_pge->pos_x;
	live_pge->pos_y = init_pge->pos_y;
	live_pge->anim_seq = 0;
	live_pge->room_location = init_pge->init_room;
	
	live_pge->life = init_pge->life;
	if (_game_skillLevel >= 2 && init_pge->object_family == 10) {
		live_pge->life *= 2;
	}
	live_pge->counter_value = 0;
	live_pge->collision_slot = 0xFF;
	live_pge->next_inventory_PGE = 0xFF;
	live_pge->current_inventory_PGE = 0xFF;
	live_pge->unkF = 0xFF;
	live_pge->anim_number = 0;
	live_pge->index = index;
	live_pge->next_PGE_in_room = 0;
	
	// let's cheat :)
	// if (index == 0) live_pge->life = 0x7FFF;

	uint16 _dx = 0;
	if (init_pge->skill <= _game_skillLevel) {
		if (init_pge->room_location != 0 || ((init_pge->flags & 4) && (_game_currentRoom == init_pge->init_room))) {
			_dx |= 4;
			_pge_liveTable2[index] = live_pge;
		}
		if (init_pge->unk1A != 0) {
			_dx |= 1;
		}
		if (init_pge->init_flags & 8) {
			_dx |= 0x10;
		}
		_dx |= (init_pge->init_flags & 3) << 5;
		if (init_pge->flags & 2) {
			_dx |= 0x80;
		}
		live_pge->flags = _dx;
		assert(init_pge->obj_node_number < _res._numObjectNodes);		
		ObjectNode *on = _res._objectNodesMap[init_pge->obj_node_number];
		Object *obj = on->objects;
		int _ax = 0;
		while (obj->type != live_pge->obj_type) {
			++_ax;
			++obj;
		}
		assert(_ax < on->num_objects);
		live_pge->first_obj_number = _ax;
		pge_setupDefaultAnim(live_pge);
	}
}

// c
void Game::pge_process(LivePGE *pge) {
	debug(DBG_PGE, "Game::pge_process() pge_num = %d", pge - &_pgeLive[0]);
	_pge_playAnimSound = true;
	_pge_currentPiegeFacingDir = 0;
	if (pge->flags & 1) {
		_pge_currentPiegeFacingDir = 0xFFFF;
	}		
	_pge_currentPiegeRoom = pge->room_location;
	GroupPGE *le = _pge_groupsTable[pge->index];
	if (le) {
		pge_setupNextAnimFrame(pge, le);
	}
	const uint8 *anim_data = _res._ani + READ_LE_UINT16(_res._ani + 2 * pge->obj_type);
	if (READ_LE_UINT16(anim_data) <= pge->anim_seq) {
		InitPGE *init_pge = pge->init_PGE;
		assert(init_pge->obj_node_number < _res._numObjectNodes);
		ObjectNode *on = _res._objectNodesMap[init_pge->obj_node_number];
		Object *obj = &on->objects[pge->first_obj_number];
		while (1) {
			if (obj->type != pge->obj_type) {
				pge_removeFromGroup(pge->index);
				return;
			}
			uint16 _ax = pge_execute(pge, init_pge, obj);
			if (_ax != 0) {
				anim_data = _res._ani + READ_LE_UINT16(_res._ani + 2 * pge->obj_type);
				uint8 snd = anim_data[2];
				if (snd) {
					pge_playAnimSound(pge, snd);
				}
				pge_setupOtherPieges(pge, init_pge);
				break;
			}
			++obj;
		}
	}
	pge_setupAnim(pge);
	++pge->anim_seq;
	pge_removeFromGroup(pge->index);
}

// c
void Game::pge_setupNextAnimFrame(LivePGE *pge, GroupPGE *le) {
	InitPGE *init_pge = pge->init_PGE;
	assert(init_pge->obj_node_number < _res._numObjectNodes);
	ObjectNode *on = _res._objectNodesMap[init_pge->obj_node_number];
	Object *obj = &on->objects[pge->first_obj_number];
	int i = pge->first_obj_number;
	while (pge->obj_type == obj->type && i <= on->last_obj_number) {
		GroupPGE *next_le = le;
		while (next_le) {
			uint16 _ax = next_le->group_id;
			if (obj->opcode2 == 0x6B) {
				if (obj->opcode_arg2 == 0) {
					if (_ax == 1 || _ax == 2) goto set_anim;
				}
				if (obj->opcode_arg2 == 1) {
					if (_ax == 3 || _ax == 4) goto set_anim;
				}
			} else if (_ax == obj->opcode_arg2) {
				if (obj->opcode2 == 0x22 || obj->opcode2 == 0x6F) goto set_anim;
			}
			if (obj->opcode1 == 0x6B) {
				if (obj->opcode_arg1 == 0) {
					if (_ax == 1 || _ax == 2) goto set_anim;
				}
				if (obj->opcode_arg1 == 1) {
					if (_ax == 3 || _ax == 4) goto set_anim;
				}
			} else if (_ax == obj->opcode_arg1) {
				if (obj->opcode1 == 0x22 || obj->opcode1 == 0x6F) goto set_anim;
			}
			next_le = next_le->next_entry;
		}
		++obj;
		++i;
	}	
	return;
	
set_anim:
	const uint8 *anim_data = _res._ani + READ_LE_UINT16(_res._ani + pge->obj_type * 2);
	uint8 _dh = anim_data[0];
	uint8 _dl = pge->anim_seq;
	anim_data += 6 + _dl * 4;	
	while (_dh > _dl) {
		if (READ_LE_UINT16(anim_data) != 0xFFFF) {
			if (_pge_currentPiegeFacingDir) {
				pge->pos_x -= (int8)anim_data[2];
			} else {
				pge->pos_x += (int8)anim_data[2];
			}
			pge->pos_y += (int8)anim_data[3];
		}
		anim_data += 4;
		++_dl;
	}
	pge->anim_seq = _dh;
	_col_currentPiegeGridPosY = (pge->pos_y / 36) & ~1;
	_col_currentPiegeGridPosX = (pge->pos_x + 8) / 16;
}

void Game::pge_playAnimSound(LivePGE *pge, uint16 arg2) {
	if ((pge->flags & 4) && _pge_playAnimSound) {
		if (_game_currentRoom == pge->room_location) {
			snd_playSound((arg2 & 0xFF) - 1);
		} else {
			if (_res._ctData[0x40 + _game_currentRoom] == pge->room_location ||
				_res._ctData[0x00 + _game_currentRoom] == pge->room_location ||
				_res._ctData[0x80 + _game_currentRoom] == pge->room_location ||
				_res._ctData[0xC0 + _game_currentRoom] == pge->room_location) {
				snd_playSound(0x100 | ((arg2 & 0xFF) - 1));
			}
		}
	}
}

// c
void Game::pge_setupAnim(LivePGE *pge) {
	debug(DBG_PGE, "Game::pge_setupAnim() pgeNum = %d", pge - &_pgeLive[0]);
	const uint8 *var4 = _res._ani + READ_LE_UINT16(_res._ani + pge->obj_type * 2);
	if (var4[0] < pge->anim_seq) {
		pge->anim_seq = 0;
	}
	const uint8 *var8 = var4 + 6 + pge->anim_seq * 4;
	if (READ_LE_UINT16(var8) != 0xFFFF) {
		uint16 _bx = READ_LE_UINT16(var8);
		if (pge->flags & 1) {
			_bx ^= 0x8000;
			pge->pos_x -= (int8)var8[2];
		} else {
			pge->pos_x += (int8)var8[2];
		}
		pge->pos_y += (int8)var8[3];
		pge->flags &= 0xFD;
		if (_bx & 0x8000) {
			pge->flags |= 2;
		}
		pge->flags &= 0xF7;
		if (READ_LE_UINT16(var4 + 4) & 0xFF) {
			pge->flags |= 8;
		}
		pge->anim_number = READ_LE_UINT16(var8) & 0x7FFF;
		debug(DBG_PGE, "Game::pge_setupAnim() pgeNum = %d pge->flags = 0x%X pge->anim_number = 0x%X", pge - &_pgeLive[0], pge->flags, pge->anim_number);
	}
}

int Game::pge_execute(LivePGE *live_pge, InitPGE *init_pge, const Object *obj) {	
	debug(DBG_PGE, "Game::pge_execute() pge_num = %d op1=0x%X op2=0x%X op3=0x%X", live_pge - &_pgeLive[0], obj->opcode1, obj->opcode2, obj->opcode3);
	pge_OpcodeProc op;
	ObjectOpcodeArgs args;
	if (obj->opcode1) {
		args.pge = live_pge;
		args.a = obj->opcode_arg1;
		args.b = 0;
		debug(DBG_PGE, "pge_execute op = 0x%X", obj->opcode1);
		op = _pge_opcodeTable[obj->opcode1];
		if (!op) {
			warning("Game::pge_execute() missing call to pge_opcode 0x%X", obj->opcode1);
			return 0;
		}
		if (!((this->*op)(&args) & 0xFF))
			return 0;
	}
	if (obj->opcode2) {
		args.pge = live_pge;
		args.a = obj->opcode_arg2;
		args.b = obj->opcode_arg1;		
		debug(DBG_PGE, "pge_execute op = 0x%X", obj->opcode2);
		op = _pge_opcodeTable[obj->opcode2];
		if (!op) {
			warning("Game::pge_execute() missing call to pge_opcode 0x%X", obj->opcode2);
			return 0;
		}
		if (!((this->*op)(&args) & 0xFF))
			return 0;
	}
	if (obj->opcode3) {
		args.pge = live_pge;
		args.a = obj->opcode_arg3;
		args.b = 0;
		debug(DBG_PGE, "pge_execute op = 0x%X", obj->opcode3);
		op = _pge_opcodeTable[obj->opcode3];
		if (op) {
			(this->*op)(&args);
		} else {
			warning("Game::pge_execute() missing call to pge_opcode 0x%X", obj->opcode3);
		}
	}
	live_pge->obj_type = obj->init_obj_type;
	live_pge->first_obj_number = obj->init_obj_number;
	live_pge->anim_seq = 0;
	if (obj->flags & 0xFF) {
		_game_score += _scoreTable[obj->flags >> 4];
	}
	if (obj->flags & 1) {
		live_pge->flags ^= 1;
	}
	if (obj->flags & 2) {
		--live_pge->life;
		if (init_pge->object_family == 1) {
			_pge_processOBJ = true;
		} else if (init_pge->object_family == 10) {
			_game_score += 100;
		}
	}
	if (obj->flags & 4) {
		++live_pge->life;
	}
	if (obj->flags & 8) {
		live_pge->life = 0xFFFF;
	}

	if (live_pge->flags & 1) {
		live_pge->pos_x -= obj->dx;
	} else {
		live_pge->pos_x += obj->dx;
	}
	live_pge->pos_y += obj->dy;
	
	if (_pge_processOBJ) {
		if (init_pge->object_family == 1) {
			if (pge_processOBJ(live_pge) != 0) {
				_game_blinkingConradCounter = 60;
				_pge_processOBJ = false;
			}
		}
	}
	return 0xFFFF;	
}

// c
void Game::pge_prepare() {
	col_clearState();
	if (!(_game_currentRoom & 0x80)) {
		LivePGE *pge = _pge_liveTable1[_game_currentRoom];
		while (pge) {
			col_preparePiegeState(pge);
			if (!(pge->flags & 4) && (pge->init_PGE->flags & 4)) {
				debug(DBG_PGE, "pge_prepare() stage1 add_pge = %d", pge - &_pgeLive[0]);
				_pge_liveTable2[pge->index] = pge;
				pge->flags |= 4;
			}
			pge = pge->next_PGE_in_room;
		}		
	}
	for (int i = 0; i < _res._pgeNum; ++i) {
		LivePGE *pge = _pge_liveTable2[i];
		if (pge && _game_currentRoom != pge->room_location) {			
			col_preparePiegeState(pge);
		}
	}
}

void Game::pge_setupDefaultAnim(LivePGE *pge) {
	const uint8 *anim_data = _res._ani + READ_LE_UINT16(_res._ani + pge->obj_type * 2);
	if (pge->anim_seq < anim_data[0]) {
		pge->anim_seq = 0;
	}
	const uint8 *anim_frame = anim_data + 6 + pge->anim_seq * 4;
	if (READ_LE_UINT16(anim_frame) != 0xFFFF) {
		uint16 f = READ_LE_UINT16(anim_data);
		if (pge->flags & 1) {
			f ^= 0x8000;
		}
		pge->flags &= 0xFD;
		if (f & 0x8000) {
			pge->flags |= 2;
		}
		pge->flags &= 0xF7;
		if (READ_LE_UINT16(anim_data + 4) & 0xFFFF) {
			pge->flags |= 8;
		}
		pge->anim_number = READ_LE_UINT16(anim_frame) & 0x7FFF;
		debug(DBG_PGE, "Game::pge_setupDefaultAnim() pgeNum = %d pge->flags = 0x%X pge->anim_number = 0x%X", pge - &_pgeLive[0], pge->flags, pge->anim_number);
	}
}

uint16 Game::pge_processOBJ(LivePGE *pge) {
	InitPGE *init_pge = pge->init_PGE;
	assert(init_pge->obj_node_number < _res._numObjectNodes);
	ObjectNode *on = _res._objectNodesMap[init_pge->obj_node_number];
	Object *obj = &on->objects[pge->first_obj_number];
	int i = pge->first_obj_number;
	while (pge->obj_type == obj->type && on->last_obj_number > i) {
		if (obj->opcode2 == 0x6B) return 0xFFFF;
		if (obj->opcode2 == 0x22 && obj->opcode_arg2 <= 4) return 0xFFFF;
		
		if (obj->opcode1 == 0x6B) return 0xFFFF;
		if (obj->opcode1 == 0x22 && obj->opcode_arg1 <= 4) return 0xFFFF;
		
		++obj;
		++i;
	}
	return 0;
}

// c
void Game::pge_setupOtherPieges(LivePGE *pge, InitPGE *init_pge) {
	const int8 *room_ct_data = 0;
	if (pge->pos_x <= -10) {
		pge->pos_x += 256;
		room_ct_data = &_res._ctData[0xC0];
	} else if (pge->pos_x >= 256) {
		pge->pos_x -= 256;
		room_ct_data = &_res._ctData[0x80];
	} else if (pge->pos_y < 0) {
		pge->pos_y += 216;
		room_ct_data = &_res._ctData[0x00];
	} else if (pge->pos_y >= 216) {
		pge->pos_y -= 216;
		room_ct_data = &_res._ctData[0x40];
	}
	if (room_ct_data) {
		int8 room = pge->room_location;
		if (room >= 0) {
			room = room_ct_data[room];
			pge->room_location = room;
		}
		if (init_pge->object_family == 1) {
			_game_currentRoom = room;
			col_prepareRoomState();
			_game_loadMap = true;
			if (!(_game_currentRoom & 0x80) && _game_currentRoom < 0x40) {
				LivePGE *pge_it = _pge_liveTable1[_game_currentRoom];
				while (pge_it) {
					if (pge_it->init_PGE->flags & 4) {
						_pge_liveTable2[pge_it->index] = pge_it;
						pge_it->flags |= 4;
					}
					pge_it = pge_it->next_PGE_in_room;
				}
				room = _res._ctData[0x00 + _game_currentRoom];
				if (room >= 0 && room < 0x40) {
					pge_it = _pge_liveTable1[room];
					while (pge_it) {
						if (pge_it->init_PGE->object_family != 10 && pge_it->pos_y >= 48 && (pge_it->init_PGE->flags & 4)) {
							_pge_liveTable2[pge_it->index] = pge_it;
							pge_it->flags |= 4;
						}
						pge_it = pge_it->next_PGE_in_room;
					}
				}
				room = _res._ctData[0x40 + _game_currentRoom];
				if (room >= 0 && room < 0x40) {
					pge_it = _pge_liveTable1[room];
					while (pge_it) {
						if (pge_it->init_PGE->object_family != 10 && pge_it->pos_y >= 176 && (pge_it->init_PGE->flags & 4)) {
							_pge_liveTable2[pge_it->index] = pge_it;
							pge_it->flags |= 4;
						}
						pge_it = pge_it->next_PGE_in_room;
					}
				}
			}
		}
	}
	pge_addToCurrentRoomList(pge, _pge_currentPiegeRoom);
}

// c
void Game::pge_addToCurrentRoomList(LivePGE *pge, uint8 room) {
	debug(DBG_PGE, "Game::pge_addToCurrentRoomList() pgeNum=%d room=%d", pge - &_pgeLive[0], room);
	if (room != pge->room_location) {
		LivePGE *cur_pge = _pge_liveTable1[room];
		LivePGE *prev_pge = 0;
		while (cur_pge && cur_pge != pge) {
			prev_pge = cur_pge;
			cur_pge = cur_pge->next_PGE_in_room;
		}
		if (cur_pge) {
			if (!prev_pge) {
				_pge_liveTable1[room] = pge->next_PGE_in_room;
			} else {
				prev_pge->next_PGE_in_room = cur_pge->next_PGE_in_room;
			}
			LivePGE *temp = _pge_liveTable1[pge->room_location];
			pge->next_PGE_in_room = temp;
			_pge_liveTable1[pge->room_location] = pge;
		}
	}
}

// c
int Game::pge_op_isInpUp(ObjectOpcodeArgs *args) {
	if (1 == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpBackward(ObjectOpcodeArgs *args) {
	uint8 mask = 8; // right
	if (_pge_currentPiegeFacingDir & 0xFFFF) {
		mask = 4; // left
	}
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpDown(ObjectOpcodeArgs *args) {
	if (2 == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpForward(ObjectOpcodeArgs *args) {
	uint8 mask = 4;
	if (_pge_currentPiegeFacingDir & 0xFFFF) {
		mask = 8;
	}
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpUpMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8 mask = _pge_modKeysTable[args->a] | 1;
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}	
}

int Game::pge_op_isInpBackwardMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8 mask = _pge_modKeysTable[args->a];
	if (_pge_currentPiegeFacingDir & 0xFFFF) {
		mask |= 4;
	} else {
		mask |= 8;
	}
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}	
}

int Game::pge_op_isInpDownMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8 mask = _pge_modKeysTable[args->a] | 2;
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpForwardMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8 mask = _pge_modKeysTable[args->a];
	if (_pge_currentPiegeFacingDir & 0xFFFF) {
		mask |= 8;
	} else {
		mask |= 4;
	}
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpIdle(ObjectOpcodeArgs *args) {
	if (_pge_inpKeysMask == 0) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpNoMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8 mask = _pge_modKeysTable[args->a];
	if (((_pge_inpKeysMask & 0xF) | mask) == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_getCollision0u(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 0, -args->a);
}

int Game::pge_op_getCollision00(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 0, 0);
}

int Game::pge_op_getCollision0d(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 0, args->a);
}

int Game::pge_op_getCollision1u(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 1, -args->a);
}

int Game::pge_op_getCollision10(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 1, 0);
}

int Game::pge_op_getCollision1d(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 1, args->a);
}

int Game::pge_op_getCollision2u(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 2, -args->a);
}

int Game::pge_op_getCollision20(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 2, 0);
}

int Game::pge_op_getCollision2d(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 2, args->a);
}

int Game::pge_op_doesNotCollide0u(ObjectOpcodeArgs *args) {
	int16 r = col_getGridData(args->pge, 0, -args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide00(ObjectOpcodeArgs *args) {
	int16 r = col_getGridData(args->pge, 0, 0);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide0d(ObjectOpcodeArgs *args) {
	int16 r = col_getGridData(args->pge, 0, args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide1u(ObjectOpcodeArgs *args) {
	int16 r = col_getGridData(args->pge, 1, -args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide10(ObjectOpcodeArgs *args) {
	int16 r = col_getGridData(args->pge, 1, 0);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide1d(ObjectOpcodeArgs *args) {
	int16 r = col_getGridData(args->pge, 1, args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide2u(ObjectOpcodeArgs *args) {
	int16 r = col_getGridData(args->pge, 2, -args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide20(ObjectOpcodeArgs *args) {
	int16 r = col_getGridData(args->pge, 2, 0);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide2d(ObjectOpcodeArgs *args) {
	int16 r = col_getGridData(args->pge, 2, args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_collides0o0d(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 0, args->a) & 0xFFFF) {
		if (col_getGridData(args->pge, 0, args->a + 1) == 0) {
			if (col_getGridData(args->pge, -1, args->a) == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_op_collides2o2d(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 2, args->a) & 0xFFFF) {
		if (col_getGridData(args->pge, 2, args->a + 1) == 0) {
			if (col_getGridData(args->pge, 1, args->a) == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_op_collides0o0u(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 0, args->a) & 0xFFFF) {
		if (col_getGridData(args->pge, 0, args->a - 1) == 0) {
			if (col_getGridData(args->pge, -1, args->a) == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_op_collides2o2u(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 2, args->a) & 0xFFFF) {
		if (col_getGridData(args->pge, 2, args->a - 1) == 0) {
			if (col_getGridData(args->pge, 1, args->a) == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_op_collides2u2o(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 2, args->a - 1) & 0xFFFF) {
		if (col_getGridData(args->pge, 2, args->a) == 0) {
			if (col_getGridData(args->pge, 1, args->a - 1) == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_op_isInGroup(ObjectOpcodeArgs *args) {
	GroupPGE *le = _pge_groupsTable[args->pge->index];
	while (le) {
		if (le->group_id == args->a) {
			return 0xFFFF;
		}
		le = le->next_entry;
	}
	return 0;
}

int Game::pge_op_updateGroup0(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge_updateGroup(pge->index, pge->init_PGE->counter_values[0], args->a);
	return 0xFFFF;
}

int Game::pge_op_updateGroup1(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge_updateGroup(pge->index, pge->init_PGE->counter_values[1], args->a);
	return 0xFFFF;
}

int Game::pge_op_updateGroup2(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge_updateGroup(pge->index, pge->init_PGE->counter_values[2], args->a);
	return 0xFFFF;
}

int Game::pge_op_updateGroup3(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge_updateGroup(pge->index, pge->init_PGE->counter_values[3], args->a);
	return 0xFFFF;
}

// c
int Game::pge_op_isPiegeDead(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	if (pge->life <= 0) {
		if (pge->init_PGE->object_family == 10) {
			_game_score += 100; 
		}
		return 1;
	}
	return 0;
}

int Game::pge_op_collides1u2o(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 1, args->a - 1) & 0xFFFF) {
		if (col_getGridData(args->pge, 2, args->a) == 0) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_op_collides1u1o(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 1, args->a - 1) & 0xFFFF) {
		if (col_getGridData(args->pge, 1, args->a) == 0) {
			return 0xFFFF;	
		}
	}
	return 0;
}

int Game::pge_op_collides1o1u(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 1, args->a - 1) == 0) {
		if (col_getGridData(args->pge, 1, args->a) & 0xFFFF) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_o_unk0x2B(ObjectOpcodeArgs *args) {
	return pge_isPiegeCollidable(args->pge, args->a, &Game::pge_compareByFlags2, 0);
}

int Game::pge_o_unk0x2C(ObjectOpcodeArgs *args) {
	return pge_isPiegeCollidable(args->pge, args->a, &Game::pge_compareByFlags1, 0);
}

// c
int Game::pge_o_unk0x2D(ObjectOpcodeArgs *args) {
	return pge_isPiegeCollidable(args->pge, args->a, &Game::pge_compareByObjFlags, 0) ^ 1;
}

// c
int Game::pge_op_pickupObject(ObjectOpcodeArgs *args) {
	LivePGE *pge = col_findPiege(args->pge, 3);
	if (pge) {
		pge_updateGroup(args->pge->index, pge->index, args->a);
		return 0xFFFF;
	}
	return 0;
}

// cc
int Game::pge_o_unk0x30(ObjectOpcodeArgs *args) {
	pge_updateInventory(&_pgeLive[args->a], args->pge);
	args->pge->room_location = 0xFF;
	return 0xFFFF;
}

// c
int Game::pge_op_copyPiege(ObjectOpcodeArgs *args) {
	LivePGE *src = &_pgeLive[args->a];
	LivePGE *dst = args->pge;
	
	dst->pos_x = src->pos_x;
	dst->pos_y = src->pos_y;
	dst->room_location = src->room_location;
	
	dst->flags &= 0xFE;
	if (src->flags & 1) {
		dst->flags |= 1;
	}
	pge_reorderInventory(args->pge);
	return 0xFFFF;
}

// pickupObject related
int Game::pge_o_unk0x33(ObjectOpcodeArgs *args) {
	if (args->pge->current_inventory_PGE != 0xFF) {
		pge_updateGroup(args->pge->index, args->pge->current_inventory_PGE, args->a);
	}
	return 1;
}

int Game::pge_op_canUseCurrentInventoryItem(ObjectOpcodeArgs *args) {
	LivePGE *pge = &_pgeLive[0];
	if (pge->current_inventory_PGE != 0xFF && _res._pgeInit[pge->current_inventory_PGE].state == args->a) {
		return 1;
	}
	return 0;
}

// useObject related
int Game::pge_o_unk0x34(ObjectOpcodeArgs *args) {
	uint8 mask = (_pge_inpKeysMask & 0xF) | _pge_modKeysTable[0];
	if (mask == _pge_inpKeysMask) {
		if (col_getGridData(args->pge, 2, -args->a) == 0) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_op_isInpMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8 mask = _pge_modKeysTable[args->a];
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

// cc
int Game::pge_o_unk0x36(ObjectOpcodeArgs *args) {
	return pge_updateCollisionState(args->pge, args->a, 1);
}

// cc
int Game::pge_o_unk0x37(ObjectOpcodeArgs *args) {
	return pge_updateCollisionState(args->pge, args->a, 0);
}

// c
int Game::pge_o_unk0x3C(ObjectOpcodeArgs *args) {
	return pge_isPiegeCollidable(args->pge, args->a, &Game::pge_compareByAnim2, args->b);
}

// c
int Game::pge_o_unk0x3D(ObjectOpcodeArgs *args) {
	return pge_isPiegeCollidable(args->pge, args->a, &Game::pge_compareByAnim1, 0);
}

// c
int Game::pge_op_setPiegeCounter(ObjectOpcodeArgs *args) {
	args->pge->counter_value = args->a;
	return 1;
}

// c
int Game::pge_op_decPiegeCounter(ObjectOpcodeArgs *args) {
	args->pge->counter_value -= 1;
	if (args->a == args->pge->counter_value) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_o_unk0x40(ObjectOpcodeArgs *args) {
	int8 pge_room = args->pge->room_location;
	if (pge_room < 0 || pge_room >= 0x40) return 0;
	int col_area;
	if (_game_currentRoom == pge_room) {
		col_area = 1;
	} else if (_col_currentLeftRoom == pge_room) {
		col_area = 0;
	} else if (_col_currentRightRoom == pge_room) {
		col_area = 2;
	} else {
		return 0;
	}
	int16 grid_pos_x = (args->pge->pos_x + 8) / 16;
	int16 grid_pos_y = args->pge->pos_y / 72;
	if (grid_pos_y >= 0 && grid_pos_y <= 2) {
		grid_pos_y *= 16;
		int16 _cx = args->a;
		if (_pge_currentPiegeFacingDir) {
			_cx = -_cx;
		}
		int8 _bl;
		if (_cx >= 0) {
			if (_cx > 0x10) {
				_cx = 0x10;
			}
			int8 *var2 = &_res._ctData[0x100] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + grid_pos_x;
			uint8 *var4 = _col_activeCollisionSlots + col_area * 0x30 + grid_pos_y + grid_pos_x;
			int16 var12 = grid_pos_x;
			--_cx;
			do {
				--var12;
				if (var12 < 0) {
					--col_area;
					if (col_area < 0) return 0;
					pge_room = _res._ctData[0xC0 + pge_room];
					if (pge_room < 0) return 0;			
					var12 = 15;
					var2 = &_res._ctData[0x101] + pge_room * 0x70 + grid_pos_y * 2 + 15 + 0x10;
					var4 = var4 - 31;
				}
				--var4;
				_bl = *var4;
				if (_bl >= 0) {
					CollisionSlot *col_slot = _col_slotsTable[_bl];
					do {
						if (args->pge != col_slot->live_pge && (col_slot->live_pge->flags & 4)) {
							if (col_slot->live_pge->init_PGE->object_family == args->b) {
								return 1;
							}
						}
						col_slot = col_slot->prev_slot;
					} while (col_slot);
				}
				--var2;
				if (*var2 != 0) return 0;
				--_cx;
			} while (_cx >= 0);
		} else {
			_cx = -_cx;
			if (_cx > 0x10) {
				_cx = 0x10;
			}
			int8 *var2 = &_res._ctData[0x101] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + grid_pos_x;
			uint8 *var4 = _col_activeCollisionSlots + 1 + col_area * 0x30 + grid_pos_y + grid_pos_x;
			int16 var12 = grid_pos_x;
			--_cx;
			do {
				++var12;
				if (var12 == 0x10) {
					++col_area;
					if (col_area > 2) return 0;
					pge_room = _res._ctData[0x80 + pge_room];
					if (pge_room < 0) return 0;
			
					var12 = 0;
					var2 = &_res._ctData[0x101] + pge_room * 0x70 + grid_pos_y * 2 + 0x10;
					var4 += 32;
				}
				var4++;
				_bl = *var4;
				if (_bl >= 0) {
					CollisionSlot *col_slot = _col_slotsTable[_bl];
					do {
						if (args->pge != col_slot->live_pge && (col_slot->live_pge->flags & 4)) {
							if (col_slot->live_pge->init_PGE->object_family == args->b) {
								return 1;
							}
						}
						col_slot = col_slot->prev_slot;
					} while (col_slot);
				}
				_bl = *var2;
				++var2;
				if (_bl != 0) return 0;
				--_cx;
			} while (_cx >= 0);
		}		
	}
	return 0;
}

// c
int Game::pge_o_unk0x43(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	if (!(pge->init_PGE->flags & 4)) goto kill_pge;
	if (_game_currentRoom & 0x80) goto skip_pge;
	if (pge->room_location & 0x80) goto kill_pge;
	if (pge->room_location > 0x3F) goto kill_pge;
	if (pge->room_location == _game_currentRoom) goto skip_pge;
	if (pge->room_location == _res._ctData[0x00 + _game_currentRoom]) goto skip_pge;
	if (pge->room_location == _res._ctData[0x40 + _game_currentRoom]) goto skip_pge;
	if (pge->room_location == _res._ctData[0x80 + _game_currentRoom]) goto skip_pge;
	if (pge->room_location == _res._ctData[0xC0 + _game_currentRoom]) goto skip_pge;

kill_pge:	
	pge->flags &= 0xFB;
	pge->collision_slot = 0xFF;
	_pge_liveTable2[pge->index] = 0;
			
skip_pge:
	_pge_playAnimSound = false;
	return 1;
}

int Game::pge_op_loadPiegeCounter(ObjectOpcodeArgs *args) {
	args->pge->counter_value = args->pge->init_PGE->counter_values[args->a];
	return 1;
}

int Game::pge_o_unk0x45(ObjectOpcodeArgs *args) {
	return pge_isPiegeCollidable(args->pge, args->a, &Game::pge_compareByPtr, 0);
}

int Game::pge_o_unk0x46(ObjectOpcodeArgs *args) {
	_pge_compareVar1 = 0;
	pge_isPiegeCollidable(args->pge, args->a, &Game::pge_compareByUnk1, 0);
	return _pge_compareVar1;
}

int Game::pge_o_unk0x47(ObjectOpcodeArgs *args) {
	_pge_compareVar2 = 0;
	pge_isPiegeCollidable(args->pge, args->a, &Game::pge_compareByUnk2, 0);
	return _pge_compareVar2;
}

// used with Ian in level2
int Game::pge_o_unk0x48(ObjectOpcodeArgs *args) {
	LivePGE *pge = col_findPiege(&_pgeLive[0], args->pge->init_PGE->counter_values[0]);
	if (pge && pge->life == args->pge->life) {
		pge_updateGroup(args->pge->index, pge->index, args->a);
		return 1;
	}
	return 0;
}

int Game::pge_o_unk0x4A(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge->room_location = 0xFE;
	pge->flags &= 0xFB;
	_pge_liveTable2[pge->index] = 0;
	LivePGE *inv_pge = pge_getInventoryItemBefore(&_pgeLive[args->a], pge); // _bx
	if (inv_pge == &_pgeLive[args->a]) {
		if (pge->index != inv_pge->current_inventory_PGE) {
			return 1;
		}
	} else {
		if (pge->index != inv_pge->next_inventory_PGE) {
			return 1;
		}		
	}
	pge_removeFromInventory(inv_pge, pge, &_pgeLive[args->a]);
	return 1;
}

int Game::pge_op_killPiege(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge->room_location = 0xFE;
	pge->flags &= 0xFB;
	_pge_liveTable2[pge->index] = 0;
	if (pge->init_PGE->object_family == 10) {
		_game_score += 200;
	}
	return 0xFFFF;
}

int Game::pge_op_isInCurrentRoom(ObjectOpcodeArgs *args) {
	return (args->pge->room_location == _game_currentRoom) ? 1 : 0;
}

int Game::pge_op_isNotInCurrentRoom(ObjectOpcodeArgs *args) {
	return (args->pge->room_location == _game_currentRoom) ? 0 : 1;
}

// elevator related
int Game::pge_op_scrollPosY(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	args->pge->pos_y += args->a;
	uint8 pge_num = pge->current_inventory_PGE;
	while (pge_num != 0xFF) {
		pge = &_pgeLive[pge_num];
		pge->pos_y += args->a;
		pge_num = pge->next_inventory_PGE;
	}
	return 1;
}

int Game::pge_op_playDeathCutscene(ObjectOpcodeArgs *args) {
	if (_game_deathCutsceneCounter == 0) {
		_game_deathCutsceneCounter = args->a;
	}
	return 1;
}

int Game::pge_o_unk0x50(ObjectOpcodeArgs *args) {
	return pge_isPiegeCollidable(args->pge, args->a, &Game::pge_compareByObjFlags, 0);
}

int Game::pge_o_unk0x54(ObjectOpcodeArgs *args) {
	if (col_findPiege(&_pgeLive[0], args->a) != 0) {
		return 1;
	}
	return 0;
}

int Game::pge_op_setLife(ObjectOpcodeArgs *args) {
	args->pge->life = args->a;
	return 1;
}

int Game::pge_op_incLife(ObjectOpcodeArgs *args) {
	args->pge->life += args->a;
	return 1;
}

// level2, Ian
int Game::pge_o_unk0x57(ObjectOpcodeArgs *args) {
	assert(args->a >= 0 && args->a < 4);
	int16 r = args->pge->init_PGE->counter_values[args->a];	
	args->pge->room_location = r;
	if (r == 1) {
		warning("setting _game_loadMap to true");
		_game_loadMap = true;
	}
	pge_setupDefaultAnim(args->pge);
	return 1;	
}

int Game::pge_op_setLifeCounter(ObjectOpcodeArgs *args) {
	_pgeLive[args->a].life = args->pge->init_PGE->counter_values[0];
	return 1;
}

int Game::pge_op_decLifeCounter(ObjectOpcodeArgs *args) {
	args->pge->life = _pgeLive[args->a].life - 1;
	return 1;
}

int Game::pge_op_playCutscene(ObjectOpcodeArgs *args) {
	if (_game_deathCutsceneCounter == 0) {
		_cut_id = args->a;
	}
	return 1;	
}

// c
int Game::pge_o_unk0x5F(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;

	int8 pge_room = pge->room_location; // _bl
	if (pge_room < 0 || pge_room >= 0x40) return 0;

	int16 dx;
	int16 _cx = pge->init_PGE->counter_values[0];
	if (_cx <= 0) {
		dx = 1;
		_cx = -_cx;
	} else {
		dx = -1;
	}
	if (_pge_currentPiegeFacingDir) {
		dx = -dx;
	}
	int16 grid_pos_x = (pge->pos_x + 8) / 16;
	int16 grid_pos_y = 0;
	do {
		int16 _ax = col_getGridData(pge, 1, -grid_pos_y);
		if (_ax != 0) {
			if (!(_ax & 2) || args->a != 1) {
				pge->room_location = pge_room;
				pge->pos_x = grid_pos_x * 16;
				return 1;
			}
		}
		if (grid_pos_x < 0) {
			pge_room = _res._ctData[0xC0 + pge_room];
			if (pge_room < 0 || pge_room >= 0x40) return 0;
			grid_pos_x += 16;
		} else if (grid_pos_x > 15) {
			pge_room = _res._ctData[0x80 + pge_room];
			if (pge_room < 0 || pge_room >= 0x40) return 0;
			grid_pos_x -= 16;
		}
		grid_pos_x += dx;
		++grid_pos_y;
	} while (grid_pos_y <= _cx);
	return 0;
}

int Game::pge_o_unk0x60(ObjectOpcodeArgs *args) {
	GroupPGE *le = _pge_groupsTable[args->pge->index];
	while (le) {
		if (le->group_id == args->a) {
			args->a = le->index;
			args->b = 0;
			pge_op_copyPiege(args);
			return 1;
		}
		le = le->next_entry;
	}
	return 0;
}

int Game::pge_op_isInRandomRange(ObjectOpcodeArgs *args) {
	int n = args->a;
	if (n != 0) {
		if (((game_getRandomNumber() / n) & 0xFFFF0000) == 0) {
			return 1;
		}
	}
	return 0;
}

// c
int Game::pge_o_unk0x62(ObjectOpcodeArgs *args) {
	return col_detectHit(args->pge, args->a, args->b, &Game::col_detectHitCallback3, &Game::col_detectHitCallback1, 0, -1);
}

// c
int Game::pge_o_unk0x63(ObjectOpcodeArgs *args) {
	return col_detectHit(args->pge, args->a, args->b, &Game::col_detectHitCallback2, &Game::col_detectHitCallback1, 0, -1);
}

int Game::pge_o_unk0x64(ObjectOpcodeArgs *args) {
	return col_detectGunHit(args->pge, args->a, args->b, &Game::col_detectGunHitCallback3, &Game::col_detectGunHitCallback1, 1, -1);
}

int Game::pge_op_addToCredits(ObjectOpcodeArgs *args) {
	assert(args->a >= 0 && args->a < 3);
	uint8 pge = args->pge->init_PGE->counter_values[args->a];
	int16 val = args->pge->init_PGE->counter_values[args->a + 1];
	_pgeLive[pge].life += val;
	return 1;
}

int Game::pge_op_subFromCredits(ObjectOpcodeArgs *args) {
	assert(args->a >= 0 && args->a < 3);
	uint8 pge = args->pge->init_PGE->counter_values[args->a];
	int16 val = args->pge->init_PGE->counter_values[args->a + 1];
	_pgeLive[pge].life -= val;
	return 1;
}

// c
int Game::pge_o_unk0x67(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 1, -args->a) & 2) {
		return 0xFFFF;
	}
	return 0;
}

// c
int Game::pge_o_unk0x68(ObjectOpcodeArgs *args) {
	return pge_updateCollisionState(args->pge, args->a, 2);
}

int Game::pge_op_saveState(ObjectOpcodeArgs *args) {
	warning("saving state not yet implemented");
	return 0xFFFF;
}

// useGun related
int Game::pge_o_unk0x6A(ObjectOpcodeArgs *args) {
	LivePGE *_si = args->pge;
	int8 pge_room = _si->room_location;
	if (pge_room < 0 || pge_room >= 0x40) return 0;
	int8 _bl;
	int col_area = 0;
	int8 *ct_data;
	if (_game_currentRoom == pge_room) {
		col_area = 1;
	} else if (_col_currentLeftRoom == pge_room) {
		col_area = 0;
	} else if (_col_currentRightRoom == pge_room) {
		col_area = 2;
	} else {
		return 0;
	}
	int16 grid_pos_x = (_si->pos_x + 8) / 16;
	int16 grid_pos_y = (_si->pos_y / 72);
	if (grid_pos_y >= 0 && grid_pos_y <= 2) {
		grid_pos_y *= 16;
		int16 _cx = args->a;
		if (_pge_currentPiegeFacingDir) {
			_cx = -_cx;
		}
		if (_cx >= 0) {
			if (_cx > 0x10) {
				_cx = 0x10;
			}
			ct_data = &_res._ctData[0x100] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + grid_pos_x;
			uint8 *var4 = _col_activeCollisionSlots + col_area * 0x30 + grid_pos_y + grid_pos_x;
			++var4;
			++ct_data;
			int16 varA = grid_pos_x;
			do {
				--varA;
				if (varA < 0) {
					--col_area;
					if (col_area < 0) return 0;
					pge_room = _res._ctData[0xC0 + pge_room];
					if (pge_room < 0) return 0;			
					varA = 0xF;
					ct_data = &_res._ctData[0x101] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + varA;
					var4 -= 0x1F;
				}
				--var4;
				_bl = *var4;
				if (_bl >= 0) {
					CollisionSlot *collision_slot = _col_slotsTable[_bl];
					do {
						_si = collision_slot->live_pge;
						if (args->pge != _si && (_si->flags & 4) && _si->life >= 0) {
							if (_si->init_PGE->object_family == 1 || _si->init_PGE->object_family == 10) {
								return 1;
							}
						}
						collision_slot = collision_slot->prev_slot;
					} while (collision_slot);
				}
				--ct_data;
				if (*ct_data != 0) return 0;
				--_cx;
			} while (_cx >= 0);
					
		} else {
			_cx = -_cx;
			if (_cx > 0x10) {
				_cx = 0x10;
			}		
			ct_data = &_res._ctData[0x101] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + grid_pos_x;
			uint8 *var4 = _col_activeCollisionSlots + 1 + col_area * 0x30 + grid_pos_y + grid_pos_x;
			int16 varA = grid_pos_x;
			goto loc_0_15446;
			do {
				++varA;
				if (varA == 0x10) {
					++col_area;
					if (col_area > 2) return 0;
					pge_room = _res._ctData[0x80 + pge_room];
					if (pge_room < 0) return 0;
					varA = 0;
					ct_data = &_res._ctData[0x100] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + varA;
					var4 += 0x20;
				}
loc_0_15446:							
				_bl = *var4;
				++var4;
				if (_bl >= 0) {
					CollisionSlot *collision_slot = _col_slotsTable[_bl];
					do {
						_si = collision_slot->live_pge;
						if (args->pge != _si && (_si->flags & 4) && _si->life >= 0) {
							if (_si->init_PGE->object_family == 1 || _si->init_PGE->object_family == 10) {
								return 1;
							}
						}
						collision_slot = collision_slot->prev_slot;
					} while (collision_slot);
				}						
				_bl = *ct_data;
				++ct_data;
				if (_bl != 0) return 0;
				--_cx;
			} while (_cx >= 0);
		}
	}
	return 0;
}

int Game::pge_op_isInGroupSlice(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	GroupPGE *le = _pge_groupsTable[pge->index];
	if (le != 0) {
		if (args->a == 0) {
			do {
				if (le->group_id == 1 || le->group_id == 2) {
					return 1;
				}
				le = le->next_entry;
			} while (le != 0);
		} else {
			do {
				if (le->group_id == 3 || le->group_id == 4) {
					return 1;
				}
				le = le->next_entry;
			} while (le != 0);		
		}
	}
	return 0;
}

int Game::pge_o_unk0x6C(ObjectOpcodeArgs *args) {
	LivePGE *pge = col_findPiege(&_pgeLive[0], args->pge->init_PGE->counter_values[0]);
	if (pge) {
		if (pge->life <= args->pge->life) {
			pge_updateGroup(args->pge->index, pge->index, args->a);
			return 1;
		}
	}
	return 0;
}

int Game::pge_op_isCollidingObject(ObjectOpcodeArgs *args) {
	uint8 r = col_findCurrentCollidingObject(args->pge, 3, 0xFF, 0xFF, 0);
	if (r == args->a) {
		return 1;
	} else {
		return 0;
	}
}

// elevator
int Game::pge_o_unk0x6E(ObjectOpcodeArgs *args) {
	GroupPGE *le = _pge_groupsTable[args->pge->index];
	while (le) {
		if (args->a == le->group_id) {
			pge_updateInventory(&_pgeLive[le->index], args->pge);
			return 0xFFFF;
		}
		le = le->next_entry;
	}
	return 0;
}

// c
int Game::pge_o_unk0x6F(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	GroupPGE *le = _pge_groupsTable[pge->index];
	while (le) {
		if (args->a == le->group_id) {
			pge_updateGroup(pge->index, le->index, 0xC);
			return 1;
		}
		le = le->next_entry;
	}
	return 0;
}

// c elevator
int Game::pge_o_unk0x71(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	GroupPGE *le = _pge_groupsTable[pge->index];
	while (le) {
		if (le->group_id == args->a) {
			pge_reorderInventory(args->pge);
			return 1;
		}
		le = le->next_entry;
	}
	return 0;
}

// c
int Game::pge_o_unk0x72(ObjectOpcodeArgs *args) {
	int8 *var4 = &_res._ctData[0x100] + args->pge->room_location * 0x70;
	var4 += (((args->pge->pos_y / 36) & ~1) + args->a) * 16 + (args->pge->pos_x + 8) / 16;

	CollisionSlot2 *_di = _col_slots2Next;
	int _cx = 0x100;
	while (_di && _cx != 0) {
		if (_di->unk2 != var4) {
			_di = _di->next_slot;
			--_cx;
		} else {
			memcpy(_di->unk2, _di->data_buf, _di->data_size + 1);
			break;
		}
	}
	return 0xFFFF; // XXX var4;
}

// c
int Game::pge_o_unk0x73(ObjectOpcodeArgs *args) {
	LivePGE *pge = col_findPiege(args->pge, args->a);
	if (pge != 0) {
		pge_updateInventory(pge, args->pge);
		return 0xFFFF;
	}
	return 0;
}

int Game::pge_o_unk0x74(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 4, -args->a) != 0) {
		return 0xFFFF;		
	}
	return 0;
}

int Game::pge_o_unk0x75(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 4, -args->a) == 0) {
		return 0xFFFF;		
	}
	return 0;
}

// level == 2,3
int Game::pge_o_unk0x76(ObjectOpcodeArgs *args) {
	LivePGE *_si = args->pge;
	LivePGE *_di = &_pgeLive[0];
	if (_di->room_location == _si->room_location) {
		if ((_di->pos_y - 8) / 72 < _si->pos_y / 72) {
			return 0xFFFF;
		}
	} else if (!(_si->room_location & 0x80) && _si->room_location < 0x40) {
		if (_di->room_location == _res._ctData[0x00 + _si->room_location]) {
			return 0xFFFF;
		}
	}
	return 0;	
}

// level == 2,3
int Game::pge_o_unk0x77(ObjectOpcodeArgs *args) {
	LivePGE *_si = args->pge;
	LivePGE *_di = &_pgeLive[0];
	if (_di->room_location == _si->room_location) {
		if ((_di->pos_y - 8) / 72 > _si->pos_y / 72) {
			return 0xFFFF;
		}
	} else if (!(_si->room_location & 0x80) && _si->room_location < 0x40) {
		if (_di->room_location == _res._ctData[0x40 + _si->room_location]) {
			return 0xFFFF;
		}
	}
	return 0;	
}

int Game::pge_op_isLeftFacingConrad(ObjectOpcodeArgs *args) {
	LivePGE *_si = args->pge;
	LivePGE *_di = &_pgeLive[0];
	
	int16 _cx = _si->pos_y / 72;
	int16 _ax = (_di->pos_y - 8) / 72;
	
	if (_cx == _ax) { // same grid cell
		if (_si->room_location == _di->room_location) {
			if (args->a == 0) {
				if (_pge_currentPiegeFacingDir) {
					if (_si->pos_x < _di->pos_x) {
						return 0xFFFF;
					}
				} else {
					if (_si->pos_x > _di->pos_x) {
						return 0xFFFF;
					}
				}				
			} else {
				int16 dx;
				if (_pge_currentPiegeFacingDir) {
					dx = _di->pos_x - _si->pos_x;
				} else {
					dx = _si->pos_x - _di->pos_x;
				}
				if (dx > 0 && dx < args->a * 16) {
					return 0xFFFF;
				}
			}
		} else if (args->a == 0) {
			if (!(_si->room_location & 0x80) && _si->room_location < 0x40) {
				if (_pge_currentPiegeFacingDir) {
					if (_di->room_location == _res._ctData[0x80 + _si->room_location])
						return 0xFFFF;
				} else {
					if (_di->room_location == _res._ctData[0xC0 + _si->room_location])
						return 0xFFFF;
				}
			}
		}
	}
	return 0;
}

int Game::pge_op_isRightFacingConrad(ObjectOpcodeArgs *args) {
	LivePGE *_si = args->pge;
	LivePGE *_di = &_pgeLive[0];
	if (_si->pos_y / 72 == (_di->pos_y - 8) / 72) {
		if (_si->room_location == _di->room_location) {
			if (args->a == 0) {
				if (_pge_currentPiegeFacingDir) {
					if (_si->pos_x > _di->pos_x) {
						return 0xFFFF;
					}
				} else {
					if (_si->pos_x <= _di->pos_x) {
						return 0xFFFF;
					}
				}
			} else {
				int16 dx;
				if (_pge_currentPiegeFacingDir) {
					dx = _si->pos_x - _di->pos_x;
				} else {
					dx = _di->pos_x - _si->pos_x;
				}
				if (dx > 0 && dx < args->a * 16) {
					return 0xFFFF;
				}
			}
		} else if (args->a == 0) {
			if (!(_si->room_location & 0x80) && _si->room_location < 0x40) {
				if (_pge_currentPiegeFacingDir) {
					if (_di->room_location == _res._ctData[0xC0 + _si->room_location])
						return 0xFFFF;
				} else {
					if (_di->room_location == _res._ctData[0x80 + _si->room_location])
						return 0xFFFF;
				}
			}
				
		}
	}
	return 0;	
}

int Game::pge_o_unk0x7A(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 1, -args->a) == 0) {
		if (col_getGridData(args->pge, 2, -(args->a + 1)) & 0xFFFF) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_op_displayText(ObjectOpcodeArgs *args) {
	_game_textToDisplay = args->a;
	return 0xFFFF;
}

int Game::pge_o_unk0x7C(ObjectOpcodeArgs *args) {
	LivePGE *_ax = col_findPiege(args->pge, 3);
	if (_ax == 0) {
		_ax = col_findPiege(args->pge, 5);
		if (_ax == 0) {
			_ax = col_findPiege(args->pge, 9);
			if (_ax == 0) {
				_ax = col_findPiege(args->pge, 0xFFFF);				
			}			
		}
	}
	if (_ax != 0) {
		pge_updateGroup(args->pge->index, _ax->index, args->a);
	}
	return 0;
}

int Game::pge_op_playSound(ObjectOpcodeArgs *args) {
	snd_playSound(args->a);
	return 0xFFFF;
}

int Game::pge_op_unk0x7E(ObjectOpcodeArgs *args) {
	_pge_compareVar1 = 0;
	pge_isPiegeCollidable(args->pge, args->a, &Game::pge_compareByIndex2, 0);
	return _pge_compareVar1;
}

int Game::pge_op_unk0x7F(ObjectOpcodeArgs *args) {
	LivePGE *_si = args->pge;
	uint8 var4 = _si->collision_slot;
	uint8 var2 = _si->index;
	while (var4 != 0xFF) {
		CollisionSlot *slot = _col_slotsTable[var4];
		while (slot) {
			if (slot->live_pge != args->pge) {
				if (slot->live_pge->init_PGE->object_family == 3 && var2 != slot->live_pge->unkF) {
					return 0;
				}
			}
			if (slot->live_pge == args->pge) {
				var4 = slot->index;
			}
			slot = slot->prev_slot;
		}
	}
	return 0xFFFF;
}

// cc, called for example before using gun, to check its presence
int Game::pge_op_hasInventoryItem(ObjectOpcodeArgs *args) {
	LivePGE *pge = &_pgeLive[0];
	uint8 _dl = pge->current_inventory_PGE;
	while (_dl != 0xFF) {
		pge = &_pgeLive[_dl];
		if (pge->init_PGE->state == args->a) {
			return 0xFFFF;
		}
		_dl = pge->next_inventory_PGE;
	}
	return 0;
}

int Game::pge_op_changeLevel(ObjectOpcodeArgs *args) {
	_game_currentLevel = args->a - 1;
	return _game_currentLevel;
}

int Game::pge_o_unk0x86(ObjectOpcodeArgs *args) {
	return col_detectGunHit(args->pge, args->a, args->b, &Game::col_detectGunHitCallback2, &Game::col_detectGunHitCallback1, 1, 0);
}

int Game::pge_op_playSoundGroup(ObjectOpcodeArgs *args) {
	assert(args->a < 4);
	snd_playSound(args->pge->init_PGE->counter_values[args->a]);
	return 0xFFFF;
}

int Game::pge_op_adjustPos(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge->pos_x &= 0xFFF0;
	if (pge->pos_y != 70 && pge->pos_y != 142 && pge->pos_y != 214) {
		pge->pos_y = ((pge->pos_y / 72) + 1) * 72 - 2;
	}
	return 0xFFFF;
}

int Game::pge_op_setTempVar(ObjectOpcodeArgs *args) {
	_pge_opTempVar = args->a;
	return 0xFFFF;
}

int Game::pge_op_isTempVarSet(ObjectOpcodeArgs *args) {
	if (_pge_opTempVar != args->a) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_setCurrentInventoryObject(LivePGE *pge) {
	LivePGE *_bx = pge_getInventoryItemBefore(&_pgeLive[0], pge);
	if (_bx == &_pgeLive[0]) {
		if (_bx->current_inventory_PGE != pge->index) {
			return 0;
		}
	} else {
		if (_bx->next_inventory_PGE != pge->index) {
			return 0;
		}
	}
	pge_removeFromInventory(_bx, pge, &_pgeLive[0]);
	pge_addToInventory(&_pgeLive[0], pge, &_pgeLive[0]);
	return 0xFFFF;
}

void Game::pge_updateInventory(LivePGE *pge1, LivePGE *pge2) {
	if (pge2->unkF != 0xFF) {
		pge_reorderInventory(pge2);
	}
	LivePGE *_ax = pge_getInventoryItemBefore(pge1, 0);
	pge_addToInventory(_ax, pge2, pge1);
}

// c
void Game::pge_reorderInventory(LivePGE *pge) {
	if (pge->unkF != 0xFF) {
		LivePGE *_bx = &_pgeLive[pge->unkF];
		LivePGE *_di = pge_getInventoryItemBefore(_bx, pge);
		if (_di == _bx) {
			if (_di->current_inventory_PGE == pge->index) {
				pge_removeFromInventory(_di, pge, _bx);
			}
		} else {
			if (_di->next_inventory_PGE == pge->index) {
				pge_removeFromInventory(_di, pge, _bx);
			}			
		}
	}
}

LivePGE *Game::pge_getInventoryItemBefore(LivePGE *pge, LivePGE *last_pge) {
	LivePGE *_di = pge;
	uint8 n = _di->current_inventory_PGE;
	while (n != 0xFF) {
		LivePGE *_si = &_pgeLive[n];
		if (_si == last_pge) {
			break;
		} else {
			_di = _si;
			n = _di->next_inventory_PGE;
		}
	}
	return _di;
}

void Game::pge_addToInventory(LivePGE *pge1, LivePGE *pge2, LivePGE *pge3) {
	pge2->unkF = pge3->index;
	if (pge1 == pge3) {
		pge2->next_inventory_PGE = pge1->current_inventory_PGE;
		pge1->current_inventory_PGE = pge2->index;
	} else {
		pge2->next_inventory_PGE = pge1->next_inventory_PGE;
		pge1->next_inventory_PGE = pge2->index;		
	}
}

// c
int Game::pge_updateCollisionState(LivePGE *pge, int16 pge_dy, uint8 var8) {
	uint8 pge_unk1C = pge->init_PGE->unk1C;
	if (!(pge->room_location & 0x80) && pge->room_location < 0x40) {
		int8 *grid_data = &_res._ctData[0x100] + 0x70 * pge->room_location;
		int16 pge_pos_y = ((pge->pos_y / 36) & ~1) + pge_dy;
		int16 pge_pos_x = (pge->pos_x + 8) / 16;
		
		grid_data += pge_pos_x + pge_pos_y * 16;
		
		CollisionSlot2 *slot1 = _col_slots2Next;
		CollisionSlot2 *slot2 = 0;
		int16 i = 255;
		pge_pos_x = i;
		if (_pge_currentPiegeFacingDir) {
			i = pge_unk1C - 1;
			grid_data -= i;
		}
		while (slot1) {
			if (slot1->unk2 == grid_data) {
				slot1->data_size = pge_unk1C - 1;
				memset(grid_data, var8, pge_unk1C);
				grid_data += pge_unk1C;
				return 1;
			} else {
				++i;
				slot2 = slot1;
				slot1 = slot1->next_slot;
				if (--i == 0) {
					break;
				}
			}	
		}
		if (_col_slots2Cur < &_col_slots2[255]) {
			slot1 = _col_slots2Cur;
			slot1->unk2 = grid_data;
			slot1->data_size = pge_unk1C - 1;
			uint8 *dst = &slot1->data_buf[0];
			int8 *src = grid_data;
			int n = pge_unk1C;
			assert(n < 0x10);
			while (n--) {
				*dst++ = *src;
				*src++ = var8;
			}
			++_col_slots2Cur;
			slot1->next_slot = _col_slots2Next;
			_col_slots2Next = slot1;
		}
	}
	return 1;
}

// c
int Game::pge_isPiegeCollidable(LivePGE *pge, int16 num, pge_CompareProc compare, uint16 unk) {
	uint8 slot = pge->collision_slot;
	while (slot != 0xFF) {
		CollisionSlot *cs = _col_slotsTable[slot];
		if (cs == 0) {
			return 0;
		}
		uint8 slot_bak = slot;
		slot = 0xFF;
		while (cs != 0) {
			if ((this->*compare)(cs->live_pge, pge, num, unk) != 0) {
				return 1;
			}
			if (pge == cs->live_pge) {
				slot = cs->index;
			}
			cs = cs->prev_slot;
			if (slot == slot_bak) {
				return 0;
			}
		}
	}
	return 0;
}

// c
void Game::pge_updateGroup(uint8 index, uint8 unk1, int16 unk2) {
	debug(DBG_GAME, "Game::pge_updateGroup() index=0x%X unk1=0x%X unk2=0x%X", index, unk1, unk2);
	LivePGE *pge = &_pgeLive[unk1];
	if (!(pge->flags & 4)) {
		if (!(pge->init_PGE->flags & 1)) {
			return;
		}
		pge->flags |= 4;
		_pge_liveTable2[unk1] = pge;
	}
	if (unk2 <= 4) {
		uint8 pge_room = pge->room_location;
		pge = &_pgeLive[index];
		if (pge_room != pge->room_location) {
			return;
		}
		if (unk1 == 0 && _game_blinkingConradCounter != 0) {
			return;
		}
		// XXX
	}
	GroupPGE *le = _pge_nextFreeGroup;
	if (le) {
		// append to the list
		_pge_nextFreeGroup = le->next_entry;
		GroupPGE *_ax = _pge_groupsTable[unk1];
		_pge_groupsTable[unk1] = le;
		le->next_entry = _ax;
		le->index = index;
		le->group_id = unk2;
	}
}

// c
void Game::pge_removeFromInventory(LivePGE *pge1, LivePGE *pge2, LivePGE *pge3) {
	pge2->unkF = 0xFF;
	if (pge3 == pge1) {
		pge3->current_inventory_PGE = pge2->next_inventory_PGE;
		pge2->next_inventory_PGE = 0xFF;
	} else {
		pge1->next_inventory_PGE = pge2->next_inventory_PGE;
		pge2->next_inventory_PGE = 0xFF;
	}
}

// c
int Game::pge_compareByAnim1(LivePGE *pge1, LivePGE *pge2, uint8 comp, uint8 comp2) {
	if (pge1 != pge2) {
		uint16 off = READ_LE_UINT16(_res._ani + pge1->obj_type * 2);
		if (_res._ani[off + 3] == comp) {
			return 1;
		}
	}
	return 0;
}

// c
int Game::pge_compareByAnim2(LivePGE *pge1, LivePGE *pge2, uint8 comp, uint8 comp2) {
	if (pge1->init_PGE->object_family == comp2) {
		uint16 off = READ_LE_UINT16(_res._ani + pge1->obj_type * 2);
		if (_res._ani[off + 3] == comp) {
			return 1;
		}		
	}
	return 0;
}

// c
int Game::pge_compareByIndex2(LivePGE *pge1, LivePGE *pge2, uint8 comp, uint8 comp2) {
	if (pge1 != pge2) {
		pge_updateGroup(pge2->index, pge1->index, comp);
		_pge_compareVar1 = 0xFFFF;
	}
	return 0;
}

// c
int Game::pge_compareByObjFlags(LivePGE *pge1, LivePGE *pge2, uint8 comp, uint8 comp2) {
	if (comp == 10) {
		if (pge1->init_PGE->object_family == comp && pge1->life >= 0) {
			return 1;
		}
	} else {
		if (pge1->init_PGE->object_family == comp) {
			return 1;
		}
	}
	return 0;
}

// c
int Game::pge_compareByUnk1(LivePGE *pge1, LivePGE *pge2, uint8 comp, uint8 comp2) {
	if (pge1 != pge2) {
		if ((pge1->flags & 1) != (pge2->flags & 1)) {
			_pge_compareVar1 = 1;
			pge_updateGroup(pge2->index, pge1->index, comp);
			if (pge2->index == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

// c
int Game::pge_compareByUnk2(LivePGE *pge1, LivePGE *pge2, uint8 comp, uint8 comp2) {
	if (pge1 != pge2) {
		if ((pge1->flags & 1) == (pge2->flags & 1)) {
			_pge_compareVar2 = 1;
			pge_updateGroup(pge2->index, pge1->index, comp);
			if (pge2->index == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

// c
int Game::pge_compareByFlags1(LivePGE *pge1, LivePGE *pge2, uint8 comp, uint8 comp2) {
	if (pge1->init_PGE->object_family == comp) {
		if ((pge1->flags & 1) == (pge2->flags & 1)) {
			return 1;
		}
	}
	return 0;
}

// c
int Game::pge_compareByFlags2(LivePGE *pge1, LivePGE *pge2, uint8 comp, uint8 comp2) {
	if (pge1->init_PGE->object_family == comp) {
		if ((pge1->flags & 1) != (pge2->flags & 1)) {
			return 1;
		}
	}
	return 0;
}

int Game::pge_compareByPtr(LivePGE *pge1, LivePGE *pge2, uint8 comp, uint8 comp2) {
	return pge1 - pge2;
}