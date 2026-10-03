# Actor animation timing

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

Source and diff checks and direct GRF inspection were performed. No build or
in-game validation was run, following the workspace instruction.

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
