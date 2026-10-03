# rAthena <-> OpenMidgard Compatibility Patch Notes

This document lists the source-level edits that were required to make rAthena interoperate with the old `2008-09-10aSakexe` client behavior we were matching against OpenMidgard.

For opcode behavior, OpenMidgard is following the `2008-09-10aSakexe` packet family, not the nearby early-RE packet families. That distinction matters because rAthena has separate packet-db branches for those clients, and letting the server fall into the RE opcode path produces request opcodes and field layouts that this client does not support.


## Packet Version Setup

Set these in `src/custom/defines_pre.hpp`:

```cpp
#define PACKETVER 20080910
#define PACKETVER_SAK_NUM 20080910
```

### What these do

- `PACKETVER 20080910` selects the general packet family for `2008-09-10aSakexe`.
- `PACKETVER_SAK_NUM 20080910` is also required so the code can distinguish this old Sakexe client from newer packet layouts that still fall under older `PACKETVER` ranges.

Without `PACKETVER_SAK_NUM`, the server still compiles parts of the newer character-info layout and sends fields the client does not decode correctly.

## Source Patches

### 1. `src/map/clif_packetdb.hpp`

Add a guard so this client does not fall into the early Renewal packet-db branch when it should stay on the early Sakexe-compatible path:

```cpp
// Renewal Clients
#ifdef PACKETVER_RE_NUM

// 2008-08-27aRagexeRE
#if PACKETVER_RE_NUM >= 20080827
    parseable_packet(0x0072,22,clif_parse_UseSkillToId,9,15,18);
    packet(0x007c,44);
    parseable_packet(0x007e,105,clif_parse_UseSkillToPosMoreInfo,10,14,18,23,25);
    parseable_packet(0x0085,10,clif_parse_ChangeDir,4,9);
    parseable_packet(0x0089,11,clif_parse_TickSend,7);
    parseable_packet(0x008c,14,clif_parse_GetCharNameRequest,10);
    parseable_packet(0x0094,19,clif_parse_MoveToKafra,3,15);
    parseable_packet(0x009b,34,clif_parse_WantToConnection,7,15,25,29,33);
    parseable_packet(0x009f,20,clif_parse_UseItem,7,20);
    parseable_packet(0x00a2,14,clif_parse_SolveCharName,10);
    parseable_packet(0x00a7,9,clif_parse_WalkToXY,6);
    parseable_packet(0x00f5,11,clif_parse_TakeItem,7);
    parseable_packet(0x00f7,17,clif_parse_MoveFromKafra,3,13);
    parseable_packet(0x0113,25,clif_parse_UseSkillToPos,10,14,18,23);
    parseable_packet(0x0116,17,clif_parse_DropItem,6,15);
    parseable_packet(0x0190,23,clif_parse_ActionRequest,9,22);
    packet(0x02e2,20);
    packet(0x02e3,22);
    packet(0x02e4,11);
    packet(0x02e5,9);
#endif

// 2008-09-10aRagexeRE
#if PACKETVER_RE_NUM >= 20080910
    parseable_packet(0x0436,19,clif_parse_WantToConnection,2,6,10,14,18);
    parseable_packet(0x0437,7,clif_parse_ActionRequest,2,6);
    parseable_packet(0x0438,10,clif_parse_UseSkillToId,2,4,6);
    parseable_packet(0x0439,8,clif_parse_UseItem,2,4);
#endif

// 2008-11-12aRagexeRE
#if PACKETVER_RE_NUM >= 20081112
    packet(0x043f,8);
#endif

// 2008-12-17bRagexeRE
#if PACKETVER_RE_NUM >= 20081217
    packet(0x006d,114);
#endif

// 2009-01-21aRagexeRE
#if PACKETVER_RE_NUM >= 20090121
    packet(0x043f,25);
#endif

// 2009-05-20aRagexeRE
#if PACKETVER_RE_NUM >= 20090520
    parseable_packet(0x0447,2,clif_parse_blocking_playcancel,0);
#endif

#endif
```

### Why this patch was needed

This block is the map-server packet table for the early RE client family. In this compatibility patch, the point of calling it out is not that OpenMidgard should use that RE family. The point is to guard against rAthena selecting that nearby RE packet-db path for a client that is actually speaking the `2008-09-10aSakexe` opcode family.

The important note for OpenMidgard is that our client is intentionally following the `2008-09-10aSakexe` opcode family. The reason to call out this RE block is to make the boundary explicit: without the guard, rAthena can drift into the nearby early-RE opcode mappings for the same general era. If the server uses those RE mappings, it starts expecting different opcodes and field positions for core requests than the client actually sends.

The important consequence is that login/map bootstrap and normal gameplay traffic stop agreeing on the wire format. Requests such as `WantToConnection`, `ActionRequest`, `UseSkillToId`, `UseItem`, `TickSend`, `WalkToXY`, `TakeItem`, and the Kafra move packets get parsed using the wrong layouts, which leads to dropped requests, desync, or apparently random behavior once the client reaches the map server. In short: this patch exists so rAthena does not treat our Sakexe-style client as an early-RE client and switch to RE packet opcodes that OpenMidgard does not implement.

This patch also carries the early RE-side packet-size overrides for server packets that changed during that window, including the `0x006d` character-info packet becoming 114 bytes for `2008-12-17bRagexeRE` and the later `0x043f` size change in January 2009. Those overrides are part of the same compatibility story: the packet-db has to match the exact 2008 RE family, not just the broad `PACKETVER` date.

Compared against the upstream packet table in rAthena's `src/map/clif_packetdb.hpp`, this is the missing section needed to document and enforce that OpenMidgard must not be treated as one of those early-RE packet-db clients.

### 2. `src/common/packets.hpp`

Add a legacy Sakexe guard before `CHARACTER_INFO`:

```cpp
#if defined(PACKETVER_SAK_NUM) && PACKETVER_SAK_NUM > 0 && PACKETVER_SAK_NUM < 20090225
    #define PACKETVER_SAK_LEGACY_CHARINFO
#endif
```

Then make `CHARACTER_INFO` use the old 108-byte layout for this client:

```cpp
struct CHARACTER_INFO{
    uint32 GID;
#if PACKETVER >= 20170830
    int64 exp;
#else
    int32 exp;
#endif
    int32 money;
#if PACKETVER >= 20170830
    int64 jobexp;
#else
    int32 jobexp;
#endif
    int32 joblevel;
    int32 bodystate;
    int32 healthstate;
    int32 effectstate;
    int32 virtue;
    int32 honor;
    int16 jobpoint;
#ifdef PACKETVER_SAK_LEGACY_CHARINFO
    int16 hp;
    int16 maxhp;
    int16 sp;
    int16 maxsp;
#else
#if PACKETVER_RE_NUM >= 20211103 || PACKETVER_MAIN_NUM >= 20220330
    int64 hp;
    int64 maxhp;
    int64 sp;
    int64 maxsp;
#else
    int32 hp;
    int32 maxhp;
    int16 sp;
    int16 maxsp;
#endif
#endif
    int16 speed;
    int16 job;
    int16 head;
#if PACKETVER >= 20141022
    int16 body;
#endif
    int16 weapon;
    int16 level;
    int16 sppoint;
    int16 accessory;
    int16 shield;
    int16 accessory2;
    int16 accessory3;
    int16 headpalette;
    int16 bodypalette;
    char name[24];
    uint8 Str;
    uint8 Agi;
    uint8 Vit;
    uint8 Int;
    uint8 Dex;
    uint8 Luk;
#ifdef PACKETVER_SAK_LEGACY_CHARINFO
    int16 CharNum;
    int16 hairColor;
#else
    uint8 CharNum;
    uint8 hairColor;
    int16 bIsChangedCharName;
#endif
#if (PACKETVER >= 20100720 && PACKETVER <= 20100727) || PACKETVER >= 20100803
    char mapName[16];
#endif
#if PACKETVER >= 20100803
    int32 DelRevDate;
#endif
#if PACKETVER >= 20110111
    int32 robePalette;
#endif
#if PACKETVER >= 20110928
    int32 chr_slot_changeCnt;
#endif
#if PACKETVER >= 20111025
    int32 chr_name_changeCnt;
#endif
#if PACKETVER >= 20141016
    uint8 sex;
#endif
} __attribute__((packed));

#ifdef PACKETVER_SAK_LEGACY_CHARINFO
static_assert( sizeof( CHARACTER_INFO ) == 108, "Legacy Sakexe CHARACTER_INFO size mismatch" );
#endif
```

### Why this patch was needed

The old Sakexe client expects the classic 108-byte character block in the character list and character-create success packets. rAthena was otherwise sending a newer layout, which caused the client to decode the slot, class visuals, stats, and name fields incorrectly.

### 3. `src/char/char.cpp`

Inside `char_mmo_char_tobuf`, serialize the legacy fields with the old widths when `PACKETVER_SAK_LEGACY_CHARINFO` is active:

```cpp
#ifdef PACKETVER_SAK_LEGACY_CHARINFO
    info.hp = u32min( p.hp, INT16_MAX );
    info.maxhp = u32min( p.max_hp, INT16_MAX );
    info.sp = u32min( p.sp, INT16_MAX );
    info.maxsp = u32min( p.max_sp, INT16_MAX );
#else
    info.hp = p.hp;
    info.maxhp = p.max_hp;
    info.sp = min( p.sp, INT16_MAX );
    info.maxsp = min( p.max_sp, INT16_MAX );
#endif
```

Also serialize slot and hair color using the legacy field sizes and omit the rename flag in the old path:

```cpp
#ifdef PACKETVER_SAK_LEGACY_CHARINFO
    info.CharNum = p.slot;
    info.hairColor = u16min( p.hair_color, INT16_MAX );
#else
    info.CharNum = p.slot;
    info.hairColor = (uint8)u16min( p.hair_color, UINT8_MAX );
    info.bIsChangedCharName = ( p.rename > 0 ) ? 0 : 1;
#endif
```

### Why this patch was needed

Changing the struct definition alone is not enough. The serializer must write values using the old field sizes, or the packet layout still becomes invalid for the client.

### 4. `src/char/char_clif.cpp`

In `chclif_block_character`, do not send `HC_BLOCK_CHARACTER (0x020D)` to the old Sakexe client:

```cpp
void chclif_block_character( int32 fd, char_session_data& sd){
#ifdef PACKETVER_SAK_LEGACY_CHARINFO
    return;
#endif

    time_t now = time( nullptr );
    ...
}
```

### Why this patch was needed

This client does not handle that packet correctly. Suppressing it avoids sending a character-server packet that belongs to later client behavior.

### 5. `src/map/clif.cpp`

Adjust `clif_refresh_clothcolor` to use const-correct parameters:

```cpp
void clif_refresh_clothcolor( const block_list& bl, enum send_target target, const block_list* tbl = nullptr ){
#if PACKETVER < 20091103
    const view_data* vd = status_get_viewdata( &bl );

    if( vd == nullptr ){
        return;
    }

    if( vd->look[LOOK_CLOTHES_COLOR] == 0 ){
        return;
    }

    if( tbl == nullptr ){
        tbl = &bl;
    }

    clif_sprite_change( tbl, bl.id, LOOK_CLOTHES_COLOR, vd->look[LOOK_CLOTHES_COLOR], 0, target );
#endif
}
```

### Why this patch was needed

The older packet path exposed a compile-time mismatch in this helper. This change does not alter protocol behavior; it fixes the source so the old-client code path builds cleanly.

## Resulting Behavior

With the packet version defines set to:

```cpp
#define PACKETVER 20080910
#define PACKETVER_SAK_NUM 20080910
```

and the five source edits above applied:

- early RE map-server packet parsing follows the intended 2008 client family instead of later/default packet-db entries
- rAthena stays on the Sakexe-compatible opcode path the OpenMidgard client actually speaks, instead of falling into unsupported early-RE opcode mappings
- character creation uses the correct old Sakexe packet family
- character list packets use the expected 108-byte `CHARACTER_INFO` layout
- character slot and appearance data decode correctly on the client
- unsupported newer char-server packets are not sent to this client

This is the source patch set required on the rAthena side for the old client compatibility work that was completed.

## Runtime Entity Visibility

rAthena controls when nearby NPCs, monsters, players, and ground items are sent
to the client through the `area_size` battle setting. The default is 14 map cells
in each direction. OpenMidgard renders the actors it receives without imposing
a separate distance limit in map cells.

For the local server at `C:\ragnarok\ragnarok`, put this override in
`conf/import/battle_conf.txt`, which is imported after the default battle settings:

```conf
// Send nearby entities sooner for OpenMidgard's wider camera view.
// Default: 14 map cells in each direction; 28 doubles the visibility radius.
area_size: 28
```

Apply it with the GM command `@reloadbattleconf`, then reconnect or change maps
to populate the enlarged area. Alternatively, restart the map server. This is
a runtime configuration change and requires no client or server rebuild.

A doubled radius covers nearly four times as many cells and can increase network
traffic and server/client work in crowded areas. rAthena also uses `AREA_SIZE`
for some AI and gameplay checks, so this setting is not exclusively visual.
To restore the default, remove the override and reload the battle configuration.
