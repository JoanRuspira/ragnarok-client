#pragma once

#include "GameActor.h"

struct WhiteDamageNumberSettings {
	int rightDisplacement = 16;
	int maxTravelHeight = 48;
	int maxTravelWidth = 32;
	u32 delayBetweenNumbersMs = 160;
};

struct YellowDamageNumberSettings {
	u32 delayAfterLastHitMs = 320;
	int startingHeightDisplacement = 0;
	int maxTravelHeight = 48;
};

// Distances are screen pixels. Settings are loaded once from open-midgard.ini.
const WhiteDamageNumberSettings& GetWhiteDamageNumberSettings();
const YellowDamageNumberSettings& GetYellowDamageNumberSettings();
bool IsWhiteDamageNumber(int kind, u32 colorArgb);

bool QueueQueuedMsgEffectsQuads();
bool GetQueuedMsgEffectsBounds(RECT* outRect);
void ClearQueuedMsgEffects();
bool HasQueuedMsgEffects();
bool HasActiveMsgEffects();
bool RenderAttachedActorEffectPass(CMsgEffect& effect,
	matrix* viewMatrix,
	float ownerDepthKey,
	float ownerScreenY,
	bool afterOwner,
	u32 frameId);
