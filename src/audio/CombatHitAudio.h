#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Element and weapon numbers match the server's ELE_* and W_* values.
// Unknown properties stay silent rather than producing an unrelated hit sound.
enum class CombatDamageKind { Unknown, Physical, Magic };

struct CombatHitAudio {
    CombatDamageKind kind = CombatDamageKind::Unknown;
    int element = -1;
    int weaponType = -1;
    bool player = false;
    bool longRange = false;
    int weaponImpactElement = -1; // Resolved property for the additional neutral impact.
};

inline constexpr std::uint16_t kActorDamageAudioPacket = 0x7F10;
inline constexpr std::uint16_t kSkillDamageAudioPacket = 0x7F11;
inline constexpr int kDamageAudioPacketSize = 38;
inline constexpr std::uint16_t kSpellFistAttackSkillId = 158;

struct CombatWeaponInfo {
    int firstItemId;
    int lastItemId;
    int weaponType;
    int element;
};

inline const CombatWeaponInfo* FindCombatWeaponInfo(int itemId)
{
    static constexpr CombatWeaponInfo weapons[] = {
#include "audio/WeaponDamageAudio.generated.inc"
    };
    for (const CombatWeaponInfo& weapon : weapons) {
        if (itemId < weapon.firstItemId) break;
        if (itemId <= weapon.lastItemId) return &weapon;
    }
    return nullptr;
}

inline const char* CombatElementWave(int element, bool weaponHit = false)
{
    switch (element) {
    case 0: return "data\\wav\\effect\\cd_reparatio1.wav";
    case 1: return "data\\wav\\effect\\bo_acidified_zone_water.wav";
    case 2: return weaponHit ? "data\\wav\\_stonecurse.wav" : "data\\wav\\_stone_explosion.wav";
    case 3: return "data\\wav\\effect\\ef_fireball.wav";
    case 4: return "data\\wav\\effect\\s\xC0\xCF\xB7\xBA\xC6\xAE\xB8\xAF\xBF\xF6\xC5\xA9.wav";
    case 5: return "data\\wav\\_poison.wav";
    case 6: return "data\\wav\\effect\\dk_chargingpierce2.wav";
    case 7: return weaponHit ? "data\\wav\\effect\\ef_firearrow1.wav" : "data\\wav\\effect\\bo_hell_howling.wav";
    case 8: return "data\\wav\\effect\\dk_servant_w_demol.wav";
    default: return nullptr;
    }
}

inline const char* CombatWeaponWave(int weaponType, std::uint32_t randomValue)
{
    switch (weaponType) {
    case 0: // Bare hands.
    case 12: return "data\\wav\\attack_fist.wav";
    case 1: return "data\\wav\\_attack_dagger.wav";
    case 2: return "data\\wav\\attack_sword.wav";
    case 3: return "data\\wav\\attack_twohand_sword.wav";
    case 4: return "data\\wav\\_attack_spear.wav";
    case 5: return "data\\wav\\attack_spear.wav";
    case 6: return "data\\wav\\_attack_axe.wav";
    case 7: return "data\\wav\\attack_axe.wav";
    case 8: // One/two-handed maces share their impact.
    case 9: return "data\\wav\\attack_mace.wav";
    case 10:
    case 23: return "data\\wav\\_attack_sword.wav";
    case 11: return "data\\wav\\attack_bow.wav";
    case 13: {
        static constexpr const char* waves[] = {
            "data\\wav\\effect\\sign_center.wav",
            "data\\wav\\effect\\sign_down.wav",
            "data\\wav\\effect\\sign_left.wav",
        };
        return waves[randomValue % 3];
    }
    case 15: return "data\\wav\\_attack_book.wav";
    case 16: return "data\\wav\\attack_katar.wav";
    default: return nullptr; // No unrequested fallback for other weapon families.
    }
}

// Extra impact layer for the requested player weapons. Keep the existing
// weapon/element recipe, and require a known neutral physical property.
inline const char* CombatNeutralWeaponWave(const CombatHitAudio& hit, std::uint32_t randomValue)
{
    if (!hit.player || hit.kind != CombatDamageKind::Physical
        || hit.weaponImpactElement != 0) return nullptr;

    switch (hit.weaponType) {
    case 1: // Dagger / katar.
    case 16: return "data\\wav\\_hit_dagger.wav";
    case 2: // One/two-handed swords.
    case 3: return "data\\wav\\_hit_sword.wav";
    case 4: // One/two-handed spears.
    case 5: return "data\\wav\\_hit_spear.wav";
    case 6: // One/two-handed axes.
    case 7: return "data\\wav\\_hit_axe.wav";
    case 8: // Maces.
    case 9: return "data\\wav\\_hit_mace.wav";
    case 10: // One/two-handed staves.
    case 23: return "data\\wav\\_enemy_hit4.wav";
    case 11: return "data\\wav\\_hit_arrow.wav";
    case 12: {
        static constexpr const char* waves[] = {
            "data\\wav\\_hit_fist1.wav",
            "data\\wav\\_hit_fist2.wav",
            "data\\wav\\_hit_fist3.wav",
            "data\\wav\\_hit_fist4.wav",
        };
        return waves[randomValue % 4];
    }
    case 13: return "data\\wav\\_enemy_hit1.wav"; // Stored outside effect in the installed GRFs.
    case 15: return "data\\wav\\_hit_fist3.wav";
    default: return nullptr;
    }
}

inline std::array<const char*, 3> CombatHitWaves(const CombatHitAudio& hit,
    std::uint32_t randomValue, bool spellFistAttack = false)
{
    if (hit.kind == CombatDamageKind::Magic) return { CombatElementWave(hit.element, spellFistAttack), nullptr };
    if (hit.kind != CombatDamageKind::Physical) return { nullptr, nullptr };
    if (!hit.player && hit.element == 0) {
        static constexpr const char* waves[] = {
            "data\\wav\\_enemy_hit1.wav",
            "data\\wav\\_enemy_hit2.wav",
            "data\\wav\\_enemy_hit3.wav",
        };
        return { waves[randomValue % 3], hit.longRange ? "data\\wav\\attack_bow.wav" : nullptr };
    }
    const char* weaponWave = CombatWeaponWave(hit.weaponType, randomValue);
    const char* neutralWave = CombatNeutralWeaponWave(hit, randomValue);
    // The book already uses the requested WAV. Play it once per hit.
    if (weaponWave && neutralWave && std::strcmp(weaponWave, neutralWave) == 0) neutralWave = nullptr;
    return { weaponWave, hit.element > 0 ? CombatElementWave(hit.element, true) : nullptr, neutralWave };
}

struct SkillDamageAudio {
    int skillId;
    CombatDamageKind kind;
    int element; // -1 inherits the weapon/ammunition property; -2 is unknown/dynamic.
    bool singleHit; // One impact at every level; ground/area/persistent skills are excluded.
    bool longRange;
};

inline const SkillDamageAudio* FindSkillDamageAudio(int skillId)
{
    static constexpr SkillDamageAudio skills[] = {
#include "skill/SkillDamageAudio.generated.inc"
    };
    // Sorted, generated metadata also provides a fallback for legacy servers.
    std::size_t first = 0, last = sizeof(skills) / sizeof(skills[0]);
    while (first < last) {
        const std::size_t middle = first + (last - first) / 2;
        if (skills[middle].skillId < skillId) first = middle + 1;
        else last = middle;
    }
    return first < sizeof(skills) / sizeof(skills[0]) && skills[first].skillId == skillId
        ? &skills[first] : nullptr;
}

inline bool UsesDedicatedMagicHitAudio(int skillId, const CombatHitAudio& hit, std::size_t hitCount)
{
    if (hit.kind != CombatDamageKind::Magic || skillId == kSpellFistAttackSkillId) return false;
    const SkillDamageAudio* skill = FindSkillDamageAudio(skillId);
    return hitCount == 1 && (!skill || skill->singleHit);
}

