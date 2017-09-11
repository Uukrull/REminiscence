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

#include "unpack.h"


bool Unpack::unpack(uint8 *dst, const uint8 *src, int size, int &dec_size) {
	_src = src + size - 4;		
	_datasize = READ_BE_UINT32(_src); _src -= 4;
	dec_size = _datasize;
	_dst = dst + _datasize - 1;
	_size = 0;
	_crc = READ_BE_UINT32(_src); _src -= 4;
	_chk = READ_BE_UINT32(_src); _src -= 4;
	debug(DBG_UNPACK, "Unpack::unpack() crc = 0x%X _datasize = 0x%X", _crc, _datasize);
	_crc ^= _chk;
	do {
		if (!nextChunk()) {
			_size = 1;
			if (!nextChunk()) {
				decUnk1(3, 0);
			} else {
				decUnk2(8);
			}
		} else {
			uint16 c = getCode(2);
			if (c == 3) {
				decUnk1(8, 8);
			} else if (c < 2) {
				_size = c + 2;
				decUnk2(c + 9);
			} else {
				_size = getCode(8);
				decUnk2(12);
			}
		}
	} while (_datasize > 0);
	return _crc == 0;
}

void Unpack::decUnk1(uint8 numChunks, uint8 addCount) {
	uint16 count = getCode(numChunks) + addCount + 1;
	_datasize -= count;
	while (count--) {
		*_dst = (uint8)getCode(8);
		--_dst;
	}
}

void Unpack::decUnk2(uint8 numChunks) {
	uint16 i = getCode(numChunks);
	uint16 count = _size + 1;
	_datasize -= count;
	while (count--) {
		*_dst = *(_dst + i);
		--_dst;
	}
}

uint16 Unpack::getCode(uint8 numChunks) {
	uint16 c = 0;
	while (numChunks--) {
		c <<= 1;
		if (nextChunk()) {
			c |= 1;
		}			
	}
	return c;
}

bool Unpack::nextChunk() {
	bool CF = rcr(false);
	if (_chk == 0) {
		_chk = READ_BE_UINT32(_src); _src -= 4;
		_crc ^= _chk;
		CF = rcr(true);
	}
	return CF;
}

bool Unpack::rcr(bool CF) {
	bool rCF = (_chk & 1);
	_chk >>= 1;
	if (CF) _chk |= 0x80000000;
	return rCF;
}
