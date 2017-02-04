/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "text.h"


TextData::TextData()
	: _txtBuf(0) {
	_commonText = -1;
	_prepositionsOffset = 0;
	_itemsOffset = 0;
	_responsesOffset = 0;
	_verbsOffset = 0;
	memset(_commonTextsOffset, 0, sizeof(_commonTextsOffset));
	_currentRoom = -1;
	memset(_roomsOffset, 0, sizeof(_roomsOffset));
}

TextData::~TextData() {
	unload();
}

static uint32_t findOffset(const char *buf, const char *name) {
	uint32_t offset = 0;
	const char *p = strstr(buf, name);
	if (p) {
		offset = p - buf;
		debug(DBG_TEXT, "Found %s at 0x%X\n", name, offset);
	}
	return offset;
}

bool TextData::load(const char *dataPath, const char *language) {
	char name[32];
	snprintf(name, sizeof(name), "igor_%s.txt", language);
	AssetFile f;
	if (f.open(name, dataPath)) {
		const int len = f.size() + 1;
		_txtBuf = (char *)malloc(len);
		if (!_txtBuf) {
			warning("Failed to allocate %d bytes", len);
		} else {
			const int count = f.read(_txtBuf, len - 1);
			if (count != len - 1) {
				warning("Failed to read %d bytes, count %d", len - 1, count);
			} else {
				_txtBuf[count] = 0;
				_prepositionsOffset = findOffset(_txtBuf, "@prepositions");
				_itemsOffset = findOffset(_txtBuf, "@items");
				_responsesOffset = findOffset(_txtBuf, "@responses");
				_verbsOffset = findOffset(_txtBuf, "@verbs");
				for (int i = 1; i < MAX_TEXT_ROOMS; ++i) {
					snprintf(name, sizeof(name), "@room%03d", i);
					_roomsOffset[i] = findOffset(_txtBuf, name);
				}
				return true;
			}
		}
	}
	return false;
}

void TextData::unload() {
	free(_txtBuf);
	_txtBuf = 0;
	_currentRoom = -1;
}

void TextData::loadCommonText(uint32_t txtOffset, uint32_t *offsets, int count) {
	if (txtOffset != 0) {
		const char *p = _txtBuf + txtOffset;
		const char *next = 0;
		do {
			next = strchr(p, '\n');
			if (p[0] != '#') {
				if (strncmp(p, "@end", 4) == 0) {
					break;
				}
				int index = -1;
				if (sscanf(p, "%03d : ", &index) == 1 && index >= 0) {
					p += 5;
					assert(index < count);
					offsets[index] = p - _txtBuf;
				}
			}
			p = next + 1;
		} while (next);
	}
}

void TextData::loadRoomText(int room) {
	if (_roomsOffset[room] == 0) {
		warning("No text for room %d\n", room);
		return;
	}
	memset(_roomItemsOffset, 0, sizeof(_roomItemsOffset));
	memset(_roomDialoguesOffset, 0, sizeof(_roomDialoguesOffset));
	const char *p = _txtBuf + _roomsOffset[room];
	const char *next = 0;
	do {
		next = strchr(p, '\n');
		if (p[0] != '#') {
			if (strncmp(p, "@end", 4) == 0) {
				break;
			}
			int index = -1;
			if (sscanf(p, "%03d : ", &index) == 1 && index >= 0) {
				p += 5;
				if (index < 200) {
					assert(index < MAX_TEXT_ROOM_ITEMS);
					_roomItemsOffset[index] = p - _txtBuf;
				} else if (index >= 200) {
					index -= 200;
					assert(index < MAX_TEXT_ROOM_DIALOGUES);
					_roomDialoguesOffset[index] = p - _txtBuf;
				}
			}
		}
		p = next + 1;
	} while (next);
}

int TextData::readText(uint32_t offset, uint8_t *dst) {
	if (offset == 0) {
		return 0;
	}
	const char *p = _txtBuf + offset;
	int i = 0;
	while (p[i] && !strchr("\r\n", p[i])) {
		++i;
	}
	assert(i < 256);
	*dst = i;
	memcpy(dst + 1, p, i);
	return i + 1;
}

int TextData::readPreposition(int num, uint8_t *dst) {
	if (_commonText != COMMON_TEXT_PREPOSITIONS) {
		loadCommonText(_prepositionsOffset, _commonTextsOffset, MAX_PREPOSITIONS);
		_commonText = COMMON_TEXT_PREPOSITIONS;
	}
	return readText(_commonTextsOffset[num], dst);
}

int TextData::readItem(int num, uint8_t *dst) {
	if (_commonText != COMMON_TEXT_ITEMS) {
		loadCommonText(_itemsOffset, _commonTextsOffset, MAX_ITEMS);
		_commonText = COMMON_TEXT_ITEMS;
	}
	return readText(_commonTextsOffset[num], dst);
}

int TextData::readResponse(int num, uint8_t *dst) {
	if (_commonText != COMMON_TEXT_RESPONSES) {
		loadCommonText(_responsesOffset, _commonTextsOffset, MAX_RESPONSES);
		_commonText = COMMON_TEXT_RESPONSES;
	}
	return readText(_commonTextsOffset[num], dst);
}

int TextData::readVerb(int num, uint8_t *dst) {
	if (_commonText != COMMON_TEXT_VERBS) {
		loadCommonText(_verbsOffset, _commonTextsOffset, MAX_VERBS);
		_commonText = COMMON_TEXT_VERBS;
	}
	return readText(_commonTextsOffset[num], dst);
}

int TextData::readRoomItem(int room, int num, uint8_t *dst) {
	if (_currentRoom != room) {
		loadRoomText(room);
		_currentRoom = room;
	}
	assert(num >= 0 && num < MAX_TEXT_ROOM_ITEMS);
	return readText(_roomItemsOffset[num], dst);
}

int TextData::readRoomDialogue(int room, int num, uint8_t *dst) {
	if (_currentRoom != room) {
		loadRoomText(room);
		_currentRoom = room;
	}
	num -= 200;
	assert(num >= 0 && num < MAX_TEXT_ROOM_DIALOGUES);
	return readText(_roomDialoguesOffset[num], dst);
}
