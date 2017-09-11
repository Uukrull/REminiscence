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

#include "resource.h"
#include "systemstub.h"
#include "video.h"


Video::Video(Resource *res, SystemStub *stub)
	: _res(res), _stub(stub) {
}

void Video::init() {
	_frontLayer = (uint8 *)malloc(GAMESCREEN_W * GAMESCREEN_H);
	_backLayer = (uint8 *)malloc(GAMESCREEN_W * GAMESCREEN_H);	
	_tempLayer = (uint8 *)malloc(GAMESCREEN_W * GAMESCREEN_H);	
}

void Video::fadeOut() {
	for (int i = 0; i <= 6; ++i) {
		for (int c = 0; c < 256; ++c) {
			Color col;
			_stub->getPaletteEntry(c, &col);
			col.r >>= 1;
			col.g >>= 1;
			col.b >>= 1;
			_stub->setPaletteEntry(c, &col);
		}
		_stub->copyRect(0, 0, GAMESCREEN_W, GAMESCREEN_H, _frontLayer, GAMESCREEN_W);
		_stub->sleep(120);
	}
}

void Video::setPaletteSlotBE(int palSlot, int palNum) {
	const uint8 *p = _res->_pal + palNum * 0x20;
	for (int i = 0; i < 16; ++i) {
		uint16 color = READ_BE_UINT16(p); p += 2;
		uint8 t = (color == 0) ? 0 : 3;
		Color c;
		c.r = ((color & 0x00F) << 2) | t;
		c.g = ((color & 0x0F0) >> 2) | t;
		c.b = ((color & 0xF00) >> 6) | t;
		_stub->setPaletteEntry(palSlot * 0x10 + i, &c);
	}
}

void Video::setPaletteSlotLE(int palSlot, const uint8 *palData) {
	for (int i = 0; i < 16; ++i) {
		uint16 color = READ_LE_UINT16(palData); palData += 2;
		Color c;
		c.b = (color & 0x00F) << 2;
		c.g = (color & 0x0F0) >> 2;
		c.r = (color & 0xF00) >> 6;
		_stub->setPaletteEntry(palSlot * 0x10 + i, &c);
	}
}

void Video::setPalette0xE() {
	const uint8 *p = _palSlot0xE;
	for (int i = 0; i < 16; ++i) {
		uint16 color = READ_LE_UINT16(p); p += 2;
		Color c;
		c.b = (color & 0x00F) << 2;
		c.g = (color & 0x0F0) >> 2;
		c.r = (color & 0xF00) >> 6;
		_stub->setPaletteEntry(0xE0 + i, &c);
	}
}

void Video::setPalette0xF() {
	const uint8 *p = _palSlot0xF;
	for (int i = 0; i < 16; ++i) {
		Color c;
		c.r = *p++ >> 2;
		c.g = *p++ >> 2;
		c.b = *p++ >> 2;
		_stub->setPaletteEntry(0xF0 + i, &c);
	}
}

void Video::copyLevelMap(uint16 room) {
	debug(DBG_VIDEO, "Video::copyLevelMap(%d)", room);
	assert(room < 0x40);
	int32 off = READ_LE_UINT32(_res->_map + room * 6);
	if (off == 0) {
		error("Invalid room %d", room);
	}	
	bool packed = true;
	if (off < 0) {
		off = -off;
		packed = false;
	}
	const uint8 *p = _res->_map + off;
	_mapPalSlot1 = *p++;
	_mapPalSlot2 = *p++;
	_mapPalSlot3 = *p++;
	_mapPalSlot4 = *p++;
	if (packed != 0) {
		uint8 *vid = _frontLayer;
		for (int i = 0; i < 4; ++i) {
			uint16 sz = READ_LE_UINT16(p); p += 2;
			decodeLevelMap(sz, p, _res->_memBuf); p += sz;
			memcpy(vid, _res->_memBuf, 256 * 56);
			vid += 256 * 56;
		}
	} else {
		for (int i = 0; i < 4; ++i) {
			for (int y = 0; y < 224; ++y) {
				for (int x = 0; x < 64; ++x) {
					_frontLayer[i + x * 4 + 256 * y] = p[0x3800 * i + x + 64 * y];
				}
			}
		}
	}
}

void Video::decodeLevelMap(uint16 sz, const uint8 *src, uint8 *dst) {
	debug(DBG_VIDEO, "Video::decodeLevelMap() sz = 0x%X", sz);
	const uint8 *end = src + sz;
	while (src < end) {
		int16 code = (int8)*src++;
		if (code < 0) {
			int len = 1 - code;
			memset(dst, *src++, len);
			dst += len;
		} else {
			++code;
			memcpy(dst, src, code);
			src += code;
			dst += code;
		}
	}
}

void Video::setLevelPalettes() {
	if (_unkPalSlot2 == 0) {
		_unkPalSlot2 = _mapPalSlot3;
	}
	if (_unkPalSlot1 == 0) {
		_unkPalSlot1 = _mapPalSlot3;
	}
	setPaletteSlotBE(0x0, _mapPalSlot1);
	setPaletteSlotBE(0x1, _mapPalSlot2);
	setPaletteSlotBE(0x2, _mapPalSlot3);
	setPaletteSlotBE(0x3, _mapPalSlot4);
	if (_unkPalSlot1 == _mapPalSlot3) {
		setPaletteSlotLE(4, _conradPal1);
	} else {
		setPaletteSlotLE(4, _conradPal2);
	}
	// slot 5 is monster palette
	setPaletteSlotBE(0x8, _mapPalSlot1);
	setPaletteSlotBE(0x9, _mapPalSlot2);
	setPaletteSlotBE(0xA, _unkPalSlot2);
	setPaletteSlotBE(0xB, _mapPalSlot4);
	setPalette0xE(); // text palette
}

void Video::drawSpriteSub1(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask) {
	debug(DBG_VIDEO, "Video::drawSpriteSub1(0x%X, 0x%X, 0x%X, 0x%X)", pitch, w, h, colMask);
	while (h--) {
		for (int i = 0; i < w; ++i) {
			if (src[i] != 0) {
				dst[i] = src[i] | colMask;
			}
		}
		src += pitch;
		dst += 256;
	}
}

void Video::drawSpriteSub2(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask) {
	debug(DBG_VIDEO, "Video::drawSpriteSub2(0x%X, 0x%X, 0x%X, 0x%X)", pitch, w, h, colMask);
	while (h--) {
		for (int i = 0; i < w; ++i) {
			if (src[-i] != 0) {
				dst[i] = src[-i] | colMask;
			}
		}
		src += pitch;
		dst += 256;
	}
}	

void Video::drawSpriteSub3(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask) {
	debug(DBG_VIDEO, "Video::drawSpriteSub3(0x%X, 0x%X, 0x%X, 0x%X)", pitch, w, h, colMask);
	while (h--) {
		for (int i = 0; i < w; ++i) {
			if (src[i] != 0 && !(dst[i] & 0x80)) {
				dst[i] = src[i] | colMask;
			}
		}
		src += pitch;
		dst += 256;
	}
}

void Video::drawSpriteSub4(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask) {
	debug(DBG_VIDEO, "Video::drawSpriteSub4(0x%X, 0x%X, 0x%X, 0x%X)", pitch, w, h, colMask);
	while (h--) {
		for (int i = 0; i < w; ++i) {
			if (src[-i] != 0 && !(dst[i] & 0x80)) {
				dst[i] = src[-i] | colMask;
			}
		}
		src += pitch;
		dst += 256;
	}
}

void Video::drawSpriteSub5(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask) {
	debug(DBG_VIDEO, "Video::drawSpriteSub5(0x%X, 0x%X, 0x%X, 0x%X)", pitch, w, h, colMask);
	while (h--) {
		for (int i = 0; i < w; ++i) {
			if (src[i * pitch] != 0 && !(dst[i] & 0x80)) {
				dst[i] = src[i * pitch] | colMask;
			}
		}
		++src;
		dst += 256;
	}
}

void Video::drawSpriteSub6(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask) {
	debug(DBG_VIDEO, "Video::drawSpriteSub6(0x%X, 0x%X, 0x%X, 0x%X)", pitch, w, h, colMask);
	while (h--) {
		for (int i = 0; i < w; ++i) {
			if (src[-i * pitch] != 0 && !(dst[i] & 0x80)) {
				dst[i] = src[-i * pitch] | colMask;
			}
		}
		++src;
		dst += 256;
	}
}

void Video::drawChar(char c, int y, int x) {
	y *= 8;
	x *= 8;
	const uint8 *src = _res->_fnt + (c - 32) * 32;
	uint8 *dst = _frontLayer + x + 256 * y;
	for (int h = 0; h < 8; ++h) {
		for (int i = 0; i < 4; ++i) {
			uint8 c1 = (*src & 0xF0) >> 4;
			uint8 c2 = (*src & 0x0F) >> 0;
			++src;
			
			if (c1 != 0) {
				if (c1 != 2) {
					*dst = _drawCharColor1;
				} else {
					*dst = _drawCharColor3;
				}
			} else if (_drawCharColor2 != 0xFF) {
				*dst = _drawCharColor2;
			}
			++dst;
			
			if (c2 != 0) {
				if (c2 != 2) {
					*dst = _drawCharColor1;
				} else {
					*dst = _drawCharColor3;
				}
			} else if (_drawCharColor2 != 0xFF) {
				*dst = _drawCharColor2;
			}
			++dst;
		}
		dst += 256 - 8;
	}
}

const char *Video::drawString(const char *str, int x, int y, uint8 col) {
	debug(DBG_VIDEO, "Video::drawString('%s', %d, %d, 0x%X)", str, x, y, col);
	int offset = y * 256 + x;
	uint8 *dst = _frontLayer + offset;
	while (1) {
		uint8 c = *str++;
		if (c == 0 || c == 0xB || c == 0xA) {
			break;
		}
		uint8 *dst_char = dst;
		const uint8 *src = _res->_fnt + (c - 32) * 32;
		for (int h = 0; h < 8; ++h) {
			for (int w = 0; w < 4; ++w) {
				uint8 c1 = (*src & 0xF0) >> 4;
				uint8 c2 = (*src & 0x0F) >> 0;
				++src;
				if (c1 != 0) {
					*dst_char = (c1 == 0xF) ? col : (0xE0 + c1);
				}
				++dst_char;
				if (c2 != 0) {
					*dst_char = (c2 == 0xF) ? col : (0xE0 + c2);
				}
				++dst_char;
			}
			dst_char += 256 - 8;
		}
		dst += 8; // character width
	}
	return str - 1;
}

