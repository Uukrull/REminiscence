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
 
#ifndef __MOD_PLAYER_H__
#define __MOD_PLAYER_H__

#include "intern.h"

struct Mixer;

struct Player {
	enum {
		NUM_SAMPLES = 31,
		NUM_TRACKS = 4,
		NUM_PATTERNS = 128,
		FRAC_BITS = 12,
		PAULA_FREQ = 3546897
	};

	struct SampleInfo {
		char name[23];
		uint16 len;
		uint8 fineTune;
		uint8 volume;
		uint16 repeatPos;
		uint16 repeatLen;
		int8 *data;
	};
	
	struct ModuleInfo {
		char songName[21];
		SampleInfo samples[NUM_SAMPLES];
		uint8 numPatterns;
		uint8 patternOrderTable[NUM_PATTERNS];
		uint8 *patternsTable;
	};
	
	struct Track {
		SampleInfo *sample;
		uint8 volume;
		int pos;
		int freq;
		uint16 period;
		uint16 periodIndex;
		uint16 effectData;
		int vibratoSpeed;
		int vibratoAmp;
		int vibratoPos;
		int portamento;
		int portamentoSpeed;
		int retriggerCounter;
		int delayCounter;
		int cutCounter;
	};
	
	static const uint16 _periodTable[];
	static const char *_moduleFiles[];
	
	ModuleInfo _modInfo;
	uint8 _currentPatternOrder;
	uint8 _currentPatternPos;
	uint8 _currentTick;
	uint8 _songSpeed;
	uint8 _songTempo;
	int _patternDelay;
	int _patternLoopPos;
	int _patternLoopCount;
	bool _playing;
	int _samplesLeft;
	Track _tracks[NUM_TRACKS];
	uint8 _vibratoSineWaveform[64];
	Mixer *_mix;
	const char *_dataPath;
	
	Player(Mixer *mixer, const char *dataPath);

	void startSong(uint8 songNum);
	void stopSong();
	uint16 findPeriod(uint16 period, uint8 fineTune) const;
	void loadModule(const char *filename, const char *directory);
	void start();
	void stop();
	void handleNote(int trackNum, uint32 noteData);
	void handleTick();
	void applyVolumeSlide(int trackNum, int amount);
	void applyVibrato(int trackNum);
	void applyPortamento(int trackNum);
	void handleEffect(int trackNum, bool tick);
	void mixSamples(int8 *buf, int len);
	void mix(int8 *buf, int len);

	static void mixCallback(void *param, int8 *buf, int len);
};

#endif // __MOD_PLAYER_H__
