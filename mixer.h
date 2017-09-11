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

#ifndef __MIXER_H__
#define __MIXER_H__

#include "intern.h"

struct MixerChunk {
	const uint8 *data;
	uint16 len;
};

struct MixerChannel {
	uint8 active;
	uint8 volume;
	MixerChunk chunk;
	int chunkPos;
	int chunkInc;
};

struct SystemStub;

struct Mixer {
	typedef void (*PremixHook)(void *userData, int8 *buf, int len);
	
	enum {
		NUM_CHANNELS = 4,
		FRAC_BITS = 12
	};

	void *_mutex;
	SystemStub *_stub;
	MixerChannel _channels[NUM_CHANNELS];
	PremixHook _premixHook;
	void *_premixHookData;

	Mixer(SystemStub *stub);
	void init();
	void free();
	void setPremixHook(PremixHook premixHook, void *userData);
	void play(const MixerChunk *mc, uint16 freq, uint8 volume);
	void stopAll();
	uint32 getSampleRate() const;
	void mix(int8 *buf, int len);

	static void mixCallback(void *param, uint8 *buf, int len);
};

#endif // __MIXER_H__
