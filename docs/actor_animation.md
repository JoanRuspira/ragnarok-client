# Actor animation timing

The client also supports [sprite pose interpolation](sprite_interpolation.md),
enabled by default. It adds rendered poses between ACT frames without changing
the animation clocks described below.

Player and monster positions and action states advance once per rendered frame.
The 24 ms world tick still advances game objects, items and periodic status
effects. Actor updates use elapsed time, so a delayed frame catches up to the
current pose instead of advancing the animation a fixed number of frames.

ACT delays use 25 ms units. Runtime motion, billboard selection, skill duration
and attack impact scheduling share that conversion. The
[roBrowser ACT loader](https://github.com/vthibault/roBrowser/blob/master/src/Loaders/Action.js)
also documents this conversion in its implementation.

Walking remains tied to distance travelled. Extending a moving actor's path
preserves its animation and sound event cursor. Attack timing uses the selected
direction and weapon's `atk` marker and the server attack motion time. Fast
attacks can use frame durations below 25 ms. Hit reactions fit the full action
to the server damage motion time, with a 200 ms fallback. Completed attacks
return to idle or the player's ready stance; death actions hold their last pose.
Player idle and sitting frames retain their head direction semantics.

Visible equipment uses its own elapsed-time clock while idle or sitting. The
client reads each equipment ACT's motion count and delay for the current facing
direction. Animated actions contain three head-direction groups; frames loop
within the selected group, while the body and hair retain their head pose.
Walking and combat equipment poses continue to follow the character action.
The same selection applies to normal equipment and costumes in the bottom,
middle and top slots, with independent frame counts and delays for each slot.

Equipment frame indices are included in both the actor's billboard cache and
the shared player texture cache. Attachment offsets use the selected equipment
frame with the current body/head pose. The equipment clock survives movement,
sitting, head turns and billboard invalidation. Static three-frame actions and
actions without complete head-direction groups keep their existing pose
selection.

Character selection uses the same ACT frame resolver for all three equipment
slots in both the legacy and Qt preview renderers. Its clock belongs to the
selection window, so rebuilding previews for layout, image requests or page
changes does not restart equipment animations. The Qt preview image revision
includes equipment frames and appearance, causing the panel image to refresh
as animations advance on any visible character. `OnProcess()` advances the
equipment frames even while the login UI texture is cached, and invalidates
the window whenever a frame changes. The outer login UI cache token also
includes the preview revision; checking only the Qt image revision would leave
the entire screen frozen in the CPU composition path. Body and hair remain in
their idle pose.

Item 19006, `Costume_Books_of_Magic`, maps to view 1691 (`book_of_magic`).
Both male and female ACTs contain 24 idle and sitting frames per facing
direction: three groups of eight frames at 100 ms per frame. Each selected
head-direction group therefore repeats every 800 ms.

The shared GRFs in `C:\ragnarok\Runerok_Dev` were inspected directly. These
monster action bands and frame counts differ from the player layout:

| Sprite | Idle (0) | Walk (8) | Attack (16) | Hit (24) | Death (32) | Attack marker frame |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Poring | 4 | 10 | 28 | 5 | 17 | 12 |
| Lunatic | 12 | 15 | 19 | 7 | 34 | 11 |
| Fabre | 4 | 4 | 7 | 5 | 11 | 2 |

Player bodies instead use hit band 48 and death band 64. The client now selects
these bands by actor type. The local rAthena `src/map/clif.cpp` damage packet
supplies the attacker and defender motion times as `sdelay` and `ddelay`.

Vulkan billboard textures are cached as their poses are requested, including
actions longer than 32 frames. First use no longer prepares every direction
and action of a monster or every frame of a player action at once. Hover bounds
are invalidated when an actor's exact position or animation pose changes.
Per-frame motion trace logging is disabled by default to avoid synchronous
log writes while attacking.

## Validation

Source and diff checks and direct GRF inspection were performed. The user
confirmed equipment animations work in game. The character selection follow-up
was reviewed through both preview renderers, the window process/invalidation
path and the outer login UI texture cache as well as the Qt image revision.
No build or character selection runtime validation was run.

The equipment fix was checked against all 3,621 equipment ACT resources in the
shared GRFs, accounting for `patchRunerok.grf` priority over `data.grf`. The
inspection covered both sexes, all eight facing directions, idle and sitting
actions, ACT delays and complete animation groups with 2, 8, 12, 14, 15, 16
and 18 frames per head direction. Animated action delays range from 100 to
300 ms per frame in these assets. A few static assets contain extra pose frames;
incomplete groups retain the original head-frame selection.

For visual comparison with the official client, use the same character, map,
GRFs, movement speed and attack speed:

1. Walk straight, diagonally and around corners; extend the destination while
   walking. Compare the local player with another player and a moving monster.
2. Attack Poring, Lunatic and Fabre at ordinary and high ASPD. Check the impact
   pose, weapon layers, sound timing and return to idle after the last attack.
3. Take damage and kill each monster. Check the full hit reaction and death
   sequence, including Lunatic's 34-frame death action and the final fade.
4. Change head direction while idle and sitting, then walk and cast skills.
5. Enter an area with an unfamiliar monster and turn the camera during combat;
   check for pose preparation hitches and that hover bounds follow the sprites.
6. Equip Books of Magic (19006/view 1691) and remain stationary for several
   cycles. Check the eight-frame loop while standing and sitting, with each
   head direction and camera facing, on male and female characters. Walk and
   stop, then attack and cast; verify equipment attachments and return to the
   idle loop. Compare the local player with another stationary player.
7. Repeat with animated normal headgear and costumes in each visible slot,
   including several animated items equipped together. Check each item's own
   loop and delay, then remove or replace an item. Check static headgear for
   unintended animation and hover bounds as animated equipment changes pose.
8. Return to character selection and leave the screen idle for several cycles.
   Keep the mouse still to confirm updates occur without user input.
   Check animated equipment on selected and unselected visible characters in
   legacy and Qt UI modes, including Qt CPU composition. Change selection and
   pages, then return; loops should continue, static equipment should stay
   still, and body/hair poses
   and equipment attachment positions should remain correct.
