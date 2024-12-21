
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "game.h"
#include "resource.h"
#include "systemstub.h"
#include "util.h"

void Game::pge_resetMessages() {
	memset(_pge_messagesTable, 0, sizeof(_pge_messagesTable));
	MessagePGE *le = &_pge_messages[0];
	_pge_nextFreeMessage = le;
	int n = 0xFF;
	while (n--) {
		le->next_entry = le + 1;
		le->src_pge = 0;
		le->msg_num = 0;
		++le;
	}
	le->next_entry = 0;
	le->src_pge = 0;
	le->msg_num = 0;
}

void Game::pge_clearMessages(uint8_t pge_index) {
	MessagePGE *le = _pge_messagesTable[pge_index];
	if (le) {
		_pge_messagesTable[pge_index] = 0;
		MessagePGE *next = _pge_nextFreeMessage;
		while (le) {
			MessagePGE *cur = le->next_entry;
			le->next_entry = next;
			le->src_pge = 0;
			le->msg_num = 0;
			next = le;
			le = cur;
		}
		_pge_nextFreeMessage = next;
	}
}

int Game::pge_hasMessageData(LivePGE *pge, uint16_t msg_num, uint16_t counter) const {
	assert(counter >= 1 && counter <= 4);
	uint16_t pge_src_index = pge->init_PGE->data[counter - 1];
	const MessagePGE *le = _pge_messagesTable[pge->index];
	while (le) {
		if (le->msg_num == msg_num && le->src_pge == pge_src_index) {
			return 1;
		}
		le = le->next_entry;
	}
	return 0;
}

void Game::pge_loadForCurrentLevel(uint16_t idx) {
	debug(DBG_PGE, "Game::pge_loadForCurrentLevel() idx=%d", idx);

	LivePGE *live_pge = &_pgeLive[idx];
	InitPGE *init_pge = &_res._pgeInit[idx];

	live_pge->init_PGE = init_pge;
	live_pge->obj_type = init_pge->type;
	live_pge->pos_x = init_pge->pos_x;
	live_pge->pos_y = init_pge->pos_y;
	live_pge->anim_seq = 0;
	live_pge->room_location = init_pge->init_room;

	live_pge->life = init_pge->life;
	if (_skillLevel >= 2 && init_pge->object_type == 10) {
		live_pge->life *= 2;
	}
	live_pge->counter_value = 0;
	live_pge->collision_slot = 0xFF;
	live_pge->next_inventory_PGE = 0xFF;
	live_pge->current_inventory_PGE = 0xFF;
	live_pge->ref_inventory_PGE = 0xFF;
	live_pge->anim_number = 0;
	live_pge->index = idx;
	live_pge->next_PGE_in_room = 0;

	uint16_t flags = 0;
	if (init_pge->skill <= _skillLevel) {
		if (init_pge->room_location != 0 || ((init_pge->flags & 4) && (_currentRoom == init_pge->init_room))) {
			flags |= 4;
			_pge_liveTable2[idx] = live_pge;
		}
		if (init_pge->mirror_x != 0) {
			flags |= 1;
		}
		if (init_pge->init_flags & 8) {
			flags |= 0x10;
		}
		flags |= (init_pge->init_flags & 3) << 5;
		if (init_pge->flags & 2) {
			flags |= 0x80;
		}
		live_pge->flags = flags;
		assert(init_pge->obj_node_number < _res._numObjectNodes);
		ObjectNode *on = _res._objectNodesMap[init_pge->obj_node_number];
		Object *obj = on->objects;
		int i = 0;
		while (obj->type != live_pge->obj_type) {
			++i;
			++obj;
		}
		assert(i < on->num_objects);
		live_pge->first_obj_number = i;
		pge_setupDefaultAnim(live_pge);
	}
}

void Game::pge_process(LivePGE *pge) {
	debug(DBG_PGE, "Game::pge_process() pge_num=%ld", pge - &_pgeLive[0]);
	_pge_playAnimSound = true;
	_pge_currentPiegeFacingDir = (pge->flags & 1) != 0;
	_pge_currentPiegeRoom = pge->room_location;
	MessagePGE *le = _pge_messagesTable[pge->index];
	if (le) {
		pge_setupNextAnimFrame(pge, le);
	}
	const uint8_t *anim_data = _res.getAniData(pge->obj_type);
	if (_res._readUint16(anim_data) <= pge->anim_seq) {
		InitPGE *init_pge = pge->init_PGE;
		assert(init_pge->obj_node_number < _res._numObjectNodes);
		ObjectNode *on = _res._objectNodesMap[init_pge->obj_node_number];
		Object *obj = &on->objects[pge->first_obj_number];
		while (1) {
			if (obj->type != pge->obj_type) {
				pge_clearMessages(pge->index);
				return;
			}
			uint16_t _ax = pge_execute(pge, init_pge, obj);
			if (_res.isDOS()) {
				if (_currentLevel == 6 && (_currentRoom == 50 || _currentRoom == 51)) {
					if (pge->index == 79 && _ax == 0xFFFF && obj->opcode1 == 0x60 && obj->opcode2 == 0 && obj->opcode3 == 0) {
						if (col_getGridPos(&_pgeLive[79], 0) == col_getGridPos(&_pgeLive[0], 0)) {
							pge_sendMessage(79, 0, 4);
						}
					}
				}
			}
			if (_ax != 0) {
				anim_data = _res.getAniData(pge->obj_type);
				uint8_t snd = anim_data[2];
				if (snd) {
					pge_playAnimSound(pge, snd);
				}
				pge_setupOtherPieges(pge, init_pge);
				break;
			}
			++obj;
		}
	}
	pge_setupAnim(pge);
	++pge->anim_seq;
	pge_clearMessages(pge->index);
}

void Game::pge_setupNextAnimFrame(LivePGE *pge, MessagePGE *le) {
	InitPGE *init_pge = pge->init_PGE;
	assert(init_pge->obj_node_number < _res._numObjectNodes);
	ObjectNode *on = _res._objectNodesMap[init_pge->obj_node_number];
	Object *obj = &on->objects[pge->first_obj_number];
	int i = pge->first_obj_number;
	while (i < on->num_objects && pge->obj_type == obj->type) {
		MessagePGE *next_le = le;
		while (next_le) {
			uint16_t msgNum = next_le->msg_num;
			if (obj->opcode2 == 0x6B) { // pge_op_isMessageReceived
				if (obj->opcode_arg2 == 0) {
					if (msgNum == 1 || msgNum == 2) goto set_anim;
				}
				if (obj->opcode_arg2 == 1) {
					if (msgNum == 3 || msgNum == 4) goto set_anim;
				}
			} else if (msgNum == obj->opcode_arg2) {
				if (obj->opcode2 == 0x22 || obj->opcode2 == 0x6F) goto set_anim;
			}
			if (obj->opcode1 == 0x6B) { // pge_op_isMessageReceived
				if (obj->opcode_arg1 == 0) {
					if (msgNum == 1 || msgNum == 2) goto set_anim;
				}
				if (obj->opcode_arg1 == 1) {
					if (msgNum == 3 || msgNum == 4) goto set_anim;
				}
			} else if (msgNum == obj->opcode_arg1) {
				if (obj->opcode1 == 0x22 || obj->opcode1 == 0x6F) goto set_anim;
			}
			next_le = next_le->next_entry;
		}
		++obj;
		++i;
	}
	return;

set_anim:
	const uint8_t *anim_data = _res.getAniData(pge->obj_type);
	uint8_t _dh = _res._readUint16(anim_data);
	uint8_t _dl = pge->anim_seq;
	const uint8_t *anim_frame = anim_data + 6 + _dl * 4;
	while (_dh > _dl) {
		if (_res._readUint16(anim_frame) != 0xFFFF) {
			if (_pge_currentPiegeFacingDir) {
				pge->pos_x -= (int8_t)anim_frame[2];
			} else {
				pge->pos_x += (int8_t)anim_frame[2];
			}
			pge->pos_y += (int8_t)anim_frame[3];
		}
		anim_frame += 4;
		++_dl;
	}
	pge->anim_seq = _dh;
	_col_currentPiegeGridPosY = (pge->pos_y / 36) & ~1;
	_col_currentPiegeGridPosX = (pge->pos_x + 8) >> 4;
}

void Game::pge_playAnimSound(LivePGE *pge, uint16_t arg2) {
	if ((pge->flags & 4) && _pge_playAnimSound) {
		uint8_t sfxId = (arg2 & 0xFF) - 1;
		if (_currentRoom == pge->room_location) {
			playSound(sfxId, 0);
		} else {
			if (_res._ctData[CT_DOWN_ROOM + _currentRoom] == pge->room_location ||
				_res._ctData[CT_UP_ROOM + _currentRoom] == pge->room_location ||
				_res._ctData[CT_RIGHT_ROOM + _currentRoom] == pge->room_location ||
				_res._ctData[CT_LEFT_ROOM + _currentRoom] == pge->room_location) {
				playSound(sfxId, 1);
			}
		}
	}
}

void Game::pge_setupAnim(LivePGE *pge) {
	debug(DBG_PGE, "Game::pge_setupAnim() pgeNum=%ld", pge - &_pgeLive[0]);
	const uint8_t *anim_data = _res.getAniData(pge->obj_type);
	if (_res._readUint16(anim_data) < pge->anim_seq) {
		pge->anim_seq = 0;
	}
	const uint8_t *anim_frame = anim_data + 6 + pge->anim_seq * 4;
	if (_res._readUint16(anim_frame) != 0xFFFF) {
		uint16_t fl = _res._readUint16(anim_frame);
		if (pge->flags & 1) {
			fl ^= 0x8000;
			pge->pos_x -= (int8_t)anim_frame[2];
		} else {
			pge->pos_x += (int8_t)anim_frame[2];
		}
		pge->pos_y += (int8_t)anim_frame[3];
		pge->flags &= ~2;
		if (fl & 0x8000) {
			pge->flags |= 2;
		}
		pge->flags &= ~8;
		if (_res._readUint16(anim_data + 4) & 0xFFFF) {
			pge->flags |= 8;
		}
		pge->anim_number = _res._readUint16(anim_frame) & 0x7FFF;
	}
}

int Game::pge_execute(LivePGE *live_pge, InitPGE *init_pge, const Object *obj) {
	debug(DBG_PGE, "Game::pge_execute() pge_num=%ld op1=0x%X op2=0x%X op3=0x%X", live_pge - &_pgeLive[0], obj->opcode1, obj->opcode2, obj->opcode3);
	pge_OpcodeProc op;
	ObjectOpcodeArgs args;
	if (obj->opcode1) {
		args.pge = live_pge;
		args.a = obj->opcode_arg1;
		args.b = 0;
		debug(DBG_PGE, "pge_execute op1=0x%X", obj->opcode1);
		op = _pge_opcodeTable[obj->opcode1];
		if (!op) {
			warning("Game::pge_execute() missing call to pge_opcode 0x%X", obj->opcode1);
			return 0;
		}
		if (!((this->*op)(&args) & 0xFF))
			return 0;
	}
	if (obj->opcode2) {
		args.pge = live_pge;
		args.a = obj->opcode_arg2;
		args.b = obj->opcode_arg1;
		debug(DBG_PGE, "pge_execute op2=0x%X", obj->opcode2);
		op = _pge_opcodeTable[obj->opcode2];
		if (!op) {
			warning("Game::pge_execute() missing call to pge_opcode 0x%X", obj->opcode2);
			return 0;
		}
		if (!((this->*op)(&args) & 0xFF))
			return 0;
	}
	if (obj->opcode3) {
		args.pge = live_pge;
		args.a = obj->opcode_arg3;
		args.b = 0;
		debug(DBG_PGE, "pge_execute op3=0x%X", obj->opcode3);
		op = _pge_opcodeTable[obj->opcode3];
		if (op) {
			(this->*op)(&args);
		} else {
			warning("Game::pge_execute() missing call to pge_opcode 0x%X", obj->opcode3);
		}
	}
	live_pge->obj_type = obj->init_obj_type;
	live_pge->first_obj_number = obj->init_obj_number;
	live_pge->anim_seq = 0;
	if (obj->flags & 0xF0) {
		_score += _scoreTable[obj->flags >> 4];
	}
	if (obj->flags & 1) {
		live_pge->flags ^= 1;
	}
	if (obj->flags & 2) {
		--live_pge->life;
		if (init_pge->object_type == 1) {
			_pge_processOBJ = true;
			if (_cheats & kCheatLifeCounter) {
				++live_pge->life;
			}
		} else if (init_pge->object_type == 10) {
			_score += 100;
			if (_cheats & kCheatOneHitKill) {
				live_pge->life = 0;
			}
		}
	}
	if (obj->flags & 4) {
		++live_pge->life;
	}
	if (obj->flags & 8) {
		live_pge->life = -1;
	}

	if (live_pge->flags & 1) {
		live_pge->pos_x -= obj->dx;
	} else {
		live_pge->pos_x += obj->dx;
	}
	live_pge->pos_y += obj->dy;

	if (_pge_processOBJ) {
		if (init_pge->object_type == 1) {
			if (pge_processOBJ(live_pge) != 0) {
				_blinkingConradCounter = 60;
				_pge_processOBJ = false;
			}
		}
	}
	return 0xFFFF;
}

void Game::pge_prepare() {
	col_clearState();
	if (!(_currentRoom & 0x80)) {
		LivePGE *pge = _pge_liveTable1[_currentRoom];
		while (pge) {
			col_preparePiegeState(pge);
			if (!(pge->flags & 4) && (pge->init_PGE->flags & 4)) {
				_pge_liveTable2[pge->index] = pge;
				pge->flags |= 4;
			}
			pge = pge->next_PGE_in_room;
		}
	}
	for (uint16_t i = 0; i < _res._pgeNum; ++i) {
		LivePGE *pge = _pge_liveTable2[i];
		if (pge && _currentRoom != pge->room_location) {
			col_preparePiegeState(pge);
		}
	}
}

void Game::pge_setupDefaultAnim(LivePGE *pge) {
	const uint8_t *anim_data = _res.getAniData(pge->obj_type);
	if (1 || pge->anim_seq < _res._readUint16(anim_data)) { /* matches disassembly but should probably be >= */
		pge->anim_seq = 0;
	}
	const uint8_t *anim_frame = anim_data + 6 + pge->anim_seq * 4;
	if (_res._readUint16(anim_frame) != 0xFFFF) {
		uint16_t f = _res._readUint16(anim_data);
		if (pge->flags & 1) {
			f ^= 0x8000;
		}
		pge->flags &= ~2;
		if (f & 0x8000) {
			pge->flags |= 2;
		}
		pge->flags &= ~8;
		if (_res._readUint16(anim_data + 4) & 0xFFFF) {
			pge->flags |= 8;
		}
		pge->anim_number = _res._readUint16(anim_frame) & 0x7FFF;
		debug(DBG_PGE, "Game::pge_setupDefaultAnim() pgeNum=%ld pge->flags=0x%X pge->anim_number=0x%X pge->anim_seq=0x%X", pge - &_pgeLive[0], pge->flags, pge->anim_number, pge->anim_seq);
	}
}

uint16_t Game::pge_processOBJ(LivePGE *pge) {
	InitPGE *init_pge = pge->init_PGE;
	assert(init_pge->obj_node_number < _res._numObjectNodes);
	ObjectNode *on = _res._objectNodesMap[init_pge->obj_node_number];
	Object *obj = &on->objects[pge->first_obj_number];
	int i = pge->first_obj_number;
	while (i < on->num_objects && pge->obj_type == obj->type) {
		if (obj->opcode2 == 0x6B) return 0xFFFF;
		if (obj->opcode2 == 0x22 && obj->opcode_arg2 <= 4) return 0xFFFF;

		if (obj->opcode1 == 0x6B) return 0xFFFF;
		if (obj->opcode1 == 0x22 && obj->opcode_arg1 <= 4) return 0xFFFF;

		++obj;
		++i;
	}
	return 0;
}

void Game::pge_setupOtherPieges(LivePGE *pge, InitPGE *init_pge) {
	const int8_t *room_ct_data = 0;
	if (pge->pos_x <= -10) {
		pge->pos_x += 256;
		room_ct_data = &_res._ctData[CT_LEFT_ROOM];
	} else if (pge->pos_x >= 256) {
		pge->pos_x -= 256;
		room_ct_data = &_res._ctData[CT_RIGHT_ROOM];
	} else if (pge->pos_y < 0) {
		pge->pos_y += 216;
		room_ct_data = &_res._ctData[CT_UP_ROOM];
	} else if (pge->pos_y >= 216) {
		pge->pos_y -= 216;
		room_ct_data = &_res._ctData[CT_DOWN_ROOM];
	}
	if (room_ct_data) {
		int8_t room = pge->room_location;
		if (room >= 0) {
			room = room_ct_data[room];
			pge->room_location = room;
		}
		if (init_pge->object_type == 1) {
			_currentRoom = room;
			col_prepareRoomState();
			_loadMap = true;
			if (_currentRoom < 0x40) {
				LivePGE *pge_it = _pge_liveTable1[_currentRoom];
				while (pge_it) {
					if (pge_it->init_PGE->flags & 4) {
						_pge_liveTable2[pge_it->index] = pge_it;
						pge_it->flags |= 4;
					}
					pge_it = pge_it->next_PGE_in_room;
				}
				room = _res._ctData[CT_UP_ROOM + _currentRoom];
				if (room >= 0 && room < 0x40) {
					pge_it = _pge_liveTable1[room];
					while (pge_it) {
						if (pge_it->init_PGE->object_type != 10 && pge_it->pos_y >= 48 && (pge_it->init_PGE->flags & 4)) {
							_pge_liveTable2[pge_it->index] = pge_it;
							pge_it->flags |= 4;
						}
						pge_it = pge_it->next_PGE_in_room;
					}
				}
				room = _res._ctData[CT_DOWN_ROOM + _currentRoom];
				if (room >= 0 && room < 0x40) {
					pge_it = _pge_liveTable1[room];
					while (pge_it) {
						if (pge_it->init_PGE->object_type != 10 && pge_it->pos_y >= 176 && (pge_it->init_PGE->flags & 4)) {
							_pge_liveTable2[pge_it->index] = pge_it;
							pge_it->flags |= 4;
						}
						pge_it = pge_it->next_PGE_in_room;
					}
				}
			}
		}
	}
	pge_addToCurrentRoomList(pge, _pge_currentPiegeRoom);
}

void Game::pge_addToCurrentRoomList(LivePGE *pge, uint8_t room) {
	debug(DBG_PGE, "Game::pge_addToCurrentRoomList() pgeNum=%ld room=%d", pge - &_pgeLive[0], room);
	if (room != pge->room_location) {
		LivePGE *cur_pge = _pge_liveTable1[room];
		LivePGE *prev_pge = 0;
		while (cur_pge && cur_pge != pge) {
			prev_pge = cur_pge;
			cur_pge = cur_pge->next_PGE_in_room;
		}
		if (cur_pge) {
			if (!prev_pge) {
				_pge_liveTable1[room] = pge->next_PGE_in_room;
			} else {
				prev_pge->next_PGE_in_room = cur_pge->next_PGE_in_room;
			}
			LivePGE *temp = _pge_liveTable1[pge->room_location];
			pge->next_PGE_in_room = temp;
			_pge_liveTable1[pge->room_location] = pge;
		}
	}
}

void Game::pge_getInput() {
	inp_update();
	_inp_lastKeysHit = _stub->_pi.dirMask;
	if ((_inp_lastKeysHit & 0xC) && (_inp_lastKeysHit & 0x3)) {
		const uint8_t mask = (_inp_lastKeysHit & 0xF0) | (_inp_lastKeysHitLeftRight & 0xF);
		_pge_inpKeysMask = mask;
		_inp_lastKeysHit = mask;
	} else {
		_pge_inpKeysMask = _inp_lastKeysHit;
		_inp_lastKeysHitLeftRight = _inp_lastKeysHit;
	}
	if (_stub->_pi.enter) {
		_pge_inpKeysMask |= 0x10;
	}
	if (_stub->_pi.space) {
		_pge_inpKeysMask |= 0x20;
	}
	if (_stub->_pi.shift) {
		_pge_inpKeysMask |= 0x40;
	}
}

int Game::pge_op_isInpUp(ObjectOpcodeArgs *args) {
	if (1 == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpBackward(ObjectOpcodeArgs *args) {
	uint8_t mask = 8; // right
	if (_pge_currentPiegeFacingDir) {
		mask = 4; // left
	}
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpDown(ObjectOpcodeArgs *args) {
	if (2 == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpForward(ObjectOpcodeArgs *args) {
	uint8_t mask = 4;
	if (_pge_currentPiegeFacingDir) {
		mask = 8;
	}
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpUpMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8_t mask = _pge_modKeysTable[args->a] | 1;
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpBackwardMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8_t mask = _pge_modKeysTable[args->a];
	if (_pge_currentPiegeFacingDir) {
		mask |= 4;
	} else {
		mask |= 8;
	}
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpDownMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8_t mask = _pge_modKeysTable[args->a] | 2;
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpForwardMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8_t mask = _pge_modKeysTable[args->a];
	if (_pge_currentPiegeFacingDir) {
		mask |= 8;
	} else {
		mask |= 4;
	}
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpIdle(ObjectOpcodeArgs *args) {
	if (_pge_inpKeysMask == 0) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_isInpNoMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8_t mask = _pge_modKeysTable[args->a];
	if (((_pge_inpKeysMask & 0xF) | mask) == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_getCollision0u(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 0, -args->a);
}

int Game::pge_op_getCollision00(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 0, 0);
}

int Game::pge_op_getCollision0d(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 0, args->a);
}

int Game::pge_op_getCollision1u(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 1, -args->a);
}

int Game::pge_op_getCollision10(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 1, 0);
}

int Game::pge_op_getCollision1d(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 1, args->a);
}

int Game::pge_op_getCollision2u(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 2, -args->a);
}

int Game::pge_op_getCollision20(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 2, 0);
}

int Game::pge_op_getCollision2d(ObjectOpcodeArgs *args) {
	return col_getGridData(args->pge, 2, args->a);
}

int Game::pge_op_doesNotCollide0u(ObjectOpcodeArgs *args) {
	int16_t r = col_getGridData(args->pge, 0, -args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide00(ObjectOpcodeArgs *args) {
	int16_t r = col_getGridData(args->pge, 0, 0);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide0d(ObjectOpcodeArgs *args) {
	int16_t r = col_getGridData(args->pge, 0, args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide1u(ObjectOpcodeArgs *args) {
	int16_t r = col_getGridData(args->pge, 1, -args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide10(ObjectOpcodeArgs *args) {
	int16_t r = col_getGridData(args->pge, 1, 0);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide1d(ObjectOpcodeArgs *args) {
	int16_t r = col_getGridData(args->pge, 1, args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide2u(ObjectOpcodeArgs *args) {
	int16_t r = col_getGridData(args->pge, 2, -args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide20(ObjectOpcodeArgs *args) {
	int16_t r = col_getGridData(args->pge, 2, 0);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_doesNotCollide2d(ObjectOpcodeArgs *args) {
	int16_t r = col_getGridData(args->pge, 2, args->a);
	if (r & 0xFFFF) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_op_collides0o0d(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 0, args->a) & 0xFFFF) {
		if (col_getGridData(args->pge, 0, args->a + 1) == 0) {
			if (col_getGridData(args->pge, -1, args->a) == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_op_collides2o2d(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 2, args->a) & 0xFFFF) {
		if (col_getGridData(args->pge, 2, args->a + 1) == 0) {
			if (col_getGridData(args->pge, 1, args->a) == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_op_collides0o0u(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 0, args->a) & 0xFFFF) {
		if (col_getGridData(args->pge, 0, args->a - 1) == 0) {
			if (col_getGridData(args->pge, -1, args->a) == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_op_collides2o2u(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 2, args->a) & 0xFFFF) {
		if (col_getGridData(args->pge, 2, args->a - 1) == 0) {
			if (col_getGridData(args->pge, 1, args->a) == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_op_collides2u2o(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 2, args->a - 1) & 0xFFFF) {
		if (col_getGridData(args->pge, 2, args->a) == 0) {
			if (col_getGridData(args->pge, 1, args->a - 1) == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_hasPiegeSentMessage(ObjectOpcodeArgs *args) {
	MessagePGE *le = _pge_messagesTable[args->pge->index];
	while (le) {
		if (le->msg_num == args->a) {
			return 0xFFFF;
		}
		le = le->next_entry;
	}
	return 0;
}

int Game::pge_op_sendMessageData0(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge_sendMessage(pge->index, pge->init_PGE->data[0], args->a);
	return 0xFFFF;
}

int Game::pge_op_sendMessageData1(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge_sendMessage(pge->index, pge->init_PGE->data[1], args->a);
	return 0xFFFF;
}

int Game::pge_op_sendMessageData2(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge_sendMessage(pge->index, pge->init_PGE->data[2], args->a);
	return 0xFFFF;
}

int Game::pge_op_sendMessageData3(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge_sendMessage(pge->index, pge->init_PGE->data[3], args->a);
	return 0xFFFF;
}

int Game::pge_op_isPiegeDead(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	if (pge->life <= 0) {
		if (pge->init_PGE->object_type == 10) {
			_score += 100;
		}
		return 1;
	}
	return 0;
}

int Game::pge_op_collides1u2o(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 1, args->a - 1) & 0xFFFF) {
		if (col_getGridData(args->pge, 2, args->a) == 0) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_op_collides1u1o(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 1, args->a - 1) & 0xFFFF) {
		if (col_getGridData(args->pge, 1, args->a) == 0) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_op_collides1o1u(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 1, args->a - 1) == 0) {
		if (col_getGridData(args->pge, 1, args->a) & 0xFFFF) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_o_unk0x2B(ObjectOpcodeArgs *args) {
	return pge_ZOrder(args->pge, args->a, &Game::pge_ZOrderIfTypeAndDifferentDirection, 0);
}

int Game::pge_o_unk0x2C(ObjectOpcodeArgs *args) {
	return pge_ZOrder(args->pge, args->a, &Game::pge_ZOrderIfTypeAndSameDirection, 0);
}

int Game::pge_o_unk0x2D(ObjectOpcodeArgs *args) {
	return pge_ZOrder(args->pge, args->a, &Game::pge_ZOrderByObj, 0) ^ 1;
}

int Game::pge_op_nop(ObjectOpcodeArgs *args) {
	return 1;
}

int Game::pge_op_pickupObject(ObjectOpcodeArgs *args) {
	LivePGE *pge = col_findPiege(args->pge, 3);
	if (pge) {
		pge_sendMessage(args->pge->index, pge->index, args->a);
		return 0xFFFF;
	}
	return 0;
}

int Game::pge_op_addItemToInventory(ObjectOpcodeArgs *args) {
	pge_updateInventory(&_pgeLive[args->a], args->pge);
	args->pge->room_location = 0xFF;
	return 0xFFFF;
}

int Game::pge_op_copyPiege(ObjectOpcodeArgs *args) {
	LivePGE *src = &_pgeLive[args->a];
	LivePGE *dst = args->pge;

	dst->pos_x = src->pos_x;
	dst->pos_y = src->pos_y;
	dst->room_location = src->room_location;

	dst->flags &= ~1;
	if (src->flags & 1) {
		dst->flags |= 1;
	}
	pge_reorderInventory(args->pge);
	return 0xFFFF;
}

int Game::pge_op_removeItemFromInventory(ObjectOpcodeArgs *args) {
	if (args->pge->current_inventory_PGE != 0xFF) {
		pge_sendMessage(args->pge->index, args->pge->current_inventory_PGE, args->a);
	}
	return 1;
}

int Game::pge_op_canUseCurrentInventoryItem(ObjectOpcodeArgs *args) {
	LivePGE *pge = &_pgeLive[0];
	if (pge->current_inventory_PGE != 0xFF && _res._pgeInit[pge->current_inventory_PGE].object_id == args->a) {
		return 1;
	}
	return 0;
}

// useObject related
int Game::pge_o_unk0x34(ObjectOpcodeArgs *args) {
	uint8_t mask = (_pge_inpKeysMask & 0xF) | _pge_modKeysTable[0];
	if (mask == _pge_inpKeysMask) {
		if (col_getGridData(args->pge, 2, -args->a) == 0) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_op_isInpMod(ObjectOpcodeArgs *args) {
	assert(args->a < 3);
	uint8_t mask = _pge_modKeysTable[args->a];
	if (mask == _pge_inpKeysMask) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_op_setCollisionState1(ObjectOpcodeArgs *args) {
	return pge_updateCollisionState(args->pge, args->a, 1);
}

int Game::pge_op_setCollisionState0(ObjectOpcodeArgs *args) {
	return pge_updateCollisionState(args->pge, args->a, 0);
}

int Game::pge_hasMessageData0(ObjectOpcodeArgs *args) {
	return pge_hasMessageData(args->pge, args->a, 1);
}

int Game::pge_hasMessageData1(ObjectOpcodeArgs *args) {
	return pge_hasMessageData(args->pge, args->a, 2);
}

int Game::pge_hasMessageData2(ObjectOpcodeArgs *args) {
	return pge_hasMessageData(args->pge, args->a, 3);
}

int Game::pge_hasMessageData3(ObjectOpcodeArgs *args) {
	return pge_hasMessageData(args->pge, args->a, 4);
}

int Game::pge_o_unk0x3C(ObjectOpcodeArgs *args) {
	return pge_ZOrder(args->pge, args->a, &Game::pge_ZOrderByAnimYIfType, args->b);
}

int Game::pge_o_unk0x3D(ObjectOpcodeArgs *args) {
	return pge_ZOrder(args->pge, args->a, &Game::pge_ZOrderByAnimY, 0);
}

int Game::pge_op_setPiegeCounter(ObjectOpcodeArgs *args) {
	args->pge->counter_value = args->a;
	return 1;
}

int Game::pge_op_decPiegeCounter(ObjectOpcodeArgs *args) {
	args->pge->counter_value -= 1;
	if (args->a == args->pge->counter_value) {
		return 0xFFFF;
	} else {
		return 0;
	}
}

int Game::pge_o_unk0x40(ObjectOpcodeArgs *args) {
	int8_t pge_room = args->pge->room_location;
	if (pge_room < 0 || pge_room >= 0x40) return 0;
	int col_area;
	if (_currentRoom == pge_room) {
		col_area = 1;
	} else if (_col_currentLeftRoom == pge_room) {
		col_area = 0;
	} else if (_col_currentRightRoom == pge_room) {
		col_area = 2;
	} else {
		return 0;
	}
	int16_t grid_pos_x = (args->pge->pos_x + 8) >> 4;
	int16_t grid_pos_y = args->pge->pos_y / 72;
	if (grid_pos_y >= 0 && grid_pos_y <= 2) {
		grid_pos_y *= 16;
		int16_t _cx = args->a;
		if (_pge_currentPiegeFacingDir) {
			_cx = -_cx;
		}
		int8_t _bl;
		if (_cx >= 0) {
			if (_cx > 0x10) {
				_cx = 0x10;
			}
			int8_t *var2 = &_res._ctData[0x100] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + grid_pos_x;
			uint8_t *var4 = _col_activeCollisionSlots + col_area * 0x30 + grid_pos_y + grid_pos_x;
			int16_t var12 = grid_pos_x;
			--_cx;
			do {
				--var12;
				if (var12 < 0) {
					--col_area;
					if (col_area < 0) return 0;
					pge_room = _res._ctData[CT_LEFT_ROOM + pge_room];
					if (pge_room < 0) return 0;
					var12 = 15;
					var2 = &_res._ctData[0x101] + pge_room * 0x70 + grid_pos_y * 2 + 15 + 0x10;
					var4 = var4 - 31;
				}
				--var4;
				_bl = *var4;
				if (_bl >= 0) {
					CollisionSlot *col_slot = _col_slotsTable[_bl];
					do {
						if (args->pge != col_slot->live_pge && (col_slot->live_pge->flags & 4)) {
							if (col_slot->live_pge->init_PGE->object_type == args->b) {
								return 1;
							}
						}
						col_slot = col_slot->prev_slot;
					} while (col_slot);
				}
				--var2;
				if (*var2 != 0) return 0;
				--_cx;
			} while (_cx >= 0);
		} else {
			_cx = -_cx;
			if (_cx > 0x10) {
				_cx = 0x10;
			}
			int8_t *var2 = &_res._ctData[0x101] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + grid_pos_x;
			uint8_t *var4 = _col_activeCollisionSlots + 1 + col_area * 0x30 + grid_pos_y + grid_pos_x;
			int16_t var12 = grid_pos_x;
			--_cx;
			do {
				++var12;
				if (var12 == 0x10) {
					++col_area;
					if (col_area > 2) return 0;
					pge_room = _res._ctData[CT_RIGHT_ROOM + pge_room];
					if (pge_room < 0) return 0;

					var12 = 0;
					var2 = &_res._ctData[0x101] + pge_room * 0x70 + grid_pos_y * 2 + 0x10;
					var4 += 32;
				}
				var4++;
				_bl = *var4;
				if (_bl >= 0) {
					CollisionSlot *col_slot = _col_slotsTable[_bl];
					do {
						if (args->pge != col_slot->live_pge && (col_slot->live_pge->flags & 4)) {
							if (col_slot->live_pge->init_PGE->object_type == args->b) {
								return 1;
							}
						}
						col_slot = col_slot->prev_slot;
					} while (col_slot);
				}
				_bl = *var2;
				++var2;
				if (_bl != 0) return 0;
				--_cx;
			} while (_cx >= 0);
		}
	}
	return 0;
}

int Game::pge_op_wakeUpPiege(ObjectOpcodeArgs *args) {
	if (args->a <= 3) {
		int16_t num = args->pge->init_PGE->data[args->a];
		if (num >= 0) {
			LivePGE *pge = &_pgeLive[num];
			pge->flags |= 4;
			_pge_liveTable2[num] = pge;
		}
	}
	return 1;
}

int Game::pge_op_removePiege(ObjectOpcodeArgs *args) {
	if (args->a <= 3) {
		int16_t num = args->pge->init_PGE->data[args->a];
		if (num >= 0) {
			_pge_liveTable2[num] = 0;
			_pgeLive[num].flags &= ~4;
		}
	}
	return 1;
}

int Game::pge_op_removePiegeIfNotNear(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	if (!(pge->init_PGE->flags & 4)) goto kill_pge;
	if (_currentRoom & 0x80) goto skip_pge;
	if (!(pge->room_location < 0x40)) goto kill_pge;
	if (pge->room_location == _currentRoom) goto skip_pge;
	if (pge->room_location == _res._ctData[CT_UP_ROOM + _currentRoom]) goto skip_pge;
	if (pge->room_location == _res._ctData[CT_DOWN_ROOM + _currentRoom]) goto skip_pge;
	if (pge->room_location == _res._ctData[CT_RIGHT_ROOM + _currentRoom]) goto skip_pge;
	if (pge->room_location == _res._ctData[CT_LEFT_ROOM + _currentRoom]) goto skip_pge;

kill_pge:
	pge->flags &= ~4;
	pge->collision_slot = 0xFF;
	_pge_liveTable2[pge->index] = 0;

skip_pge:
	_pge_playAnimSound = false;
	return 1;
}

int Game::pge_op_loadPiegeCounter(ObjectOpcodeArgs *args) {
	args->pge->counter_value = args->pge->init_PGE->data[args->a];
	return 1;
}

int Game::pge_o_unk0x45(ObjectOpcodeArgs *args) {
	return pge_ZOrder(args->pge, args->a, &Game::pge_ZOrderByNumber, 0);
}

int Game::pge_o_unk0x46(ObjectOpcodeArgs *args) {
	_pge_compareVar1 = 0;
	pge_ZOrder(args->pge, args->a, &Game::pge_ZOrderIfDifferentDirection, 0);
	return _pge_compareVar1;
}

int Game::pge_o_unk0x47(ObjectOpcodeArgs *args) {
	_pge_compareVar2 = 0;
	pge_ZOrder(args->pge, args->a, &Game::pge_ZOrderIfSameDirection, 0);
	return _pge_compareVar2;
}

// used with Ian in level2
int Game::pge_o_unk0x48(ObjectOpcodeArgs *args) {
	LivePGE *pge = col_findPiege(&_pgeLive[0], args->pge->init_PGE->data[0]);
	if (pge && pge->life == args->pge->life) {
		pge_sendMessage(args->pge->index, pge->index, args->a);
		return 1;
	}
	return 0;
}

int Game::pge_o_unk0x49(ObjectOpcodeArgs *args) {
	return pge_ZOrder(&_pgeLive[0], args->a, &Game::pge_ZOrderIfIndex, args->pge->init_PGE->data[0]);
}

int Game::pge_op_killInventoryPiege(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge->room_location = 0xFE;
	pge->flags &= ~4;
	_pge_liveTable2[pge->index] = 0;
	LivePGE *inv_pge = pge_getPreviousInventoryItem(&_pgeLive[args->a], pge);
	if (inv_pge == &_pgeLive[args->a]) {
		if (pge->index != inv_pge->current_inventory_PGE) {
			return 1;
		}
	} else {
		if (pge->index != inv_pge->next_inventory_PGE) {
			return 1;
		}
	}
	pge_removeFromInventory(inv_pge, pge, &_pgeLive[args->a]);
	return 1;
}

int Game::pge_op_killPiege(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge->room_location = 0xFE;
	pge->flags &= ~4;
	_pge_liveTable2[pge->index] = 0;
	if (pge->init_PGE->object_type == 10) {
		_score += 200;
	}
	return 0xFFFF;
}

int Game::pge_op_isInCurrentRoom(ObjectOpcodeArgs *args) {
	return (args->pge->room_location == _currentRoom) ? 1 : 0;
}

int Game::pge_op_isNotInCurrentRoom(ObjectOpcodeArgs *args) {
	return (args->pge->room_location == _currentRoom) ? 0 : 1;
}

int Game::pge_op_scrollPosY(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	args->pge->pos_y += args->a;
	uint8_t pge_num = pge->current_inventory_PGE;
	while (pge_num != 0xFF) {
		pge = &_pgeLive[pge_num];
		pge->pos_y += args->a;
		pge_num = pge->next_inventory_PGE;
	}
	return 1;
}

int Game::pge_op_playDefaultDeathCutscene(ObjectOpcodeArgs *args) {
	if (_deathCutsceneCounter == 0) {
		_deathCutsceneCounter = args->a;
	}
	return 1;
}

int Game::pge_o_unk0x50(ObjectOpcodeArgs *args) {
	return pge_ZOrder(args->pge, args->a, &Game::pge_ZOrderByObj, 0);
}

int Game::pge_o_unk0x52(ObjectOpcodeArgs *args) {
	return col_detectHit(args->pge, args->a, args->b, &Game::col_detectHitCallback4, &Game::col_detectHitCallback1, 0, 0);
}

int Game::pge_o_unk0x53(ObjectOpcodeArgs *args) {
	return col_detectHit(args->pge, args->a, args->b, &Game::col_detectHitCallback5, &Game::col_detectHitCallback1, 0, 0);
}

int Game::pge_op_isPiegeNear(ObjectOpcodeArgs *args) {
	if (col_findPiege(&_pgeLive[0], args->a) != 0) {
		return 1;
	}
	return 0;
}

int Game::pge_op_setLife(ObjectOpcodeArgs *args) {
	args->pge->life = args->a;
	return 1;
}

int Game::pge_op_incLife(ObjectOpcodeArgs *args) {
	args->pge->life += args->a;
	return 1;
}

int Game::pge_op_setPiegeDefaultAnim(ObjectOpcodeArgs *args) {
	assert(args->a >= 0 && args->a < 4);
	InitPGE *init_pge = args->pge->init_PGE;
	args->pge->room_location = init_pge->data[args->a];
	if (init_pge->object_type == 1) {
		_loadMap = true;
	}
	pge_setupDefaultAnim(args->pge);
	return 1;
}

int Game::pge_op_setLifeCounter(ObjectOpcodeArgs *args) {
	_pgeLive[args->a].life = args->pge->init_PGE->data[0];
	return 1;
}

int Game::pge_op_decLifeCounter(ObjectOpcodeArgs *args) {
	args->pge->life = _pgeLive[args->a].life - 1;
	return 1;
}

int Game::pge_op_playCutscene(ObjectOpcodeArgs *args) {
	if (_deathCutsceneCounter == 0) {
		_cut._id = args->a;
	}
	return 1;
}

// unused
int Game::pge_op_compareUnkVar(ObjectOpcodeArgs *args) {
	if (args->a == -1) {
		return 1;
	}
	return 0;
}

int Game::pge_op_playDeathCutscene(ObjectOpcodeArgs *args) {
	if (_deathCutsceneCounter == 0) {
		_deathCutsceneCounter = args->pge->init_PGE->data[3] + 1;
		_cut._deathCutsceneId = args->a;
	}
	return 1;
}

int Game::pge_o_unk0x5D(ObjectOpcodeArgs *args) {
	return col_detectHit(args->pge, args->a, args->b, &Game::col_detectHitCallback4, &Game::col_detectHitCallback6, 0, 0);
}

int Game::pge_o_unk0x5E(ObjectOpcodeArgs *args) {
	return col_detectHit(args->pge, args->a, args->b, &Game::col_detectHitCallback5, &Game::col_detectHitCallback6, 0, 0);
}

int Game::pge_o_unk0x5F(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;

	int8_t pge_room = pge->room_location;
	if (pge_room < 0 || pge_room >= 0x40) return 0;

	int16_t dx;
	int16_t _cx = pge->init_PGE->data[0];
	if (_cx <= 0) {
		dx = 1;
		_cx = -_cx;
	} else {
		dx = -1;
	}
	if (_pge_currentPiegeFacingDir) {
		dx = -dx;
	}
	int16_t grid_pos_x = (pge->pos_x + 8) >> 4;
	int16_t grid_pos_y = 0;
	do {
		int16_t _ax = col_getGridData(pge, 1, -grid_pos_y);
		if (_ax != 0) {
			if (!(_ax & 2) || args->a != 1) {
				pge->room_location = pge_room;
				pge->pos_x = grid_pos_x * 16;
				return 1;
			}
		}
		if (grid_pos_x < 0) {
			pge_room = _res._ctData[CT_LEFT_ROOM + pge_room];
			if (pge_room < 0 || pge_room >= 0x40) return 0;
			grid_pos_x += 16;
		} else if (grid_pos_x > 15) {
			pge_room = _res._ctData[CT_RIGHT_ROOM + pge_room];
			if (pge_room < 0 || pge_room >= 0x40) return 0;
			grid_pos_x -= 16;
		}
		grid_pos_x += dx;
		++grid_pos_y;
	} while (grid_pos_y <= _cx);
	return 0;
}

int Game::pge_op_findAndCopyPiege(ObjectOpcodeArgs *args) {
	MessagePGE *le = _pge_messagesTable[args->pge->index];
	while (le) {
		if (le->msg_num == args->a) {
			args->a = le->src_pge;
			args->b = 0;
			pge_op_copyPiege(args);
			return 1;
		}
		le = le->next_entry;
	}
	return 0;
}

int Game::pge_op_isInRandomRange(ObjectOpcodeArgs *args) {
	uint16_t n = args->a;
	if (n != 0) {
		if ((getRandomNumber() % n) == 0) {
			return 1;
		}
	}
	return 0;
}

int Game::pge_o_unk0x62(ObjectOpcodeArgs *args) {
	return col_detectHit(args->pge, args->a, args->b, &Game::col_detectHitCallback3, &Game::col_detectHitCallback1, 0, -1);
}

int Game::pge_o_unk0x63(ObjectOpcodeArgs *args) {
	return col_detectHit(args->pge, args->a, args->b, &Game::col_detectHitCallback2, &Game::col_detectHitCallback1, 0, -1);
}

int Game::pge_o_unk0x64(ObjectOpcodeArgs *args) {
	return col_detectGunHit(args->pge, args->a, args->b, &Game::col_detectGunHitCallback3, &Game::col_detectGunHitCallback1, 1, -1);
}

int Game::pge_op_addToCredits(ObjectOpcodeArgs *args) {
	assert(args->a >= 0 && args->a < 3);
	uint8_t pge = args->pge->init_PGE->data[args->a];
	int16_t val = args->pge->init_PGE->data[args->a + 1];
	_pgeLive[pge].life += val;
	return 1;
}

int Game::pge_op_subFromCredits(ObjectOpcodeArgs *args) {
	assert(args->a >= 0 && args->a < 3);
	uint8_t pge = args->pge->init_PGE->data[args->a];
	int16_t val = args->pge->init_PGE->data[args->a + 1];
	_pgeLive[pge].life -= val;
	return 1;
}

int Game::pge_o_unk0x67(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 1, -args->a) & 2) {
		return 0xFFFF;
	}
	return 0;
}

int Game::pge_op_setCollisionState2(ObjectOpcodeArgs *args) {
	return pge_updateCollisionState(args->pge, args->a, 2);
}

int Game::pge_op_saveState(ObjectOpcodeArgs *args) {
	_saveStateCompleted = true;
	_validSaveState = saveGameState(kIngameSaveSlot);
	if (_validSaveState && g_options.play_gamesaved_sound) {
		_mix.play(Resource::_gameSavedSoundData, Resource::_gameSavedSoundLen, 8000, Mixer::MAX_VOLUME);
	}
	return 0xFFFF;
}

// useGun related
int Game::pge_o_unk0x6A(ObjectOpcodeArgs *args) {
	LivePGE *_si = args->pge;
	int8_t pge_room = _si->room_location;
	if (pge_room < 0 || pge_room >= 0x40) return 0;
	int8_t _bl;
	int col_area = 0;
	int8_t *ct_data;
	if (_currentRoom == pge_room) {
		col_area = 1;
	} else if (_col_currentLeftRoom == pge_room) {
		col_area = 0;
	} else if (_col_currentRightRoom == pge_room) {
		col_area = 2;
	} else {
		return 0;
	}
	int16_t grid_pos_x = (_si->pos_x + 8) >> 4;
	int16_t grid_pos_y = (_si->pos_y / 72);
	if (grid_pos_y >= 0 && grid_pos_y <= 2) {
		grid_pos_y *= 16;
		int16_t _cx = args->a;
		if (_pge_currentPiegeFacingDir) {
			_cx = -_cx;
		}
		if (_cx >= 0) {
			if (_cx > 0x10) {
				_cx = 0x10;
			}
			ct_data = &_res._ctData[0x100] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + grid_pos_x;
			uint8_t *var4 = _col_activeCollisionSlots + col_area * 0x30 + grid_pos_y + grid_pos_x;
			++var4;
			++ct_data;
			int16_t varA = grid_pos_x;
			do {
				--varA;
				if (varA < 0) {
					--col_area;
					if (col_area < 0) return 0;
					pge_room = _res._ctData[CT_LEFT_ROOM + pge_room];
					if (pge_room < 0) return 0;
					varA = 0xF;
					ct_data = &_res._ctData[0x101] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + varA;
					var4 -= 0x1F;
				}
				--var4;
				_bl = *var4;
				if (_bl >= 0) {
					CollisionSlot *collision_slot = _col_slotsTable[_bl];
					do {
						_si = collision_slot->live_pge;
						if (args->pge != _si && (_si->flags & 4) && _si->life >= 0) {
							if (_si->init_PGE->object_type == 1 || _si->init_PGE->object_type == 10) {
								return 1;
							}
						}
						collision_slot = collision_slot->prev_slot;
					} while (collision_slot);
				}
				--ct_data;
				if (*ct_data != 0) return 0;
				--_cx;
			} while (_cx >= 0);

		} else {
			_cx = -_cx;
			if (_cx > 0x10) {
				_cx = 0x10;
			}
			ct_data = &_res._ctData[0x101] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + grid_pos_x;
			uint8_t *var4 = _col_activeCollisionSlots + 1 + col_area * 0x30 + grid_pos_y + grid_pos_x;
			int16_t varA = grid_pos_x;
			goto loc_0_15446;
			do {
				++varA;
				if (varA == 0x10) {
					++col_area;
					if (col_area > 2) return 0;
					pge_room = _res._ctData[CT_RIGHT_ROOM + pge_room];
					if (pge_room < 0) return 0;
					varA = 0;
					ct_data = &_res._ctData[0x100] + pge_room * 0x70 + grid_pos_y * 2 + 0x10 + varA;
					var4 += 0x20;
				}
loc_0_15446:
				_bl = *var4;
				++var4;
				if (_bl >= 0) {
					CollisionSlot *collision_slot = _col_slotsTable[_bl];
					do {
						_si = collision_slot->live_pge;
						if (args->pge != _si && (_si->flags & 4) && _si->life >= 0) {
							if (_si->init_PGE->object_type == 1 || _si->init_PGE->object_type == 10) {
								return 1;
							}
						}
						collision_slot = collision_slot->prev_slot;
					} while (collision_slot);
				}
				_bl = *ct_data;
				++ct_data;
				if (_bl != 0) return 0;
				--_cx;
			} while (_cx >= 0);
		}
	}
	return 0;
}

int Game::pge_op_isMessageReceived(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	MessagePGE *le = _pge_messagesTable[pge->index];
	if (le) {
		if (args->a == 0) {
			do {
				if (le->msg_num == 1 || le->msg_num == 2) {
					return 1;
				}
				le = le->next_entry;
			} while (le);
		} else {
			do {
				if (le->msg_num == 3 || le->msg_num == 4) {
					return 1;
				}
				le = le->next_entry;
			} while (le);
		}
	}
	return 0;
}

int Game::pge_o_unk0x6C(ObjectOpcodeArgs *args) {
	LivePGE *pge = col_findPiege(&_pgeLive[0], args->pge->init_PGE->data[0]);
	if (pge) {
		if (pge->life <= args->pge->life) {
			pge_sendMessage(args->pge->index, pge->index, args->a);
			return 1;
		}
	}
	return 0;
}

int Game::pge_op_isCollidingObject(ObjectOpcodeArgs *args) {
	uint8_t r = col_findCurrentCollidingObject(args->pge, 3, 0xFF, 0xFF, 0);
	if (r == args->a) {
		return 1;
	} else {
		return 0;
	}
}

// elevator
int Game::pge_o_unk0x6E(ObjectOpcodeArgs *args) {
	MessagePGE *le = _pge_messagesTable[args->pge->index];
	while (le) {
		if (args->a == le->msg_num) {
			pge_updateInventory(&_pgeLive[le->src_pge], args->pge);
			return 0xFFFF;
		}
		le = le->next_entry;
	}
	return 0;
}


int Game::pge_o_unk0x6F(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	MessagePGE *le = _pge_messagesTable[pge->index];
	while (le) {
		if (args->a == le->msg_num) {
			pge_sendMessage(pge->index, le->src_pge, 0xC);
			return 1;
		}
		le = le->next_entry;
	}
	return 0;
}

int Game::pge_o_unk0x70(ObjectOpcodeArgs *args) {
	uint8_t pge_num = args->pge->current_inventory_PGE;
	while (pge_num != 0xFF) {
		pge_sendMessage(args->pge->index, _pgeLive[pge_num].index, args->a);
		pge_num = _pgeLive[pge_num].next_inventory_PGE;
	}
	return 1;
}

// elevator
int Game::pge_o_unk0x71(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	MessagePGE *le = _pge_messagesTable[pge->index];
	while (le) {
		if (le->msg_num == args->a) {
			pge_reorderInventory(args->pge);
			return 1;
		}
		le = le->next_entry;
	}
	return 0;
}

int Game::pge_o_unk0x72(ObjectOpcodeArgs *args) {
	int8_t *grid_data = &_res._ctData[0x100] + args->pge->room_location * 0x70;
	int16_t pge_pos_y = ((args->pge->pos_y / 36) & ~1) + args->a;
	int16_t pge_pos_x = (args->pge->pos_x + 8) >> 4;
	grid_data += pge_pos_y * 16 + pge_pos_x;

	CollisionSlot2 *_di = _col_slots2Next;
	int count = 256; // ARRAYSIZE(_col_slots2)
	while (_di && count != 0) {
		if (_di->unk2 != grid_data) {
			_di = _di->next_slot;
			--count;
		} else {
			memcpy(_di->unk2, _di->data_buf, _di->data_size + 1);
			break;
		}
	}
	// original returns the pointer to ctData
	return 0xFFFF;
}

int Game::pge_o_unk0x73(ObjectOpcodeArgs *args) {
	LivePGE *pge = col_findPiege(args->pge, args->a);
	if (pge != 0) {
		pge_updateInventory(pge, args->pge);
		return 0xFFFF;
	}
	return 0;
}

int Game::pge_op_collides4u(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 4, -args->a) != 0) {
		return 0xFFFF;
	}
	return 0;
}

int Game::pge_op_doesNotCollide4u(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 4, -args->a) == 0) {
		return 0xFFFF;
	}
	return 0;
}

int Game::pge_op_isBelowConrad(ObjectOpcodeArgs *args) {
	LivePGE *_si = args->pge;
	LivePGE *pge_conrad = &_pgeLive[0];
	if (pge_conrad->room_location == _si->room_location) {
		if ((pge_conrad->pos_y - 8) / 72 < _si->pos_y / 72) {
			return 0xFFFF;
		}
	} else if (_si->room_location < 0x40) {
		if (pge_conrad->room_location == _res._ctData[CT_UP_ROOM + _si->room_location]) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_op_isAboveConrad(ObjectOpcodeArgs *args) {
	LivePGE *_si = args->pge;
	LivePGE *pge_conrad = &_pgeLive[0];
	if (pge_conrad->room_location == _si->room_location) {
		if ((pge_conrad->pos_y - 8) / 72 > _si->pos_y / 72) {
			return 0xFFFF;
		}
	} else if (_si->room_location < 0x40) {
		if (pge_conrad->room_location == _res._ctData[CT_DOWN_ROOM + _si->room_location]) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_op_isNotFacingConrad(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	LivePGE *pge_conrad = &_pgeLive[0];
	if (pge->pos_y / 72 == (pge_conrad->pos_y - 8) / 72) { // same grid cell
		if (pge->room_location == pge_conrad->room_location) {
			if (args->a == 0) {
				if (_pge_currentPiegeFacingDir) {
					if (pge->pos_x < pge_conrad->pos_x) {
						return 0xFFFF;
					}
				} else {
					if (pge->pos_x > pge_conrad->pos_x) {
						return 0xFFFF;
					}
				}
			} else {
				int16_t dx;
				if (_pge_currentPiegeFacingDir) {
					dx = pge_conrad->pos_x - pge->pos_x;
				} else {
					dx = pge->pos_x - pge_conrad->pos_x;
				}
				if (dx > 0 && dx < args->a * 16) {
					return 0xFFFF;
				}
			}
		} else if (args->a == 0) {
			if (pge->room_location < 0x40) {
				if (_pge_currentPiegeFacingDir) {
					if (pge_conrad->room_location == _res._ctData[CT_RIGHT_ROOM + pge->room_location])
						return 0xFFFF;
				} else {
					if (pge_conrad->room_location == _res._ctData[CT_LEFT_ROOM + pge->room_location])
						return 0xFFFF;
				}
			}
		}
	}
	return 0;
}

int Game::pge_op_isFacingConrad(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	LivePGE *pge_conrad = &_pgeLive[0];
	if (pge->pos_y / 72 == (pge_conrad->pos_y - 8) / 72) {
		if (pge->room_location == pge_conrad->room_location) {
			if (args->a == 0) {
				if (_pge_currentPiegeFacingDir) {
					if (pge->pos_x > pge_conrad->pos_x) {
						return 0xFFFF;
					}
				} else {
					if (pge->pos_x <= pge_conrad->pos_x) {
						return 0xFFFF;
					}
				}
			} else {
				int16_t dx;
				if (_pge_currentPiegeFacingDir) {
					dx = pge->pos_x - pge_conrad->pos_x;
				} else {
					dx = pge_conrad->pos_x - pge->pos_x;
				}
				if (dx > 0 && dx < args->a * 16) {
					return 0xFFFF;
				}
			}
		} else if (args->a == 0) {
			if (pge->room_location < 0x40) {
				if (_pge_currentPiegeFacingDir) {
					if (pge_conrad->room_location == _res._ctData[CT_LEFT_ROOM + pge->room_location])
						return 0xFFFF;
				} else {
					if (pge_conrad->room_location == _res._ctData[CT_RIGHT_ROOM + pge->room_location])
						return 0xFFFF;
				}
			}
		}
	}
	return 0;
}

int Game::pge_op_collides2u1u(ObjectOpcodeArgs *args) {
	if (col_getGridData(args->pge, 1, -args->a) == 0) {
		if (col_getGridData(args->pge, 2, -(args->a + 1)) & 0xFFFF) {
			return 0xFFFF;
		}
	}
	return 0;
}

int Game::pge_op_displayText(ObjectOpcodeArgs *args) {
	_textToDisplay = args->a;
	return 0xFFFF;
}

int Game::pge_o_unk0x7C(ObjectOpcodeArgs *args) {
	LivePGE *pge = col_findPiege(args->pge, 3);
	if (pge == 0) {
		pge = col_findPiege(args->pge, 5);
		if (pge == 0) {
			pge = col_findPiege(args->pge, 9);
			if (pge == 0) {
				pge = col_findPiege(args->pge, 0xFFFF);
			}
		}
	}
	if (pge != 0) {
		pge_sendMessage(args->pge->index, pge->index, args->a);
	}
	return 0;
}

int Game::pge_op_playSound(ObjectOpcodeArgs *args) {
	uint8_t sfxId = args->a & 0xFF;
	uint8_t softVol = args->a >> 8;
	playSound(sfxId, softVol);
	return 0xFFFF;
}

int Game::pge_o_unk0x7E(ObjectOpcodeArgs *args) {
	_pge_compareVar1 = 0;
	pge_ZOrder(args->pge, args->a, &Game::pge_ZOrderByIndex, 0);
	return _pge_compareVar1;
}

int Game::pge_o_unk0x7F(ObjectOpcodeArgs *args) {
	LivePGE *_si = args->pge;
	uint8_t var4 = _si->collision_slot;
	uint8_t var2 = _si->index;
	while (var4 != 0xFF) {
		CollisionSlot *slot = _col_slotsTable[var4];
		while (slot) {
			if (slot->live_pge != args->pge) {
				if (slot->live_pge->init_PGE->object_type == 3 && var2 != slot->live_pge->ref_inventory_PGE) {
					return 0;
				}
			}
			if (slot->live_pge == args->pge) {
				var4 = slot->index;
			}
			slot = slot->prev_slot;
		}
	}
	return 0xFFFF;
}

int Game::pge_op_setPiegePosX(ObjectOpcodeArgs *args) {
	uint8_t pge_num = args->pge->ref_inventory_PGE;
	if (pge_num != 0xFF) {
		args->pge->pos_x = _pgeLive[pge_num].pos_x;
	}
	return 0xFFFF;
}

int Game::pge_op_setPiegePosModX(ObjectOpcodeArgs *args) {
	uint8_t pge_num = args->pge->ref_inventory_PGE;
	if (pge_num != 0xFF) {
		int16_t dx = _pgeLive[pge_num].pos_x % 256;
		if (dx >= args->pge->pos_x) {
			dx -= args->pge->pos_x;
		}
		args->pge->pos_x += dx;
	}
	return 0xFFFF;
}

// taxi and teleporter
int Game::pge_op_changeRoom(ObjectOpcodeArgs *args) {
	// pge_op_protectionScreen
	InitPGE *init_pge_1 = args->pge->init_PGE;
	const int16_t _ax = init_pge_1->data[args->a];
	if (_ax == 0 && !g_options.bypass_protection && !g_options.use_words_protection && g_features->has_copy_protection) {
		if (!handleProtectionScreenShape()) {
			// _pge_opcodeTable[0x82] = &Game::pge_op_nop;
			// _pge_opGunVar = 0;
			// return;
		}
	}
	// pge_op_changeRoom
	const int16_t _bx = init_pge_1->data[args->a + 1];
	LivePGE *live_pge_1 = &_pgeLive[_bx];
	LivePGE *live_pge_2 = &_pgeLive[_ax];
	int8_t pge_room = live_pge_1->room_location;
	if (pge_room >= 0 && pge_room < 0x40) {
		const int8_t room = live_pge_2->room_location;
		live_pge_2->pos_x = live_pge_1->pos_x;
		live_pge_2->pos_y = live_pge_1->pos_y;
		live_pge_2->room_location = live_pge_1->room_location;
		pge_addToCurrentRoomList(live_pge_2, room);
		InitPGE *init_pge_2 = live_pge_2->init_PGE;
		init_pge_1 = live_pge_1->init_PGE;
		if (init_pge_2->obj_node_number == init_pge_1->obj_node_number) {
			live_pge_2->flags &= ~1;
			if (live_pge_1->flags & 1) {
				live_pge_2->flags |= 1;
			}
			live_pge_2->obj_type = live_pge_1->obj_type;
			live_pge_2->anim_seq = 0;
			assert(init_pge_2->obj_node_number < _res._numObjectNodes);
			ObjectNode *on = _res._objectNodesMap[init_pge_2->obj_node_number];
			Object *obj = on->objects;
			int i = 0;
			while (obj->type != live_pge_2->obj_type) {
				++i;
				++obj;
			}
			live_pge_2->first_obj_number = i;
		}
		if (init_pge_2->object_type == 1) {
			if (_currentRoom != live_pge_2->room_location) {
				_currentRoom = live_pge_2->room_location;
				loadLevelRoom();
				_vid.fullRefresh();
			}
		}
		pge_setupDefaultAnim(live_pge_2);
	}
	return 0xFFFF;
}

// called for example before using gun, to check its presence
int Game::pge_op_hasInventoryItem(ObjectOpcodeArgs *args) {
	LivePGE *pge = &_pgeLive[0];
	uint8_t _dl = pge->current_inventory_PGE;
	while (_dl != 0xFF) {
		pge = &_pgeLive[_dl];
		if (pge->init_PGE->object_id == args->a) {
			return 0xFFFF;
		}
		_dl = pge->next_inventory_PGE;
	}
	return 0;
}

int Game::pge_op_changeLevel(ObjectOpcodeArgs *args) {
	_currentLevel = args->a - 1;
	return _currentLevel;
}

int Game::pge_op_shakeScreen(ObjectOpcodeArgs *args) {
	_vid._shakeOffset = getRandomNumber() & 7;
	return 0xFFFF;
}

int Game::pge_o_unk0x86(ObjectOpcodeArgs *args) {
	return col_detectGunHit(args->pge, args->a, args->b, &Game::col_detectGunHitCallback2, &Game::col_detectGunHitCallback1, 1, 0);
}

int Game::pge_op_playSoundGroup(ObjectOpcodeArgs *args) {
	assert(args->a < 4);
	uint16_t c = args->pge->init_PGE->data[args->a];
	uint8_t sfxId = c & 0xFF;
	uint8_t softVol = c >> 8;
	playSound(sfxId, softVol);
	return 0xFFFF;
}

int Game::pge_op_adjustPos(ObjectOpcodeArgs *args) {
	LivePGE *pge = args->pge;
	pge->pos_x &= 0xFFF0;
	if (pge->pos_y != 70 && pge->pos_y != 142 && pge->pos_y != 214) {
		pge->pos_y = ((pge->pos_y / 72) + 1) * 72 - 2;
	}
	return 0xFFFF;
}

int Game::pge_op_setGunVar(ObjectOpcodeArgs *args) {
	_pge_opGunVar = args->a;
	return 0xFFFF;
}

int Game::pge_op_compareGunVar(ObjectOpcodeArgs *args) {
	if (_pge_opGunVar != args->a) {
		return 0;
	} else {
		return 0xFFFF;
	}
}

int Game::pge_setCurrentInventoryObject(LivePGE *pge) {
	LivePGE *_bx = pge_getPreviousInventoryItem(&_pgeLive[0], pge);
	if (_bx == &_pgeLive[0]) {
		if (_bx->current_inventory_PGE != pge->index) {
			return 0;
		}
	} else {
		if (_bx->next_inventory_PGE != pge->index) {
			return 0;
		}
	}
	pge_removeFromInventory(_bx, pge, &_pgeLive[0]);
	pge_addToInventory(&_pgeLive[0], pge, &_pgeLive[0]);
	return 0xFFFF;
}

void Game::pge_updateInventory(LivePGE *pge1, LivePGE *pge2) {
	if (pge2->ref_inventory_PGE != 0xFF) {
		pge_reorderInventory(pge2);
	}
	LivePGE *_ax = pge_getPreviousInventoryItem(pge1, 0);
	pge_addToInventory(_ax, pge2, pge1);
}

void Game::pge_reorderInventory(LivePGE *pge) {
	if (pge->ref_inventory_PGE != 0xFF) {
		LivePGE *_bx = &_pgeLive[pge->ref_inventory_PGE];
		LivePGE *_di = pge_getPreviousInventoryItem(_bx, pge);
		if (_di == _bx) {
			if (_di->current_inventory_PGE == pge->index) {
				pge_removeFromInventory(_di, pge, _bx);
			}
		} else {
			if (_di->next_inventory_PGE == pge->index) {
				pge_removeFromInventory(_di, pge, _bx);
			}
		}
	}
}

LivePGE *Game::pge_getPreviousInventoryItem(LivePGE *pge, LivePGE *last_pge) {
	LivePGE *_di = pge;
	uint8_t n = _di->current_inventory_PGE;
	while (n != 0xFF) {
		LivePGE *_si = &_pgeLive[n];
		if (_si == last_pge) {
			break;
		} else {
			_di = _si;
			n = _di->next_inventory_PGE;
		}
	}
	return _di;
}

void Game::pge_addToInventory(LivePGE *pge1, LivePGE *pge2, LivePGE *pge3) {
	pge2->ref_inventory_PGE = pge3->index;
	if (pge1 == pge3) {
		pge2->next_inventory_PGE = pge1->current_inventory_PGE;
		pge1->current_inventory_PGE = pge2->index;
	} else {
		pge2->next_inventory_PGE = pge1->next_inventory_PGE;
		pge1->next_inventory_PGE = pge2->index;
	}
}

int Game::pge_updateCollisionState(LivePGE *pge, int16_t pge_dy, uint8_t value) {
	const uint8_t pge_collision_data_len = pge->init_PGE->collision_data_len;
	if (pge->room_location < 0x40) {
		int8_t *grid_data = &_res._ctData[0x100] + 0x70 * pge->room_location;
		int16_t pge_pos_y = ((pge->pos_y / 36) & ~1) + pge_dy;
		int16_t pge_pos_x = (pge->pos_x + 8) >> 4;

		grid_data += pge_pos_x + pge_pos_y * 16;

		CollisionSlot2 *slot1 = _col_slots2Next;
		int16_t i = 255;
		if (_pge_currentPiegeFacingDir) {
			i = pge_collision_data_len - 1;
			grid_data -= i;
		}
		while (slot1) {
			if (slot1->unk2 == grid_data) {
				slot1->data_size = pge_collision_data_len - 1;
				assert(pge_collision_data_len < 0x70);
				memset(grid_data, value, pge_collision_data_len);
				return 1;
			} else {
				// the increment looks wrong but matches the DOS disassembly
				//
				// seg000:667B    inc cx
				// seg000:667C    mov si, bx
				// seg000:667E    mov bx, [bx+t_collision_slot2.next_slot]
				// seg000:6680    loop loc_0_1665B
				//
				// interestingly Amiga does not have it
				//
				// CODE:000042BA  movea.l a4,a5
				// CODE:000042BC  movea.l 0(a4),a4
				// CODE:000042C0  dbf     d0,loc_4290

				++i;

				slot1 = slot1->next_slot;
				if (--i == 0) {
					break;
				}
			}
		}
		if (_col_slots2Cur < &_col_slots2[255]) {
			slot1 = _col_slots2Cur;
			slot1->unk2 = grid_data;
			slot1->data_size = pge_collision_data_len - 1;
			uint8_t *dst = &slot1->data_buf[0];
			int8_t *src = grid_data;
			int n = pge_collision_data_len;
			assert(n < 0x10);
			while (n--) {
				*dst++ = *src;
				*src++ = value;
			}
			++_col_slots2Cur;
			slot1->next_slot = _col_slots2Next;
			_col_slots2Next = slot1;
		}
	}
	return 1;
}

int Game::pge_ZOrder(LivePGE *pge, int16_t num, pge_ZOrderCallback compare, uint16_t unk) {
	uint8_t slot = pge->collision_slot;
	while (slot != 0xFF) {
		CollisionSlot *cs = _col_slotsTable[slot];
		if (cs == 0) {
			return 0;
		}
		uint8_t slot_bak = slot;
		slot = 0xFF;
		while (cs != 0) {
			if ((this->*compare)(cs->live_pge, pge, num, unk) != 0) {
				return 1;
			}
			if (pge == cs->live_pge) {
				slot = cs->index;
			}
			cs = cs->prev_slot;
			if (slot == slot_bak) {
				return 0;
			}
		}
	}
	return 0;
}

void Game::pge_sendMessage(uint8_t src_pge_index, uint8_t dst_pge_index, int16_t num) {
	debug(DBG_GAME, "Game::pge_sendMessage() src=0x%X dst=0x%X num=0x%X", src_pge_index, dst_pge_index, num);
	LivePGE *pge = &_pgeLive[dst_pge_index];
	if (!(pge->flags & 4)) {
		if (!(pge->init_PGE->flags & 1)) {
			return;
		}
		pge->flags |= 4;
		_pge_liveTable2[dst_pge_index] = pge;
	}
	if (num <= 4) {
		uint8_t pge_room = pge->room_location;
		pge = &_pgeLive[src_pge_index];
		if (pge_room != pge->room_location) {
			return;
		}
		if (dst_pge_index == 0 && (_blinkingConradCounter != 0 || (_cheats & kCheatNoHit) != 0)) {
			return;
		}
		if (_stub->_pi.dbgMask & PlayerInput::DF_AUTOZOOM) {
			const int type = _pgeLive[dst_pge_index].init_PGE->object_type;
			if (type == 1 || type == 10) {
				_pge_zoomPiegeNum = dst_pge_index;
				_pge_zoomCounter = 0;
			}
		}
	}
	MessagePGE *le = _pge_nextFreeMessage;
	if (le) {
		// append to the list
		_pge_nextFreeMessage = le->next_entry;
		MessagePGE *next = _pge_messagesTable[dst_pge_index];
		_pge_messagesTable[dst_pge_index] = le;
		le->next_entry = next;
		le->src_pge = src_pge_index;
		le->msg_num = num;
	}
}

void Game::pge_removeFromInventory(LivePGE *pge1, LivePGE *pge2, LivePGE *pge3) {
	pge2->ref_inventory_PGE = 0xFF;
	if (pge3 == pge1) {
		pge3->current_inventory_PGE = pge2->next_inventory_PGE;
		pge2->next_inventory_PGE = 0xFF;
	} else {
		pge1->next_inventory_PGE = pge2->next_inventory_PGE;
		pge2->next_inventory_PGE = 0xFF;
	}
}

int Game::pge_ZOrderByAnimY(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2) {
	if (pge1 != pge2) {
		if (_res.getAniData(pge1->obj_type)[3] == comp) {
			return 1;
		}
	}
	return 0;
}

int Game::pge_ZOrderByAnimYIfType(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2) {
	if (pge1->init_PGE->object_type == comp2) {
		if (_res.getAniData(pge1->obj_type)[3] == comp) {
			return 1;
		}
	}
	return 0;
}

int Game::pge_ZOrderIfIndex(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2) {
	if (pge1->index != comp2) {
		pge_sendMessage(pge2->index, pge1->index, comp);
		return 1;
	}
	return 0;
}

int Game::pge_ZOrderByIndex(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2) {
	if (pge1 != pge2) {
		pge_sendMessage(pge2->index, pge1->index, comp);
		_pge_compareVar1 = 0xFFFF;
	}
	return 0;
}

int Game::pge_ZOrderByObj(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2) {
	if (comp == 10) {
		if (pge1->init_PGE->object_type == comp && pge1->life >= 0) {
			return 1;
		}
	} else {
		if (pge1->init_PGE->object_type == comp) {
			return 1;
		}
	}
	return 0;
}

int Game::pge_ZOrderIfDifferentDirection(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2) {
	if (pge1 != pge2) {
		if ((pge1->flags & 1) != (pge2->flags & 1)) {
			_pge_compareVar1 = 1;
			pge_sendMessage(pge2->index, pge1->index, comp);
			if (pge2->index == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_ZOrderIfSameDirection(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2) {
	if (pge1 != pge2) {
		if ((pge1->flags & 1) == (pge2->flags & 1)) {
			_pge_compareVar2 = 1;
			pge_sendMessage(pge2->index, pge1->index, comp);
			if (pge2->index == 0) {
				return 0xFFFF;
			}
		}
	}
	return 0;
}

int Game::pge_ZOrderIfTypeAndSameDirection(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2) {
	if (pge1->init_PGE->object_type == comp) {
		if ((pge1->flags & 1) == (pge2->flags & 1)) {
			return 1;
		}
	}
	return 0;
}

int Game::pge_ZOrderIfTypeAndDifferentDirection(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2) {
	if (pge1->init_PGE->object_type == comp) {
		if ((pge1->flags & 1) != (pge2->flags & 1)) {
			return 1;
		}
	}
	return 0;
}

int Game::pge_ZOrderByNumber(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2) {
	return pge1 - pge2;
}

static int pge_zoomDx(int prev_x, int cur_x) {
	int dx = ABS(cur_x - prev_x);
	if (dx < 4) {
		dx = 1;
	} else if (dx < 8) {
		dx = 2;
	} else if (dx < 16) {
		dx = 4;
	} else {
		dx = 8;
	}
	return (prev_x < cur_x) ? dx : -dx;
}

static int pge_zoomDy(int prev_y, int cur_y, bool flag) {
	int dy = ABS(cur_y - prev_y);
	if (flag) {
		if (dy < 2) {
			return 0;
		}
	} else {
		if (dy < 4) {
			return 0;
		}
	}
	if (dy < 8) {
		dy = 2;
	} else if (dy < 16) {
		dy = 4;
	} else {
		dy = 8;
	}
	return (prev_y < cur_y) ? dy : -dy;
}

void Game::pge_updateZoom() {
	static const int kZoomW = Video::GAMESCREEN_W / 2;
	static const int kZoomH = Video::GAMESCREEN_H / 2;
	if (_pge_zoomPiegeNum != 0) {
		LivePGE *pge = &_pgeLive[_pge_zoomPiegeNum];
		if (pge->room_location != _currentRoom) {
			_pge_zoomPiegeNum = 0;
		} else if (_pge_zoomCounter < 30) {
			int x = pge->pos_x + ((_pgeLive[0].flags & 1) ? 22 - kZoomW : -12);
			x = CLIP(x, 0, Video::GAMESCREEN_W - kZoomW);
			if (_pge_zoomCounter != 0 && _pge_zoomX != x) {
				const int dx = pge_zoomDx(_pge_zoomX, x);
				x = _pge_zoomX + dx;
			}
			_pge_zoomX = x;
			int y = pge->pos_y - 24 - kZoomH / 2;
			y = CLIP(y, 0, Video::GAMESCREEN_H - kZoomH);
			if (_pge_zoomCounter != 0 && _pge_zoomY != y) {
				const int dy = pge_zoomDy(_pge_zoomY, y, (pge->ref_inventory_PGE != 0xFF));
				y = _pge_zoomY + dy;
			}
			_pge_zoomY = y;
			_stub->zoomRect(x, y, kZoomW, kZoomH);
		}
		++_pge_zoomCounter;
		if (_pge_zoomCounter == 40) {
			_pge_zoomPiegeNum = 0;
		}
	}
}
