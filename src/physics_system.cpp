// internal
#include "physics_system.hpp"
#include "world_init.hpp"

#include <iostream>

// Returns the local bounding coordinates scaled by the current size of the entity
vec2 get_bounding_box(const Motion &motion)
{
	return {abs(motion.bb_scale.x), abs(motion.bb_scale.y)};
}

vec2 xy(const vec3 &v)
{
	return {v.x, v.y};
}

bool collides(const Motion &motion1, const Motion &motion2)
{
	// axis-aligned bounding box (AABB) collision detection
	vec2 bb1 = get_bounding_box(motion1);
	vec2 bb2 = get_bounding_box(motion2);
	vec2 pos1 = motion1.position + motion1.bb_offset - bb1 / 2.f;
	vec2 pos2 = motion2.position + motion2.bb_offset - bb2 / 2.f;
	return pos1.x + bb1.x >= pos2.x && pos2.x + bb2.x >= pos1.x && pos1.y + bb1.y >= pos2.y && pos2.y + bb2.y >= pos1.y;
}

bool is_point_in_box(const vec2 &p, const vec2 &box_pos, const vec2 &box_bb)
{
	return p.x >= box_pos.x && p.x <= box_pos.x + box_bb.x && p.y >= box_pos.y && p.y <= box_pos.y + box_bb.y;
}

// computes the cross product of two vectors
float cross(const vec2 &a, const vec2 &b)
{
	return a.x * b.y - a.y * b.x;
}

bool line_intersects(const vec2 &a1, const vec2 &a2, const vec2 &b1, const vec2 &b2)
{
	// checks if the line segment (a1, a2) intersects with (b1, b2)
	// inspired by https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
	vec2 p = a1;
	vec2 r = a2 - a1;
	vec2 q = b1;
	vec2 s = b2 - b1;
	float rxs = cross(r, s);
	float qpxr = cross(q - p, r);
	if (rxs == 0)
	{
		if (qpxr == 0)
		{
			// colinear
			float t0 = glm::dot(q - p, r) / glm::dot(r, r);
			float t1 = t0 + glm::dot(s, r) / glm::dot(r, r);
			// intersects if [t0, t1] intersects [0, 1]
			return !(min(t0, t1) > 1 || max(t0, t1) < 0);
		}
		// parallel
		return false;
	}
	float t = cross(q - p, s) / rxs;
	float u = cross(q - p, r) / rxs;
	return t >= 0 && t <= 1 && u >= 0 && u <= 1;
}

bool mesh_collides(Entity mesh_entity, const Motion &mesh_motion, const Motion &box_motion)
{
	// ONLY WEAPON MESH is supported
	if (!registry.texturedMeshPtrs.has(mesh_entity))
	{
		std::cout << "mesh_collides: mesh_entity not a textured mesh" << std::endl;
		return false;
	}
	TexturedMesh *mesh = registry.texturedMeshPtrs.get(mesh_entity);
	auto &vertices = mesh->vertices;
	auto &indices = mesh->vertex_indices;
	// use original bounding box (without bb_scale) for actual mesh collision use
	// vec2 mesh_bb = {abs(mesh_motion.scale.x), abs(mesh_motion.scale.y)};
	// vec2 mesh_center = mesh_motion.position;
	vec2 box_bb = get_bounding_box(box_motion);
	vec2 box_pos = box_motion.position + box_motion.bb_offset - box_bb / 2.f;
	glm::mat3 transform = get_transform(mesh_motion);
	for (uint i = 0; i < indices.size(); i += 3)
	{
		vec2 v1 = vertices[indices[i]].position;
		vec2 v2 = vertices[indices[i + 1]].position;
		vec2 v3 = vertices[indices[i + 2]].position;
		vec2 p1 = xy(transform * vec3(v1, 1));
		vec2 p2 = xy(transform * vec3(v2, 1));
		vec2 p3 = xy(transform * vec3(v3, 1));
		// collides if any of the three points of the triangle is inside the box
		if (is_point_in_box(p1, box_pos, box_bb) || is_point_in_box(p2, box_pos, box_bb) || is_point_in_box(p3, box_pos, box_bb))
		{
			// std::cout << "mesh_collides point_in_box" << std::endl;
			return true;
		}
		// collides if any of the three edges of the triangle intersects with the box
		if (line_intersects(p1, p2, box_pos, box_pos + vec2{box_bb.x, 0}) ||
				line_intersects(p1, p2, box_pos, box_pos + vec2{0, box_bb.y}) ||
				line_intersects(p1, p2, box_pos + box_bb, box_pos + box_bb - vec2{box_bb.x, 0}) ||
				line_intersects(p1, p2, box_pos + box_bb, box_pos + box_bb - vec2{0, box_bb.y}) ||
				line_intersects(p2, p3, box_pos, box_pos + vec2{box_bb.x, 0}) ||
				line_intersects(p2, p3, box_pos, box_pos + vec2{0, box_bb.y}) ||
				line_intersects(p2, p3, box_pos + box_bb, box_pos + box_bb - vec2{box_bb.x, 0}) ||
				line_intersects(p2, p3, box_pos + box_bb, box_pos + box_bb - vec2{0, box_bb.y}) ||
				line_intersects(p3, p1, box_pos, box_pos + vec2{box_bb.x, 0}) ||
				line_intersects(p3, p1, box_pos, box_pos + vec2{0, box_bb.y}) ||
				line_intersects(p3, p1, box_pos + box_bb, box_pos + box_bb - vec2{box_bb.x, 0}) ||
				line_intersects(p3, p1, box_pos + box_bb, box_pos + box_bb - vec2{0, box_bb.y}))
		{
			// std::cout << "mesh_collides line_intersects" << std::endl;
			return true;
		}
	}
	return false;
}

void PhysicsSystem::step(float elapsed_ms)
{
	// Move all entities according to their velocity
	auto &motion_registry = registry.motions;
	for (uint i = 0; i < motion_registry.size(); i++)
	{
		Motion &motion = motion_registry.components[i];
		// Entity entity = motion_registry.entities[i];
		float step_seconds = elapsed_ms / 1000.f;
		motion.position += motion.velocity * step_seconds;
	}

	// After movement, before collision checks. Do all relative position calculations here
	Entity player = registry.players.entities[0];
	Player &player_comp = registry.players.components[0];
	Motion &player_motion = registry.motions.get(player);
	// Set weapon position to correct offset from player
	Entity weapon = player_comp.weapon;
	Motion &weapon_motion = registry.motions.get(weapon);
	vec2 &weapon_offset = player_comp.weapon_offset;
	if (player_motion.scale.x < 0 && weapon_offset.x < 0)
	{
		weapon_offset.x = abs(weapon_offset.x);
		weapon_motion.angle = -weapon_motion.angle;
	}
	else if (player_motion.scale.x > 0 && weapon_offset.x > 0)
	{
		weapon_offset.x = -abs(weapon_offset.x);
		weapon_motion.angle = -weapon_motion.angle;
	}
	weapon_motion.position = player_motion.position + weapon_offset;

	// Update damage area status & position
	for (int i = registry.damageAreas.components.size() - 1; i >= 0; i--)
	{
		DamageArea &damage_area = registry.damageAreas.components[i];
		Entity owner_entity = damage_area.owner;
		Entity damage_area_entity = registry.damageAreas.entities[i];

		if (damage_area.relative_position && registry.motions.has(owner_entity) && registry.motions.has(damage_area_entity))
		{
			Motion &owner_motion = registry.motions.get(owner_entity);
			Motion &damage_area_motion = registry.motions.get(damage_area_entity);

			damage_area_motion.position = owner_motion.position + damage_area.offset_from_owner;
		}

		damage_area.time_until_destroyed -= elapsed_ms;
		if (damage_area.time_until_destroyed <= 0.f)
		{
			registry.remove_all_components_of(damage_area_entity);
		}

		if (!damage_area.active)
		{
			damage_area.time_until_active -= elapsed_ms;
			if (damage_area.time_until_active <= 0.f)
			{
				damage_area.active = true;
			}
		}
	}

	// Check for collisions between all moving entities
	ComponentContainer<PhysicsBody> &physicsBody_container = registry.physicsBodies;
	std::set<Entity> entities_to_remove;
	for (uint i = 0; i < physicsBody_container.components.size(); i++)
	{
		PhysicsBody &physicsBody_i = physicsBody_container.components[i];
		if (physicsBody_i.body_type == BodyType::STATIC)
		{
			continue;
		}
		Entity entity_i = physicsBody_container.entities[i];
		Motion &motion_i = motion_registry.get(entity_i);

		for (uint j = 0; j < physicsBody_container.components.size(); j++)
		{
			if (j == i)
			{
				continue;
			}
			PhysicsBody &physicsBody_j = physicsBody_container.components[j];
			if (j < i && physicsBody_j.body_type != BodyType::STATIC)
			{
				// this pair is already processed
				continue;
			}
			Entity entity_j = physicsBody_container.entities[j];
			Motion &motion_j = motion_registry.get(entity_j);

			vec2 b1 = get_bounding_box(motion_i);
			vec2 b2 = get_bounding_box(motion_j);
			vec2 p1 = motion_i.position + motion_i.bb_offset - b1 / 2.f;
			vec2 p2 = motion_j.position + motion_j.bb_offset - b2 / 2.f;

			if (collides(motion_i, motion_j))
			{
				if (entity_i == weapon || entity_j == weapon)
				{
					if (entity_i == player || entity_j == player)
					{
						// ignore weapon collision with player
						continue;
					}
					if (entity_i == weapon && !mesh_collides(entity_i, motion_i, motion_j))
					{
						continue;
					}
					if (entity_j == weapon && !mesh_collides(entity_j, motion_j, motion_i))
					{
						continue;
					}
				}

				if (physicsBody_i.body_type == BodyType::PROJECTILE && physicsBody_j.body_type == BodyType::PROJECTILE)
				{
					// projectiles do not collide with each other
					continue;
				}
				if (physicsBody_i.body_type == BodyType::PROJECTILE || physicsBody_j.body_type == BodyType::PROJECTILE)
				{
					bool is_i_projectile = physicsBody_i.body_type == BodyType::PROJECTILE;
					Entity projectile_entity = is_i_projectile ? entity_i : entity_j;
					Entity other_entity = is_i_projectile ? entity_j : entity_i;
					PhysicsBody &other_body = is_i_projectile ? physicsBody_j : physicsBody_i;

					// std::cout << "projectile_entity: " << projectile_entity << " other_entity: " << other_entity << std::endl;
					// registry.list_all_components_of(projectile_entity);
					// registry.list_all_components_of(other_entity);
					// std::cout << "END" << std::endl;

					// projectiles can only damage players but not other entities
					if (other_entity == player)
					{
						if (player_comp.can_take_damage())
						{
							Health &player_health = registry.healths.get(player);
							Damage &projectile_damage = registry.damages.get(projectile_entity);
							player_health.take_damage(projectile_damage.damage);
						}

						entities_to_remove.insert(projectile_entity);
					}
					else if (other_body.body_type == BodyType::STATIC)
					{
						// projectiles are destroyed when they hit walls
						entities_to_remove.insert(projectile_entity);
					}
					continue;
				}

				// std::cout << "position of i: " << p1.x << "," << p1.y << "; position of j: " << p2.x << "," << p2.y << std::endl;
				// std::cout << "bb of i: " << b1.x << "," << b1.y << "; bb of j: " << b2.x << "," << b2.y << std::endl;

				// Create a collisions event
				// We are abusing the ECS system a bit in that we potentially insert muliple collisions for the same entity
				registry.collisions.emplace_with_duplicates(entity_i, entity_j);
				registry.collisions.emplace_with_duplicates(entity_j, entity_i);

				if (physicsBody_i.body_type == BodyType::NONE || physicsBody_j.body_type == BodyType::NONE)
				{
					// None bodies do not need collision resolution
					continue;
				}

				// aabb collision resolution
				float overlap_x = min(p1.x + b1.x - p2.x, p2.x + b2.x - p1.x);
				float overlap_y = min(p1.y + b1.y - p2.y, p2.y + b2.y - p1.y);
				// std::cout << "overlap: " << overlap_x << "," << overlap_y << std::endl;
				if (physicsBody_j.body_type == BodyType::STATIC)
				{
					if (overlap_x < overlap_y)
					{
						if (p1.x < p2.x)
						{
							motion_i.position.x -= overlap_x;
						}
						else
						{
							motion_i.position.x += overlap_x;
						}
					}
					else
					{
						if (p1.y < p2.y)
						{
							motion_i.position.y -= overlap_y;
						}
						else
						{
							motion_i.position.y += overlap_y;
						}
					}
				}
				else
				{
					if (overlap_x < overlap_y)
					{
						if (p1.x < p2.x)
						{
							motion_i.position.x -= overlap_x / 2;
							motion_j.position.x += overlap_x / 2;
						}
						else
						{
							motion_i.position.x += overlap_x / 2;
							motion_j.position.x -= overlap_x / 2;
						}
					}
					else
					{
						if (p1.y < p2.y)
						{
							motion_i.position.y -= overlap_y / 2;
							motion_j.position.y += overlap_y / 2;
						}
						else
						{
							motion_i.position.y += overlap_y / 2;
							motion_j.position.y -= overlap_y / 2;
						}
					}
				}
			}
		}
	}

	for (Entity entity : entities_to_remove)
	{
		// std::cout << "removing entity " << entity << std::endl;
		registry.remove_all_components_of(entity);
	}
}