#pragma once
#include <vector>

#include "tiny_ecs.hpp"
#include "components.hpp"

class ECSRegistry
{
	// Callbacks to remove a particular or all entities in the system
	std::vector<ContainerInterface *> registry_list;

public:
	// Manually created list of all components this game has
	ComponentContainer<DeathTimer> deathTimers;
	ComponentContainer<Motion> motions;
	ComponentContainer<Animation> animations;
	ComponentContainer<Interpolation> interpolations;
	// ComponentContainer<Bezier> beziers;
	ComponentContainer<Dash> dashes;
	ComponentContainer<Collision> collisions;
	ComponentContainer<PhysicsBody> physicsBodies;
	ComponentContainer<Player> players;
	ComponentContainer<Mesh *> meshPtrs;
	ComponentContainer<TexturedMesh *> texturedMeshPtrs;
	ComponentContainer<RenderRequest> renderRequests;
	ComponentContainer<ScreenState> screenStates;
	ComponentContainer<Damage> damages;
	ComponentContainer<DebugComponent> debugComponents;
	ComponentContainer<vec3> colors;
	ComponentContainer<float> opacities;
	ComponentContainer<Enemy> enemies;
	ComponentContainer<Weapon> weapons;
	ComponentContainer<HealthBar> healthbar;
	ComponentContainer<Health> healths;
	ComponentContainer<EnergyBar> energybar;
	ComponentContainer<Energy> energys;
	ComponentContainer<Flow> flows;
	ComponentContainer<Chef> chef;
	ComponentContainer<Pan> pans;
	ComponentContainer<SpinArea> spinareas;
	ComponentContainer<Attachment> attachments;
	ComponentContainer<SpriteAnimation> spriteAnimations;
	ComponentContainer<CameraUI> cameraUI;
	ComponentContainer<DamageArea> damageAreas;
	ComponentContainer<BossAnimation> bossAnimations;
	ComponentContainer<Knight> knight;
	ComponentContainer<Prince> prince;
	ComponentContainer<King> king;
	ComponentContainer<PopupUI> popupUI;
	ComponentContainer<Fountain> fountains;
	ComponentContainer<TreasureBox> treasureBoxes;
	ComponentContainer<BoneAnimation> boneAnimations;
	ComponentContainer<MeshBones> meshBones;
	ComponentContainer<PlayerRemnant> playerRemnants;
	ComponentContainer<RangedMinion> rangedminions;
	ComponentContainer<BackGround> backgrounds;

	// constructor that adds all containers for looping over them
	// IMPORTANT: Don't forget to add any newly added containers!
	ECSRegistry()
	{
		registry_list.push_back(&deathTimers);
		registry_list.push_back(&motions);
		registry_list.push_back(&animations);
		registry_list.push_back(&collisions);
		registry_list.push_back(&physicsBodies);
		registry_list.push_back(&players);
		registry_list.push_back(&meshPtrs);
		registry_list.push_back(&texturedMeshPtrs);
		registry_list.push_back(&renderRequests);
		registry_list.push_back(&screenStates);
		registry_list.push_back(&damages);
		registry_list.push_back(&debugComponents);
		registry_list.push_back(&colors);
		registry_list.push_back(&opacities);
		registry_list.push_back(&enemies);
		registry_list.push_back(&healthbar);
		registry_list.push_back(&healths);
		registry_list.push_back(&energybar);
		registry_list.push_back(&energys);
		registry_list.push_back(&flows);
		registry_list.push_back(&weapons);
		registry_list.push_back(&interpolations);
		// registry_list.push_back(&beziers);
		registry_list.push_back(&dashes);
		registry_list.push_back(&pans);
		registry_list.push_back(&spinareas);
		registry_list.push_back(&attachments);
		registry_list.push_back(&chef);
		registry_list.push_back(&spriteAnimations);
		registry_list.push_back(&cameraUI);
		registry_list.push_back(&damageAreas);
		registry_list.push_back(&bossAnimations);
		registry_list.push_back(&knight);
		registry_list.push_back(&prince);
		registry_list.push_back(&king);
		registry_list.push_back(&popupUI);
		registry_list.push_back(&fountains);
		registry_list.push_back(&treasureBoxes);
		registry_list.push_back(&boneAnimations);
		registry_list.push_back(&meshBones);
		registry_list.push_back(&playerRemnants);
		registry_list.push_back(&rangedminions);
		registry_list.push_back(&backgrounds);
	}

	void clear_all_components()
	{
		for (ContainerInterface *reg : registry_list)
			reg->clear();
	}

	void list_all_components()
	{
		printf("Debug info on all registry entries:\n");
		for (ContainerInterface *reg : registry_list)
			if (reg->size() > 0)
				printf("%4d components of type %s\n", (int)reg->size(), typeid(*reg).name());
	}

	void list_all_components_of(Entity e)
	{
		printf("Debug info on components of entity %u:\n", (unsigned int)e);
		for (ContainerInterface *reg : registry_list)
			if (reg->has(e))
				printf("type %s\n", typeid(*reg).name());
	}

	void remove_all_components_of(Entity e)
	{
		for (ContainerInterface *reg : registry_list)
			reg->remove(e);
	}
};

extern ECSRegistry registry;