// internal
#include "ai_system.hpp"

#include <iostream>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>
#include <queue>
#include <unordered_map>
#include "world_system.hpp"
#include "world_init.hpp"
#include "physics_system.hpp"

extern bool line_intersects(const vec2 &a1, const vec2 &a2, const vec2 &b1, const vec2 &b2);

extern std::vector<std::vector<int>> level_grid;
float MINION_SPEED = 80.f;

bool isWalkable(int x, int y)
{
	if (x >= 0 && y >= 0 && x < level_grid.size() && y < level_grid[x].size())
	{
		return level_grid[x][y] == 1;
	}
	return false;
}

float distance_squared(vec2 a, vec2 b)
{
	return pow(a.x - b.x, 2) + pow(a.y - b.y, 2);
}

float detection_radius_squared = 280.f * 280.f;
float attack_radius_squared = 110.f * 110.f;

void AISystem::perform_chef_attack(ChefAttack attack)
{
	Chef &chef = registry.chef.components[0];
	Entity chef_entity = registry.chef.entities[0];
	Motion &chef_motion = registry.motions.get(chef_entity);
	Motion &player_motion = registry.motions.get(registry.players.entities[0]);
	vec2 player_position = player_motion.position + player_motion.bb_offset;
	vec2 chef_position = chef_motion.position + chef_motion.bb_offset;
	if (attack == ChefAttack::TOMATO)
	{
		vec2 direction = normalize(player_position - chef_position);
		for (int i = 0; i < 5; i++)
		{
			float angle_offset = -0.3f + (0.6f / 9.0f) * i;
			vec2 spread_direction = vec2((direction.x * cos(angle_offset) - direction.y * sin(angle_offset)), (direction.x * sin(angle_offset) + direction.y * cos(angle_offset)));
			vec2 velocity = spread_direction * 200.f;
			createTomato(renderer, chef_position, velocity);
		}
	}
	else if (attack == ChefAttack::PAN)
	{
		if (chef.pan_active)
		{
			return;
		}
		chef.pan_active = true;
		vec2 direction = normalize(player_position - chef_position);
		vec2 velocity = direction * 500.f;
		createPan(renderer, chef_position, velocity);
	}
	else if (attack == ChefAttack::DASH)
	{
		float dash_speed = 500.f;

		vec2 direction = normalize(player_position - chef_position);
		chef_motion.velocity = direction * dash_speed;
	}
}

void AISystem::play_knight_animation(std::vector<BoneKeyframe> &keyframes)
{
	Entity knight_entity = registry.knight.entities[0];

	if (registry.boneAnimations.has(knight_entity))
	{
		registry.boneAnimations.remove(knight_entity);
	}
	BoneAnimation &bone_animation = registry.boneAnimations.emplace(knight_entity);
	bone_animation.keyframes = keyframes;
}

void AISystem::perform_knight_attack(KnightAttack attack)
{
	if (registry.knight.size() == 0 || registry.players.size() == 0)
		return;

	Knight &knight = registry.knight.components[0];
	Entity knight_entity = registry.knight.entities[0];

	if (!registry.motions.has(knight_entity))
		return;

	Motion &knight_motion = registry.motions.get(knight_entity);

	switch (attack)
	{
	case KnightAttack::DASH_ATTACK:
	{
		std::cout << "Knight dash attack" << std::endl;

		knight.state = KnightState::ATTACK;
		knight.attack_duration = 10000.f;
		knight.time_since_last_attack = 20000.f; // immediately trigger first dash
		knight.dash_has_started = true;
		knight.dash_has_ended = true;
		break;
	}

	case KnightAttack::SHIELD_HOLD:
	{
		std::cout << "Knight shield" << std::endl;

		knight.state = KnightState::SHIELD;
		knight.shield_active = true;
		knight.shield_broken = false;
		knight.shield_duration = 3000.f;
		knight_motion.velocity = {0.f, 0.f};

		std::vector<BoneKeyframe> keyframes = {
				{0.0f, 500.f, {{}, {}, {}, {}}},
				{500.f, 2000.f, {{}, {{0.1f, 0.f}, 0.f, {1.1f, 1.1f}}, {}, {}}},
				{2500.f, 500.f, {{}, {{0.1f, 0.f}, 0.f, {1.1f, 1.1f}}, {}, {}}},
				{3000.f, 0.f, {{}, {}, {}, {}}}};
		play_knight_animation(keyframes);
		break;
	}

	case KnightAttack::MULTI_DASH:
	{
		std::cout << "Knight multi dash" << std::endl;

		knight.state = KnightState::MULTI_DASH;
		knight.dash_count = 0;
		knight.time_since_last_attack = 10000.f; // immediately trigger first dash
		knight.dash_has_started = true;					 // prevent triggering 0th dash
		knight.dash_has_ended = true;
		break;
	}

	default:
		break;
	}
}

void AISystem::play_prince_animation(std::vector<BoneKeyframe> &keyframes)
{
	Entity prince_entity = registry.prince.entities[0];

	if (registry.boneAnimations.has(prince_entity))
	{
		registry.boneAnimations.remove(prince_entity);
	}
	BoneAnimation &bone_animation = registry.boneAnimations.emplace(prince_entity);
	bone_animation.keyframes = keyframes;
}

void AISystem::perform_prince_attack(PrinceAttack attack)
{
	if (registry.prince.size() == 0 || registry.players.size() == 0)
		return;

	Prince &prince = registry.prince.components[0];

	prince.state = PrinceState::ATTACK;
	prince.attack_time_elapsed = 0.f;

	switch (attack)
	{
	case PrinceAttack::WAND_SWING:
	{
		prince.damage_field_created = false;

		std::vector<BoneKeyframe> keyframes = {
				{0.0f, 300.f, {{}, {}, {}, {}, {}}},
				{300.f, 700.f, {{}, {}, {}, {{0.f, 0.f}, -5.f / 180.f * M_PI, {1.f, 1.f}}, {}}}, // pre-attack animation
				{1000.f, 700.f, {{}, {}, {}, {{0.f, 0.f}, 30.f / 180.f * M_PI, {1.f, 1.f}}, {}}},
				{1700.f, 0.f, {{}, {}, {}, {}, {}}}};
		play_prince_animation(keyframes);
		break;
	}
	case PrinceAttack::TELEPORT:
	{
		prince.has_teleported = false;
		prince.has_fired = false;

		Motion &prince_motion = registry.motions.get(registry.prince.entities[0]);
		prince.original_scale = prince_motion.scale;

		break;
	}
	case PrinceAttack::FIELD:
	{
		prince.damage_field_created = false;

		std::vector<BoneKeyframe> keyframes = {
				{0.0f, 500.f, {{}, {}, {}, {}, {}}},
				{500.f, 2000.f, {{}, {}, {}, {{0.f, 0.f}, 30.f / 180.f * M_PI, {1.f, 1.f}}, {{0.03f, -0.18f}, -30.f / 180.f * M_PI, {1.f, 1.f}}}}, // raise wand with arm
				{2500.f, 500.f, {{}, {}, {}, {{0.f, 0.f}, 30.f / 180.f * M_PI, {1.f, 1.f}}, {{0.03f, -0.18f}, -30.f / 180.f * M_PI, {1.f, 1.f}}}},
				{3000.f, 0.f, {{}, {}, {}, {}, {}}}};
		play_prince_animation(keyframes);
		break;
	}
	case PrinceAttack::SUMMON_SPIRITS:
	{
		prince.has_spirits = false;
		prince.spirits_time_elapsed = 0.f;

		std::vector<BoneKeyframe> keyframes = {
				{0.0f, 500.f, {{}, {}, {}, {}, {}}},
				{500.f, 1000.f, {{}, {{0.f, 0.05f}, 0.f, {.8f, .8f}}, {}, {}, {}}}, // retract head
				{1500.f, 500.f, {{}, {{0.f, 0.05f}, 0.f, {.8f, .8f}}, {}, {}, {}}},
				{2000.f, 0.f, {{}, {}, {}, {}, {}}}};
		play_prince_animation(keyframes);
		break;
	}
	default:
		break;
	}
}

void AISystem::process_prince_attack(float elapsed_ms)
{
	if (registry.prince.size() == 0 || registry.players.size() == 0)
		return;

	Prince &prince = registry.prince.components[0];
	Entity prince_entity = registry.prince.entities[0];
	Motion &prince_motion = registry.motions.get(prince_entity);

	if (prince.state != PrinceState::ATTACK)
		return;

	prince.attack_time_elapsed += elapsed_ms;

	switch (prince.current_attack)
	{
	case PrinceAttack::WAND_SWING:
	{
		if (!prince.damage_field_created && prince.attack_time_elapsed >= 300.f)
		{
			prince.damage_field_created = true;
			vec2 prince_position = prince_motion.position + prince_motion.bb_offset;
			createDamageArea(prince_entity, prince_position, prince_motion.bb_scale * 1.5f, 10.f, 1100.f); // last 300ms of swing has no damage
		}
		if (prince.attack_time_elapsed >= 1700.f)
		{
			prince.combat_cooldown = 2500.f;
			prince.state = PrinceState::COMBAT;
		}
		break;
	}
	case PrinceAttack::TELEPORT:
	{
		if (!prince.has_teleported)
		{
			float t = min(prince.attack_time_elapsed / 500.f, 1.f);
			prince_motion.scale = prince.original_scale * (1.f - t);
			if (t >= 1.f)
			{
				prince.has_teleported = true;
				Motion &player_motion = registry.motions.get(registry.players.entities[0]);
				vec2 player_position = player_motion.position + player_motion.bb_offset;
				float teleport_target_side = player_motion.position.x > prince_motion.position.x ? 1.f : -1.f;
				prince_motion.position = player_position + vec2(teleport_target_side * (abs(prince.original_scale.x) * 0.5f + 50.f), 0.f) - prince_motion.bb_offset;
			}
		}
		else if (!prince.has_fired)
		{
			float t = min((prince.attack_time_elapsed - 500.f) / 500.f, 1.f);
			prince_motion.scale = prince.original_scale * t;
			if (t >= 1.f)
			{
				prince.has_fired = true;

				// TODO: replace with pulse that lasts 1000ms
				createDamageArea(prince_entity, prince_motion.position + prince_motion.bb_offset, prince_motion.bb_scale * 1.5f, 10.f, 600.f);

				std::vector<BoneKeyframe> keyframes = {
						{0.0f, 300.f, {{}, {}, {}, {}, {}}},
						{300.f, 300.f, {{}, {}, {{-0.015f, -0.015f}, -30.f / 180.f * M_PI, {1.f, 1.f}}, {}, {}}}, // rotate hand
						{600.f, 0.f, {{}, {}, {}, {}, {}}}};
				play_prince_animation(keyframes);
			}
		}
		else if (prince.has_fired && prince.attack_time_elapsed >= 1600.f)
		{
			prince.combat_cooldown = 6000.f;
			prince.state = PrinceState::COMBAT;
		}
		break;
	}
	case PrinceAttack::FIELD:
	{
		if (!prince.damage_field_created && prince.attack_time_elapsed >= 500.f)
		{
			prince.damage_field_created = true;
			vec2 prince_position = prince_motion.position + prince_motion.bb_offset;
			createDamageArea(prince_entity, prince_position, prince_motion.bb_scale * 2.f, 15.f, 2000.f, 500.f); // damage every 500ms
		}
		if (prince.attack_time_elapsed >= 3000.f)
		{
			prince.combat_cooldown = 6000.f;
			prince.state = PrinceState::COMBAT;
		}
		break;
	}
	case PrinceAttack::SUMMON_SPIRITS:
	{
		if (!prince.has_spirits && prince.attack_time_elapsed >= 500.f)
		{
			prince.has_spirits = true;
			// vec2 prince_position = prince_motion.position + prince_motion.bb_offset;
			std::cout << "Prince summons several spirits (soldiers)!!" << std::endl;

			// summon 5 soldiers around prince
			vec2 pos = prince_motion.position + prince_motion.bb_offset;
			vec2 bb = prince_motion.bb_scale / 2.f;

			float soldier_health = 1.f; // can be defeated in one hit
			float soldier_damage = 10.f;
			std::vector<vec2> positions = {
					{pos.x + bb.x + 40.f, pos.y},
					{pos.x - bb.x - 40.f, pos.y},
					{pos.x, pos.y + bb.y + 40.f},
					{pos.x, pos.y - bb.y - 40.f},
					{pos.x, pos.y + bb.y + 100.f}};

			for (vec2 position : positions)
			{
				// check if position is on a floor
				int tileX = static_cast<int>(position.x / 60);
				int tileY = static_cast<int>(position.y / 60);
				if (isWalkable(tileX, tileY))
				{
					createSoldier(renderer, position, soldier_health, soldier_damage);
				}
			}
		}
		if (prince.attack_time_elapsed >= 2000.f)
		{
			prince.combat_cooldown = 3000.f;
			prince.state = PrinceState::COMBAT;
		}
	}
	default:
		break;
	}
}

void AISystem::play_king_animation(std::vector<BoneKeyframe> &keyframes)
{
	Entity king_entity = registry.king.entities[0];

	if (registry.boneAnimations.has(king_entity))
	{
		registry.boneAnimations.remove(king_entity);
	}
	BoneAnimation &bone_animation = registry.boneAnimations.emplace(king_entity);
	bone_animation.keyframes = keyframes;
}

void AISystem::perform_king_attack(KingAttack attack)
{
	if (registry.king.size() == 0 || registry.players.size() == 0)
		return;

	King &king = registry.king.components[0];
	Entity king_entity = registry.king.entities[0];
	Motion &king_motion = registry.motions.get(king_entity);

	king.state = KingState::ATTACK;
	king.attack_time_elapsed = 0.f;

	switch (attack)
	{
	case KingAttack::LASER:
	{
		std::cout << "Laser attack" << std::endl;
		if ((unsigned int)king.laser_entity != 0)
		{
			registry.remove_all_components_of(king.laser_entity);
			king.laser_entity = Entity(0);
		}

		king.has_fired = false;

		std::vector<BoneKeyframe> keyframes = {
				{0.0f, 500.f, {{}, {}, {}, {}, {}}},
				{500.f, 3000.f, {{}, {}, {}, {{0.f, -0.05f}, 0.f, {1.f, 1.05f}}, {}}}, // raise staff and arm
				{3500.f, 500.f, {{}, {}, {}, {{0.f, -0.05f}, 0.f, {1.f, 1.05f}}, {}}},
				{4000.f, 0.f, {{}, {}, {}, {}, {}}}};
		play_king_animation(keyframes);

		break;
	}
	case KingAttack::DASH_HIT:
	{
		king.dash_counter = 0;
		break;
	}
	case KingAttack::SUMMON_SOLDIERS:
	{
		vec2 pos = king_motion.position + king_motion.bb_offset;
		vec2 bb = king_motion.bb_scale / 2.f;

		float soldier_health = 100.f;
		float soldier_damage = 10.f;

		std::vector<vec2> positions = {
				{pos.x + bb.x + 40.f, pos.y},
				{pos.x - bb.x - 40.f, pos.y}};

		for (vec2 position : positions)
		{
			// check if position is on a floor
			int tileX = static_cast<int>(position.x / 60);
			int tileY = static_cast<int>(position.y / 60);
			if (isWalkable(tileX, tileY))
			{
				createSoldier(renderer, position, soldier_health, soldier_damage);
			}
		}

		king.combat_cooldown = 6000.f;
		king.state = KingState::COMBAT;

		break;
	}
	case KingAttack::FIRE_RAIN:
	{
		if ((unsigned int)king.fire_rain_entity != 0)
		{
			registry.remove_all_components_of(king.fire_rain_entity);
			king.fire_rain_entity = Entity(0);
		}

		king.has_fired = false;
		king.damage_field_created = false;

		std::vector<BoneKeyframe> keyframes = {
				{0.0f, 500.f, {{}, {}, {}, {}, {}}},
				{500.f, 3000.f, {{}, {{0.f, 0.05f}, 0.f, {1.2f, 1.2f}}, {}, {{0.f, -0.05f}, 0.f, {1.f, 1.05f}}, {}}}, // raise staff and arm, enlarge head
				{3500.f, 500.f, {{}, {{0.f, 0.05f}, 0.f, {1.2f, 1.2f}}, {}, {{0.f, -0.05f}, 0.f, {1.f, 1.05f}}, {}}},
				{4000.f, 0.f, {{}, {}, {}, {}, {}}}};
		play_king_animation(keyframes);

		break;
	}
	case KingAttack::TRIPLE_DASH:
	{
		king.dash_counter = 0;
		break;
	}
	case KingAttack::TELEPORT:
	{
		king.has_teleported = false;
		king.has_fired = false;

		// create remnant of king (no longer used)
		// king.remnant_entity = createKingRemnant(renderer, king_motion);

		if (!registry.opacities.has(king_entity))
		{
			registry.opacities.insert(king_entity, 0.5f);
		}
		else
		{
			float &opacity = registry.opacities.get(king_entity);
			opacity = 0.5f;
		}

		// make king temporarily invincible
		king.is_invincible = true;

		break;
	}
	case KingAttack::MORE_SOLDIERS:
	{
		// summon 5 soldiers around king
		vec2 pos = king_motion.position + king_motion.bb_offset;
		vec2 bb = king_motion.bb_scale / 2.f;

		float soldier_health = 150.f;
		float soldier_damage = 10.f;
		std::vector<vec2> positions = {
				{pos.x + bb.x + 40.f, pos.y},
				{pos.x - bb.x - 40.f, pos.y},
				{pos.x, pos.y + bb.y + 40.f},
				{pos.x, pos.y - bb.y - 40.f},
				{pos.x, pos.y + bb.y + 100.f}};

		for (vec2 position : positions)
		{
			// check if position is on a floor
			int tileX = static_cast<int>(position.x / 60);
			int tileY = static_cast<int>(position.y / 60);
			if (isWalkable(tileX, tileY))
			{
				createSoldier(renderer, position, soldier_health, soldier_damage);
			}
		}

		king.combat_cooldown = 8000.f;
		king.state = KingState::COMBAT;

		break;
	}
	default:
		break;
	}
}

void AISystem::process_king_attack(float elapsed_ms)
{
	if (registry.king.size() == 0 || registry.players.size() == 0)
		return;

	King &king = registry.king.components[0];
	Entity king_entity = registry.king.entities[0];
	Motion &king_motion = registry.motions.get(king_entity);

	if (king.state != KingState::ATTACK)
		return;

	king.attack_time_elapsed += elapsed_ms;

	switch (king.current_attack)
	{
	case KingAttack::LASER:
	{
		if (!king.has_fired && king.attack_time_elapsed >= 500.f)
		{
			king.has_fired = true;
			king.laser_damage_cooldown = 0.f;

			king.laser_entity = createLaser(renderer, king_motion.position + king_motion.bb_offset);
		}
		else if (king.attack_time_elapsed >= 4000.f)
		{
			if ((unsigned int)king.laser_entity != 0)
			{
				registry.remove_all_components_of(king.laser_entity);
				king.laser_entity = Entity(0);
			}

			king.combat_cooldown = 5000.f;
			king.state = KingState::COMBAT;
		}
		else if (king.has_fired && king.attack_time_elapsed >= 500.f)
		{
			if (king.laser_damage_cooldown > 0.f)
			{
				king.laser_damage_cooldown -= elapsed_ms;
			}

			float t = min((king.attack_time_elapsed - 500.f) / 3000.f, 1.f);
			float angle = t * 2.f * M_PI;

			if (registry.motions.has(king.laser_entity))
			{
				Motion &laser_motion = registry.motions.get(king.laser_entity);
				laser_motion.angle = angle;

				// Check for angled collision (laser start pos is king pos)
				if (king.laser_damage_cooldown <= 0.f)
				{
					vec2 laser_start = king_motion.position + king_motion.bb_offset;
					float laser_length = laser_motion.bb_scale.x;
					vec2 laser_end = {laser_start.x + cos(angle) * laser_length, laser_start.y + sin(angle) * laser_length};

					Motion &player_motion = registry.motions.get(registry.players.entities[0]);
					vec2 player_position = player_motion.position + player_motion.bb_offset;
					vec2 player_bb = player_motion.bb_scale / 2.f;

					// check laser line intersection with the two diagonals of the player bounding box
					if (line_intersects(laser_start, laser_end, player_position - player_bb, player_position + player_bb) || line_intersects(laser_start, laser_end, player_position + vec2(-player_bb.x, player_bb.y), player_position + vec2(player_bb.x, -player_bb.y)))
					{
						Player &player = registry.players.get(registry.players.entities[0]);
						if (player.can_take_damage())
						{
							std::cout << "Laser hit player for 10 damage" << std::endl;
							Health &player_health = registry.healths.get(registry.players.entities[0]);
							player_health.take_damage(10.f);
							king.laser_damage_cooldown = 500.f;
						}
						else
						{
							std::cout << "Laser damage prevented by player" << std::endl;
						}
					}
				}
			}
		}
		break;
	}
	case KingAttack::DASH_HIT:
	{
		Motion &player_motion = registry.motions.get(registry.players.entities[0]);
		vec2 player_position = player_motion.position + player_motion.bb_offset;
		vec2 king_position = king_motion.position + king_motion.bb_offset;
		if (king.attack_time_elapsed >= 2000.f * (float)king.dash_counter)
		{
			king.dash_counter++;
			king.has_fired = false;
			king.damage_field_created = false;

			if (king.dash_counter == 3)
			{
				king.combat_cooldown = 4000.f;
				king.state = KingState::COMBAT;
				break;
			}

			vec2 direction = normalize(player_position - king_position);
			float dash_speed = 500.f;
			king_motion.velocity = direction * dash_speed;
		}
		else if (!king.has_fired && king.attack_time_elapsed >= 2000.f * (float)king.dash_counter - 1000.f)
		{
			king.has_fired = true;
			// after 1s dash, hit with staff for 1s
			king_motion.velocity = {0.f, 0.f};

			std::vector<BoneKeyframe> keyframes = {
					{0.0f, 500.f, {{}, {}, {}, {}, {}}},
					{500.f, 500.f, {{}, {}, {}, {{0.03f, 0.02f}, 30.f / 180.f * M_PI, {1.f, 1.f}}, {}}}, // rotate arm with staff
					{1000.f, 0.f, {{}, {}, {}, {}, {}}}};
			play_king_animation(keyframes);
		}
		else if (!king.damage_field_created && king.attack_time_elapsed >= 2000.f * (float)king.dash_counter - 500.f)
		{
			king.damage_field_created = true;
			createDamageArea(king_entity, king_position, king_motion.bb_scale * 2.1f, 10.f, 300.f);
		}

		if (length(player_position - king_position) < 150.f)
		{
			king_motion.velocity = {0.f, 0.f};
		}
		break;
	}
	case KingAttack::SUMMON_SOLDIERS:
	{
		break;
	}
	case KingAttack::FIRE_RAIN:
	{
		if (!king.has_fired && king.attack_time_elapsed >= 500.f)
		{
			king.has_fired = true;
			Motion &player_motion = registry.motions.get(registry.players.entities[0]);
			king.fire_rain_entity = createFireRain(renderer, player_motion.position + player_motion.bb_offset);

			registry.opacities.insert(king.fire_rain_entity, 0.2f);
		}
		if (!king.damage_field_created && king.attack_time_elapsed >= 1000.f)
		{
			king.damage_field_created = true;

			if (registry.opacities.has(king.fire_rain_entity))
			{
				registry.opacities.remove(king.fire_rain_entity);
			}

			Motion &fire_rain_motion = registry.motions.get(king.fire_rain_entity);
			createDamageArea(king_entity, fire_rain_motion.position + fire_rain_motion.bb_offset, fire_rain_motion.bb_scale, 15.f, 2000.f, 1000.f);
		}
		else if (!king.damage_field_created && king.has_fired)
		{
			float t = min((king.attack_time_elapsed - 500.f) / 500.f, 1.f);
			if (registry.opacities.has(king.fire_rain_entity))
			{
				float &opacity = registry.opacities.get(king.fire_rain_entity);
				opacity = 0.2f + 0.8f * t;
			}
		}
		if (king.attack_time_elapsed >= 3000.f)
		{
			if ((unsigned int)king.fire_rain_entity != 0)
			{
				registry.remove_all_components_of(king.fire_rain_entity);
				king.fire_rain_entity = Entity(0);
			}

			king.combat_cooldown = 3000.f;
			king.state = KingState::COMBAT;
		}
		break;
	}
	case KingAttack::TRIPLE_DASH:
	{
		Motion &player_motion = registry.motions.get(registry.players.entities[0]);
		vec2 player_position = player_motion.position + player_motion.bb_offset;
		vec2 king_position = king_motion.position + king_motion.bb_offset;
		if (king.attack_time_elapsed >= 1500.f * (float)king.dash_counter)
		{
			king.dash_counter++;
			king.damage_field_created = false;
			king.has_dashed = false;
			king_motion.velocity = {0.f, 0.f};

			if (king.dash_counter == 4)
			{
				king.combat_cooldown = 4000.f;
				king.state = KingState::COMBAT;
				break;
			}

			std::vector<BoneKeyframe> keyframes = {
					{0.0f, 500.f, {{}, {}, {}, {}, {}}},
					{500.f, 400.f, {{}, {{0.f, 0.05f}, 0.f, {.8f, .8f}}, {}, {}, {{0.f, 0.f}, 0.f, {.8f, .8f}}}}, // retract head and feet
					{900.f, 100.f, {{}, {{0.f, 0.05f}, 0.f, {.8f, .8f}}, {}, {}, {{0.f, 0.f}, 0.f, {.8f, .8f}}}}, // retract head and feet
					{1000.f, 0.f, {{}, {}, {}, {}, {}}}};
			play_king_animation(keyframes);
		}
		else if (!king.has_dashed && king.attack_time_elapsed >= 1500.f * (float)king.dash_counter - 1000.f)
		{
			king.has_dashed = true;

			vec2 direction = normalize(player_position - king_position);
			float dash_speed = 500.f;
			king_motion.velocity = direction * dash_speed;

			king.damage_field_created = true;
			createDamageArea(king_entity, king_position, king_motion.bb_scale * 1.8f, 10.f, 1000.f, 0.f, true, king_motion.bb_offset);
		}
		break;
	}
	case KingAttack::TELEPORT:
	{
		if (!king.has_teleported && king.attack_time_elapsed >= 500.f)
		{
			king.has_teleported = true;
			king.is_invincible = false;

			Motion &player_motion = registry.motions.get(registry.players.entities[0]);
			vec2 player_position = player_motion.position + player_motion.bb_offset;
			float teleport_target_side = player_motion.position.x > king_motion.position.x ? 1.f : -1.f;
			king_motion.position = player_position + vec2(teleport_target_side * (abs(king_motion.bb_scale.x) * 0.5f + 50.f), 0.f) - king_motion.bb_offset;

			// destroy remnant
			if ((unsigned int)king.remnant_entity != 0)
			{
				registry.remove_all_components_of(king.remnant_entity);
			}

			// reveal actual king
			if (registry.opacities.has(king_entity))
			{
				registry.opacities.remove(king_entity);
			}

			// play staff animation
			std::vector<BoneKeyframe> keyframes = {
					{0.0f, 500.f, {{}, {}, {}, {}, {}}},
					{500.f, 500.f, {{}, {}, {}, {{0.03f, 0.02f}, 30.f / 180.f * M_PI, {1.f, 1.f}}, {}}}, // rotate arm with staff
					{1000.f, 0.f, {{}, {}, {}, {}, {}}}};
			play_king_animation(keyframes);
		}
		if (!king.has_fired && king.attack_time_elapsed >= 1000.f)
		{
			king.has_fired = true;

			createDamageArea(king_entity, king_motion.position + king_motion.bb_offset, king_motion.bb_scale * 1.5f, 25.f, 200.f);
		}
		if (king.attack_time_elapsed >= 1500.f)
		{
			king.combat_cooldown = 3000.f;
			king.state = KingState::COMBAT;
			king.is_invincible = false;
		}
		break;
	}
	case KingAttack::MORE_SOLDIERS:
	{
		break;
	}
	default:
		break;
	}
}

DecisionNode *AISystem::create_chef_decision_tree()
{
	DecisionNode *chef_decision_tree = new DecisionNode(
			nullptr,
			[](float)
			{
				Chef &chef = registry.chef.components[0];
				return chef.state == ChefState::PATROL;
			});

	// Patrol logic

	DecisionNode *patrol_node = new DecisionNode(
			nullptr,
			[](float)
			{
				Entity &entity = registry.chef.entities[0];
				Motion &motion = registry.motions.get(entity);

				Motion &player_motion = registry.motions.get(registry.players.entities[0]);
				vec2 player_position = player_motion.position + player_motion.bb_offset;
				vec2 chef_position = motion.position + motion.bb_offset;
				return distance_squared(player_position, chef_position) < 330.f * 330.f;
			});
	chef_decision_tree->trueBranch = patrol_node;

	DecisionNode *detected_player = new DecisionNode(
			[](float)
			{
				std::cout << "Chef enters combat" << std::endl;
				Chef &chef = registry.chef.components[0];
				Motion &motion = registry.motions.get(registry.chef.entities[0]);
				chef.state = ChefState::COMBAT;
				chef.trigger = true;
				chef.sound_trigger_timer = 1200.f;
				motion.velocity = {0.f, 0.f};
			},
			nullptr);
	patrol_node->trueBranch = detected_player;

	DecisionNode *patrol_time = new DecisionNode(
			[](float elapsed_ms)
			{
				Chef &chef = registry.chef.components[0];
				chef.time_since_last_patrol += elapsed_ms;
			},
			[](float)
			{
				Chef &chef = registry.chef.components[0];
				return chef.time_since_last_patrol > 2000.f;
			});
	patrol_node->falseBranch = patrol_time;

	DecisionNode *change_patrol_direction = new DecisionNode(
			[](float)
			{
				// std::cout << "Chef is patrolling" << std::endl;
				Chef &chef = registry.chef.components[0];
				Entity &entity = registry.chef.entities[0];
				Motion &motion = registry.motions.get(entity);
				motion.velocity.x *= -1;
				chef.time_since_last_patrol = 0.f;
			},
			nullptr);
	patrol_time->trueBranch = change_patrol_direction;

	// Combat logic

	DecisionNode *combat_state_check = new DecisionNode(
			nullptr,
			[](float)
			{
				Chef &chef = registry.chef.components[0];
				return chef.state == ChefState::COMBAT;
			});
	chef_decision_tree->falseBranch = combat_state_check;

	DecisionNode *combat_node = new DecisionNode(
			[](float elapsed_ms)
			{
				Chef &chef = registry.chef.components[0];
				chef.time_since_last_attack += elapsed_ms;
				if (chef.time_since_last_attack > 1500.f)
				{
					Motion &motion = registry.motions.get(registry.chef.entities[0]);
					motion.velocity = {0.f, 0.f};
				}
			},
			[](float)
			{
				Chef &chef = registry.chef.components[0];
				return chef.time_since_last_attack > 3000.f;
			});
	combat_state_check->trueBranch = combat_node;

	DecisionNode *initiate_attack = new DecisionNode(
			[](float)
			{
				Chef &chef = registry.chef.components[0];
				chef.state = ChefState::ATTACK;
			},
			nullptr);
	combat_node->trueBranch = initiate_attack;

	DecisionNode *attack_node = new DecisionNode(
			[this](float)
			{
				Chef &chef = registry.chef.components[0];
				std::cout << "Chef attacks " << (int)chef.current_attack << std::endl;

				this->perform_chef_attack(chef.current_attack);
				// set is attacking to true
				auto &bossAnimation = registry.bossAnimations.get(registry.chef.entities[0]);
				bossAnimation.is_attacking = true;
				bossAnimation.elapsed_time = 0.f;
				bossAnimation.attack_id = static_cast<int>(chef.current_attack); // Cast to int
				bossAnimation.current_frame = 1;
				// Reset attack and choose next attack
				chef.time_since_last_attack = 0.f;
				chef.current_attack = static_cast<ChefAttack>(rand() % static_cast<int>(ChefAttack::ATTACK_COUNT));
				if (chef.current_attack == ChefAttack::SPIN)
				{
					// TODO: not implemented yet
					chef.current_attack = ChefAttack::DASH;
				}
				chef.state = ChefState::COMBAT;
			},
			nullptr);
	combat_state_check->falseBranch = attack_node;

	return chef_decision_tree;
}

DecisionNode *AISystem::create_knight_decision_tree()
{
	DecisionNode *knight_decision_tree = new DecisionNode(
			nullptr,
			[](float)
			{
				if (registry.knight.size() == 0)
					return false;

				Knight &knight = registry.knight.components[0];
				return knight.state == KnightState::PATROL;
			});

	// PATROL logic
	DecisionNode *patrol_node = new DecisionNode(
			nullptr,
			[](float)
			{
				Entity &entity = registry.knight.entities[0];
				Motion &motion = registry.motions.get(entity);

				Motion &player_motion = registry.motions.get(registry.players.entities[0]);
				vec2 player_position = player_motion.position + player_motion.bb_offset;
				vec2 knight_position = motion.position + motion.bb_offset;
				return distance_squared(player_position, knight_position) < detection_radius_squared;
			});
	knight_decision_tree->trueBranch = patrol_node;

	DecisionNode *detected_player = new DecisionNode(
			[](float)
			{
				std::cout << "Knight enters combat" << std::endl;
				Knight &knight = registry.knight.components[0];
				Motion &motion = registry.motions.get(registry.knight.entities[0]);
				knight.state = KnightState::COMBAT;
				motion.velocity = {0.f, 0.f};
			},
			nullptr);
	patrol_node->trueBranch = detected_player;

	DecisionNode *patrol_time = new DecisionNode(
			[](float elapsed_ms)
			{
				Knight &knight = registry.knight.components[0];
				knight.time_since_last_patrol += elapsed_ms;
			},
			[](float)
			{
				Knight &knight = registry.knight.components[0];
				return knight.time_since_last_patrol > 2000.f;
			});
	patrol_node->falseBranch = patrol_time;

	DecisionNode *change_patrol_direction = new DecisionNode(
			[](float)
			{
				Knight &knight = registry.knight.components[0];
				Entity &entity = registry.knight.entities[0];
				Motion &motion = registry.motions.get(entity);
				if (motion.velocity.x == 0)
				{
					motion.velocity.x = 50.f; // Set initial patrol speed
				}
				else
				{
					motion.velocity.x *= -1; // Change direction
				}
				knight.time_since_last_patrol = 0.f;
			},
			nullptr);
	patrol_time->trueBranch = change_patrol_direction;

	// COMBAT logic
	DecisionNode *combat_state_check = new DecisionNode(
			nullptr,
			[](float)
			{
				Knight &knight = registry.knight.components[0];
				return knight.state == KnightState::COMBAT;
			});
	knight_decision_tree->falseBranch = combat_state_check;

	DecisionNode *combat_cooldown = new DecisionNode(
			[](float elapsed_ms)
			{
				Knight &knight = registry.knight.components[0];
				knight.combat_cooldown -= elapsed_ms;
			},
			[](float)
			{
				Knight &knight = registry.knight.components[0];
				return knight.combat_cooldown <= 0.f;
			});
	combat_state_check->trueBranch = combat_cooldown;

	DecisionNode *attack_selection = new DecisionNode(
			[this](float)
			{
				Knight &knight = registry.knight.components[0];

				// Randomly select an attack
				int random_attack = rand() % 3;
				knight.current_attack = static_cast<KnightAttack>(random_attack);
				// knight.current_attack = KnightAttack::DASH_ATTACK;
				this->perform_knight_attack(knight.current_attack);
			},
			nullptr);
	combat_cooldown->trueBranch = attack_selection;

	return knight_decision_tree;
}

void knight_attack_finished(Knight &knight)
{
	knight.state = KnightState::COMBAT;
	knight.combat_cooldown = 5000.f;
}

DecisionNode *AISystem::create_prince_decision_tree()
{
	DecisionNode *prince_decision_tree = new DecisionNode(
			nullptr,
			[](float)
			{
				Prince &prince = registry.prince.components[0];
				return prince.state != PrinceState::IDLE;
			});

	DecisionNode *combat_state_check = new DecisionNode(
			nullptr,
			[](float)
			{
				Prince &prince = registry.prince.components[0];
				return prince.state == PrinceState::COMBAT;
			});
	prince_decision_tree->trueBranch = combat_state_check;

	DecisionNode *combat_cooldown = new DecisionNode(
			[](float elapsed_ms)
			{
				Prince &prince = registry.prince.components[0];
				prince.combat_cooldown -= elapsed_ms;
			},
			[](float)
			{
				Prince &prince = registry.prince.components[0];
				return prince.combat_cooldown <= 0.f;
			});
	combat_state_check->trueBranch = combat_cooldown;

	DecisionNode *attack_selection = new DecisionNode(
			[this](float)
			{
				Prince &prince = registry.prince.components[0];

				Health &prince_health = registry.healths.get(registry.prince.entities[0]);
				float health_percentage = prince_health.health / prince_health.max_health;
				if ((health_percentage < 0.66f && prince.health_percentage >= 0.66f) || (health_percentage < 0.33f && prince.health_percentage >= 0.33f))
				{
					std::cout << "Prince attack: SUMMON_SPIRITS" << std::endl;
					prince.health_percentage = health_percentage;
					prince.current_attack = PrinceAttack::SUMMON_SPIRITS;
				}
				else
				{
					// Randomly select an attack
					int random_attack = rand() % 3; // 0 to 2

					std::cout << "Prince attack: " << random_attack << std::endl;
					prince.current_attack = static_cast<PrinceAttack>(random_attack);
				}

				// prince.current_attack = PrinceAttack::TELEPORT;
				this->perform_prince_attack(prince.current_attack);
			},
			nullptr);
	combat_cooldown->trueBranch = attack_selection;

	DecisionNode *attack_processing = new DecisionNode(
			[this](float elapsed_ms)
			{
				process_prince_attack(elapsed_ms);
			},
			nullptr);
	combat_state_check->falseBranch = attack_processing; // if not IDLE or COMBAT, must be in attack

	DecisionNode *idle_processing = new DecisionNode(
			[](float)
			{
				Motion &prince_motion = registry.motions.get(registry.prince.entities[0]);
				Motion &player_motion = registry.motions.get(registry.players.entities[0]);
				vec2 player_position = player_motion.position + player_motion.bb_offset;
				vec2 prince_position = prince_motion.position + prince_motion.bb_offset;
				if (distance_squared(player_position, prince_position) < detection_radius_squared)
				{
					Prince &prince = registry.prince.components[0];
					prince.state = PrinceState::COMBAT;
				}
			},
			nullptr);
	prince_decision_tree->falseBranch = idle_processing;

	return prince_decision_tree;
}

DecisionNode *AISystem::create_king_decision_tree()
{
	DecisionNode *king_decision_tree = new DecisionNode(
			nullptr,
			[](float)
			{
				King &king = registry.king.components[0];
				return king.state != KingState::IDLE;
			});

	DecisionNode *combat_state_check = new DecisionNode(
			nullptr,
			[](float)
			{
				King &king = registry.king.components[0];
				return king.state == KingState::COMBAT;
			});
	king_decision_tree->trueBranch = combat_state_check;

	DecisionNode *combat_cooldown = new DecisionNode(
			[](float elapsed_ms)
			{
				King &king = registry.king.components[0];
				king.combat_cooldown -= elapsed_ms;
			},
			[](float)
			{
				King &king = registry.king.components[0];
				return king.combat_cooldown <= 0.f;
			});
	combat_state_check->trueBranch = combat_cooldown;

	DecisionNode *attack_selection = new DecisionNode(
			[this](float)
			{
				King &king = registry.king.components[0];

				if (king.is_second_stage)
				{
					Health &king_health = registry.healths.get(registry.king.entities[0]);
					float health_percentage = king_health.health / king_health.max_health;
					if ((health_percentage < 0.66f && king.health_percentage >= 0.66f) || (health_percentage < 0.33f && king.health_percentage >= 0.33f))
					{
						king.health_percentage = health_percentage;
						king.current_attack = KingAttack::MORE_SOLDIERS; // 6
					}
					else
					{
						int random_attack = 3 + (rand() % 3); // 3 to 5
						king.current_attack = static_cast<KingAttack>(random_attack);
					}
				}
				else
				{
					int random_attack = rand() % 3; // 0 to 2
					king.current_attack = static_cast<KingAttack>(random_attack);
				}

				// king.current_attack = !king.is_second_stage ? KingAttack::TRIPLE_DASH : KingAttack::MORE_SOLDIERS;
				// king.current_attack = KingAttack::MORE_SOLDIERS;
				this->perform_king_attack(king.current_attack);
			},
			nullptr);
	combat_cooldown->trueBranch = attack_selection;

	DecisionNode *attack_processing = new DecisionNode(
			[this](float elapsed_ms)
			{
				this->process_king_attack(elapsed_ms);
			},
			nullptr);
	combat_state_check->falseBranch = attack_processing; // if not IDLE or COMBAT, must be in attack

	DecisionNode *idle_processing = new DecisionNode(
			[](float)
			{
				Motion &king_motion = registry.motions.get(registry.king.entities[0]);
				Motion &player_motion = registry.motions.get(registry.players.entities[0]);
				vec2 player_position = player_motion.position + player_motion.bb_offset;
				vec2 king_position = king_motion.position + king_motion.bb_offset;
				if (distance_squared(player_position, king_position) < detection_radius_squared)
				{
					King &king = registry.king.components[0];
					king.state = KingState::COMBAT;
				}
			},
			nullptr);
	king_decision_tree->falseBranch = idle_processing;

	return king_decision_tree;
}

AISystem::AISystem()
{
	chef_decision_tree = create_chef_decision_tree();
	knight_decision_tree = create_knight_decision_tree();
	prince_decision_tree = create_prince_decision_tree();
	king_decision_tree = create_king_decision_tree();
}

AISystem::~AISystem()
{
	delete chef_decision_tree;
	delete knight_decision_tree;
	delete prince_decision_tree;
	delete king_decision_tree;
}

void AISystem::init(RenderSystem *renderer)
{
	this->renderer = renderer;
}

void AISystem::step(float elapsed_ms, std::vector<std::vector<int>> &levelMap)
{
	Entity player = registry.players.entities[0];
	assert(player);

	Player &player_comp = registry.players.get(player);
	if (player_comp.state == PlayerState::DYING || player_comp.stealth_mode)
	{
		// skip all ai processing if player is dead (or in stealth mode); also make enemies stop moving/attacking
		ComponentContainer<Enemy> &enemies = registry.enemies;
		for (uint i = 0; i < enemies.components.size(); i++)
		{
			Enemy &enemy = enemies.components[i];
			Entity entity = enemies.entities[i];
			Motion &motion = registry.motions.get(entity);
			motion.velocity = {0.f, 0.f};
			if (registry.spriteAnimations.has(entity))
			{
				auto &animation = registry.spriteAnimations.get(entity);
				auto &render_request = registry.renderRequests.get(entity);
				animation.current_frame = 0;
				render_request.used_texture = animation.frames[animation.current_frame];
				registry.spriteAnimations.remove(entity);
			}
			enemy.state = EnemyState::IDLE;
		}
		return;
	}

	Motion &player_motion = registry.motions.get(player);
	vec2 player_position = player_motion.position + player_motion.bb_offset;
	ComponentContainer<Enemy> &enemies = registry.enemies;
	for (uint i = 0; i < enemies.components.size(); i++)
	{
		Enemy &enemy = enemies.components[i];
		Entity entity = enemies.entities[i];

		if (registry.chef.has(entity) || registry.knight.has(entity) || registry.prince.has(entity) || registry.king.has(entity)) // Skip all bosses
		{
			continue;
		}

		Motion &motion = registry.motions.get(entity);
		vec2 enemy_position = motion.position + motion.bb_offset;

		vec2 adjusted_position = enemy_position;

		// consider enemy as still on the last tile if it has not fully moved onto a new tile
		// -- this solves the issue of turning corners
		if (glm::length(enemy_position - enemy.last_tile_position) < TILE_SCALE)
		{
			adjusted_position = enemy.last_tile_position;
		}

		float distance_to_player = distance_squared(player_position, enemy_position);
		if (registry.rangedminions.has(entity))
        {
            RangedMinion& rangedMinion = registry.rangedminions.get(entity);

            if (enemy.state == EnemyState::IDLE)
            {
                if (distance_to_player < detection_radius_squared)
                {
                    enemy.state = EnemyState::COMBAT;
                    std::cout << "Ranged Enemy " << i << " enters combat" << std::endl;
                }
            }
            else if (enemy.state == EnemyState::COMBAT)
            {
                if (distance_to_player > detection_radius_squared * 2)
                {
                    enemy.state = EnemyState::IDLE;
                    motion.velocity = {0.f, 0.f};
                    std::cout << "Ranged Enemy " << i << " enters idle" << std::endl;
                }
                else if (distance_to_player > rangedMinion.attack_radius_squared)
                {
                    // Move towards player
                    vec2 direction = player_position - enemy_position;
                    direction = normalize(direction);
                    motion.velocity = direction * rangedMinion.movement_speed;

                    // Face the player
                    motion.scale.x = (direction.x < 0) ? abs(motion.scale.x) : -abs(motion.scale.x);
                }
                else
                {
                    motion.velocity = {0.f, 0.f};

                    // Face the player
                    vec2 direction = player_position - enemy_position;
                    direction = normalize(direction);
                    motion.scale.x = (direction.x < 0) ? abs(motion.scale.x) : -abs(motion.scale.x);

                    // Attack cooldown
                    enemy.time_since_last_attack += elapsed_ms;
                    if (enemy.time_since_last_attack > rangedMinion.attack_cooldown)
                    {
                        // Shoot arrow
                        vec2 arrow_velocity = normalize(player_position - enemy_position) * rangedMinion.arrow_speed;
                        createArrow(renderer, enemy_position, arrow_velocity);

                        enemy.time_since_last_attack = 0.f;

                        // Play attack animation if available
                        if (registry.spriteAnimations.has(entity))
                        {
                            auto& animation = registry.spriteAnimations.get(entity);
                            auto& render_request = registry.renderRequests.get(entity);

                            // animation.current_frame = 1;
                            // render_request.used_texture = animation.frames[animation.current_frame];
                        }
                    }
                }
            }
            else if (enemy.state == EnemyState::DEAD)
            {
                motion.velocity = {0.f, 0.f};
                continue;
            }
        }
        else if (!registry.rangedminions.has(entity))
		{
		if (enemy.state == EnemyState::IDLE)
		{
			if (distance_to_player < detection_radius_squared)
			{
				enemy.state = EnemyState::COMBAT;
				std::cout << "Enemy " << i << " enters combat" << std::endl;
			}
		}
		else if (enemy.state == EnemyState::COMBAT)
		{
			if (distance_to_player > detection_radius_squared * 2)
			{
				enemy.state = EnemyState::IDLE;
				motion.velocity = {0.f, 0.f};
				std::cout << "Enemy " << i << " returns to idle" << std::endl;
			}
			else
			{

				// std::cout << "Level Grid:" << std::endl;
				// for (int y = 0; y < level_grid[0].size(); ++y)
				// {
				// 	for (int x = 0; x < level_grid.size(); ++x)
				// 	{
				// 		std::cout << level_grid[x][y] << " ";
				// 	}
				// 	std::cout << std::endl;
				// }
				// return;

				std::vector<vec2> path = findPathAStar(adjusted_position, player_position);

				if (debugging.in_debug_mode)
				{
					for (int i = 0; i < path.size(); i++)
					{
						vec2 path_point = path[i];
						createLine(path_point, {10.f, 10.f}, {1.f, 0.f, 0.f}, 0.f);
					}
				}

				if (path.size() >= 1)
				{
					// stagger update last_tile_position only when enemy is fully on the next tile
					if (glm::length(enemy_position - enemy.last_tile_position) > TILE_SCALE)
					{
						// std::cout << "LAST TILE POS UPDATED" << std::endl;
						enemy.last_tile_position = path[0];
					}
				}

				if (path.size() >= 2)
				{
					// Move towards the next point in the path
					vec2 target_position = path[1]; // The next node in the path
					vec2 direction = normalize(target_position - enemy_position);
					motion.velocity = direction * MINION_SPEED;
					// std::cout << "enemy_position: " << enemy_position.x << ", " << enemy_position.y << "; adjusted_position: " << adjusted_position.x << ", " << adjusted_position.y << "; direction: " << direction.x << ", " << direction.y << std::endl;

					enemy.path = path;
				}
				else
				{
					// No path found or already at the goal
					motion.velocity = {0.f, 0.f};
				}

				if (distance_to_player <= attack_radius_squared)
				{
					motion.velocity = {0.f, 0.f};
					enemy.time_since_last_attack += elapsed_ms;
					if (enemy.time_since_last_attack > 2000.f)
					{
						// Attack logic
						enemy.state = EnemyState::ATTACK;

						// get motion of the enemy
						auto &enemy_motion = registry.motions.get(entity);
						enemy_motion.scale.x *= 1.1;

						enemy.time_since_last_attack = 0.f;

						createDamageArea(entity, motion.position, {100.f, 70.f}, 7.f, 500.f, 0.f, true, {50.f, 50.f});
					}
				}
			}
		}

		// reset state to combat after attack
		else if (enemy.state == EnemyState::ATTACK)
		{
			auto &render_request = registry.renderRequests.get(entity);
			enemy.attack_countdown -= elapsed_ms;

			if (enemy.attack_countdown <= 0)
			{
				enemy.state = EnemyState::COMBAT;
				// printf("Enemy %d finish attack\n", i);
				enemy.attack_countdown = 500;
				if (registry.spriteAnimations.has(entity))
				{
					auto &animation = registry.spriteAnimations.get(entity);
					// change to combat animation sprite
					render_request.used_texture = animation.frames[0];
				}
				// get motion of the enemy
				auto &enemy_motion = registry.motions.get(entity);
				enemy_motion.scale.x /= 1.1;
			}
		}
		else if (enemy.state == EnemyState::DEAD)
		{
			motion.velocity = {0.f, 0.f};
			continue;
		}
		}
	}


	if (registry.chef.size() > 0)
	{
		// special behavior for chef
		Entity chef_entity = registry.chef.entities[0];
		Health &chef_health = registry.healths.get(chef_entity);
		if (!chef_health.is_dead)
		{
			chef_decision_tree->execute(elapsed_ms);
		}
		// perform animation if chef is attacking
		auto &bossAnimation = registry.bossAnimations.get(chef_entity);

		if (bossAnimation.is_attacking)
		{
			boss_attack(chef_entity, bossAnimation.attack_id, elapsed_ms);
		}
	}

	if (registry.knight.size() > 0)
	{
		Entity knight_entity = registry.knight.entities[0];
		Health &knight_health = registry.healths.get(knight_entity);
		if (!knight_health.is_dead)
		{
			knight_decision_tree->execute(elapsed_ms);
			Knight &knight = registry.knight.get(knight_entity);

			Motion &knight_motion = registry.motions.get(knight_entity);
			Motion &player_motion = registry.motions.get(registry.players.entities[0]);
			vec2 player_position = player_motion.position + player_motion.bb_offset;
			vec2 knight_position = knight_motion.position + knight_motion.bb_offset;

			if (knight.state == KnightState::SHIELD)
			{
				knight.shield_duration -= elapsed_ms;
				if (knight.shield_duration <= 0.f)
				{
					knight.shield_active = false;
					knight_attack_finished(knight);
				}
				knight_motion.velocity = {0.f, 0.f}; // Stay stationary during shield
			}
			else if (knight.state == KnightState::ATTACK)
			{
				knight.attack_duration -= elapsed_ms;
				knight.time_since_last_attack += elapsed_ms;

				if (knight.attack_duration <= 0.f)
				{
					knight_attack_finished(knight);
					knight_motion.velocity = {0.f, 0.f};
				}
				else if (knight.time_since_last_attack >= 2500.f) // so 10s we can do 4 attacks
				{
					// Dash to player (no damage)
					vec2 direction = normalize(player_position - knight_position);
					knight_motion.velocity = direction * 300.f;
					knight.time_since_last_attack = 0.f;
					knight.dash_has_ended = false;

					float angle_to_player = atan2(player_position.y - knight_position.y, player_position.x - knight_position.x);

					std::cout << "angle to player: " << angle_to_player << std::endl;

					// limit head tilt to +- 45 degrees
					if (angle_to_player > 45.f / 180.f * M_PI)
					{
						angle_to_player = 45.f / 180.f * M_PI;
					}
					else if (angle_to_player < -45.f / 180.f * M_PI)
					{
						angle_to_player = -45.f / 180.f * M_PI;
					}

					// Start dash animation
					std::vector<BoneKeyframe> keyframes = {
							{0.0f, 250.f, {{}, {}, {}, {}}},
							{250.f, 500.f, {{}, {}, {{0.f, 0.f}, angle_to_player, {1.f, 1.f}}, {}}},
							{750.f, 250.f, {{}, {}, {{0.f, 0.f}, angle_to_player, {1.f, 1.f}}, {}}},
							{1000.f, 0.f, {{}, {}, {}, {}}}};
					play_knight_animation(keyframes);
				}
				else if (!knight.dash_has_ended && knight.time_since_last_attack >= 1000.f)
				{
					knight.dash_has_ended = true;
					knight_motion.velocity = {0.f, 0.f};

					// Start attack animation
					std::vector<BoneKeyframe> keyframes = {
							{0.0f, 500.f, {{}, {}, {}, {}}},
							{500.f, 500.f, {{}, {}, {}, {{-0.08f, 0.1f}, -45.f / 180.f * M_PI, {1.f, 1.f}}}},
							{1000.f, 0.f, {{}, {}, {}, {}}}};
					play_knight_animation(keyframes);

					// immediately start damage area (no delay)
					createDamageArea(knight_entity, knight_position, knight_motion.bb_scale * 2.2f, 9.f, 1000.f, 0.f);
				}
			}
			else if (knight.state == KnightState::MULTI_DASH)
			{
				knight.time_since_last_attack += elapsed_ms;

				// dash animation takes 1500; then there is a 1s gap
				if (knight.time_since_last_attack >= 2500.f)
				{
					if (knight.dash_count >= 3)
					{
						// Start damage field after 3 dashes
						knight_motion.velocity = {0.f, 0.f};
						knight.state = KnightState::DAMAGE_FIELD;
						knight.time_since_damage_field = 0.f;
						knight.damage_field_active = false;

						// Start damage field animation
						std::vector<BoneKeyframe> keyframes = {
								{0.0f, 500.f, {{}, {}, {}, {}}},
								{500.f, 3000.f, {{}, {}, {}, {{0.f, 0.2f}, 0.f, {1.f, 1.1f}}}},
								{3500.f, 500.f, {{}, {}, {}, {{0.f, 0.2f}, 0.f, {1.f, 1.1f}}}},
								{4000.f, 0.f, {{}, {}, {}, {}}}};
						play_knight_animation(keyframes);
					}
					else
					{
						knight.dash_has_started = false;
						knight.dash_has_ended = false;
						knight.time_since_last_attack = 0.f;
						knight.dash_count++;

						// Start dash-attack animation
						std::vector<BoneKeyframe> keyframes = {
								{0.0f, 750.f, {{}, {}, {}, {}}},
								{750.f, 750.f, {{}, {}, {}, {{-0.12f, -0.14f}, 45.f / 180.f * M_PI, {1.f, 1.f}}}},
								{1500.f, 0.f, {{}, {}, {}, {}}}};
						play_knight_animation(keyframes);
					}
				}
				else if (!knight.dash_has_ended && knight.time_since_last_attack >= 1500.f)
				{
					knight.dash_has_ended = true;

					knight_motion.velocity = {0.f, 0.f};
				}
				else if (!knight.dash_has_started && knight.time_since_last_attack >= 500.f)
				{
					knight.dash_has_started = true;

					vec2 direction = normalize(player_position - knight_position);
					knight_motion.velocity = direction * 400.f;
					createDamageArea(knight_entity, knight_position, knight_motion.bb_scale * 1.8f, 15.f, 1000.f, 0.f, true, knight_motion.bb_offset);
				}
			}
			else if (knight.state == KnightState::DAMAGE_FIELD)
			{
				knight.time_since_damage_field += elapsed_ms;

				// only start actual damage after 500ms to allow animation to play first
				if (!knight.damage_field_active && knight.time_since_damage_field >= 500.f)
				{
					// actual damage area lasts 3000ms, 500ms before and after is for animation
					// can repeatly damage every 1000ms
					createDamageArea(knight_entity, knight_position, vec2(400.f, 400.f), 18.f, 3000.f, 1000.f);
					knight.damage_field_active = true;
				}

				// 4000.f matches total animation time
				if (knight.time_since_damage_field >= 4000.f)
				{
					knight_attack_finished(knight);
				}
			}
		}
	}

	if (registry.prince.size() > 0)
	{
		Entity prince_entity = registry.prince.entities[0];
		Health &prince_health = registry.healths.get(prince_entity);
		if (!prince_health.is_dead)
		{
			prince_decision_tree->execute(elapsed_ms);
		}
	}

	if (registry.king.size() > 0)
	{
		Entity king_entity = registry.king.entities[0];
		Health &king_health = registry.healths.get(king_entity);
		if (!king_health.is_dead)
		{
			king_decision_tree->execute(elapsed_ms);
		}
	}
}

void AISystem::boss_attack(Entity entity, int attack_id, float elapsed_ms)
{
	// change to attack animation sprite
	auto &bossAnimation = registry.bossAnimations.get(entity);
	auto &render_request = registry.renderRequests.get(entity);
	bossAnimation.elapsed_time += elapsed_ms;

	std::vector<TEXTURE_ASSET_ID> &frames = bossAnimation.attack_1;
	// get motion
	auto &motion = registry.motions.get(entity);

	switch (attack_id)
	{
	case 0:
		frames = bossAnimation.attack_1;
		bossAnimation.frame_duration = 100.f;
		break;
	case 1:
		frames = bossAnimation.attack_2;
		bossAnimation.frame_duration = 70.f;
		break;
	case 2:
		frames = bossAnimation.attack_3;
		bossAnimation.frame_duration = 100.f;
		break;
	case 3:
		frames = bossAnimation.attack_1; // modify if have >3 animations, default to attack_1
		break;
	case 4:
		frames = bossAnimation.attack_1;
		break;
	}

	if (frames.empty())
	{
		std::cerr << "Error: No frames available for Knight's attack animation (attack_id: " << attack_id << ")." << std::endl;
		bossAnimation.is_attacking = false;
		return;
	}

	if (bossAnimation.elapsed_time >= bossAnimation.frame_duration)
	{
		motion.scale.x /= 1.6;
		// Reset elapsed time for the next frame
		bossAnimation.elapsed_time = 0.0f;

		// Move to the next frame
		bossAnimation.current_frame = (bossAnimation.current_frame + 1) % frames.size();
		render_request.used_texture = frames[bossAnimation.current_frame];
		motion.scale.x *= 1.6;
	}
	if (bossAnimation.current_frame == 0 && bossAnimation.is_attacking)
	{
		bossAnimation.is_attacking = false;
		// change to combat animation sprite
		render_request.used_texture = frames[0];
	}
}

float heuristic(int x1, int y1, int x2, int y2)
{
	return static_cast<float>(std::abs(x1 - x2) + std::abs(y1 - y2)); // Manhattan distance
}

int nodeKey(int x, int y, int maxHeight)
{
	return x * maxHeight + y;
}

std::vector<vec2> AISystem::findPathAStar(vec2 startPos, vec2 goalPos)
{
	const int TILE_SIZE = 60;
	int startX = static_cast<int>(startPos.x / TILE_SIZE);
	int startY = static_cast<int>(startPos.y / TILE_SIZE);
	int goalX = static_cast<int>(goalPos.x / TILE_SIZE);
	int goalY = static_cast<int>(goalPos.y / TILE_SIZE);

	// std::cout << "===========================" << std::endl;

	// for (int dy = -2; dy <= 2; dy++)
	// {
	// 	for (int dx = -2; dx <= 2; dx++)
	// 	{
	// 		std::cout << isWalkable(goalX + dx, goalY + dy) << " ";
	// 	}
	// 	std::cout << std::endl;
	// }
	// std::cout << "===========================" << std::endl;

	int maxHeight = level_grid[0].size();

	// Comparator for priority queue
	struct CompareAStarNode
	{
		bool operator()(AStarNode *a, AStarNode *b)
		{
			return a->fCost > b->fCost;
		}
	};

	std::priority_queue<AStarNode *, std::vector<AStarNode *>, CompareAStarNode> openSet;
	std::unordered_map<int, AStarNode *> allNodes;

	int key = nodeKey(startX, startY, maxHeight);
	AStarNode *startNode = new AStarNode{startX, startY, 0, heuristic(startX, startY, goalX, goalY), 0, nullptr};
	startNode->fCost = startNode->gCost + startNode->hCost;
	openSet.push(startNode);
	allNodes[key] = startNode;

	std::vector<std::pair<int, int>> directions = {
			{0, -1},	// Up
			{1, 0},		// Right
			{0, 1},		// Down
			{-1, 0},	// Left
			{-1, -1}, // Up-Left
			{1, -1},	// Up-Right
			{1, 1},		// Down-Right
			{-1, 1}		// Down-Left
	};

	while (!openSet.empty())
	{
		// Get the node with the lowest fCost
		AStarNode *current = openSet.top();
		openSet.pop();

		// If we've reached the goal, reconstruct and return the path
		if (current->x == goalX && current->y == goalY)
		{
			// Reconstruct path
			std::vector<vec2> path;
			AStarNode *node = current;
			while (node != nullptr)
			{
				path.push_back(vec2{node->x * TILE_SIZE + TILE_SIZE / 2.0f,
														node->y * TILE_SIZE + TILE_SIZE / 2.0f});
				node = node->parent;
			}
			std::reverse(path.begin(), path.end());

			// Clean up all dynamically allocated nodes
			for (auto &pair : allNodes)
			{
				delete pair.second;
			}
			return path;
		}

		// Explore all valid neighbors of the current node
		for (const auto &dir : directions)
		{
			int nx = current->x + dir.first;	// Neighbor x-coordinate
			int ny = current->y + dir.second; // Neighbor y-coordinate

			if (dir.first != 0 && dir.second != 0)
			{ // Diagonal movement
				if (!isWalkable(current->x + dir.first, current->y) ||
						!isWalkable(current->x, current->y + dir.second))
				{
					continue;
				}
			}

			if (isWalkable(nx, ny))
			{
				int neighborKey = nodeKey(nx, ny, maxHeight); // Generate unique key for the neighbor
				// float gCost = current->gCost + ((dir.first != 0 && dir.second != 0) ? 1.4f : 1.0f); // Diagonal cost is ~?2 (1.4)
				float baseCost = (dir.first != 0 && dir.second != 0) ? 1.4f : 1.0f;
				float gCost = current->gCost + baseCost;
				float hCost = heuristic(nx, ny, goalX, goalY); // Calculate heuristic cost (Manhattan distance)
				float fCost = gCost + hCost;									 // Total cost = gCost + hCost

				if (allNodes.find(neighborKey) == allNodes.end() || gCost < allNodes[neighborKey]->gCost)
				{
					// Create or update the neighbor node
					AStarNode *neighbor = new AStarNode{nx, ny, gCost, hCost, fCost, current};
					openSet.push(neighbor);						// Add to the open set
					allNodes[neighborKey] = neighbor; // Update the allNodes map
				}
			}
		}
	}

	// No path found; clean up dynamically allocated nodes
	for (auto &pair : allNodes)
	{
		delete pair.second;
	}

	return {};
}

// sources for A*:
// https://www.youtube.com/watch?v=-L-WgKMFuhE
// https://www.youtube.com/watch?v=NJOf_MYGrYs&t=876s
