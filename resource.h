/* REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir
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

#ifndef RESOURCE_H__
#define RESOURCE_H__

#include "intern.h"

struct File;
struct FileSystem;

struct LocaleData {
	enum Id {
		LI_01_CONTINUE_OR_ABORT = 0,
		LI_02_TIME,
		LI_03_CONTINUE,
		LI_04_ABORT,
		LI_05_COMPLETED,
		LI_06_LEVEL,
		LI_07_START,
		LI_08_SKILL,
		LI_09_PASSWORD,
		LI_10_INFO,
		LI_11_QUIT,
		LI_12_SKILL_LEVEL,
		LI_13_EASY,
		LI_14_NORMAL,
		LI_15_EXPERT,
		LI_16_ENTER_PASSWORD1,
		LI_17_ENTER_PASSWORD2,
		LI_18_RESUME_GAME,
		LI_19_ABORT_GAME,
		LI_20_LOAD_GAME,
		LI_21_SAVE_GAME,
		LI_22_SAVE_SLOT,

		LI_NUM
	};

	static const char *_textsTableFR[];
	static const char *_textsTableEN[];
	static const char *_textsTableDE[];
	static const char *_textsTableSP[];
	static const char *_textsTableIT[];
	static const uint8_t _stringsTableFR[];
	static const uint8_t _stringsTableEN[];
	static const uint8_t _stringsTableDE[];
	static const uint8_t _stringsTableSP[];
	static const uint8_t _stringsTableIT[];
};

struct Resource {
	typedef void (Resource::*LoadStub)(File *);

	enum ObjectType {
		OT_MBK,
		OT_PGE,
		OT_PAL,
		OT_CT,
		OT_MAP,
		OT_SPC,
		OT_RP,
		OT_RPC,
		OT_DEMO,
		OT_ANI,
		OT_OBJ,
		OT_TBN,
		OT_SPR,
		OT_TAB,
		OT_ICN,
		OT_FNT,
		OT_TXTBIN,
		OT_CMD,
		OT_POL,
		OT_SPRM,
		OT_OFF,
		OT_CMP,
		OT_OBC,
		OT_SPL,
		OT_LEV,
		OT_SGD,
		OT_SPM
	};

	enum {
		NUM_SFXS = 66,
		NUM_SPRITES = 1287
	};

	static const uint16_t _voicesOffsetsTable[];
	static const uint32_t _spmOffsetsTable[];
	static const char *_splNames[];

	FileSystem *_fs;
	ResourceType _type;
	Language _lang;
	bool _hasSeqData;
	char _entryName[32];
	uint8_t *_fnt;
	uint8_t *_mbk;
	uint8_t *_icn;
	int _icnLen;
	uint8_t *_tab;
	uint8_t *_spc; // BE
	uint16_t _numSpc;
	uint8_t _rp[0x4A];
	uint8_t *_pal; // BE
	uint8_t *_ani;
	uint8_t *_tbn;
	int8_t _ctData[0x1D00];
	uint8_t *_spr1;
	uint8_t *_sprData[NUM_SPRITES]; // 0-0x22F + 0x28E-0x2E9 ... conrad, 0x22F-0x28D : junkie
	uint8_t _sprm[0x10000];
	uint16_t _pgeNum;
	InitPGE _pgeInit[256];
	uint8_t *_map;
	uint8_t *_lev;
	int _levNum;
	uint8_t *_sgd;
	uint16_t _numObjectNodes;
	ObjectNode *_objectNodesMap[255];
	uint8_t *_memBuf;
	SoundFx *_sfxList;
	uint8_t _numSfx;
	uint8_t *_cmd;
	uint8_t *_pol;
	uint8_t *_cine_off;
	uint8_t *_cine_txt;
	char **_extTextsTable;
	const char **_textsTable;
	uint8_t *_extStringsTable;
	const uint8_t *_stringsTable;
	uint8_t *_bankData;
	uint8_t *_bankDataHead;
	uint8_t *_bankDataTail;
	BankSlot _bankBuffers[50];
	int _bankBuffersCount;

	Resource(FileSystem *fs, ResourceType type, Language lang);
	~Resource();

	void clearLevelRes();
	void load_FIB(const char *fileName);
	void load_SPL_demo();
	void load_MAP_menu(const char *fileName, uint8_t *dstPtr);
	void load_PAL_menu(const char *fileName, uint8_t *dstPtr);
	void load_SPR_OFF(const char *fileName, uint8_t *sprData);
	void load_CINE();
	void load_TEXT();
	void free_TEXT();
	void load(const char *objName, int objType, const char *ext = 0);
	void load_CT(File *pf);
	void load_FNT(File *pf);
	void load_MBK(File *pf);
	void load_ICN(File *pf);
	void load_SPR(File *pf);
	void load_SPRM(File *pf);
	void load_RP(File *pf);
	void load_SPC(File *pf);
	void load_PAL(File *pf);
	void load_MAP(File *pf);
	void load_OBJ(File *pf);
	void free_OBJ();
	void load_OBC(File *pf);
	void decodeOBJ(const uint8_t *, int);
	void load_PGE(File *pf);
	void load_ANI(File *pf);
	void load_TBN(File *pf);
	void load_CMD(File *pf);
	void load_POL(File *pf);
	void load_CMP(File *pf);
	void load_VCE(int num, int segment, uint8_t **buf, uint32_t *bufSize);
	void load_SPL(File *pf);
	void load_LEV(File *pf);
	void load_SGD(File *pf);
	void load_SPM(File *f);
	const uint8_t *getAniData(int num) const {
		const int offset = READ_LE_UINT16(_ani + num * 2);
		return _ani + offset;
	}
	const uint8_t *getGameString(int num) {
		return _stringsTable + READ_LE_UINT16(_stringsTable + num * 2);
	}
	const uint8_t *getCineString(int num) {
		if (_cine_off) {
			const int offset = READ_BE_UINT16(_cine_off + num * 2);
			return _cine_txt + offset;
		}
		return 0;
	}
	const char *getMenuString(int num) {
		return (num >= 0 && num < LocaleData::LI_NUM) ? _textsTable[num] : "";
	}
	void clearBankData();
	int getBankDataSize(uint16_t num);
	uint8_t *findBankData(uint16_t num);
	uint8_t *loadBankData(uint16_t num);
};

#endif // RESOURCE_H__
