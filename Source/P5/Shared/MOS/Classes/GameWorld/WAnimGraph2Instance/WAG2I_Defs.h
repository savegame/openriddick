#ifndef WAG2I_Defs_h
#define WAG2I_Defs_h

//--------------------------------------------------------------------------------

#define AG2I_UNDEFINEDTIME	(CMTime::CreateFromSeconds(-1.0f))
#define AG2I_LINKEDTIME		(CMTime::CreateFromSeconds(-2.0f))
#define AG2I_UNDEFINEDTIMEFP32 (-1.0f)

#define AG2I_MAXANIMLAYERS	(16)

//--------------------------------------------------------------------------------

#define AG2I_DEBUGFLAGS_ENTERSTATE_LO_MASK		0x00000003
#define AG2I_DEBUGFLAGS_ENTERSTATE_LO_SHIFT		0x00000000

#define AG2I_DEBUGFLAGS_ENTERSTATE_HI_MASK		0x0000000C
#define AG2I_DEBUGFLAGS_ENTERSTATE_HI_SHIFT		0x00000002

#define AG2I_DEBUGFLAGS_ENTERSTATE_SERVER		0x00000001
#define AG2I_DEBUGFLAGS_ENTERSTATE_CLIENT		0x00000002
#define AG2I_DEBUGFLAGS_ENTERSTATE_PLAYER		0x00000004
#define AG2I_DEBUGFLAGS_ENTERSTATE_NONPLAYERS	0x00000008

// В движке уже есть готовая трасса анимграфа: на каждый вход в состояние
// печатается объект, токен, игровое время, имя целевого состояния и индекс
// анимации каждого слоя. Включалась она через реестр (AG2I_DEBUG_FLAGS в
// Environment / серверном реестре), а у порта реестр правится только через
// Environment.cfg игрока -- неудобно, когда лог собирается одним запуском.
// RIDDICK_AG2_DEBUGFLAGS даёт те же биты из окружения и просто добавляется
// к реестровому значению (0 = поведение как раньше):
//   1 сервер, 2 клиент, 4 игрок, 8 остальные персонажи (обычно 9 или 0xD).
// RIDDICK_AG2_DEBUGOBJ=<iObject> сужает вывод до одного объекта -- то же,
// что реестровый 'agdbgobj'.
// Inline, а не отдельный TU: читают это и GameWorld, и GameClasses, а
// заводить ради двух getenv межбиблиотечный символ и зависимость по
// порядку линковки — лишнее.
#include <stdlib.h>

static inline uint32 Riddick_AG2DebugFlagsEnv()
{
	static int32 s_Flags = -1;
	if (s_Flags < 0)
	{
		const char* e = getenv("RIDDICK_AG2_DEBUGFLAGS");
		s_Flags = (e && *e) ? (int32)strtol(e, NULL, 0) : 0;
	}
	return (uint32)s_Flags;
}

static inline int32 Riddick_AG2DebugObjEnv()
{
	static int32 s_iObj = -1;
	if (s_iObj < 0)
	{
		const char* e = getenv("RIDDICK_AG2_DEBUGOBJ");
		s_iObj = (e && *e) ? (int32)strtol(e, NULL, 0) : 0;
	}
	return s_iObj;
}

//--------------------------------------------------------------------------------

#define AG2I_PACKEDAG2I_RESOURCEINDICES			0x01
#define AG2I_PACKEDAG2I_TOKENS					0x02
#define AG2I_PACKEDAG2I_REMTOKENIDS				0x04
#define AG2I_PACKEDAG2I_OVERLAYANIM				0x08
#define AG2I_PACKEDAG2I_RANDSEED					0x10
#define AG2I_PACKEDAG2I_HASENTERSTATEENTRIES		0x20
#define AG2I_PACKEDAG2I_ISDISABLED				0x40
#define AG2I_PACKEDAG2I_OVERLAYANIMLIPSYNC		0x80

#define AG2I_PACKEDTOKEN_REFRESHGAMETIME			0x01
#define AG2I_PACKEDTOKEN_PLAYINGSTATEINSTANCE	0x02
#define AG2I_PACKEDTOKEN_SIPS					0x04
#define AG2I_PACKEDTOKEN_REMSIIDS				0x08

//--------------------------------------------------------------------------------

#define AG2I_TOKENREFRESHFLAGS_REFRESH			0x01
#define AG2I_TOKENREFRESHFLAGS_PERFORMEDACTION	0x02
#define AG2I_TOKENREFRESHFLAGS_FINISHED			0x04
#define AG2I_TOKENREFRESHFLAGS_FAILED			0x08

//--------------------------------------------------------------------------------

#endif /* WAG2I_Defs_h */
