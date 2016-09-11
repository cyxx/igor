/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef TEXT_H__
#define TEXT_H__

#include "util.h"

#define MAX_PREPOSITIONS 3
#define MAX_ITEMS 35
#define MAX_RESPONSES 250
#define MAX_VERBS 8
#define MAX_TEXT_ROOMS 256
#define MAX_TEXT_ROOM_ITEMS 20
#define MAX_TEXT_ROOM_DIALOGUES 50

struct TextData {

	enum {
		COMMON_TEXT_PREPOSITIONS,
		COMMON_TEXT_ITEMS,
		COMMON_TEXT_RESPONSES,
		COMMON_TEXT_VERBS,
	};

	char *_txtBuf;

	int _commonText;
	uint32_t _prepositionsOffset; // @prepositions
	uint32_t _itemsOffset; // @items
	uint32_t _responsesOffset; // @reponses
	uint32_t _verbsOffset; // @verbs
	uint32_t _commonTextsOffset[256];

	int _currentRoom;
	uint32_t _roomsOffset[MAX_TEXT_ROOMS]; // @room
	uint32_t _roomItemsOffset[MAX_TEXT_ROOM_ITEMS]; // 000...
	uint32_t _roomDialoguesOffset[MAX_TEXT_ROOM_DIALOGUES]; // 200...

	TextData();
	~TextData();

	bool load(const char *dataPath, const char *language);
	void unload();

	void loadCommonText(uint32_t txtOffset, uint32_t *offsets, int count);
	void loadRoomText(int room);
	int readText(uint32_t offset, uint8_t *dst);

	int readPreposition(int num, uint8_t *dst);
	int readItem(int num, uint8_t *dst);
	int readResponse(int num, uint8_t *dst);
	int readVerb(int num, uint8_t *dst);
	int readRoomItem(int room, int num, uint8_t *dst);
	int readRoomDialogue(int room, int num, uint8_t *dst);
};

#endif
