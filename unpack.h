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

#ifndef __UNPACK_H__
#define __UNPACK_H__

#include "intern.h"

struct Unpack {
	int _size, _datasize;
	uint32 _crc;
	uint32 _chk;
	uint8 *_dst;
	const uint8 *_src;

	bool unpack(uint8 *dst, const uint8 *src, int size, int &dec_size);
	void decUnk1(uint8 numChunks, uint8 addCount);
	void decUnk2(uint8 numChunks);
	uint16 getCode(uint8 numChunks);
	bool nextChunk();
	bool rcr(bool CF);
};

#endif // __UNPACK_H__
