/* REminiscence - Flashback interpreter
 * Copyright (C) 2005-2011 Gregory Montoir
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SEQ_PLAYER_H__
#define SEQ_PLAYER_H__

#include "intern.h"

struct File;
struct SystemStub;
struct Video;

struct SeqDemuxer {
	enum {
		kFrameSize = 6144,
		kAudioBufferSize = 882,
		kBuffersCount = 30
	};

	bool open(File *f);
	void close();

	bool readHeader();
	void readFrameData();
	void fillBuffer(int num, int offset, int size);
	void clearBuffer(int num);
	void readPalette(uint8 *dst);

	int _frameOffset;
	int _audioDataOffset;
	int _audioDataSize;
	int _paletteDataOffset;
	int _paletteDataSize;
	int _videoData;
	struct {
		int size;
		int avail;
		uint8 *data;
	} _buffers[kBuffersCount];
	File *_f;
};

struct SeqPlayer {
	enum {
		kVideoWidth = 256,
		kVideoHeight = 128
	};

	static const char *_namesTable[];

	SeqPlayer(SystemStub *stub);

	void setBackBuffer(uint8 *buf) { _buf = buf; }
	void play(File *f);

	SystemStub *_stub;
	uint8 *_buf;
	SeqDemuxer _demux;
};

#endif // SEQ_PLAYER_H__

