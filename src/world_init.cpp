#include "world_init.hpp"
#include "tiny_ecs_registry.hpp"
#include "ai_system.hpp"
#include "iostream"
#include "world_system.hpp"

extern float player_max_health;
extern float player_max_energy;

// Create floor tile entity and add to registry.
Entity createFloorTile(RenderSystem *renderer, vec2 pos)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = {mesh.original_size.x * TILE_SCALE, mesh.original_size.x * TILE_SCALE};
	motion.ignore_render_order = true;
	motion.layer = 0;
	// std::cout << mesh.original_size.x << "," << mesh.original_size.y << std::endl;
	// create an empty floor tile component for our character
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::FLOOR_TILE,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createWall(RenderSystem *renderer, vec2 pos)
{
	auto entity = Entity();

	// Get the mesh for the wall, similar to how you got the floor mesh
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Set initial motion values, ensuring the wall is properly scaled and positioned
	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos; // Use the same position or adjust for wall placement
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = {mesh.original_size.x * TILE_SCALE, mesh.original_size.x * TILE_SCALE};
	motion.bb_scale = motion.scale;
	motion.bb_offset = {0.f, 0.f};
	motion.ignore_render_order = true;
	motion.layer = 0;

	// Print mesh size for debugging if needed
	// std::cout << mesh.original_size.x << "," << mesh.original_size.y << std::endl;

	registry.physicsBodies.insert(entity, {BodyType::STATIC});

	// Set up the render request for the wall
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::WALL, // Make sure to use a wall texture
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

// void createRoom(RenderSystem* renderer, WallMap& wallMap, int x_start, int y_start, int width, int height, float tile_scale) {
//	for (int i = x_start; i < x_start + width; ++i) {
//		for (int j = y_start; j < y_start + height; ++j) {
//			// Calculate tile position in the game world
//			vec2 pos = { i * tile_scale, j * tile_scale };
//
//			// Place walls around the edges of the room
//			if (i == x_start || i == x_start + width - 1 || j == y_start || j == y_start + height - 1) {
//				createWall(renderer, pos, tile_scale);
//				wallMap.addWall(i, j);
//			}
//			else {
//				// Place floor tiles inside the room
//				createFloorTile(renderer, pos, tile_scale);
//			}
//		}
//	}
// }

Entity createSpy(RenderSystem *renderer, vec2 pos)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = mesh.original_size * 150.f;
	motion.scale.x *= -0.8;
	motion.bb_scale = {60.f, 60.f};
	motion.bb_offset = {0.f, 40.f};
	motion.layer = 2;

	// create an empty Spy component for our character
	Player &player = registry.players.emplace(entity);
	player.last_health = player_max_health;

	registry.physicsBodies.insert(entity, {BodyType::KINEMATIC});
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::SPY, // TEXTURE_COUNT indicates that no texture is needed
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	Entity healthbar = createHealthBar(renderer, {50.f, 50.f}, entity);
	registry.healths.insert(entity, {player_max_health, player_max_health, healthbar});
	registry.cameraUI.emplace(healthbar);
	Entity energybar = createEnergyBar(renderer, {50.f, 75.f}, entity);
	registry.energys.insert(entity, {player_max_energy, player_max_energy, energybar});
	registry.cameraUI.emplace(energybar);

	return entity;
}

Entity createChef(RenderSystem *renderer, vec2 pos)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = 0.f;
	// motion.velocity = {0.f, 0.f};
	motion.velocity = {70.f, 0.f};
	motion.scale = mesh.original_size * 300.f;
	motion.scale.x *= 1.6;
	motion.bb_scale = {150.f, 130.f};
	motion.bb_offset = {0.f, 40.f};
	motion.layer = 2;

	// create an empty Spy component for our character
	registry.chef.emplace(entity);
	registry.enemies.emplace(entity);

	auto &bossAnimation = registry.bossAnimations.emplace(entity);
	bossAnimation.attack_1 = std::vector<TEXTURE_ASSET_ID>{
			TEXTURE_ASSET_ID::CHEF1_0,
			TEXTURE_ASSET_ID::CHEF1_1,
			TEXTURE_ASSET_ID::CHEF1_2,
			TEXTURE_ASSET_ID::CHEF1_3,
			TEXTURE_ASSET_ID::CHEF1_4,
			TEXTURE_ASSET_ID::CHEF1_5,
			TEXTURE_ASSET_ID::CHEF1_6,
			TEXTURE_ASSET_ID::CHEF1_7,
			TEXTURE_ASSET_ID::CHEF1_8,
			TEXTURE_ASSET_ID::CHEF1_9,
			TEXTURE_ASSET_ID::CHEF1_10,
			TEXTURE_ASSET_ID::CHEF1_11,
	};

	bossAnimation.attack_2 = std::vector<TEXTURE_ASSET_ID>{
			TEXTURE_ASSET_ID::CHEF2_0,
			TEXTURE_ASSET_ID::CHEF2_1,
			TEXTURE_ASSET_ID::CHEF2_2,
			TEXTURE_ASSET_ID::CHEF2_3,
			TEXTURE_ASSET_ID::CHEF2_4,
			TEXTURE_ASSET_ID::CHEF2_5,
			TEXTURE_ASSET_ID::CHEF2_6,
			TEXTURE_ASSET_ID::CHEF2_7,
			TEXTURE_ASSET_ID::CHEF2_8,
			TEXTURE_ASSET_ID::CHEF2_9,
			TEXTURE_ASSET_ID::CHEF2_10,
			TEXTURE_ASSET_ID::CHEF2_11,
			TEXTURE_ASSET_ID::CHEF2_12,
			TEXTURE_ASSET_ID::CHEF2_13,
			TEXTURE_ASSET_ID::CHEF2_14,
			TEXTURE_ASSET_ID::CHEF2_15,
			TEXTURE_ASSET_ID::CHEF2_16,
			TEXTURE_ASSET_ID::CHEF2_17,
			TEXTURE_ASSET_ID::CHEF2_18,
			TEXTURE_ASSET_ID::CHEF2_19,
			TEXTURE_ASSET_ID::CHEF2_20,
	};

	bossAnimation.attack_3 = std::vector<TEXTURE_ASSET_ID>{
			TEXTURE_ASSET_ID::CHEF3_0,
			TEXTURE_ASSET_ID::CHEF3_1,
			TEXTURE_ASSET_ID::CHEF3_2,
			TEXTURE_ASSET_ID::CHEF3_3,
			TEXTURE_ASSET_ID::CHEF3_4,
			TEXTURE_ASSET_ID::CHEF3_5,
			TEXTURE_ASSET_ID::CHEF3_6,
			TEXTURE_ASSET_ID::CHEF3_7,
			TEXTURE_ASSET_ID::CHEF3_8,
			TEXTURE_ASSET_ID::CHEF3_9,
			TEXTURE_ASSET_ID::CHEF3_10,
			TEXTURE_ASSET_ID::CHEF3_11,
			TEXTURE_ASSET_ID::CHEF3_12,
			TEXTURE_ASSET_ID::CHEF3_13,
			TEXTURE_ASSET_ID::CHEF3_14,
			TEXTURE_ASSET_ID::CHEF3_15,
			TEXTURE_ASSET_ID::CHEF3_16,
			TEXTURE_ASSET_ID::CHEF3_17,
			TEXTURE_ASSET_ID::CHEF3_18,
			TEXTURE_ASSET_ID::CHEF3_19,
			TEXTURE_ASSET_ID::CHEF3_20,
	};

	bossAnimation.frame_duration = 100.f; // 0.1s per frame

	registry.physicsBodies.insert(entity, {BodyType::KINEMATIC});
	registry.renderRequests.insert(
			entity,
			{bossAnimation.attack_1[bossAnimation.current_frame], // TEXTURE_COUNT indicates that no texture is needed
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	Entity healthbar = createHealthBar(renderer, motion.position + vec2(0.f, 100.f), entity, vec3(1.f, 0.f, 0.f));
	registry.healths.insert(entity, {350.f, 350.f, healthbar});
	// registry.healths.insert(entity, {5.f, 5.f, healthbar});

	return entity;
}

Entity createKnight(RenderSystem *renderer, vec2 pos)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::KNIGHT);
	registry.meshPtrs.emplace(entity, &mesh);
	TexturedMesh &knight_mesh = renderer->getTexturedMesh(GEOMETRY_BUFFER_ID::KNIGHT);
	registry.texturedMeshPtrs.emplace(entity, &knight_mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);

	motion.position = pos;
	// motion.position = {pos.x - 2000.f, pos.y};

	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	// motion.velocity = {50.f, 0.f};
	motion.scale = mesh.original_size * 300.f;
	motion.scale.x *= 1.2;
	motion.bb_scale = {150.f, 130.f};
	motion.bb_offset = {0.f, 40.f};
	motion.layer = 2;

	registry.knight.emplace(entity);
	registry.enemies.emplace(entity);

	registry.meshBones.insert(entity, {renderer->skinned_meshes[(int)GEOMETRY_BUFFER_ID::KNIGHT].bones});

	registry.physicsBodies.insert(entity, {BodyType::KINEMATIC});
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::KNIGHT,
			 EFFECT_ASSET_ID::SKINNED,
			 GEOMETRY_BUFFER_ID::KNIGHT});

	Entity healthbar = createHealthBar(renderer, motion.position + vec2(0.f, 100.f), entity, vec3(1.f, 0.f, 0.f));
	// registry.healths.insert(entity, {0.5f, 500.f, healthbar});
	registry.healths.insert(entity, {750.f, 750.f, healthbar});

	return entity;
}

Entity createRangedMinion(RenderSystem *renderer, vec2 position)
{

	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.position = position;
	motion.scale = mesh.original_size * 100.f;
	motion.scale.x *= -1;
	motion.bb_scale = {60.f, 60.f};
	motion.bb_offset = {-5.f, 25.f};
	motion.layer = 2;

	registry.rangedminions.emplace(entity);
	registry.enemies.emplace(entity);

	auto &spriteAnimation = registry.spriteAnimations.emplace(entity);
	spriteAnimation.frames = std::vector<TEXTURE_ASSET_ID>{
			TEXTURE_ASSET_ID::RANGEDMINION,
			TEXTURE_ASSET_ID::RANGEDMINION_ATTACK,
	};
	// spriteAnimation.current_frame = 0; // Initialize to a valid frame index
	spriteAnimation.frame_duration = 1000.f;

	registry.physicsBodies.insert(entity, {BodyType::KINEMATIC});
	registry.renderRequests.insert(
			entity,
			{spriteAnimation.frames[spriteAnimation.current_frame],
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	Entity healthbar = createHealthBar(renderer, position + vec2(0.f, 50.f), entity, vec3(1.f,0.f,0.f));
	registry.healths.insert(entity, {50.f, 50.f, healthbar});

	return entity;
}

Entity createArrow(RenderSystem *renderer, vec2 position, vec2 velocity)
{
	auto entity = Entity();

	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	Motion &motion = registry.motions.emplace(entity);
	motion.position = position;
	motion.velocity = velocity;
	motion.angle = atan2(velocity.y, velocity.x);
	motion.scale = {100.f, 20.f};
	float w = motion.scale.x;
	float h = motion.scale.y;
	float cos_theta = std::abs(std::cos(motion.angle));
	float sin_theta = std::abs(std::sin(motion.angle));
	float new_bb_width = w * cos_theta + h * sin_theta;
	float new_bb_height = w * sin_theta + h * cos_theta;
	motion.bb_scale = {new_bb_width, new_bb_height};
	// motion.bb_scale = motion.scale;
	motion.bb_offset = {0.f, 0.f};
	motion.layer = 2;

	registry.damages.insert(entity, {10.f});

	registry.physicsBodies.insert(entity, {BodyType::PROJECTILE});

	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::ARROW,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	// registry.deathTimers.emplace(entity, DeathTimer{5000.f});

	return entity;
}

Entity createPrince(RenderSystem *renderer, vec2 pos)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::PRINCE);
	registry.meshPtrs.emplace(entity, &mesh);
	TexturedMesh &prince_mesh = renderer->getTexturedMesh(GEOMETRY_BUFFER_ID::PRINCE);
	registry.texturedMeshPtrs.emplace(entity, &prince_mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);

	motion.position = pos;
	// motion.position = {pos.x - 2000.f, pos.y};

	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = mesh.original_size * 200.f;
	motion.scale.y *= 1.34;
	motion.bb_scale = {180.f, 180.f};
	motion.bb_offset = {0.f, 30.f};
	motion.layer = 2;

	registry.prince.emplace(entity);
	registry.enemies.emplace(entity);

	// {3000.f, 3000.f, {{}, {}, {}, {{0.f, 0.f}, 30.f / 180.f * M_PI, {1.f, 1.f}}, {}}}, // rotate arm with wand
	// {9000.f, 3000.f, {{}, {}, {}, {{0.f, 0.f}, 30.f / 180.f * M_PI, {1.f, 1.f}}, {{0.03f, -0.18f}, -30.f / 180.f * M_PI, {1.f, 1.f}}}}, // raise wand with arm

	// std::vector<BoneKeyframe> keyframes = {
	// 		{0.0f, 3000.f, {{}, {}, {}, {}, {}}},
	// 		{3000.f, 3000.f, {{}, {}, {{-0.015f, -0.015f}, -30.f / 180.f * M_PI, {1.f, 1.f}}, {}, {}}}, // rotate hand
	// 		{6000.f, 3000.f, {{}, {}, {}, {}, {}}},
	// 		{9000.f, 3000.f, {{}, {{0.f, 0.05f}, 0.f, {.8f, .8f}}, {}, {}, {}}}, // retract head
	// 		{12000.f, 3000.f, {{}, {}, {}, {}, {}}}};

	// BoneAnimation &bone_animation = registry.boneAnimations.emplace(entity);
	// bone_animation.keyframes = keyframes;

	registry.meshBones.insert(entity, {renderer->skinned_meshes[(int)GEOMETRY_BUFFER_ID::PRINCE].bones});

	registry.physicsBodies.insert(entity, {BodyType::KINEMATIC});
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::PRINCE,
			 EFFECT_ASSET_ID::SKINNED,
			 GEOMETRY_BUFFER_ID::PRINCE});

	Entity healthbar = createHealthBar(renderer, motion.position + vec2(0.f, 100.f), entity, vec3(1.f, 0.f, 0.f));
	// registry.healths.insert(entity, {0.5f, 500.f, healthbar});
	registry.healths.insert(entity, {400.f, 400.f, healthbar});

	return entity;
}

Entity createKing(RenderSystem *renderer, vec2 pos)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::KING);
	registry.meshPtrs.emplace(entity, &mesh);
	TexturedMesh &king_mesh = renderer->getTexturedMesh(GEOMETRY_BUFFER_ID::KING);
	registry.texturedMeshPtrs.emplace(entity, &king_mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);

	motion.position = pos;
	// motion.position = {pos.x - 2000.f, pos.y};

	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = mesh.original_size * 210.f;
	motion.scale.y *= 1.36;
	motion.bb_scale = {150.f, 130.f};
	motion.bb_offset = {0.f, 50.f};
	motion.layer = 2;

	registry.king.emplace(entity);
	registry.enemies.emplace(entity);

	// {9000.f, 3000.f, {{}, {{0.f, 0.05f}, 0.f, {.8f, .8f}}, {}, {}, {}}}, // retract head

	// std::vector<BoneKeyframe> keyframes = {
	// 		{0.0f, 3000.f, {{}, {}, {}, {}, {}}},
	// 		{3000.f, 3000.f, {{}, {}, {}, {{0.03f, 0.02f}, 30.f / 180.f * M_PI, {1.f, 1.f}}, {}}}, // rotate arm with staff
	// 		{6000.f, 3000.f, {{}, {}, {}, {}, {}}},
	// 		{9000.f, 3000.f, {{}, {}, {}, {{0.f, -0.05f}, 0.f, {1.f, 1.05f}}, {}}}, // raise staff and arm
	// 		{12000.f, 3000.f, {{}, {}, {}, {}, {}}}};

	// BoneAnimation &bone_animation = registry.boneAnimations.emplace(entity);
	// bone_animation.keyframes = keyframes;

	registry.meshBones.insert(entity, {renderer->skinned_meshes[(int)GEOMETRY_BUFFER_ID::KING].bones});

	registry.physicsBodies.insert(entity, {BodyType::KINEMATIC});
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::KING,
			 EFFECT_ASSET_ID::SKINNED,
			 GEOMETRY_BUFFER_ID::KING});

	Entity healthbar = createHealthBar(renderer, motion.position + vec2(0.f, 100.f), entity, vec3(1.f, 0.f, 0.f));
	// registry.healths.insert(entity, {0.5f, 500.f, healthbar});
	registry.healths.insert(entity, {500.f, 500.f, healthbar});

	return entity;
}

TEXTURE_ASSET_ID getWeaponTexture(WeaponType type, WeaponLevel level)
{
	TEXTURE_ASSET_ID texture;
	switch (type)
	{
	case WeaponType::SWORD:
		switch (level)
		{
		case WeaponLevel::BASIC:
			texture = TEXTURE_ASSET_ID::SWORD_BASIC;
			break;
		case WeaponLevel::RARE:
			texture = TEXTURE_ASSET_ID::SWORD_RARE;
			break;
		case WeaponLevel::LEGENDARY:
			texture = TEXTURE_ASSET_ID::SWORD_LEGENDARY;
			break;
		}
		break;
	case WeaponType::DAGGER:
		switch (level)
		{
		case WeaponLevel::BASIC:
			texture = TEXTURE_ASSET_ID::DAGGER_BASIC;
			break;
		case WeaponLevel::RARE:
			texture = TEXTURE_ASSET_ID::DAGGER_RARE;
			break;
		case WeaponLevel::LEGENDARY:
			texture = TEXTURE_ASSET_ID::DAGGER_LEGENDARY;
			break;
		}
		break;
	}
	return texture;
}

Entity createWeapon(RenderSystem *renderer, vec2 pos, WeaponType type, WeaponLevel level)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = (type == WeaponType::DAGGER)
									 ? renderer->getMesh(GEOMETRY_BUFFER_ID::DAGGER)
									 : renderer->getMesh(GEOMETRY_BUFFER_ID::WEAPON);
	registry.meshPtrs.emplace(entity, &mesh);

	TexturedMesh &weapon_mesh = (type == WeaponType::DAGGER)
																	? renderer->getTexturedMesh(GEOMETRY_BUFFER_ID::DAGGER)
																	: renderer->getTexturedMesh(GEOMETRY_BUFFER_ID::WEAPON);
	registry.texturedMeshPtrs.emplace(entity, &weapon_mesh);

	printf("Mesh selected: %s\n",
				 (type == WeaponType::DAGGER) ? "DAGGER" : "SWORD");

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = type == WeaponType::SWORD ? M_PI / 6 : 0; // 30 degrees
	// motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = mesh.original_size * 100.f;
	motion.scale.x *= 0.45;
	motion.scale.y *= 1.7;
	motion.bb_scale = {max(motion.scale.x, motion.scale.y) * 2.f, max(motion.scale.x, motion.scale.y) * 2.f};
	motion.bb_offset = {0.f, 50.f};
	motion.pivot_offset = {0.f, -0.35f};
	motion.layer = 3;

	registry.physicsBodies.insert(entity, {BodyType::NONE});

	registry.renderRequests.insert(
			entity,
			{getWeaponTexture(type, level),
			 EFFECT_ASSET_ID::TEXTURED,
			 (type == WeaponType::DAGGER)
					 ? GEOMETRY_BUFFER_ID::DAGGER
					 : GEOMETRY_BUFFER_ID::WEAPON});

	printf("RenderRequest: Geometry Buffer ID = %d\n",
				 (type == WeaponType::DAGGER) ? (int)GEOMETRY_BUFFER_ID::DAGGER : (int)GEOMETRY_BUFFER_ID::WEAPON);

	Weapon &weapon = registry.weapons.emplace(entity);
	weapon.type = type;
	weapon.level = level;

	// Assign attack speed and damage based on weapon type and level
	switch (type)
	{
	case WeaponType::SWORD:
		switch (level)
		{
		case WeaponLevel::BASIC:
			weapon.attack_speed = 1.5f; // seconds per attack
			weapon.damage = 20.f;
			break;
		case WeaponLevel::RARE:
			weapon.attack_speed = 1.2f;
			weapon.damage = 35.f;
			break;
		case WeaponLevel::LEGENDARY:
			weapon.attack_speed = 1.0f;
			weapon.damage = 50.f;
			break;
		}
		break;

	case WeaponType::DAGGER:
		switch (level)
		{
		case WeaponLevel::BASIC:
			weapon.attack_speed = 0.8f;
			weapon.damage = 10.f;
			break;
		case WeaponLevel::RARE:
			weapon.attack_speed = 0.6f;
			weapon.damage = 20.f;
			break;
		case WeaponLevel::LEGENDARY:
			weapon.attack_speed = 0.4f;
			weapon.damage = 30.f;
			break;
		}
		break;

	default:
		weapon.attack_speed = 1.0f;
		weapon.damage = 20.f;
		break;
	}

	printf("Weapon created: Type=%d, Level=%d, Damage=%.2f, Attack Speed=%.2f\n",
				 static_cast<int>(type), static_cast<int>(level), weapon.damage, weapon.attack_speed);

	return entity;
}

Entity createHealthBar(RenderSystem *renderer, vec2 pos, Entity owner_entity, vec3 color)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::PROGRESS_BAR);
	registry.meshPtrs.emplace(entity, &mesh);
	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = {200.f, 20.f};
	motion.layer = 5;

	registry.healthbar.insert(entity, {motion.scale});
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::TEXTURE_COUNT, // TEXTURE_COUNT indicates that no texture is needed
			 EFFECT_ASSET_ID::PROGRESS_BAR,
			 GEOMETRY_BUFFER_ID::PROGRESS_BAR});
	// Set to green
	registry.colors.emplace(entity, color);

	return entity;
}

Entity createEnergyBar(RenderSystem *renderer, vec2 pos, Entity owner_entity, vec3 color)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::PROGRESS_BAR);
	registry.meshPtrs.emplace(entity, &mesh);
	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = {200.f, 20.f};
	motion.layer = 5;

	registry.energybar.insert(entity, {motion.scale});
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::TEXTURE_COUNT, // TEXTURE_COUNT indicates that no texture is needed
			 EFFECT_ASSET_ID::PROGRESS_BAR,
			 GEOMETRY_BUFFER_ID::PROGRESS_BAR});
	registry.colors.emplace(entity, color);

	return entity;
}

Entity createFlowMeter(RenderSystem *renderer, vec2 pos, float scale)
{
	auto entity = Entity();

	// Store a reference to the mesh object (assumed you've already defined it)
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Initialize the position, scale, and motion components
	auto &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.scale = {scale, scale};

	// Initialize the flow component
	Flow &flow = registry.flows.emplace(entity);
	flow.flowLevel = 0.f;			 // Start with no flow
	flow.maxFlowLevel = 100.f; // Max flow level can be adjusted as needed

	CameraUI &camera_ui = registry.cameraUI.emplace(entity);
	camera_ui.layer = 1;

	// Create a render request for the flow meter texture
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::FLOW_METER, EFFECT_ASSET_ID::LIQUID_FILL, GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createDamageArea(Entity owner, vec2 position, vec2 scale, float damage, float duration, float damage_cooldown, bool relative_position, vec2 offset_from_owner)
{
	Entity entity = Entity();

	Motion &motion = registry.motions.emplace(entity);
	motion.position = position;
	motion.scale = scale;
	motion.bb_scale = scale;

	registry.damages.insert(entity, {damage});
	registry.physicsBodies.insert(entity, {BodyType::NONE});

	DamageArea &damage_area = registry.damageAreas.emplace(entity);
	damage_area.owner = owner;
	damage_area.active = true;
	damage_area.time_until_active = 0.f;				 // for damage cooldown usage
	damage_area.time_until_destroyed = duration; // when this is 0, damage area will be removed
	damage_area.relative_position = relative_position;
	damage_area.offset_from_owner = offset_from_owner;

	if (damage_cooldown == 0)
	{
		damage_area.single_damage = true;
	}
	else
	{
		damage_area.single_damage = false;
		damage_area.damage_cooldown = damage_cooldown;
	}

	return entity;
}

Entity createEnemy(RenderSystem *renderer, vec2 position)
{
	// Reserve en entity
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Initialize the position, scale, and physics components
	auto &motion = registry.motions.emplace(entity);
	motion.angle = 0.f;
	motion.velocity = {0, 0};
	motion.position = position;
	motion.scale = mesh.original_size * 100.f;
	motion.scale.x *= -0.8;
	motion.bb_scale = {60.f, 60.f};
	motion.bb_offset = {-5.f, 25.f};
	motion.layer = 2;

	// Initialize the animation component with frames
	auto &spriteAnimation = registry.spriteAnimations.emplace(entity);
	spriteAnimation.frames = std::vector<TEXTURE_ASSET_ID>{
			TEXTURE_ASSET_ID::ENEMY,
			TEXTURE_ASSET_ID::ENEMY_ATTACK,
	};
	spriteAnimation.frame_duration = 1000.f; // Set duration for each frame (adjust as needed)

	Enemy &enemy = registry.enemies.emplace(entity);
	enemy.is_minion = true;

	registry.physicsBodies.insert(entity, {BodyType::KINEMATIC});
	registry.renderRequests.insert(
			entity,
			{spriteAnimation.frames[spriteAnimation.current_frame],
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	Entity healthbar = createHealthBar(renderer, position + vec2(0.f, 50.f), entity, vec3(1.f,0.f,0.f));
	registry.healths.insert(entity, {100.f, 100.f, healthbar});

	return entity;
}

Entity createTomato(RenderSystem *renderer, vec2 position, vec2 velocity)
{
	// Reserve en entity
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Initialize the position, scale, and physics components
	auto &motion = registry.motions.emplace(entity);
	motion.angle = 0.f;
	motion.velocity = velocity;
	motion.position = position;
	motion.scale = mesh.original_size * 100.f;
	motion.scale.x *= -0.8;
	motion.bb_scale = {30.f, 30.f};
	motion.bb_offset = {0.f, 0.f};
	motion.layer = 2;

	std::cout << "create tomato" << std::endl;
	registry.damages.insert(entity, {10.f});
	registry.physicsBodies.insert(entity, {BodyType::PROJECTILE});
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::TOMATO,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	// TODO: Need to disappear after hitting player or wall or boundaries.
	return entity;
}

Entity createPan(RenderSystem *renderer, vec2 position, vec2 velocity)
{
	// Reserve en entity
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Initialize the position, scale, and physics components
	auto &motion = registry.motions.emplace(entity);
	motion.angle = 0.f;
	motion.velocity = velocity;
	motion.position = position;
	motion.scale = mesh.original_size * 200.f;
	motion.scale.x *= -0.8;
	motion.bb_scale = {30.f, 30.f};
	motion.bb_offset = {0.f, 0.f};
	motion.layer = 2;

	// Create an (empty) Bug component to be able to refer to all bug
	registry.pans.emplace(entity, Pan(20.f));
	registry.physicsBodies.insert(entity, {BodyType::NONE});
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::PAN,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	// TODO: Need to disappear after hitting player or wall or boundaries.
	return entity;
}

void createBox(vec2 position, vec2 scale, vec3 color, float angle)
{
	float width = 5;
	// scale = {abs(scale.x), abs(scale.y)};
	createLine({position.x, position.y - scale.y / 2.f}, {scale.x + width, width}, color, angle);
	createLine({position.x - scale.x / 2.f, position.y}, {width, scale.y + width}, color, angle);
	createLine({position.x + scale.x / 2.f, position.y}, {width, scale.y + width}, color, angle);
	createLine({position.x, position.y + scale.y / 2.f}, {scale.x + width, width}, color, angle);
}

Entity createLine(vec2 position, vec2 scale, vec3 color, float angle)
{
	Entity entity = Entity();

	// Store a reference to the potentially re-used mesh object (the value is stored in the resource cache)
	registry.renderRequests.insert(
			entity, {TEXTURE_ASSET_ID::TEXTURE_COUNT,
							 EFFECT_ASSET_ID::DEBUG_LINE,
							 GEOMETRY_BUFFER_ID::DEBUG_LINE});

	// Create motion
	Motion &motion = registry.motions.emplace(entity);
	motion.angle = angle;
	motion.velocity = {0, 0};
	motion.position = position;
	motion.scale = scale;
	motion.layer = 6;

	registry.colors.insert(entity, color);

	registry.debugComponents.emplace(entity);
	return entity;
}

Entity createSpinArea(Entity chef_entity)
{
	Entity entity = Entity();
	Motion &chef_motion = registry.motions.get(chef_entity);

	// // Store a reference to the potentially re-used mesh object (the value is stored in the resource cache)
	// registry.renderRequests.insert(
	// 		entity, {TEXTURE_ASSET_ID::TEXTURE_COUNT,
	// 						 EFFECT_ASSET_ID::DEBUG_LINE,
	// 						 GEOMETRY_BUFFER_ID::DEBUG_LINE});

	// Create motion
	Motion &motion = registry.motions.emplace(entity);
	motion.angle = 0.f;
	motion.velocity = {0, 0};
	motion.position = chef_motion.position;
	motion.scale = {200.f, 200.f};
	motion.bb_scale = motion.scale;
	motion.bb_offset = vec2(0.f, 0.f);

	registry.attachments.emplace(entity, Attachment(chef_entity));
	registry.spinareas.emplace(entity, SpinArea());
	registry.physicsBodies.insert(entity, {BodyType::KINEMATIC});
	return entity;
}

Entity createBackdrop(RenderSystem *renderer)
{
	auto entity = Entity();

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = {window_width_px / 2.f, window_height_px / 2.f};
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = {window_width_px, window_height_px};

	CameraUI &camera_ui = registry.cameraUI.emplace(entity);
	camera_ui.layer = 10;
	registry.popupUI.emplace(entity);
	registry.colors.insert(entity, vec3(0.f, 0.f, 0.f));
	registry.opacities.insert(entity, 0.8f);
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::TEXTURE_COUNT,
			 EFFECT_ASSET_ID::PROGRESS_BAR,
			 GEOMETRY_BUFFER_ID::DEBUG_LINE});

	return entity;
}

Entity createDialogueWindow(RenderSystem *renderer, vec2 position, vec2 scale)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Initialize the position, scale, and physics components
	auto &motion = registry.motions.emplace(entity);
	motion.position = position;
	motion.scale = scale;
	// motion.scale *= mesh.original_size * 40.f;
	// motion.scale.x *= 3.3;
	// motion.scale.y *= 0.7;

	CameraUI &camera_ui = registry.cameraUI.emplace(entity);
	camera_ui.layer = 11;
	registry.popupUI.emplace(entity);
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::DIALOGUE_WINDOW,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createFountain(RenderSystem *renderer, vec2 pos)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = {mesh.original_size.x * TILE_SCALE * 4, mesh.original_size.y * TILE_SCALE * 3};
	motion.bb_scale = motion.scale;
	motion.bb_offset = {0.f, 0.f};
	motion.layer = 1;

	registry.fountains.emplace(entity);
	// fountain.is_active = false;

	// registry.physicsBodies.insert(entity, {BodyType::STATIC});
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::FOUNTAIN,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createTreasureBox(RenderSystem *renderer, vec2 pos, TreasureBoxItem item, WeaponType weapon_type, WeaponLevel weapon_level)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = {mesh.original_size.x * TILE_SCALE, mesh.original_size.x * TILE_SCALE};
	motion.bb_scale = motion.scale;
	motion.bb_offset = {0.f, 0.f};
	motion.layer = 2; // same layer as moving entities

	TreasureBox &treasureBox = registry.treasureBoxes.emplace(entity);
	treasureBox.is_open = false;
	treasureBox.item = item;
	treasureBox.weapon_level = weapon_level;
	treasureBox.weapon_type = weapon_type;

	registry.physicsBodies.insert(entity, {BodyType::STATIC});
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::TREASURE_BOX,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createPlayerRemnant(RenderSystem *renderer, Motion motion)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	Motion &created_motion = registry.motions.insert(entity, motion);
	created_motion.velocity = {0.f, 0.f};

	registry.playerRemnants.emplace(entity);
	registry.opacities.insert(entity, 0.5f);
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::SPY,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createKingRemnant(RenderSystem *renderer, Motion motion)
{
	auto entity = Entity();

	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	Motion &created_motion = registry.motions.insert(entity, motion);
	created_motion.velocity = {0.f, 0.f};

	registry.opacities.insert(entity, 0.5f);
	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::KING,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createSoldier(RenderSystem *renderer, vec2 pos, float health, float damage)
{
	auto entity = Entity();

	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = mesh.original_size * 100.f;
	motion.bb_scale = motion.scale;
	motion.bb_offset = {0.f, 0.f};
	motion.layer = 2;

	Enemy &enemy = registry.enemies.emplace(entity);

	registry.physicsBodies.insert(entity, {BodyType::KINEMATIC});

	Entity healthbar = createHealthBar(renderer, pos + vec2(0.f, 50.f), entity);
	registry.healths.insert(entity, {health, health, healthbar});

	registry.damages.insert(entity, {damage});

	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::SUMMON_SOLDIER,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createFireRain(RenderSystem *renderer, vec2 pos)
{
	auto entity = Entity();

	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = mesh.original_size * 300.f;
	motion.bb_scale = {motion.scale.x * 0.95f, motion.scale.y * 0.75f};
	motion.bb_offset = {0.f, 40.f};
	motion.layer = 1;

	registry.physicsBodies.insert(entity, {BodyType::NONE});

	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::FIRERAIN,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createLaser(RenderSystem *renderer, vec2 pos)
{
	auto entity = Entity();

	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	Motion &motion = registry.motions.emplace(entity);
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = {mesh.original_size.x * 500.f, mesh.original_size.y * 40.f};
	motion.bb_scale = motion.scale;
	motion.bb_offset = {0.f, 0.f};
	motion.pivot_offset = {0.5f, 0.f};
	motion.position = {pos.x + motion.scale.x / 2.f, pos.y};
	motion.layer = 3;

	// collision of lasers is handled by the boss ai; the physicsBodies component here is only for visualizing the pivot and bb
	// registry.physicsBodies.insert(entity, {BodyType::NONE});

	registry.renderRequests.insert(
			entity,
			{TEXTURE_ASSET_ID::LASER,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createSprite(RenderSystem *renderer, vec2 pos, vec2 scale, TEXTURE_ASSET_ID texture_id)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = pos;
	motion.angle = 0.f;
	motion.velocity = {0.f, 0.f};
	motion.scale = scale;
	motion.layer = 3;

	registry.renderRequests.insert(
			entity,
			{texture_id,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});

	return entity;
}

Entity createBackgroundSprite(RenderSystem *renderer, int levelNumber)
{
	auto entity = Entity();

	// Store a reference to the potentially re-used mesh object
	Mesh &mesh = renderer->getMesh(GEOMETRY_BUFFER_ID::SPRITE);
	registry.meshPtrs.emplace(entity, &mesh);

	// Setting initial motion values
	Motion &motion = registry.motions.emplace(entity);
	motion.position = {window_width_px / 2.f, window_height_px / 2.f};
	motion.scale = {window_width_px, window_height_px};

	TEXTURE_ASSET_ID backgroundTexture;
	switch (levelNumber)
	{
	case 0:
		backgroundTexture = TEXTURE_ASSET_ID::BACKGROUND_LEVEL_0;
		break;
	case 1:
		backgroundTexture = TEXTURE_ASSET_ID::BACKGROUND_LEVEL_1;
		break;
	case 2:
		backgroundTexture = TEXTURE_ASSET_ID::BACKGROUND_LEVEL_2;
		break;
	case 3:
		backgroundTexture = TEXTURE_ASSET_ID::BACKGROUND_LEVEL_3;
		break;
	default:
		backgroundTexture = TEXTURE_ASSET_ID::BACKGROUND_LEVEL_0;
		break;
	}
	registry.renderRequests.insert(
			entity,
			{backgroundTexture,
			 EFFECT_ASSET_ID::TEXTURED,
			 GEOMETRY_BUFFER_ID::SPRITE});
	CameraUI &camera_ui = registry.cameraUI.emplace(entity);
	camera_ui.ignore_render_order = true;
	registry.backgrounds.emplace(entity);

	return entity;
}
