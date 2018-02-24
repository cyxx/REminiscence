
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2018 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef INTERN_H__
#define INTERN_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#undef ABS
#define ABS(x) ((x)<0?-(x):(x))
#undef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#undef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#undef ARRAYSIZE
#define ARRAYSIZE(a) (int)(sizeof(a)/sizeof(a[0]))

inline uint16_t READ_BE_UINT16(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 8) | b[1];
}

inline uint32_t READ_BE_UINT32(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

inline uint16_t READ_LE_UINT16(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[1] << 8) | b[0];
}

inline uint32_t READ_LE_UINT32(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
}

inline int8_t ADDC_S8(int a, int b) {
	a += b;
	if (a < -128) {
		a = -128;
	} else if (a > 127) {
		a = 127;
	}
	return a;
}

inline int16_t ADDC_S16(int a, int b) {
	a += b;
	if (a < -32768) {
		a = -32768;
	} else if (a > 32767) {
		a = 32767;
	}
	return a;
}

template<typename T>
inline void SWAP(T &a, T &b) {
	T tmp = a;
	a = b;
	b = tmp;
}

enum Language {
	LANG_FR,
	LANG_EN,
	LANG_DE,
	LANG_SP,
	LANG_IT,
	LANG_JP,
};

enum ResourceType {
	kResourceTypeAmiga,
	kResourceTypeDOS,
	kResourceTypeMac,
};

enum Skill {
	kSkillEasy = 0,
	kSkillNormal,
	kSkillExpert,
};

struct Options {
	bool bypass_protection;
	bool enable_password_menu;
	bool fade_out_palette;
	bool use_tiledata;
	bool use_text_cutscenes;
	bool use_seq_cutscenes;
	bool play_asc_cutscene;
	bool play_caillou_cutscene;
	bool play_metro_cutscene;
	bool play_serrure_cutscene;
};

struct Color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct Point {
	int16_t x;
	int16_t y;
};

struct Demo {
	const char *name;
	int level;
	int room;
	int x, y;
};

struct Level {
	const char *name;
	const char *name2;
	const char *nameAmiga;
	uint16_t cutscene_id;
	uint8_t sound;
	uint8_t track;
};

struct InitPGE {
	uint16_t type;
	int16_t pos_x;
	int16_t pos_y;
	uint16_t obj_node_number;
	uint16_t life;
	int16_t counter_values[4];
	uint8_t object_type;
	uint8_t init_room;
	uint8_t room_location;
	uint8_t init_flags;
	uint8_t colliding_icon_num;
	uint8_t icon_num;
	uint8_t object_id;
	uint8_t skill;
	uint8_t mirror_x;
	uint8_t flags;
	uint8_t unk1C; // collidable, collision_data_len
	uint16_t text_num;
};

struct LivePGE {
	uint16_t obj_type;
	int16_t pos_x;
	int16_t pos_y;
	uint8_t anim_seq;
	uint8_t room_location;
	int16_t life;
	int16_t counter_value;
	uint8_t collision_slot;
	uint8_t next_inventory_PGE;
	uint8_t current_inventory_PGE;
	uint8_t unkF; // unk_inventory_PGE
	uint16_t anim_number;
	uint8_t flags;
	uint8_t index;
	uint16_t first_obj_number;
	LivePGE *next_PGE_in_room;
	InitPGE *init_PGE;
};

struct GroupPGE {
	GroupPGE *next_entry;
	uint16_t index;
	uint16_t group_id;
};

struct Object {
	uint16_t type;
	int8_t dx;
	int8_t dy;
	uint16_t init_obj_type;
	uint8_t opcode2;
	uint8_t opcode1;
	uint8_t flags;
	uint8_t opcode3;
	uint16_t init_obj_number;
	int16_t opcode_arg1;
	int16_t opcode_arg2;
	int16_t opcode_arg3;
};

struct ObjectNode {
	uint16_t last_obj_number;
	Object *objects;
	uint16_t num_objects;
};

struct ObjectOpcodeArgs {
	LivePGE *pge; // arg0
	int16_t a; // arg2
	int16_t b; // arg4
};

struct AnimBufferState {
	int16_t x, y;
	uint8_t w, h;
	const uint8_t *dataPtr;
	LivePGE *pge;
};

struct AnimBuffers {
	AnimBufferState *_states[4];
	uint8_t _curPos[4];

	void addState(uint8_t stateNum, int16_t x, int16_t y, const uint8_t *dataPtr, LivePGE *pge, uint8_t w = 0, uint8_t h = 0);
};

struct CollisionSlot {
	int16_t ct_pos;
	CollisionSlot *prev_slot;
	LivePGE *live_pge;
	uint16_t index;
};

struct BankSlot {
	uint16_t entryNum;
	uint8_t *ptr;
};

struct CollisionSlot2 {
	CollisionSlot2 *next_slot;
	int8_t *unk2;
	uint8_t data_size;
	uint8_t data_buf[0x10]; // XXX check size
};

struct InventoryItem {
	uint8_t icon_num;
	InitPGE *init_pge;
	LivePGE *live_pge;
};

struct SoundFx {
	uint32_t offset;
	uint16_t len;
	uint16_t freq;
	uint8_t *data;
};

extern Options g_options;
extern const char *g_caption;

#endif // INTERN_H__
