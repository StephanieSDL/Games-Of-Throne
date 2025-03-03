# Games of Throne - CPSC 427 Team 6 (Winter 2024)

For Playtest Feedback and Changes, refer to the PDF in `doc/Playtest Feedback and Changes.pdf`.

## Coding Convention

- Use snake case (`variable_name_like_this`) for variables and function names whenever possible.
- Prefer capitalized snake case for constants.

# Milestone 4 Feature List

For more details, please refer to the test plan in `doc/test-plan.docx` or the milestone video.

- Player on hit effects and heal effects
- Light attack follows mouse position
- Chest only unlocked after defeating all nearby enemies
- Level layout changes for all levels
- 2 new levels, 2 new bosses (prince, king), skinned motion for new bosses as well
- New story element dialogues
- Level background and popup background
- New font
- New help popup, help shown at start
- Balance changes to player move speed, boss damages and cooldowns.
- 2 new abilities; existing ability updated
- Chest popup improved graphics and formatting
- New, unique, original soundtracks for each level (4 in total)
- Player max health, max energy, weapon, unlocked abilities all saved at beginning of level; can be reloaded upon reopening the application or upon respawning
- Enemy healthbars are now all red
- Ranged minion - new enemy type
- Render order improvements
- View culling to improve render performance

# Milestone 3 Feature List

For more details, please refer to the test plan in `doc/test-plan.docx`.

## General Player Features

Player health and energy bars displayed on the top-left corner.
Energy regenerates while walking and depletes during actions.

## Movement

Basic directional movement using W/A/S/D or arrow keys.
Sprinting with Shift (consumes energy continuously).
Dodging with Spacebar (invincibility during dodge, perfect dodge mechanics with flow gain).

## Combat System

Light attacks with Left Mouse Click (energy required, weapon damage applied).
Flow charge with Right Mouse Click (Hold) and heavy attack on release (high damage, weapon-specific animations).
S mouse gesture enlarges the player and player hitbox for 3 seconds and then returns to normal; future updates will increase the damage area during this state.

## Interactions

Dialogues and popups are navigated with Enter. These occur at moments: when the game starts, when the first minion is killed, when the Chef is first attacked, and when the Chef is defeated.
Treasure boxes and fountains interacted with E (open boxes, heal fully, collect items).
Treasure boxes spawn random weapons.

## Minion AI:

A\* pathfinding to follow the player, visible in debug mode.
Minions attack when within range.

## Chef Boss:

Patrol and combat states.
Randomized attacks: tomato throws, pan throws, dashes.
Dialogue triggered on encounter and defeat.

## Knight Boss:

Patrol and combat states.
Three attack patterns with cooldowns.
Skinned motion animations for attacks.
Shield mechanics (counterattacks when hit during defence).

## Advanced Features M3

A\* Pathfinding: Minions find optimal paths to the player, visible in debug mode.
Skinned Motion: Knight's mesh animations reflect attack types and timing.

## Debug and Gameplay Toggles

I toggles debug mode (visualize bounding boxes, AI paths, weapon collision meshes).
P displays FPS.
O renders help text explaining commands.
0 pauses/unpauses the game.
Debug-exclusive commands:
B: Switch weapons randomly.
R: Reset game state.
H: Instantly defeat the current boss.

## Additional Mechanics

Damage areas visualized in debug mode.
Perfect dodge mechanics with sound, remnant visuals, and flow gain.
Level loader for multiple levels with unique layouts and improved treasure spawn rates.
