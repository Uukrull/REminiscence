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
#include "mod_player.h"
#include "resource.h"
#include "systemstub.h"
#include "video.h"
#include "menu.h"


Menu::Menu(Player *ply, Resource *res, SystemStub *stub, Video *vid, Version ver)
	: _ply(ply), _res(res), _stub(stub), _vid(vid), _ver(ver) {
	switch (_ver) {
	case VER_FR:
		_textOptions = _textOptionsFR;
		break;
	case VER_US:
		_textOptions = _textOptionsEN;
		break;
	case VER_DE:
		_textOptions = _textOptionsDE;
		break;
	case VER_SP:
		_textOptions = _textOptionsSP;
		break;
	}
}
	
void Menu::loadPicture(const char *prefix) {
	debug(DBG_MENU, "Menu::loadPicture('%s')", prefix);
	_res->load_MAP_menu(prefix, _res->_memBuf);
	for (int i = 0; i < 4; ++i) {
		for (int y = 0; y < 224; ++y) {
			for (int x = 0; x < 64; ++x) {
				_vid->_frontLayer[i + x * 4 + 256 * y] = _res->_memBuf[0x3800 * i + x + 64 * y];
			}
		}
	}
	_res->load_PAL_menu(prefix, _res->_memBuf);
	_stub->setPalette(_res->_memBuf, 256);
}

void Menu::drawString(const char *str, int16 y, int16 x, uint8 color) {
	debug(DBG_MENU, "Menu::drawString()");
	uint8 v1b = _vid->_drawCharColor1;
	uint8 v2b = _vid->_drawCharColor2;
	uint8 v3b = _vid->_drawCharColor3;
	switch (color) {
	case 0:
		_vid->_drawCharColor1 = _charVar1;
		_vid->_drawCharColor2 = _charVar2;
		_vid->_drawCharColor3 = _charVar2;
		break;
	case 1:
		_vid->_drawCharColor1 = _charVar2;
		_vid->_drawCharColor2 = _charVar1;
		_vid->_drawCharColor3 = _charVar1;
		break;
	case 2:
		_vid->_drawCharColor1 = _charVar3;
		_vid->_drawCharColor2 = 0xFF;
		_vid->_drawCharColor3 = _charVar1;
		break;
	case 3:
		_vid->_drawCharColor1 = _charVar4;
		_vid->_drawCharColor2 = 0xFF;
		_vid->_drawCharColor3 = _charVar1;
		break;
	case 4:
		_vid->_drawCharColor1 = _charVar2;
		_vid->_drawCharColor2 = 0xFF;
		_vid->_drawCharColor3 = _charVar1;
		break;
	case 5:
		_vid->_drawCharColor1 = _charVar2;
		_vid->_drawCharColor2 = 0xFF;
		_vid->_drawCharColor3 = _charVar5;
		break;
	}
	
	drawString2(str, y, x);
	
	_vid->_drawCharColor1 = v1b;
	_vid->_drawCharColor2 = v2b;
	_vid->_drawCharColor3 = v3b;
}

void Menu::drawString2(const char *str, int16 y, int16 x) {
	debug(DBG_MENU, "Menu::drawString2()");
	int len = 0;
	while (*str) {
		_vid->drawChar(*str, y, x + len);
		++str;
		++len;
	}	
	_vid->markBlockAsDirty(x * 8, y * 8, len * 8, 8);
}

void Menu::handleInfoScreen() {
	debug(DBG_MENU, "Menu::handleInfoScreen()");
	_vid->fadeOut();
	switch (_ver) {
	case VER_FR:
		loadPicture("instru_f");
		break;
	case VER_US:
	case VER_DE:
	case VER_SP:
		loadPicture("instru_e");
		break;
	}
	_vid->fullRefresh();
	_vid->updateScreen();
	do {
		_stub->sleep(EVENTS_DELAY);
		_stub->processEvents();
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			break;
		}
	} while (!_stub->_pi.quit);
}

void Menu::handleSkillScreen(uint8 &new_skill) {
	debug(DBG_MENU, "Menu::handleSkillScreen()");
	static const uint8 option_colors[3][3] = { { 2, 3, 3 }, { 3, 2, 3}, { 3, 3, 2 } };	
	_vid->fadeOut();
	loadPicture("menu3");
	_vid->fullRefresh();
	drawString(_textOptions[6], 12, 4, 3);
	int skill_level = new_skill;
	do {
		drawString(_textOptions[7], 15, 14, option_colors[skill_level][0]);
		drawString(_textOptions[8], 17, 14, option_colors[skill_level][1]);
		drawString(_textOptions[9], 19, 14, option_colors[skill_level][2]);

		_vid->updateScreen();
		_stub->sleep(EVENTS_DELAY);
		_stub->processEvents();

		if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
			if (skill_level != 0) {
				--skill_level;
			} else {
				skill_level = 2;
			}
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
			if (skill_level != 2) {
				++skill_level;
			} else {
				skill_level = 0;
			}
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			new_skill = skill_level;
			return;
		}
	} while (!_stub->_pi.quit);
	new_skill = 1;
}

bool Menu::handlePasswordScreen(uint8 &new_skill, uint8 &new_level) {
	debug(DBG_MENU, "Menu::handlePasswordScreen()");
	_vid->fadeOut();
	_vid->_drawCharColor3 = _charVar1;
	_vid->_drawCharColor2 = 0xFF;
	_vid->_drawCharColor1 = _charVar4;
	_vid->fullRefresh();
	char password[7];
	int len = 0;
	do {
		loadPicture("menu2");
		drawString2(_textOptions[10], 15, 3);
		drawString2(_textOptions[11], 17, 3);

		for (int i = 0; i < len; ++i) {
			_vid->drawChar(password[i], 21, i + 15);
		}
		_vid->drawChar('_', 21, len + 15);

		_vid->markBlockAsDirty(15 * 8, 21 * 8, (len + 1) * 8, 8);
		_vid->updateScreen();
		_stub->sleep(EVENTS_DELAY);
		_stub->processEvents();
		
		char c = _stub->_pi.lastChar;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
			if (len < 6) {
				password[len] = c & ~0x20;
				++len;
			}
			_stub->_pi.lastChar = 0;
		}
		if (_stub->_pi.backspace) {
			_stub->_pi.backspace = false;
			if (len > 0) {
				--len;
			}
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			password[len] = '\0';
			for (int level = 0; level < 8; ++level) {
				for (int skill = 0; skill < 3; ++skill) {
					if (strcmp(_passwords[level][skill], password) == 0) {
						new_level = level;
						new_skill = skill;
						return true;
					}
				}
			}
			return false;
		}		
	} while (!_stub->_pi.quit);
	return false;
}

bool Menu::handleTitleScreen(uint8 &new_skill, uint8 &new_level) {
	debug(DBG_MENU, "Menu::handleTitleScreen()");
	bool quit_loop = false;
	int menu_entry = 0;
	bool reinit_screen = true;
	_ply->startSong(1);
	while (!quit_loop) {
		if (reinit_screen) {
			_vid->fadeOut();
			loadPicture("menu1");
			_vid->fullRefresh();
			_charVar3 = 1;
			_charVar4 = 2;
			menu_entry = 0;
			reinit_screen = false;
		}
		int selected_menu_entry = -1;
		for (int i = 0; i < 6; ++i) {
			int color = (i == menu_entry) ? 2 : 3;
			drawString(_textOptions[i], 14 + i * 2, 20, color);
		}

		_vid->updateScreen();
		_stub->sleep(EVENTS_DELAY);
		_stub->processEvents();
		
		if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
			if (menu_entry != 0) {
				--menu_entry;
			} else {
				menu_entry = 5;
			}
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
			if (menu_entry != 5) {
				++menu_entry;
			} else {
				menu_entry = 0;
			}
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			selected_menu_entry = menu_entry;
		}
		
		if (selected_menu_entry != -1) {
			switch (selected_menu_entry) {
			case MENU_OPTION_ITEM_START:
				new_level = 0;
				quit_loop = true;
				break;
			case MENU_OPTION_ITEM_SKILL:
				handleSkillScreen(new_skill);
				reinit_screen = true;
				break;
			case MENU_OPTION_ITEM_PASSWORD:
				if (handlePasswordScreen(new_skill, new_level)) {
					quit_loop = true;
				} else {
					reinit_screen = true;
				}
				break;
			case MENU_OPTION_ITEM_INFO:
				handleInfoScreen();
				reinit_screen = true;
				break;
			case MENU_OPTION_ITEM_DEMO:
				warning("demo mode not implemented");
				break;
			case MENU_OPTION_ITEM_QUIT:
				return false;
				break;
			}
		}
		if (_stub->_pi.quit) {
			return false;
		}
	}
	_ply->stopSong();
	return true;
}
