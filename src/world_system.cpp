// Header
#include "world_system.hpp"
#include "world_init.hpp"
#include "iostream"
// stlib
#include <cassert>
#include <sstream>

#include "../ext/json.hpp"

#include "physics_system.hpp"
#include "LDtkLoader/Project.hpp"
#include <fstream>

// Game configuration
int ENEMIES_COUNT = 2;
float FLOW_CHARGE_PER_SECOND = 33.f;
vec2 player_movement_direction = {0.f, 0.f};
vec2 curr_mouse_position;
bool show_fps = false;
bool show_help_text = false;
bool is_right_mouse_button_down = false;
bool enlarged_player = false;
float enlarge_countdown = 3000.f;
const float PLAYER_SPEED = 192.f; // original speed
const float SPRINTING_MULTIPLIER = 2.5f;
bool is_holding_shift = false;
bool unlocked_stealth_ability = false;
bool unlocked_teleport_stab_ability = false;
bool unlocked_rage_ability = false;
bool dashAvailable = true;
bool dashInUse = false;
bool dialogue_active = false;	 // Indicates if the dialogue is active
int current_dialogue_line = 0; // Tracks the current line of dialogue being shown
std::vector<std::string> dialogue_to_render;
bool chef_first_damaged = false;
bool first_dead_minion = false;
float player_max_health = 100.f;
float player_max_energy = 100.f;
int current_level = 0;
std::vector<vec2> mousePath; // Stores points along the mouse path
bool isTrackingMouse = false;
// Threshold constants for gesture detection
const float S_THRESHOLD1 = 20.0f;
const float MIN_S_LENGTH = 100.0f;
bool entergame = true;

std::vector<std::vector<int>> level_grid;

const float DIALOGUE_PAUSE_DELAY = 500.f; // ms between showing dialogue and pausing game
float time_until_dialogue_pause = 0.f;

bool has_popup = false;
Popup active_popup = {};

bool is_in_chef_dialogue = false;
bool is_in_knight_dialogue = false;
bool is_in_prince_dialogue = false;
bool is_in_king_dialogue = false;

bool background_dialogue_triggered = false;

// create the underwater world
WorldSystem::WorldSystem()
{
	// Seeding rng with random device
	rng = std::default_random_engine(std::random_device()());
}

WorldSystem::~WorldSystem()
{
	// destroy music components
	for (auto music : background_music)
	{
		if (music != nullptr)
			Mix_FreeMusic(music);
	}
	if (salmon_dead_sound != nullptr)
		Mix_FreeChunk(salmon_dead_sound);
	if (perfect_dodge_sound != nullptr)
		Mix_FreeChunk(perfect_dodge_sound);
	if (spy_death_sound != nullptr)
		Mix_FreeChunk(spy_death_sound);
	if (spy_dash_sound != nullptr)
		Mix_FreeChunk(spy_dash_sound);
	if (spy_attack_sound != nullptr)
		Mix_FreeChunk(spy_attack_sound);
	if (break_sound != nullptr)
		Mix_FreeChunk(break_sound);
	if (chef_trigger_sound != nullptr)
		Mix_FreeChunk(chef_trigger_sound);
	if (minion_attack_sound != nullptr)
		Mix_FreeChunk(minion_attack_sound);
	if (heavy_attack_sound != nullptr)
		Mix_FreeChunk(heavy_attack_sound);

	Mix_CloseAudio();

	// Destroy all created components
	registry.clear_all_components();

	// Close the window
	glfwDestroyWindow(window);
}

// Debugging
namespace
{
	void glfw_err_cb(int error, const char *desc)
	{
		fprintf(stderr, "%d: %s", error, desc);
	}
}

// World initialization
// Note, this has a lot of OpenGL specific things, could be moved to the renderer
GLFWwindow *WorldSystem::create_window()
{
	///////////////////////////////////////
	// Initialize GLFW
	glfwSetErrorCallback(glfw_err_cb);
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW");
		return nullptr;
	}

	//-------------------------------------------------------------------------
	// If you are on Linux or Windows, you can change these 2 numbers to 4 and 3 and
	// enable the glDebugMessageCallback to have OpenGL catch your mistakes for you.
	// GLFW / OGL Initialization
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#if __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	glfwWindowHint(GLFW_RESIZABLE, 0);

	// Create the main window (for rendering, keyboard, and mouse input)
	window = glfwCreateWindow(window_width_px, window_height_px, "Games of Throne", nullptr, nullptr);
	if (window == nullptr)
	{
		fprintf(stderr, "Failed to glfwCreateWindow");
		return nullptr;
	}

	// Setting callbacks to member functions (that's why the redirect is needed)
	// Input is handled using GLFW, for more info see
	// http://www.glfw.org/docs/latest/input_guide.html
	glfwSetWindowUserPointer(window, this);
	auto key_redirect = [](GLFWwindow *wnd, int _0, int _1, int _2, int _3)
	{ ((WorldSystem *)glfwGetWindowUserPointer(wnd))->on_key(_0, _1, _2, _3); };
	auto cursor_pos_redirect = [](GLFWwindow *wnd, double _0, double _1)
	{ ((WorldSystem *)glfwGetWindowUserPointer(wnd))->on_mouse_move({_0, _1}); };
	auto mouse_button_redirect = [](GLFWwindow *wnd, int _0, int _1, int _2)
	{ ((WorldSystem *)glfwGetWindowUserPointer(wnd))->on_mouse_button(_0, _1, _2); };
	glfwSetKeyCallback(window, key_redirect);
	glfwSetCursorPosCallback(window, cursor_pos_redirect);
	glfwSetMouseButtonCallback(window, mouse_button_redirect);

	//////////////////////////////////////
	// Loading music and sounds with SDL
	if (SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		fprintf(stderr, "Failed to initialize SDL Audio");
		return nullptr;
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1)
	{
		fprintf(stderr, "Failed to open audio device");
		return nullptr;
	}

	background_music = {
			Mix_LoadMUS(audio_path("GamesOfThrone.1.mp3").c_str()),
			Mix_LoadMUS(audio_path("GamesOfThrone.2.mp3").c_str()),
			Mix_LoadMUS(audio_path("GamesOfThrone.3.mp3").c_str()),
			Mix_LoadMUS(audio_path("GamesOfThrone.4.mp3").c_str())};
	salmon_dead_sound = Mix_LoadWAV(audio_path("death_sound.wav").c_str());
	perfect_dodge_sound = Mix_LoadWAV(audio_path("perfect_dodge.wav").c_str());
	Mix_VolumeChunk(perfect_dodge_sound, 70);
	spy_death_sound = Mix_LoadWAV(audio_path("spy_death.wav").c_str());
	spy_dash_sound = Mix_LoadWAV(audio_path("spy_dash.wav").c_str());
	Mix_VolumeChunk(spy_dash_sound, 60);
	spy_attack_sound = Mix_LoadWAV(audio_path("spy_attack.wav").c_str());
	Mix_VolumeChunk(spy_attack_sound, 80);
	break_sound = Mix_LoadWAV(audio_path("break.wav").c_str());
	chef_trigger_sound = Mix_LoadWAV(audio_path("chef_trigger.wav").c_str());
	Mix_VolumeChunk(chef_trigger_sound, 20);
	minion_attack_sound = Mix_LoadWAV(audio_path("minion_attack.wav").c_str());
	Mix_VolumeChunk(minion_attack_sound, 5);
	heavy_attack_sound = Mix_LoadWAV(audio_path("heavy_attack.wav").c_str());
	Mix_VolumeChunk(heavy_attack_sound, 80);

	if (salmon_dead_sound == nullptr || perfect_dodge_sound == nullptr || spy_death_sound == nullptr || spy_dash_sound == nullptr || spy_attack_sound == nullptr || break_sound == nullptr)
	{
		fprintf(stderr, "Failed to load sounds: %s\n %s\n %s\n %s\n %s\n %s\n %s\n make sure the data directory is present",
						audio_path("soundtrack_1.wav").c_str(),
						audio_path("death_sound.wav").c_str(),
						audio_path("eat_sound.wav").c_str(),
						audio_path("spy_death.wav").c_str(),
						audio_path("spy_dash.wav").c_str(),
						audio_path("spy_attack.wav").c_str(),
						audio_path("break.wav").c_str());
		return nullptr;
	}

	return window;
}

void WorldSystem::init(RenderSystem *renderer_arg)
{
	this->renderer = renderer_arg;

	// Set all states to default
	restart_game();
}

float elapsed_time = 0.f;
float frame_count = 0.f;
float fps = 0.f;

float bezzy(float t, float P0, float P1, float P2)
{
	return (1 - t) * (1 - t) * P0 + 2 * (1 - t) * t * P1 + t * t * P2;
}

// Update our game world
bool WorldSystem::step(float elapsed_ms_since_last_update)
{

	elapsed_time += elapsed_ms_since_last_update / 1000.f; // ms to second convert
	frame_count++;

	Player &player = registry.players.get(player_spy);
	Motion &spy_motion = registry.motions.get(player_spy);
	dashAvailable = player.state != PlayerState::DASHING && player.dash_cooldown_remaining_ms <= 0.0f;
	dashInUse = (player.state == PlayerState::DASHING);
	// saveProgress();
	if (elapsed_time >= 1)
	{
		// Update FPS every second
		fps = frame_count / elapsed_time;
		frame_count = 0.f;
		elapsed_time = 0.f;
	}

	// Update window title
	std::stringstream title_ss;
	title_ss << "Frames Per Second: " << fps;
	glfwSetWindowTitle(window, title_ss.str().c_str());

	// Remove debug info from the last step
	while (registry.debugComponents.entities.size() > 0)
		registry.remove_all_components_of(registry.debugComponents.entities.back());

	// TODO: Remove entities that leave the screen, using new check accounting for camera view
	// Iterate backwards to be able to remove without unterfering with the next object to visit
	// (the containers exchange the last element with the current)
	// auto &motions_registry = registry.motions;
	// for (int i = (int)motions_registry.components.size() - 1; i >= 0; --i)
	// {
	// 	Motion &motion = motions_registry.components[i];
	// 	if (motion.position.x + abs(motion.scale.x) < 0.f)
	// 	{
	// 		if (!registry.players.has(motions_registry.entities[i])) // don't remove the player
	// 			registry.remove_all_components_of(motions_registry.entities[i]);
	// 	}
	// }

	assert(registry.screenStates.components.size() <= 1);
	ScreenState &screen = registry.screenStates.components[0];

	float min_counter_ms = 3000.f;
	for (Entity entity : registry.deathTimers.entities)
	{
		// progress timer
		DeathTimer &counter = registry.deathTimers.get(entity);
		counter.counter_ms -= elapsed_ms_since_last_update;
		if (counter.counter_ms < min_counter_ms)
		{
			min_counter_ms = counter.counter_ms;
		}

		// restart the game once the death timer expired
		if (counter.counter_ms < 0)
		{
			registry.deathTimers.remove(entity);
			screen.darken_screen_factor = 0;
			restart_game();
			return true;
		}
	}
	// reduce window brightness if the salmon is dying
	screen.darken_screen_factor = 1 - min_counter_ms / 3000;

	Energy &energy = registry.energys.get(player_spy);
	for (Entity entity : registry.interpolations.entities)
	{

		Interpolation &interpolate = registry.interpolations.get(entity);
		Motion &motion = registry.motions.get(entity);

		interpolate.elapsed_time += elapsed_ms_since_last_update;

		float t = (float)interpolate.elapsed_time / (float)interpolate.total_time_to_0_ms;
		t = std::min(t, 1.0f);

		float interpolation_factor = t * t * (3.0f - 2.0f * t); // basically 3t^2 - 2t^3 -> cubic

		motion.velocity.x = (1.0f - interpolation_factor + 1.f) * interpolate.initial_velocity.x;
		motion.velocity.y = (1.0f - interpolation_factor + 1.f) * interpolate.initial_velocity.y;

		if (interpolation_factor >= 0.5f && entity == player_spy && !player.current_dodge_is_perfect)
		{
			// half-way through the dodge, set perfect dodge to true even if no damage prevented
			// because we want to play the normal dodge sound sooner rather than after the dodge;
			// this way even if damage is prevented during the later half, the perfect dodge mechanism
			// will not be triggered
			player.current_dodge_is_perfect = true;
			// normal dodge
			Mix_PlayChannel(-1, spy_dash_sound, 0);
		}

		// Remove the entity if the interpolation is complete (velocity should be near zero)
		if (interpolation_factor >= 1.0f)
		{
			registry.interpolations.remove(entity);
			// std::cout << "removed entity from newton/momentum entities" << std::endl;
			motion.velocity = vec2(0.f, 0.f);
			if (entity == player_spy)
			{
				player_action_finished();
			}
		}

		// std::cout << "Interpolation factor: " << interpolation_factor << std::endl;
	}

	// Update health bar percentage
	for (unsigned int i = 0; i < registry.healths.size(); i++)
	{
		Health &health = registry.healths.components[i];
		Entity owner_entity = registry.healths.entities[i];
		Entity health_bar_entity = health.healthbar;

		if (registry.motions.has(owner_entity) && registry.motions.has(health_bar_entity))
		{
			Motion &owner_motion = registry.motions.get(owner_entity);
			Motion &health_bar_motion = registry.motions.get(health_bar_entity);

			// TODO: refactor the following lines code by introducing healthbar_offset and put code in renderer.draw()
			if (owner_entity == player_spy)
			{
				health_bar_motion.position = vec2(50.f, 50.f);
			}
			else if (registry.chef.has(owner_entity))
			{
				health_bar_motion.position = owner_motion.position + vec2(-95.f, -175.f);
			}
			else
			{
				health_bar_motion.position = owner_motion.position + vec2(-105.f, -75.f);
			}

			float health_percentage = health.health / health.max_health;
			if (registry.healthbar.has(health_bar_entity))
			{
				HealthBar &health_bar = registry.healthbar.get(health_bar_entity);
				health_bar_motion.scale.x = health_bar.original_scale.x * health_percentage;
				health_bar_motion.scale.y = health_bar.original_scale.y;
			}
		}
	}

	float energy_time = elapsed_ms_since_last_update / 1000.0f;
	update_energy(energy_time);

	for (int i = registry.dashes.components.size() - 1; i >= 0; i--)
	{
		Motion &spy_motion = registry.motions.get(player_spy);
		dashAvailable = player.state != PlayerState::DASHING && player.dash_cooldown_remaining_ms <= 0.0f;
		dashInUse = (player.state == PlayerState::DASHING);
		Dash &dash = registry.dashes.components[i];
		dash.elapsed_time += elapsed_ms_since_last_update;

		if (dash.elapsed_time <= dash.total_time_ms)
		{
			float t = dash.elapsed_time / dash.total_time_ms;
			float scaling_factor = bezzy(t, 1.0f, 2.0f, 1.0f);
			spy_motion.velocity = player_movement_direction * PLAYER_SPEED * scaling_factor * 2.f;
			// std::cout << "dash active, velocity is (" << spy_motion.velocity.x << ", " << spy_motion.velocity.y << ")" << std::endl;
		}
		else
		{
			player_action_finished();
			dash.elapsed_time = 0;
			player.dash_cooldown_remaining_ms = dash.total_time_ms * 3.f;
			registry.dashes.remove(registry.dashes.entities[i]);
			std::cout << "Dash ended" << std::endl;
			player.stealth_mode = false;

			if (registry.opacities.has(player_spy))
			{
				registry.opacities.remove(player_spy);
			}
		}
	}

	if (player.dash_cooldown_remaining_ms > 0.0f)
	{
		player.dash_cooldown_remaining_ms -= elapsed_ms_since_last_update;
	}

	auto &health = registry.healths.get(player_spy);
	if (health.is_dead)
	{
		if (!registry.deathTimers.has(player_spy))
		{
			Mix_PlayChannel(-1, spy_death_sound, 0);
			registry.deathTimers.emplace(player_spy);

			// remove collision from player
			registry.physicsBodies.remove(player_spy);
		}
		if (player.state != PlayerState::DYING)
		{
			player.state = PlayerState::DYING;
		}
		registry.motions.get(player_spy).velocity = {0.f, 0.f};
	}

	// Pan returns to chef logic
	for (Entity pan_entity : registry.pans.entities)
	{
		Pan &pan = registry.pans.get(pan_entity);
		Motion &pan_motion = registry.motions.get(pan_entity);

		if (pan.state == PanState::RETURNING)
		{
			for (Entity chef_entity : registry.chef.entities)
			{
				Motion &chef_motion = registry.motions.get(chef_entity);
				float distance_to_chef = length(chef_motion.position - pan_motion.position);
				// Pan returns to chef when distance < 50.
				if (distance_to_chef < 50.f)
				{
					registry.remove_all_components_of(pan_entity);
					Chef &chef = registry.chef.get(chef_entity);
					chef.pan_active = false;
				}
			}
		}
	}

	handle_animations(elapsed_ms_since_last_update);

	if (is_right_mouse_button_down && registry.flows.has(flowMeterEntity))
	{
		Player &player = registry.players.get(player_spy);
		if ((player.state == PlayerState::IDLE || player.state == PlayerState::SPRINTING) && energy.energy > 0)
		{
			std::cout << "Player is charging flow" << std::endl;
			player.state = PlayerState::CHARGING_FLOW;
		}
		if (player.state == PlayerState::CHARGING_FLOW)
		{
			if (energy.energy > 0)
			{
				Flow &flow = registry.flows.get(flowMeterEntity);
				flow.flowLevel = std::min(flow.flowLevel + FLOW_CHARGE_PER_SECOND * elapsed_ms_since_last_update / 1000.f, flow.maxFlowLevel); // Increase flow up to max
			}
			else
			{
				player_action_finished();
				is_right_mouse_button_down = false; // overwrite the value to false, so the player has to right click again to charge
			}
		}
	}

	// update animations
	for (Entity entity : registry.spriteAnimations.entities)
	{
		// if in attack mode, change sprite to attack sprite
		if (registry.enemies.has(entity) && registry.enemies.get(entity).state == EnemyState::ATTACK)
		{
			auto &animation = registry.spriteAnimations.get(entity);
			auto &render_request = registry.renderRequests.get(entity);

			// Increment elapsed time
			animation.elapsed_time += elapsed_ms_since_last_update;

			// Check if enough time has passed to switch to the next frame
			if (animation.elapsed_time >= animation.frame_duration)
			{
				// Reset elapsed time for the next frame
				animation.elapsed_time = 0.0f;

				// Move to the next frame
				animation.current_frame = (animation.current_frame + 1) % animation.frames.size();

				// Update the texture to the current frame
				render_request.used_texture = animation.frames[animation.current_frame];
			}
		}
	}

	// play attack sound for each enemy that is attacking
	for (Entity entity : registry.enemies.entities)
	{
		auto &enemy = registry.enemies.get(entity);
		if (enemy.state == EnemyState::ATTACK)
		{
			// lower this audio
			Mix_PlayChannel(-1, minion_attack_sound, 0);
		}
	}

	// TODO: create audio_system class and delegate audio playing to it, so this logic does not have to be in world_system
	// play chef_trigger audio if chef just entered combat mode
	for (Entity entity : registry.chef.entities)
	{
		Chef &chef = registry.chef.get(entity);
		if (chef.trigger == true)
		{
			// volume up
			Mix_PlayChannel(-1, chef_trigger_sound, 0);
			if (chef.sound_trigger_timer >= elapsed_ms_since_last_update)
			{
				chef.sound_trigger_timer -= elapsed_ms_since_last_update;
			}
			else
			{
				chef.sound_trigger_timer = 0;
				chef.trigger = false;
			}
		}
	}

	// move camera if necessary
	update_camera_view();

	// player remnant countdown
	for (int i = registry.playerRemnants.components.size() - 1; i >= 0; i--)
	{
		PlayerRemnant &player_remnant = registry.playerRemnants.components[i];
		player_remnant.time_until_destroyed -= elapsed_ms_since_last_update;
		if (player_remnant.time_until_destroyed <= 0)
		{
			registry.remove_all_components_of(registry.playerRemnants.entities[i]);
		}
	}

	// perfect dodge calculation
	if (player.state == PlayerState::DODGING && !player.current_dodge_is_perfect && player.damage_prevented)
	{
		player.current_dodge_is_perfect = true;
		std::cout << "PERFECT DODGE!" << std::endl;

		Flow &flow = registry.flows.get(flowMeterEntity);
		flow.flowLevel = std::min(flow.flowLevel + 25.f, flow.maxFlowLevel); // Increase flow by 25

		// create player remnant
		createPlayerRemnant(renderer, player.current_dodge_original_motion);

		Mix_PlayChannel(-1, perfect_dodge_sound, 0);
	}

	// mouse gesture enlarge countdown
	if (enlarged_player)
	{
		enlarge_countdown -= elapsed_ms_since_last_update;
		if (enlarge_countdown <= 0)
		{
			// reset player size
			Motion &player_motion = registry.motions.get(player_spy);
			player_motion.scale *= 0.65f;
			player_motion.bb_scale *= 0.65f;

			enlarged_player = false;
			enlarge_countdown = 5000.f;
		}
	}

	// Process chef death and first damaged
	if (registry.chef.size() > 0)
	{
		Entity chef_entity = registry.chef.entities[0];
		Health &chef_health = registry.healths.get(chef_entity);
		if (chef_health.is_dead)
		{
			registry.remove_all_components_of(chef_entity);
			trigger_dialogue(chef_death_dialogue);
			is_in_chef_dialogue = true;
		}

		if (!chef_first_damaged)
		{
			if (chef_health.health < chef_health.max_health)
			{
				trigger_dialogue(chef_being_attacked_dialogue);
				chef_first_damaged = true;
			}
		}
	}

	if (registry.knight.size() > 0)
	{
		Entity knight_entity = registry.knight.entities[0];
		Health &knight_health = registry.healths.get(knight_entity);
		if (knight_health.is_dead)
		{
			registry.remove_all_components_of(knight_entity);
			trigger_dialogue(knight_death_dialogue);
			is_in_knight_dialogue = true;
		}
	}

	if (registry.prince.size() > 0)
	{
		Entity prince_entity = registry.prince.entities[0];
		Health &prince_health = registry.healths.get(prince_entity);
		if (prince_health.is_dead)
		{
			registry.remove_all_components_of(prince_entity);
			trigger_dialogue(prince_death_dialogue);
			is_in_prince_dialogue = true;
		}
	}

	if (registry.king.size() > 0)
	{
		Entity king_entity = registry.king.entities[0];
		Health &king_health = registry.healths.get(king_entity);
		if (king_health.is_dead)
		{
			registry.remove_all_components_of(king_entity);
			trigger_dialogue(king_death_dialogue);
		}
	}

	// check the first minion death
	if (registry.enemies.size() > 0 && !first_dead_minion)
	{
		for (Entity entity : registry.enemies.entities)
		{
			Enemy &enemy = registry.enemies.get(entity);
			Health &enemy_health = registry.healths.get(entity);
			if (enemy.is_minion && enemy_health.is_dead && !first_dead_minion)
			{
				first_dead_minion = true;
				trigger_dialogue(minion_death_dialogue);
				continue;
			}
		}
	}

	if (dialogue_active && !is_paused)
	{
		time_until_dialogue_pause -= elapsed_ms_since_last_update;
		if (time_until_dialogue_pause <= 0)
		{
			is_paused = true;
		}
	}

	if (player.state == PlayerState::LIGHT_ATTACK || player.state == PlayerState::HEAVY_ATTACK || player.state == PlayerState::CHARGING_FLOW || player.state == PlayerState::DYING)
	{
		spy_motion.velocity = vec2(0.f, 0.f);
	}

	if (player.teleport_back_stab_cooldown > 0.0f)
	{
		player.teleport_back_stab_cooldown -= elapsed_ms_since_last_update;
		if (player.teleport_back_stab_cooldown < 0.0f)
		{
			player.teleport_back_stab_cooldown = 0.0f;
			printf("Teleport Backstab ability is ready!\n");
		}
	}

	if (player.rage_cooldown > 0.0f)
	{
		player.rage_cooldown -= elapsed_ms_since_last_update;
		if (player.rage_cooldown < 0.0f)
		{
			player.rage_cooldown = 0.0f;
			player.rage_activate = false; // Reset activation flag when cooldown ends
			printf("Rage ability is ready!\n");
		}
	}

	if (player.rage_remaining > 0.0f)
	{
		player.rage_remaining -= elapsed_ms_since_last_update;
		if (player.rage_remaining <= 0.0f)
		{
			// Reset rage effects
			player.rage_remaining = 0.0f;
			player.damage_multiplier = 1.0f;			 // Reset to normal damage
			player.attack_speed_multiplier = 1.0f; // Reset to normal attack speed
			player.rage_activate = false;

			printf("Rage ability ended. Damage and attack speed returned to normal.\n");
		}
	}

	// initializeAbilityIcons(renderer);
	updateAbilityIcons(renderer, elapsed_time);

	// player take damage effect
	if (player.damage_effect_duration > 0.f)
	{
		player.damage_effect_duration -= elapsed_ms_since_last_update;
		if (player.damage_effect_duration <= 0.f)
		{
			player.damage_effect_duration = 0.f;
			if (registry.colors.has(player_spy))
			{
				registry.colors.remove(player_spy);
			}
		}
	}
	if (!health.is_dead)
	{
		if (health.health != player.last_health)
		{
			if (health.health < player.last_health)
			{
				if (registry.colors.has(player_spy))
				{
					registry.colors.remove(player_spy);
				}
				registry.colors.insert(player_spy, {1.f, 0.f, 0.f});
				player.damage_effect_duration = 250.f;
			}
			else
			{
				if (registry.colors.has(player_spy))
				{
					registry.colors.remove(player_spy);
				}
				registry.colors.insert(player_spy, {0.f, 1.f, 0.f});
				player.damage_effect_duration = 250.f;
			}
			player.last_health = health.health;
		}
	}

	return true;
}

void WorldSystem::update_camera_view()
{
	float THRESHOLD = 0.4f; // will move camera if player is within 40% of the screen edge
	Motion &player_motion = registry.motions.get(player_spy);
	vec2 &camera_position = renderer->camera_position;
	vec2 on_screen_pos = player_motion.position - camera_position;
	if (on_screen_pos.x > window_width_px * (1 - THRESHOLD))
	{
		camera_position.x += on_screen_pos.x - window_width_px * (1 - THRESHOLD);
	}
	else if (on_screen_pos.x < window_width_px * THRESHOLD)
	{
		camera_position.x -= window_width_px * THRESHOLD - on_screen_pos.x;
	}
	if (on_screen_pos.y > window_height_px * (1 - THRESHOLD))
	{
		camera_position.y += on_screen_pos.y - window_height_px * (1 - THRESHOLD);
	}
	else if (on_screen_pos.y < window_height_px * THRESHOLD)
	{
		camera_position.y -= window_height_px * THRESHOLD - on_screen_pos.y;
	}
}

void createRoom(std::vector<std::vector<int>> &levelMap, int x_start, int y_start, int width, int height)
{
	for (int i = x_start; i < x_start + width; ++i)
	{
		for (int j = y_start; j < y_start + height; ++j)
		{
			// Place walls around the edges of the room
			if (i == x_start || i == x_start + width - 1 || j == y_start || j == y_start + height - 1)
			{
				levelMap[i][j] = 1; // 1 for wall
			}
			else
			{
				levelMap[i][j] = 2; // 2 for floor
			}
		}
	}
}

void createCorridor(std::vector<std::vector<int>> &levelMap, int x_start, int y_start, int length, int width,
										bool add_wall_left = true, bool add_wall_right = true, bool add_wall_top = true, bool add_wall_bottom = true)
{
	for (int i = x_start; i < x_start + length; ++i)
	{
		for (int j = y_start; j < y_start + width; ++j)
		{
			bool is_edge = (i == x_start || i == x_start + length - 1 || j == y_start || j == y_start + width - 1);

			if (is_edge)
			{
				if ((i == x_start && add_wall_left) || (i == x_start + length - 1 && add_wall_right) ||
						(j == y_start && add_wall_top) || (j == y_start + width - 1 && add_wall_bottom))
				{
					levelMap[i][j] = 1; // Wall
				}
				else
				{
					levelMap[i][j] = 2; // Floor at the edges without walls
				}
			}
			else
			{
				levelMap[i][j] = 2; // Floor in the corridor's interior
			}
		}
	}
}

// Reset the world state to its initial state
void WorldSystem::restart_game()
{
	// Debugging for memory/component leaks
	registry.list_all_components();
	printf("Restarting\n");

	int screen_width, screen_height;
	glfwGetWindowSize(window, &screen_width, &screen_height);
	loadProgress();

	std::string levelName = "Level_" + std::to_string(current_level);
	std::cout << "Loading " << levelName << std::endl;
	load_level(levelName, current_level);

	if (!background_dialogue_triggered)
	{
		// only show for first time
		instruction_screen();
	}
}

void WorldSystem::instruction_screen()
{
	show_help_text = true;
	Entity backdrop = createBackdrop(renderer);
	Entity dialogue_window = createDialogueWindow(renderer, {window_width_px / 2.f, window_height_px / 2.f}, {1300.f, 750.f});

	active_popup = {PopupType::HELP, nullptr, "Controls", "UI Interactions"};
	has_popup = true;
	is_paused = true;
	// trigger_dialogue(background_dialogue);
}

void WorldSystem::load_level(const std::string &levelName, const int levelNumber)
{
	while (registry.motions.entities.size() > 0)
		registry.remove_all_components_of(registry.motions.entities.back());

	createBackgroundSprite(renderer, levelNumber);
	// Debugging for memory/component leaks
	registry.list_all_components();

	current_level = levelNumber;

	saveProgress(); // save on load_level

	if (background_music.size() > 0)
	{
		Mix_VolumeMusic(80);
		Mix_Music *level_music = background_music.size() > levelNumber ? background_music[levelNumber] : background_music[0];
		Mix_PlayMusic(level_music, -1);
	}

	ldtk::Project ldtk_project;
	ldtk_project.loadFromFile(data_path() + "/levels/levels.ldtk");
	const ldtk::World &world = ldtk_project.getWorld();
	const ldtk::Level &level = world.getLevel(levelName);

	const int TILE_SIZE = 60; // set to match tile scale in common.hpp atm
	const float HALF_TILE_SIZE = static_cast<float>(TILE_SIZE) / 2.f;

	int gridWidth = level.size.x / TILE_SIZE;
	int gridHeight = level.size.y / TILE_SIZE;

	std::vector<std::pair<Entity, vec2>> chests;
	std::vector<std::pair<Entity, vec2>> minions;

	level_grid.clear();
	level_grid.resize(gridWidth, std::vector<int>(gridHeight, 0)); // 0 for walkable

	for (const auto &layer : level.allLayers())
	{
		if (layer.getType() == ldtk::LayerType::Tiles)
		{
			for (const auto &tile : layer.allTiles())
			{
				// Get tile position in pixels
				int px = tile.getPosition().x;
				int py = tile.getPosition().y;
				int gridX = px / TILE_SIZE;
				int gridY = py / TILE_SIZE;
				vec2 position = {static_cast<float>(px) + HALF_TILE_SIZE, static_cast<float>(py) + HALF_TILE_SIZE};
				// vec2 position = {static_cast<float>(px), static_cast<float>(py)};
				if (layer.getName() == "Floor_Tiles")
				{
					level_grid[gridX][gridY] = 1; // walkable
					createFloorTile(renderer, position);
				}
				else if (layer.getName() == "Wall_Tiles")
				{
					level_grid[gridX][gridY] = 0;
					createWall(renderer, position);
				}
			}
		}
	}

	for (const auto &layer : level.allLayers())
	{
		if (layer.getType() == ldtk::LayerType::Entities)
		{
			for (const auto &entity : layer.allEntities())
			{
				std::string entity_name = entity.getName();
				int px = entity.getPosition().x;
				int py = entity.getPosition().y;
				vec2 position = {static_cast<float>(px), static_cast<float>(py)};

				if (entity_name == "Chest")
				{
					Entity chest_entity = Entity(0);
					// Determine content of treasure box
					float type_roll = uniform_dist(rng);
					if (type_roll < 0.25f)
					{
						std::cout << "Creating treasure box with max health" << std::endl;
						chest_entity = createTreasureBox(renderer, position, TreasureBoxItem::MAX_HEALTH);
					}
					else if (type_roll < 0.5f)
					{
						std::cout << "Creating treasure box with max energy" << std::endl;
						chest_entity = createTreasureBox(renderer, position, TreasureBoxItem::MAX_ENERGY);
					}
					else
					{
						type_roll = (type_roll - 0.5f) * 2.f;

						std::vector<std::vector<float>> chance_tables = {
								{0.7, 0.25, 0.05}, // level 0
								{0.5, 0.4, 0.1},	 // level 1
								{0.3, 0.5, 0.2},	 // etc
								{0.1, 0.5, 0.4},
						};
						std::vector<float> &chance_table = chance_tables[levelNumber];
						int rarity = 0;
						for (int i = 0; i < chance_table.size(); i++)
						{
							if (type_roll < chance_table[i])
							{
								rarity = i;
								break;
							}
							type_roll -= chance_table[i];
						}
						// roll for weapon type
						float weapon_type_roll = uniform_dist(rng);
						WeaponType weapon_type = WeaponType::SWORD;
						if (weapon_type_roll < 0.5f)
						{
							weapon_type = WeaponType::SWORD;
						}
						else
						{
							weapon_type = WeaponType::DAGGER;
						}

						WeaponLevel wepaon_level = static_cast<WeaponLevel>(rarity);

						std::cout << "Creating treasure box with weapon type " << (int)weapon_type << " and rarity " << (int)wepaon_level << std::endl;
						chest_entity = createTreasureBox(renderer, position, TreasureBoxItem::WEAPON, weapon_type, wepaon_level);
					}

					if ((unsigned int)chest_entity != 0)
					{
						chests.push_back(std::make_pair(chest_entity, position));
					}
				}
				else if (entity_name == "Fountain")
				{
					createFountain(renderer, position);
				}
				else if (entity_name == "Minions")
				{
					Entity minion_entity = createEnemy(renderer, position);
					minions.push_back(std::make_pair(minion_entity, position));
				}
				else if (entity_name == "Chef")
				{
					createChef(renderer, position);
				}
				else if (entity_name == "Knight")
				{
					createKnight(renderer, position);
				}
				else if (entity_name == "Prince")
				{
					createPrince(renderer, position);
				}
				else if (entity_name == "King")
				{
					createKing(renderer, position);
				}
				else if (entity_name == "Spy")
				{
					player_spy = createSpy(renderer, position);
				}
				else if (entity_name == "Ranged_Minion")
				{
					createRangedMinion(renderer, position);
				}
			}
		}
	}

	// createEnemy(renderer, vec2(window_width_px * 2 + 600, window_height_px * 2));

	// createEnemy(renderer, vec2(window_width_px * 2 + 200, window_height_px * 2 - 200));

	Entity weapon = createWeapon(renderer, {window_width_px * 2 + 200, window_height_px * 2}, WeaponType::SWORD, WeaponLevel::BASIC);

	Player &player = registry.players.get(player_spy);
	player.weapon = weapon;
	player.weapon_offset = vec2(45.f, -50.f);

	flowMeterEntity = createFlowMeter(renderer, {window_width_px - 100.f, window_height_px - 100.f}, 100.0f);

	initializeAbilityIcons(renderer);

	const float association_distance = 600.f;

	// logic for associate minion w treasure box based on initialization positon
	for (const auto &chest_pair : chests)
	{
		Entity chest_entity = chest_pair.first;
		vec2 chest_position = chest_pair.second;
		TreasureBox &treasure_box = registry.treasureBoxes.get(chest_entity);
		std::cout << "Associating minions with chest!! " << chest_entity << std::endl;

		for (const auto &minion_pair : minions)
		{
			Entity minion_entity = minion_pair.first;
			vec2 minion_position = minion_pair.second;

			float distance = length(chest_position - minion_position);
			if (distance <= association_distance)
			{
				treasure_box.associated_minions.push_back(minion_entity);
				std::cout << " - Minion " << minion_entity << " associated with chest " << chest_entity
									<< " (distance: " << distance / TILE_SIZE << " tiles)" << std::endl;
			}
		}
	}
}

void WorldSystem::process_animation(AnimationName name, float t, Entity entity)
{
	if (name == AnimationName::LIGHT_ATTACK)
	{
		Player &player = registry.players.get(entity);
		Weapon &weapon = registry.weapons.get(player.weapon);
		Motion &weapon_motion = registry.motions.get(player.weapon);
		float angle = player.weapon_offset.x < 0 ? w_angle - sin(t * M_PI) - M_PI / 6 : w_angle + sin(t * M_PI) + M_PI / 6;

		if (t >= 1.0f)
		{
			// angle = weapon.type == WeaponType::SWORD ? M_PI / 6 : 0;
			angle = w_angle;
			player_action_finished();
		}

		if (player.weapon_offset.x < 0)
		{
			weapon_motion.angle = angle;
		}
		else
		{
			weapon_motion.angle = angle;
		}
		// std::cout << "Angle = " << weapon_motion.angle / M_PI * 180 << std::endl;
	}
	else if (name == AnimationName::HEAVY_ATTACK)
	{
		Player &player = registry.players.get(entity);
		Weapon &weapon = registry.weapons.get(player.weapon);
		Motion &weapon_motion = registry.motions.get(player.weapon);

		if (weapon.type == WeaponType::SWORD)
		{
			float angle = t * 4 * M_PI + M_PI / 6;
			while (angle > 2 * M_PI)
			{
				angle -= 2 * M_PI;
			}

			if (t >= 0.5f)
			{
				if (!player.is_heavy_attack_second_half)
				{
					player.is_heavy_attack_second_half = true;
					player.current_attack_id = ++attack_id_counter;
					// std::cout << "Heavy attack second half " << player.current_attack_id << std::endl;
				}
			}

			if (t >= 1.0f)
			{
				angle = M_PI / 6;
				player_action_finished();
			}

			if (player.weapon_offset.x < 0)
			{
				weapon_motion.angle = -angle;
			}
			else
			{
				weapon_motion.angle = angle;
			}
			// std::cout << "Angle = " << weapon_motion.angle / M_PI * 180 << std::endl;
		}
		else
		{
			// Special heavy attack animation for daggers
			float angle = 0.0f;
			int faceDir = (player.weapon_offset.x > 0) ? -1 : 1;

			if (t < 0.5f)
			{
				// First half: Rotate and stretch backward
				angle = -M_PI / 2 * faceDir; // Flat horizontal position

				player.weapon_offset.x -= 10.f * faceDir; // Move back

				// Flip scale for left-facing player to maintain correct orientation
				if (faceDir > 0)
				{
					weapon_motion.scale.y = -abs(weapon_motion.scale.y); // Flip for left-facing
					player.weapon_offset.y = 60.f;											 // Adjust position for flipped scale
				}
				else
				{
					weapon_motion.scale.y = abs(weapon_motion.scale.y); // Ensure correct for right-facing
					player.weapon_offset.y = -50.f;
				}

				printf("First half - Weapon offset: %.2f, Motion scale.y: %.2f\n", player.weapon_offset.x, weapon_motion.scale.y);
			}
			else
			{
				// Second half: Stab forward
				angle = -M_PI / 2 * faceDir; // Keep flat horizontal position

				player.weapon_offset.x += 10.f * faceDir; // Move forward

				// Ensure the scale remains consistent
				if (faceDir > 0)
				{
					weapon_motion.scale.y = -abs(weapon_motion.scale.y); // Flip for left-facing
					player.weapon_offset.y = 60.f;											 // Adjust position for flipped scale
				}
				else
				{
					weapon_motion.scale.y = abs(weapon_motion.scale.y); // Ensure correct for right-facing
					player.weapon_offset.y = -50.f;
				}

				printf("Second half - Weapon offset: %.2f, Motion scale.y: %.2f\n", player.weapon_offset.x, weapon_motion.scale.y);
			}

			weapon_motion.angle = angle;

			// Check for second half of the attack
			if (t >= 0.5f)
			{
				Player &player = registry.players.get(entity);
				if (!player.is_heavy_attack_second_half)
				{
					player.is_heavy_attack_second_half = true;
					player.current_attack_id = ++attack_id_counter;
				}
			}

			if (t >= 1.0f)
			{
				angle = 0;
				Player &player = registry.players.get(entity);
				player.state = PlayerState::IDLE;
				player.weapon_offset = vec2(45.f, -50.f);
				weapon_motion.scale.y = abs(weapon_motion.scale.y);

				// Debugging: Print offset when resetting to original
				// printf("Reset to original offset - Weapon offset: %.2f\n", player.weapon_offset.x);
			}

			if (player.weapon_offset.x < 0)
			{
				weapon_motion.angle = -angle;
			}
			else
			{
				weapon_motion.angle = angle;
			}
		}
	}
}

void WorldSystem::handle_animations(float elapsed_ms)
{
	for (int i = registry.animations.size() - 1; i >= 0; i--)
	{
		Animation &animation = registry.animations.components[i];
		Entity entity = registry.animations.entities[i];
		animation.elapsed_time += elapsed_ms;
		float t = animation.elapsed_time / animation.total_time;
		if (t >= 1.0f)
		{
			t = 1.0f;
		}
		// std::cout << "i = " << i << " t = " << t << std::endl;
		process_animation(animation.name, t, entity);

		if (animation.elapsed_time >= animation.total_time)
		{
			registry.animations.remove(entity);
		}
	}

	// bone animations
	for (int i = registry.boneAnimations.size() - 1; i >= 0; i--)
	{
		BoneAnimation &bone_animation = registry.boneAnimations.components[i];
		Entity entity = registry.boneAnimations.entities[i];
		MeshBones &mesh_bones = registry.meshBones.get(entity);

		bone_animation.elapsed_time += elapsed_ms;
		float time_in_animation = bone_animation.elapsed_time;

		BoneKeyframe &keyframe = bone_animation.keyframes[bone_animation.current_keyframe];

		bool animation_ends = false;
		float t = (time_in_animation - keyframe.start_time) / keyframe.duration;
		if (t >= 1.0f)
		{
			// must have another keyframe after next
			if (bone_animation.current_keyframe + 2 < bone_animation.keyframes.size())
			{
				bone_animation.current_keyframe++;
				keyframe = bone_animation.keyframes[bone_animation.current_keyframe];
				t = 0.0f;
			}
			else
			{
				t = 1.0f;
				animation_ends = true;
			}
		}

		// Interpolate between the two keyframes
		for (int i = 0; i < keyframe.bone_transforms.size(); i++)
		{
			BoneTransform &transform = keyframe.bone_transforms[i];
			BoneTransform &next_transform = bone_animation.keyframes[bone_animation.current_keyframe + 1].bone_transforms[i];

			BoneTransform interpolated;
			interpolated.position = glm::mix(transform.position, next_transform.position, t);
			interpolated.angle = glm::mix(transform.angle, next_transform.angle, t);
			interpolated.scale = glm::mix(transform.scale, next_transform.scale, t);
			Transform tr;
			tr.translate(interpolated.position);
			tr.rotate(interpolated.angle);
			tr.scale(interpolated.scale);
			mesh_bones.bones[i].local_transform = tr.mat;
		}

		if (animation_ends)
		{
			registry.boneAnimations.remove(entity);
		}
	}
}

void WorldSystem::draw_mesh_debug(Entity mesh_entity, bool consider_bones)
{
	Motion &mesh_motion = registry.motions.get(mesh_entity);
	glm::mat3 transform = get_transform(mesh_motion);

	TexturedMesh *mesh = registry.texturedMeshPtrs.get(mesh_entity);
	auto &mesh_vertices = mesh->vertices;
	auto &mesh_indices = mesh->vertex_indices;

	if (consider_bones && registry.meshBones.has(mesh_entity))
	{
		std::vector<glm::mat3> bone_matrices;
		std::vector<MeshBone> &bones = registry.meshBones.get(mesh_entity).bones;
		for (MeshBone &bone : bones)
		{
			glm::mat3 bone_matrix = bone.local_transform;
			int parent_index = bone.parent_index;
			if (parent_index != -1 && parent_index < bone_matrices.size())
			{
				bone_matrix = bone_matrices[parent_index] * bone_matrix;
			}
			bone_matrices.push_back(bone_matrix);
		}

		// std::cout << "bone_matrices.size() = " << bone_matrices.size() << std::endl;

		GEOMETRY_BUFFER_ID geometry_buffer_id = registry.renderRequests.get(mesh_entity).used_geometry;
		auto &bone_indices = renderer->skinned_meshes[(int)geometry_buffer_id].bone_indices;
		auto &bone_weights = renderer->skinned_meshes[(int)geometry_buffer_id].bone_weights;

		// std::cout << "bone_indices.size() = " << bone_indices.size() << std::endl;
		// std::cout << "bone_weights.size() = " << bone_weights.size() << std::endl;

		std::vector<glm::mat3> vertex_transforms;
		for (uint i = 0; i < mesh_vertices.size(); i++)
		{
			glm::mat3 vertex_transform = glm::mat3(0.0f);
			for (int j = 0; j < 4; j++)
			{
				int bone_index = bone_indices[i][j];
				float weight = bone_weights[i][j];
				if (weight == 0.0f)
				{
					continue;
				}
				vertex_transform += weight * bone_matrices[bone_index];
			}
			vertex_transforms.push_back(vertex_transform);
		}

		// std::cout << "vertex_transforms.size() = " << vertex_transforms.size() << std::endl;

		for (uint i = 0; i < mesh_indices.size(); i += 3)
		{
			vec2 p1 = xy(transform * vertex_transforms[mesh_indices[i]] * mesh_vertices[mesh_indices[i]].position);
			vec2 p2 = xy(transform * vertex_transforms[mesh_indices[i + 1]] * mesh_vertices[mesh_indices[i + 1]].position);
			vec2 p3 = xy(transform * vertex_transforms[mesh_indices[i + 2]] * mesh_vertices[mesh_indices[i + 2]].position);

			// draw line from p1 to p2, where createLine is function createLine(vec2 position, vec2 scale, vec3 color, float angle)

			createLine((p1 + p2) / 2.f, {glm::length(p2 - p1), 3.f}, {1.f, 1.f, 0.f}, atan2(p2.y - p1.y, p2.x - p1.x));
			createLine((p2 + p3) / 2.f, {glm::length(p3 - p2), 3.f}, {1.f, 1.f, 0.f}, atan2(p3.y - p2.y, p3.x - p2.x));
			createLine((p3 + p1) / 2.f, {glm::length(p1 - p3), 3.f}, {1.f, 1.f, 0.f}, atan2(p1.y - p3.y, p1.x - p3.x));
		}
	}
	else
	{
		for (uint i = 0; i < mesh_indices.size(); i += 3)
		{
			vec2 p1 = xy(transform * mesh_vertices[mesh_indices[i]].position);
			vec2 p2 = xy(transform * mesh_vertices[mesh_indices[i + 1]].position);
			vec2 p3 = xy(transform * mesh_vertices[mesh_indices[i + 2]].position);

			// draw line from p1 to p2, where createLine is function createLine(vec2 position, vec2 scale, vec3 color, float angle)

			createLine((p1 + p2) / 2.f, {glm::length(p2 - p1), 3.f}, {1.f, 1.f, 0.f}, atan2(p2.y - p1.y, p2.x - p1.x));
			createLine((p2 + p3) / 2.f, {glm::length(p3 - p2), 3.f}, {1.f, 1.f, 0.f}, atan2(p3.y - p2.y, p3.x - p2.x));
			createLine((p3 + p1) / 2.f, {glm::length(p1 - p3), 3.f}, {1.f, 1.f, 0.f}, atan2(p1.y - p3.y, p1.x - p3.x));
		}
	}
	// createBox(mesh_motion.position + mesh_motion.bb_offset, get_bounding_box(mesh_motion), {1.f, 1.f, 0.f});
}

// Compute collisions between entities
void WorldSystem::handle_collisions()
{
	Entity player = player_spy;
	Player &player_comp = registry.players.get(player);
	Entity player_weapon = player_comp.weapon;
	Weapon &player_weapon_comp = registry.weapons.get(player_weapon);

	// draw collision bounding boxes (debug lines)
	if (debugging.in_debug_mode)
	{
		for (uint i = 0; i < registry.physicsBodies.components.size(); i++)
		{
			Entity entity = registry.physicsBodies.entities[i];
			Motion &motion = registry.motions.get(entity);
			vec3 color = {1.f, 0.f, 0.f};
			if (registry.collisions.has(entity))
			{
				color = {0.f, 1.f, 0.f};
			}
			createBox(motion.position + motion.bb_offset, get_bounding_box(motion), color);
			if (motion.pivot_offset.x != 0 || motion.pivot_offset.y != 0)
			{
				createBox(motion.position - motion.pivot_offset * motion.scale, {5.f, 5.f}, {0.f, 0.f, 1.f});
			}
		}

		// Draw weapon mesh (for debugging)
		// Weapon &player_weapon = registry.weapons.get(player_spy);
		// draw_mesh_debug(player_weapon.weapon);

		// Draw knight mesh
		// if (registry.knight.size() > 0)
		// {
		// 	Entity knight_entity = registry.knight.entities[0];
		// 	draw_mesh_debug(knight_entity, false);

		// 	// to also consider bone animations:
		// 	// draw_mesh_debug(knight_entity, true);
		// }

		// Draw prince mesh
		// if (registry.prince.size() > 0)
		// {
		// 	Entity prince_entity = registry.prince.entities[0];
		// 	draw_mesh_debug(prince_entity, false);

		// 	// to also consider bone animations:
		// 	// draw_mesh_debug(prince_entity, true);
		// }

		// Draw king mesh
		if (registry.king.size() > 0)
		{
			Entity king_entity = registry.king.entities[0];
			draw_mesh_debug(king_entity, false);

			// to also consider bone animations:
			// draw_mesh_debug(king_entity, true);
		}

		// Draw damage areas
		for (uint i = 0; i < registry.damageAreas.components.size(); i++)
		{
			Entity entity = registry.damageAreas.entities[i];
			DamageArea &damage_area = registry.damageAreas.components[i];
			Motion &motion = registry.motions.get(entity);
			vec3 color = {0.6f, 0.6f, 0.f};
			if (!damage_area.active)
			{
				color = {0.3f, 0.3f, 0.f};
			}
			createBox(motion.position + motion.bb_offset, get_bounding_box(motion), color);
		}
	}

	// Loop over all collisions detected by the physics system
	// std::cout << "handle_collisions()" << std::endl;
	auto &collisionsRegistry = registry.collisions;
	for (uint i = 0; i < collisionsRegistry.components.size(); i++)
	{
		// The entity and its collider
		Entity entity = collisionsRegistry.entities[i];
		Entity entity_other = collisionsRegistry.components[i].other;
		// std::cout << "entity = " << entity << " entity_other = " << entity_other << std::endl;

		bool entity_is_damage_area = registry.damageAreas.has(entity);
		if (entity_is_damage_area && entity_other == player)
		{
			DamageArea &damage_area = registry.damageAreas.get(entity);
			if (damage_area.active)
			{
				if (player_comp.can_take_damage())
				{
					Damage &damage = registry.damages.get(entity);
					Health &player_health = registry.healths.get(player);
					player_health.take_damage(damage.damage);
					std::cout << "Damage area hit player for " << damage.damage << " damage" << std::endl;
				}
				else
				{
					Damage &damage = registry.damages.get(entity);
					std::cout << "Player prevented " << damage.damage << " damage by dodging" << std::endl;
				}

				if (damage_area.single_damage)
				{
					damage_area.time_until_destroyed = 0.f; // immediately destroy the damage area
				}
				else
				{
					damage_area.time_until_active = damage_area.damage_cooldown;
					damage_area.active = false;
				}
			}
		}

		// When pan hits player
		bool entity_is_pan = registry.pans.has(entity);
		if (entity_is_pan && entity_other == player)
		{
			Pan &pan = registry.pans.get(entity);

			if (pan.player_hit == false)
			{
				pan.player_hit = true;
				if (player_comp.can_take_damage())
				{
					Health &player_spy_health = registry.healths.get(player_spy);
					player_spy_health.take_damage(pan.damage);
				}
			}
			pan.state = PanState::RETURNING;
			for (Entity chef_entity : registry.chef.entities)
			{
				Motion &chef_motion = registry.motions.get(chef_entity);
				Motion &pan_motion = registry.motions.get(entity);
				vec2 direction_to_chef = normalize(chef_motion.position - pan_motion.position);
				pan_motion.velocity = direction_to_chef * 400.f;
			}
		}

		// When pan hits wall
		bool entity_other_is_wall = registry.physicsBodies.has(entity_other) && registry.physicsBodies.get(entity_other).body_type == BodyType::STATIC;
		if (entity_is_pan && entity_other_is_wall)
		{
			Pan &pan = registry.pans.get(entity);
			pan.state = PanState::RETURNING;
			for (Entity chef_entity : registry.chef.entities)
			{
				Motion &chef_motion = registry.motions.get(chef_entity);
				Motion &pan_motion = registry.motions.get(entity);
				vec2 direction_to_chef = normalize(chef_motion.position - pan_motion.position);
				pan_motion.velocity = direction_to_chef * 400.f;
			}
		}

		// When dash hits player
		bool entity_is_chef = registry.chef.has(entity);
		if (entity_is_chef && entity_other == player)
		{
			Chef &chef = registry.chef.get(entity);
			Health &player_spy_health = registry.healths.get(player_spy);

			if (!chef.dash_has_damaged && player_comp.can_take_damage())
			{
				float damage = 10.f;
				player_spy_health.take_damage(damage);
				chef.dash_has_damaged = true;
			}
		}

		if (entity == player_weapon && registry.enemies.has(entity_other))
		{
			if (registry.healths.has(entity_other))
			{
				Health &enemy_health = registry.healths.get(entity_other);
				if (enemy_health.is_dead)
				{
					// Enemy is dead; skip collision processing
					continue;
				}
			}

			Enemy &enemy = registry.enemies.get(entity_other);

			if (player_comp.state == PlayerState::LIGHT_ATTACK || player_comp.state == PlayerState::HEAVY_ATTACK)
			{
				if (enemy.last_hit_attack_id != player_comp.current_attack_id)
				{
					enemy.last_hit_attack_id = player_comp.current_attack_id;
				}
				else
				{
					// Skip collision processing if the enemy has already been hit by this attack
					continue;
				}

				float damage = (player_comp.state == PlayerState::LIGHT_ATTACK)
													 ? player_weapon_comp.damage * player_comp.damage_multiplier
													 : player_weapon_comp.damage * 2.5 * player_comp.damage_multiplier;

				// printf("Player attack type: %s, Damage: %.2f\n",
				//			 player_comp.state == PlayerState::LIGHT_ATTACK ? "Light" : "Heavy",
				//			 damage);

				if (registry.healths.has(entity_other))
				{
					Health &enemy_health = registry.healths.get(entity_other);

					// Special behavior when damaging the King boss
					if (registry.king.has(entity_other))
					{
						King &king = registry.king.get(entity_other);
						if (king.is_invincible)
						{
							continue;
						}
						if (enemy_health.health - damage <= 0.f && !king.is_second_stage)
						{
							king.is_second_stage = true;
							enemy_health.max_health = 600.f;
							enemy_health.health = 600.f;
							king.health_percentage = 1.f;
							continue;
						}
					}

					enemy_health.take_damage(damage);

					if (player_comp.state == PlayerState::LIGHT_ATTACK)
					{
						Flow &flow = registry.flows.get(flowMeterEntity);
						flow.flowLevel = std::min(flow.flowLevel + 10.f, flow.maxFlowLevel);
					}

					// std::cout << "Enemy health: " << enemy_health.health << ", damage: " << damage << std::endl;
				}
			}
		}

		if (entity == player_weapon && registry.knight.has(entity_other))
		{
			Knight &knight = registry.knight.get(entity_other);
			Player &player = registry.players.get(player_spy);

			if (knight.shield_active)
			{
				knight.shield_broken = true;
				knight.shield_active = false;

				std::cout << "Shield broken by player, player takes " << (player.attack_damage * 1.5f) << " damage" << std::endl;

				Health &player_health = registry.healths.get(player_spy);
				player_health.take_damage(player.attack_damage * 1.5f);

				if (registry.boneAnimations.has(entity_other))
				{
					BoneAnimation &bone_animation = registry.boneAnimations.get(entity_other);
					if (bone_animation.elapsed_time < 2500.f)
					{
						// play shield back to original position animation
						bone_animation.elapsed_time = 2500.f;
						bone_animation.current_keyframe = 2;
					}
				}
			}
		}

		// The entity and its collider
		// Entity entity = collisionsRegistry.entities[i];
		// Entity entity_other = collisionsRegistry.components[i].other;

		// registry.list_all_components_of(entity);

		// std::cout << "entity = " << entity << " entity_other = " << entity_other << " DONE" << std::endl;
	}

	// Remove all collisions from this simulation step
	registry.collisions.clear();
}

// Should the game be over ?
bool WorldSystem::is_over() const
{
	return bool(glfwWindowShouldClose(window));
}

std::string get_weapon_name(WeaponType type, WeaponLevel level)
{
	switch (type)
	{
	case WeaponType::SWORD:
		switch (level)
		{
		case WeaponLevel::BASIC:
			return "Basic Sword";
		case WeaponLevel::RARE:
			return "Rare Sword";
		case WeaponLevel::LEGENDARY:
			return "Legendary Sword";
		}
		break;
	case WeaponType::DAGGER:
		switch (level)
		{
		case WeaponLevel::BASIC:
			return "Basic Dagger";
		case WeaponLevel::RARE:
			return "Rare Dagger";
		case WeaponLevel::LEGENDARY:
			return "Legendary Dagger";
		}
		break;
	}
	return "";
}

// On key callback
void WorldSystem::on_key(int key, int, int action, int mod)
{
	// need to update is_holding_shift regardless of other actions
	if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
	{
		if (action == GLFW_PRESS)
		{
			is_holding_shift = true;
		}
		else if (action == GLFW_RELEASE)
		{
			is_holding_shift = false;
		}
	}

	// track player movement direction (needs to happen when paused as well)
	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_D)
		{
			player_movement_direction.x += 1.f;
		}
		else if (key == GLFW_KEY_LEFT || key == GLFW_KEY_A)
		{
			player_movement_direction.x -= 1.f;
		}
		else if (key == GLFW_KEY_UP || key == GLFW_KEY_W)
		{
			player_movement_direction.y -= 1.f;
		}
		else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_S)
		{
			player_movement_direction.y += 1.f;
		}
	}
	else if (action == GLFW_RELEASE)
	{
		if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_D)
		{
			player_movement_direction.x -= 1.f;
		}
		else if (key == GLFW_KEY_LEFT || key == GLFW_KEY_A)
		{
			player_movement_direction.x += 1.f;
		}
		else if (key == GLFW_KEY_UP || key == GLFW_KEY_W)
		{
			player_movement_direction.y += 1.f;
		}
		else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_S)
		{
			player_movement_direction.y -= 1.f;
		}
	}

	// close when esc key is pressed
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, true);
	}

	// Pausing game
	if (key == GLFW_KEY_0 && action == GLFW_PRESS)
	{
		if (!has_popup && !dialogue_active)
		{
			is_paused = !is_paused;
		}
	}

	if (key == GLFW_KEY_ENTER && action == GLFW_PRESS)
	{
		if (has_popup)
		{
			// dismiss popup
			has_popup = false;
			is_paused = false;
			if (active_popup.onDismiss != nullptr)
				active_popup.onDismiss();
			active_popup = {};
			// dismiss dialogue screen
			if (show_help_text)
			{
				show_help_text = false;
				while (registry.popupUI.entities.size() > 0)
				{
					registry.remove_all_components_of(registry.popupUI.entities.back());
				}
				if (entergame)
				{
					entergame = false;
					trigger_dialogue(background_dialogue);
				}
			}
		}

		if (dialogue_active)
		{
			current_dialogue_line++;
			if (current_dialogue_line >= dialogue_to_render.size())
			{
				// End the dialogue when all lines are shown
				dialogue_active = false;
				is_paused = false;
				current_dialogue_line = 0;

				while (registry.popupUI.entities.size() > 0)
				{
					registry.remove_all_components_of(registry.popupUI.entities.back());
				}

				// Process special dialogue end effects
				if (is_in_chef_dialogue)
				{
					is_in_chef_dialogue = false;

					// gain stealth ability
					unlocked_stealth_ability = true;

					createBackdrop(renderer);
					createDialogueWindow(renderer, {window_width_px / 2.f, window_height_px / 2.f}, {650.f, 650.f});

					Entity ability_sprite = createSprite(renderer, {window_width_px / 2.f, window_height_px / 2.f - 150.f}, {250.f, 250.f}, TEXTURE_ASSET_ID::STEALTH);
					CameraUI &camera_ui = registry.cameraUI.emplace(ability_sprite);
					camera_ui.layer = 12;

					active_popup = {PopupType::ABILITY, [this]()
													{ load_level("Level_1", 1); }, "stealth dash", "Become fast for a short time"};
					has_popup = true;
					is_paused = true;
				}

				if (is_in_knight_dialogue)
				{
					is_in_knight_dialogue = false;

					// gain stab ability
					unlocked_teleport_stab_ability = true;
					saveProgress();
					createBackdrop(renderer);
					createDialogueWindow(renderer, {window_width_px / 2.f, window_height_px / 2.f}, {650.f, 650.f});

					Entity ability_sprite = createSprite(renderer, {window_width_px / 2.f, window_height_px / 2.f - 150.f}, {250.f, 250.f}, TEXTURE_ASSET_ID::TELPORT_BACK_STAB);
					CameraUI &camera_ui = registry.cameraUI.emplace(ability_sprite);
					camera_ui.layer = 12;

					active_popup = {PopupType::ABILITY, [this]()
													{ load_level("Level_2", 2); }, "Backstab", "Teleport to the back of enemy and perform a backstab"};
					has_popup = true;
					is_paused = true;
				}

				if (is_in_prince_dialogue)
				{
					is_in_prince_dialogue = false;

					// gain stab ability
					unlocked_rage_ability = true;
					saveProgress(); // saving after unlock flag set - imp
					createBackdrop(renderer);
					createDialogueWindow(renderer, {window_width_px / 2.f, window_height_px / 2.f}, {650.f, 650.f});

					Entity ability_sprite = createSprite(renderer, {window_width_px / 2.f, window_height_px / 2.f - 150.f}, {250.f, 250.f}, TEXTURE_ASSET_ID::RAGE);
					CameraUI &camera_ui = registry.cameraUI.emplace(ability_sprite);
					camera_ui.layer = 12;

					active_popup = {PopupType::ABILITY, [this]()
													{ load_level("Level_3", 3); }, "Rage", "increase attack speed and damage for a short time"};
					has_popup = true;
					is_paused = true;
				}
			}
		}
	}

	if (is_paused || dialogue_active)
	{
		// skip processing all other types of keys when game is paused
		return;
	}

	// Resetting game
	if (action == GLFW_RELEASE && key == GLFW_KEY_R)
	{
		int w, h;
		glfwGetWindowSize(window, &w, &h);

		restart_game();
	}

	// For debugging flow meter:
	// if (action == GLFW_PRESS && key == GLFW_KEY_F)
	// {
	// 	if (registry.flows.has(flowMeterEntity))
	// 	{
	// 		Flow &flow = registry.flows.get(flowMeterEntity);
	// 		flow.flowLevel = std::min(flow.flowLevel + 10.f, flow.maxFlowLevel); // Increase flow up to max
	// 		std::cout << "Flow Level: " << flow.flowLevel << " / " << flow.maxFlowLevel << std::endl;
	// 	}
	// }

	// Debugging
	if (key == GLFW_KEY_I && action == GLFW_PRESS)
	{
		debugging.in_debug_mode = !debugging.in_debug_mode;
	}

	// FPS toggle
	if (key == GLFW_KEY_P && action == GLFW_PRESS)
	{
		show_fps = !show_fps;
		std::cout << "Show FPS: " << (show_fps ? "ON" : "OFF") << std::endl;
	}

	if (key == GLFW_KEY_O && action == GLFW_PRESS)
	{
		// show_help_text = !show_help_text;
		if (!show_help_text)
		{
			show_help_text = true;
			instruction_screen();
		}
		else
		{
			show_help_text = false;
			while (registry.popupUI.entities.size() > 0)
			{
				registry.remove_all_components_of(registry.popupUI.entities.back());
			}
		}
	}

	Motion &motion = registry.motions.get(player_spy);

	if (key == GLFW_KEY_E && action == GLFW_PRESS)
	{
		Motion &player_motion = registry.motions.get(player_spy);

		// Check for fountain interaction
		for (Entity &fountain : registry.fountains.entities)
		{
			Motion &fountain_motion = registry.motions.get(fountain);

			float distance = glm::length(player_motion.position - fountain_motion.position);
			if (distance <= 150.f)
			{
				Health &player_health = registry.healths.get(player_spy);
				player_health.health = player_health.max_health;
				printf("Player healed to max health: %.2f / %.2f\n",
							 player_health.health, player_health.max_health);
				return;
			}
		}

		// Check for treasure box interaction
		for (unsigned int i = 0; i < registry.treasureBoxes.components.size(); i++)
		{
			Entity &treasure_box_entity = registry.treasureBoxes.entities[i];
			Motion &treasure_box_motion = registry.motions.get(treasure_box_entity);
			float distance_to_player = length(treasure_box_motion.position - player_motion.position);

			if (distance_to_player < 150.f)
			{
				TreasureBox &treasure_box = registry.treasureBoxes.components[i];

				bool all_minions_defeated = false;

				// proceesing dead minions  associated w chest
				for (int j = 0; j < treasure_box.associated_minions.size();)
				{
					Entity &minion_entity = treasure_box.associated_minions[j];

					if (registry.healths.has(minion_entity))
					{
						Health &minion_health = registry.healths.get(minion_entity);
						if (minion_health.is_dead)
						{
							std::cout << " - Minion " << minion_entity << " has died and been removed from the list." << std::endl;
							treasure_box.associated_minions.erase(treasure_box.associated_minions.begin() + j);
						}
						else
						{
							std::cout << " - Minion " << minion_entity << " is still alive." << std::endl;
							j++;
						}
					}
					else
					{ // can delete dead min
						std::cout << " - Minion " << minion_entity << " has been removed (no health component)." << std::endl;
						treasure_box.associated_minions.erase(treasure_box.associated_minions.begin() + j);
					}
				}

				if (treasure_box.associated_minions.size() == 0)
				{
					all_minions_defeated = true;
					std::cout << "All associated minions have been defeated." << std::endl;
				}
				else
				{
					all_minions_defeated = false;
					std::cout << "There are still associated minions alive." << std::endl;
				}

				if (!all_minions_defeated)
				{
					std::cout << "Defeat all nearby minions before opening the chest!" << std::endl;
					return;
				}
				else
				{
					if (!treasure_box.is_open)
					{
						std::cout << "Treasure box opened" << std::endl;
						treasure_box.is_open = true;
						// play sound
						// Mix_PlayChannel(-1, break_sound, 0);

						float frame_y_offset = -100.f;
						vec2 frame_scale = {100.f, 100.f};
						vec2 item_scale = {50.f, 50.f};

						// create box item rendering
						TEXTURE_ASSET_ID item_texture_id;
						if (treasure_box.item == TreasureBoxItem::MAX_ENERGY)
						{
							item_texture_id = TEXTURE_ASSET_ID::MAX_ENERGY;
						}
						else if (treasure_box.item == TreasureBoxItem::MAX_HEALTH)
						{
							item_texture_id = TEXTURE_ASSET_ID::MAX_HEALTH;
						}
						else if (treasure_box.item == TreasureBoxItem::WEAPON)
						{
							item_texture_id = getWeaponTexture(treasure_box.weapon_type, treasure_box.weapon_level);
							item_scale = {80.f, 120.f};
							frame_scale = {80.f, 130.f};
							frame_y_offset = -100.f;
						}
						else
						{
							std::cout << "Invalid item type" << std::endl;
							continue;
						}

						// create ui frame background
						Entity ui_frame = createSprite(renderer, {treasure_box_motion.position.x, treasure_box_motion.position.y + frame_y_offset}, frame_scale, TEXTURE_ASSET_ID::UI_FRAME);
						Motion &ui_frame_motion = registry.motions.get(ui_frame);
						ui_frame_motion.layer = 3;

						if (treasure_box.item != TreasureBoxItem::NONE)
						{
							treasure_box.item_entity = createSprite(renderer, {treasure_box_motion.position.x, treasure_box_motion.position.y + frame_y_offset}, item_scale, item_texture_id);
							Motion &item_motion = registry.motions.get(treasure_box.item_entity);
							item_motion.layer = 4;
						}

						RenderRequest &render_request = registry.renderRequests.get(treasure_box_entity);
						render_request.used_texture = TEXTURE_ASSET_ID::TREASURE_BOX_OPEN;
					}
					else
					{
						std::cout << "Treasure box already opened" << std::endl;
						if (treasure_box.item != TreasureBoxItem::NONE && treasure_box.item_entity != 0)
						{
							std::string item_name = "";
							std::string item_description = "";
							if (treasure_box.item == TreasureBoxItem::MAX_HEALTH)
							{
								Health &player_health = registry.healths.get(player_spy);
								player_health.max_health += 50.f;
								player_max_health = player_health.max_health;
								item_name = "Max Health";
								item_description = "Max health increased by 50, now " + std::to_string(static_cast<int>(player_health.max_health));
							}
							else if (treasure_box.item == TreasureBoxItem::MAX_ENERGY)
							{
								Energy &player_energy = registry.energys.get(player_spy);
								player_energy.max_energy += 50.f;
								player_max_energy = player_energy.max_energy;
								item_name = "Max Energy";
								item_description = "Increase maximum energy by 50, now " + std::to_string(static_cast<int>(player_energy.max_energy));
							}
							else if (treasure_box.item == TreasureBoxItem::WEAPON)
							{
								WeaponType weapon_type = treasure_box.weapon_type;
								WeaponLevel weapon_level = treasure_box.weapon_level;
								Weapon &weapon = switchWeapon(player_spy, renderer, weapon_type, weapon_level);
								item_name = get_weapon_name(weapon_type, weapon_level);

								std::stringstream desc_ss;
								desc_ss << weapon.damage << " damage, " << weapon.attack_speed << " seconds per attack.";
								item_description = desc_ss.str();
							}

							Entity item_entity = treasure_box.item_entity;

							TEXTURE_ASSET_ID item_texture_id = registry.renderRequests.get(item_entity).used_texture;
							vec2 item_scale = registry.motions.get(item_entity).scale;

							Entity backdrop = createBackdrop(renderer);
							Entity dialogue_window = createDialogueWindow(renderer, {window_width_px / 2.f, window_height_px / 2.f}, {650.f, 650.f});

							std::cout << "texture id " << (int)item_texture_id << ", scale " << item_scale.x << ", " << item_scale.y << std::endl;
							Entity item_sprite = createSprite(renderer, {window_width_px / 2.f, window_height_px / 2.f - 150.f}, item_scale * 3.f, item_texture_id);
							CameraUI &camera_ui = registry.cameraUI.emplace(item_sprite);
							camera_ui.layer = 12;

							active_popup = {PopupType::TREASURE_BOX, [item_sprite, backdrop, dialogue_window]()
															{
																registry.remove_all_components_of(item_sprite);
																registry.remove_all_components_of(backdrop);
																registry.remove_all_components_of(dialogue_window);
															},
															item_name, item_description};
							has_popup = true;
							is_paused = true;

							// remove item rendering from box
							registry.remove_all_components_of(treasure_box.item_entity);

							// remove item from box
							treasure_box.item_entity = Entity(0);
							treasure_box.item = TreasureBoxItem::NONE;
						}
					}
				}
			}
		}
	}

	Player &player = registry.players.get(player_spy);
	if (player.state == PlayerState::LIGHT_ATTACK || player.state == PlayerState::HEAVY_ATTACK || player.state == PlayerState::CHARGING_FLOW || player.state == PlayerState::DYING)
	{
		motion.velocity = vec2(0.f, 0.f);
	}
	else if (player.state != PlayerState::DODGING)
	{
		Energy &energy = registry.energys.get(player_spy);
		// std::cout << "Player moving in " << player_movement_direction.x << "," << player_movement_direction.y << std::endl;
		if (key == GLFW_KEY_SPACE && action == GLFW_PRESS && motion.velocity != vec2(0.f, 0.f) && energy.energy >= 10.0f)
		{
			player.state = PlayerState::DODGING;
			player.consumedDodgeEnergy = true;
			std::cout << "Dodging" << std::endl;

			player.current_dodge_is_perfect = false;
			player.damage_prevented = false;
			player.current_dodge_original_motion = motion;

			Interpolation interpolate;
			interpolate.elapsed_time = 0.f;
			interpolate.initial_velocity = player_movement_direction * PLAYER_SPEED * 3.f;
			if (!registry.interpolations.has(player_spy))
			{
				registry.interpolations.emplace(player_spy, interpolate);
			}
		}
		else if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
		{
			if (action == GLFW_PRESS)
			{
				if (energy.energy > 0.f)
				{
					player.state = PlayerState::SPRINTING;
				}
			}
			else if (action == GLFW_RELEASE)
			{
				if (player.state == PlayerState::SPRINTING)
				{
					player.state = PlayerState::IDLE;
				}
			}
		}
		float speed_multiplier = 1.f;
		if (player.state == PlayerState::SPRINTING)
		{
			speed_multiplier = SPRINTING_MULTIPLIER;
		}
		motion.velocity = player_movement_direction * PLAYER_SPEED * speed_multiplier;
	}

	if (action == GLFW_PRESS && key == GLFW_KEY_1)
	{
		Player &player = registry.players.get(player_spy);

		if (!unlocked_stealth_ability)
		{
			std::cout << "Stealth ability not unlocked" << std::endl;
			return;
		}

		if ((player.state == PlayerState::IDLE || player.state == PlayerState::SPRINTING) && player.dash_cooldown_remaining_ms <= 0.0f)
		{
			player.state = PlayerState::DASHING;
			player.stealth_mode = true;
			std::cout << "DASHING NOW" << std::endl;

			// Set player opacity
			if (registry.opacities.has(player_spy))
			{
				float &opacity = registry.opacities.get(player_spy);
				opacity = 0.75f;
			}
			else
			{
				registry.opacities.insert(player_spy, 0.75f);
			}

			// Add dash component and play dash sound
			Dash dash;
			registry.dashes.emplace(player_spy, dash);
			Mix_PlayChannel(-1, spy_dash_sound, 0);
		}
		else if (player.dash_cooldown_remaining_ms > 0.0f)
		{
			std::cout << "Dash on cooldown: " << player.dash_cooldown_remaining_ms << "ms remaining." << std::endl;
		}
	}

	// if (action == GLFW_RELEASE && key == GLFW_KEY_B) // Trigger weapon switch on "B" key
	// {
	// 	WeaponType randomType = static_cast<WeaponType>(rand() % 2);
	// 	WeaponLevel randomLevel = static_cast<WeaponLevel>(rand() % 3);

	// 	std::cout << "Switched to weapon type: " << static_cast<int>(randomType)
	// 						<< " and level: " << static_cast<int>(randomLevel) << std::endl;

	// 	switchWeapon(player_spy, renderer, randomType, randomLevel);
	// }

	if (action == GLFW_PRESS && key == GLFW_KEY_H)
	{
		// Kill the current boss
		if (registry.chef.size() > 0)
		{
			Health &chef_health = registry.healths.get(registry.chef.entities[0]);
			chef_health.take_damage(chef_health.health);
		}
		else if (registry.knight.size() > 0)
		{
			Health &knight_health = registry.healths.get(registry.knight.entities[0]);
			knight_health.take_damage(knight_health.health);
		}
		else if (registry.prince.size() > 0)
		{
			Health &prince_health = registry.healths.get(registry.prince.entities[0]);
			prince_health.take_damage(prince_health.health);
		}
		else if (registry.king.size() > 0)
		{
			Health &king_health = registry.healths.get(registry.king.entities[0]);
			king_health.take_damage(king_health.health);
		}
	}

	if (key == GLFW_KEY_2 && action == GLFW_PRESS) // Example keybinding
	{
		Player &player = registry.players.get(player_spy); // Get the player component
		if (player.teleport_back_stab_cooldown <= 0.0f)		 // Check cooldown
		{
			if (perform_teleport_backstab(player_spy))
			{
				player.teleport_back_stab_cooldown = 12 * 1000.0f; // Set 12-second cooldown
			}
		}
		else
		{
			printf("Teleport Backstab ability is on cooldown! Remaining: %.2f seconds\n",
						 player.teleport_back_stab_cooldown / 1000.0f);
		}
	}

	if (key == GLFW_KEY_3 && action == GLFW_PRESS) // Example keybinding for rage ability
	{
		Player &player = registry.players.get(player_spy); // Get the player component

		if (!player.rage_activate && player.rage_cooldown <= 0.0f) // Check if rage is ready
		{
			player.rage_activate = true;						 // Set to true while rage is active
			player.rage_remaining = 10.0f * 1000.0f; // 10 seconds in milliseconds
			player.rage_cooldown = 40.0f * 1000.0f;	 // 40 seconds cooldown
			player.damage_multiplier = 2.0f;				 // Double damage during rage
			player.attack_speed_multiplier = 1.5f;	 // 50% faster attacks

			printf("Rage ability activated: Increased damage and attack speed for 10 seconds.\n");
		}
		else if (player.rage_cooldown > 0.0f)
		{
			printf("Rage ability is on cooldown! Remaining: %.2f seconds\n",
						 player.rage_cooldown / 1000.0f);
		}
	}
}

void WorldSystem::on_mouse_move(vec2 mouse_position)
{
	if (is_paused || dialogue_active)
	{
		// skip processing mouse movement when game is paused
		return;
	}

	curr_mouse_position = mouse_position;
	Motion &spy_motion = registry.motions.get(player_spy);

	vec2 spy_vector = spy_motion.position - renderer->camera_position - mouse_position;

	// float spy_angle = atan2(spy_vector.y, spy_vector.x);

	// spy_motion.angle = spy_angle;

	Player &player = registry.players.get(player_spy);
	Motion &weapon_motion = registry.motions.get(player.weapon);

	float weapon_angle = atan2(spy_vector.y, spy_vector.x) - (3.14159f / 2.0f);
	weapon_motion.angle = weapon_angle;
	w_angle = weapon_angle;

	if (spy_vector.x > 0)
	{
		spy_motion.scale.x = abs(spy_motion.scale.x);
	}
	else
	{
		spy_motion.scale.x = -abs(spy_motion.scale.x);
	}

	if (isTrackingMouse)
	{
		// Store the current mouse position in the path
		mousePath.push_back({mouse_position.x, mouse_position.y});
	}
	// (vec2) mouse_position; // dummy to avoid compiler warning
}

bool WorldSystem::isSGesture()
{
	if (mousePath.size() < 5)
		return false; // Not enough points for an "S" shape

	bool leftToRight = false, rightToLeft = false;
	float length = 0.0f;

	for (size_t i = 1; i < mousePath.size(); ++i)
	{
		float dx = mousePath[i].x - mousePath[i - 1].x;
		float dy = mousePath[i].y - mousePath[i - 1].y;
		length += std::sqrt(dx * dx + dy * dy);

		if (dx > S_THRESHOLD1)
		{
			leftToRight = true;
		}
		else if (dx < -S_THRESHOLD1 && leftToRight)
		{
			rightToLeft = true;
		}

		// O should not be detected as S
		// if (dy > S_THRESHOLD || dy < -S_THRESHOLD)
		// {
		// 	return false;
		// }
	}
	if (abs(mousePath[0].y - mousePath[mousePath.size() - 1].y) < 35.f)
	{
		return false;
	}

	if (leftToRight && rightToLeft && length > MIN_S_LENGTH)
	{
		return true;
	}
	// if start y coordinate is far from end y coordinate, not S
	return false;
}

void WorldSystem::on_mouse_button(int button, int action, int mods)
{
	if (is_paused || dialogue_active)
	{
		// skip processing mouse button events when game is paused
		return;
	}

	// std::cout << "Mouse button " << button << " " << action << " " << mods << std::endl;
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		isTrackingMouse = true; // tracking mouse gesture if any
		mousePath.clear();
		Player &player = registry.players.get(player_spy);
		Energy &energy = registry.energys.get(player_spy);
		if ((player.state == PlayerState::IDLE || player.state == PlayerState::SPRINTING) && energy.energy >= 10.0f)
		{
			// std::cout << "Player light attack" << std::endl;
			player.state = PlayerState::LIGHT_ATTACK;
			player.current_attack_id = ++attack_id_counter;
			player.consumedLightAttackEnergy = true;

			if (registry.animations.has(player_spy))
			{
				registry.animations.remove(player_spy);
			}
			float attack_speed = 1.0f; // Default speed
			Weapon &weapon = registry.weapons.get(player.weapon);
			attack_speed = weapon.attack_speed;

			Animation animation;
			animation.name = AnimationName::LIGHT_ATTACK;
			animation.elapsed_time = 0.f;
			animation.total_time = 500.f * attack_speed / player.attack_speed_multiplier;
			registry.animations.emplace(player_spy, animation);
		}
		// trigger player attack sound
		// volume up
		Mix_PlayChannel(1, spy_attack_sound, 0);
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	{
		isTrackingMouse = false;
		if (WorldSystem::isSGesture() && enlarged_player == false)
		{
			// make spy grow in size
			Motion &spy_motion = registry.motions.get(player_spy);
			spy_motion.scale = spy_motion.scale * 1.5f;
			spy_motion.bb_scale *= 1.5f;
			printf("S gesture detected\n");
			enlarged_player = true;
		}
		mousePath.clear();
	}
	if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		if (action == GLFW_PRESS)
		{
			is_right_mouse_button_down = true;
		}
		else if (action == GLFW_RELEASE)
		{
			if (!is_right_mouse_button_down)
			{
				// charging flow has been forcifully stopped due to insufficient energy
				return;
			}
			is_right_mouse_button_down = false;
			Player &player = registry.players.get(player_spy);
			if (player.state == PlayerState::CHARGING_FLOW)
			{
				player_action_finished();
			}
			if (registry.flows.has(flowMeterEntity))
			{
				Flow &flow = registry.flows.get(flowMeterEntity);
				if (flow.flowLevel == flow.maxFlowLevel)
				{
					if (player.state == PlayerState::IDLE || player.state == PlayerState::SPRINTING || player.state == PlayerState::CHARGING_FLOW)
					{
						std::cout << "Player heavy attack" << std::endl;
						Mix_PlayChannel(2, heavy_attack_sound, 0);
						player.state = PlayerState::HEAVY_ATTACK;
						player.current_attack_id = ++attack_id_counter;
						player.is_heavy_attack_second_half = false;
						flow.flowLevel = 0;
						if (registry.animations.has(player_spy))
						{
							registry.animations.remove(player_spy);
						}
						float attack_speed = 1.0f; // Default speed
						Weapon &weapon = registry.weapons.get(player.weapon);
						attack_speed = weapon.attack_speed;

						Animation animation;
						animation.name = AnimationName::HEAVY_ATTACK;
						animation.elapsed_time = 0.f;
						animation.total_time = 1000.f * attack_speed;
						registry.animations.emplace(player_spy, animation);
					}
				}
			}
		}
	}
}

Weapon &WorldSystem::switchWeapon(Entity player, RenderSystem *renderer, WeaponType newType, WeaponLevel newLevel)
{
	// Remove the existing weapon
	Player &player_comp = registry.players.get(player);
	registry.remove_all_components_of(player_comp.weapon);

	// Create and attach a new weapon
	vec2 playerPosition = registry.motions.get(player).position;
	Entity newWeapon = createWeapon(renderer, playerPosition, newType, newLevel);

	Weapon &newWeaponComp = registry.weapons.get(newWeapon);

	// Attach the new weapon component to the player
	player_comp.weapon = newWeapon;
	player_comp.weapon_offset = vec2(45.f, -50.f);

	printf("Switched to new weapon: Type=%d, Level=%d, Damage=%.2f, Attack Speed=%.2f\n",
				 static_cast<int>(newType), static_cast<int>(newLevel), newWeaponComp.damage, newWeaponComp.attack_speed);

	return newWeaponComp;
}

void WorldSystem::player_action_finished()
{
	Player &player = registry.players.get(player_spy);
	Energy &energy = registry.energys.get(player_spy);
	if (is_holding_shift && energy.energy > 0.f)
	{
		std::cout << "IS HOLDING SHIFT" << std::endl;
		player.state = PlayerState::SPRINTING;
	}
	else
	{
		player.state = PlayerState::IDLE;
	}
	Motion &player_motion = registry.motions.get(player_spy);
	player_motion.velocity = player_movement_direction * PLAYER_SPEED * (player.state == PlayerState::SPRINTING ? SPRINTING_MULTIPLIER : 1.f);
}

void WorldSystem::update_energy(float energy_time)
{
	Entity player = registry.players.entities[0];
	Energy &energy = registry.energys.get(player);
	Player &playerComp = registry.players.get(player);

	// Energy consumption rates
	const float sprintEnergyPerSec = 8.0f;
	const float heavyAttackEnergyPerSec = 20.0f;
	const float dodgeEnergyCost = 10.0f;
	const float lightAttackEnergyCost = 5.0f;
	const float energyRegenRate = 8.0f;
	bool regenerateEnergy = true;

	// Sprinting
	if (playerComp.state == PlayerState::SPRINTING)
	{
		if (player_movement_direction.x != 0 || player_movement_direction.y != 0)
		{
			energy.energy = std::max(0.0f, energy.energy - sprintEnergyPerSec * energy_time);
			regenerateEnergy = false;
		}
		if (energy.energy <= 0.0f)
		{
			playerComp.state = PlayerState::IDLE;
			regenerateEnergy = true;
		}
	}

	// Heavy attack charging
	if (playerComp.state == PlayerState::CHARGING_FLOW && energy.energy > 0)
	{
		energy.energy = std::max(0.0f, energy.energy - heavyAttackEnergyPerSec * energy_time);
	}

	// Light attack
	if (playerComp.consumedLightAttackEnergy)
	{
		if (energy.energy >= lightAttackEnergyCost)
		{
			energy.energy -= lightAttackEnergyCost;
		}
		else
		{
			// Not enough energy, cancel the attack
			playerComp.state = PlayerState::IDLE;
		}
		playerComp.consumedLightAttackEnergy = false; // Reset flag
	}

	// Dodge
	if (playerComp.consumedDodgeEnergy)
	{
		if (energy.energy >= dodgeEnergyCost)
		{
			energy.energy -= dodgeEnergyCost;
		}
		else
		{
			// Not enough energy, cancel the dodge
			playerComp.state = PlayerState::IDLE;
		}
		playerComp.consumedDodgeEnergy = false; // Reset flag
	}

	if (playerComp.state == PlayerState::DODGING || playerComp.state == PlayerState::LIGHT_ATTACK || playerComp.state == PlayerState::HEAVY_ATTACK || playerComp.state == PlayerState::CHARGING_FLOW)
	{
		// does not regen energy during these actions
		regenerateEnergy = false;
	}

	// Energy regeneration
	if (regenerateEnergy && energy.energy < energy.max_energy)
	{
		energy.energy = std::min(energy.max_energy, energy.energy + energyRegenRate * energy_time);
	}

	// Update energy bar
	Motion &energyBarMotion = registry.motions.get(energy.energybar);
	EnergyBar &energyBar = registry.energybar.get(energy.energybar);
	float energyRatio = energy.energy / energy.max_energy;
	energyBarMotion.scale.x = energyBar.original_scale.x * energyRatio;
}

void WorldSystem::trigger_dialogue(std::vector<std::string> dialogue)
{
	// Start the dialogue
	dialogue_to_render = dialogue;
	dialogue_active = true;
	current_dialogue_line = 0;
	createBackdrop(renderer);
	createDialogueWindow(renderer, {window_width_px / 2.f, window_height_px - 200}, {window_width_px - 100.f, 300.f});

	time_until_dialogue_pause = DIALOGUE_PAUSE_DELAY;

	// const Entity &spy_entity = registry.players.entities[0];
	// Motion &spy_motion = registry.motions.get(spy_entity);
	// createDialogueWindow(renderer, {spy_motion.position.x - 250.f, spy_motion.position.y + 60.f});
}

bool WorldSystem::perform_teleport_backstab(Entity player_spy)
{
	// Find the nearest enemy
	Entity nearest_enemy = find_nearest_enemy(player_spy);

	if (nearest_enemy == static_cast<Entity>(0))
	{
		printf("No enemy found to backstab!\n");
		return false;
	}

	// Get enemy's position
	Motion &enemy_motion = registry.motions.get(nearest_enemy);
	vec2 enemy_position = enemy_motion.position;

	vec2 screen_size = vec2(window_width_px, window_height_px);
	vec2 camera_top_left = renderer->camera_position;
	vec2 camera_bottom_right = camera_top_left + screen_size;

	if (enemy_position.x < camera_top_left.x ||
			enemy_position.x > camera_bottom_right.x ||
			enemy_position.y < camera_top_left.y ||
			enemy_position.y > camera_bottom_right.y)
	{
		printf("Enemy is not visible on the screen. Backstab canceled.\n");
		return false;
	}

	// Calculate backstab position (slightly behind the enemy)
	Motion &spy_motion = registry.motions.get(player_spy);
	vec2 backstab_position = enemy_position - vec2(30.f, 30.f); //-normalize(enemy_motion.velocity) * 50.0f;

	// Teleport the player_spy to the backstab position
	spy_motion.position = backstab_position;
	update_camera_view();
	// printf("Player position: (%.2f, %.2f)\n", spy_motion.position.x, spy_motion.position.y);
	// printf("Camera position: (%.2f, %.2f)\n", renderer->camera_position.x, renderer->camera_position.y);

	// Deal critical damage
	Health &enemy_health = registry.healths.get(nearest_enemy);
	float damage = 125.0f;
	enemy_health.take_damage(damage);

	printf("Player performed a backstab! Dealt %.2f critical damage.\n", damage);

	// Add visual effects or animations (optional)
	// createBackstabEffect(backstab_position); // Placeholder for visual effect

	return true;
}

Entity WorldSystem::find_nearest_enemy(Entity player_spy)
{
	Motion &spy_motion = registry.motions.get(player_spy);
	vec2 spy_position = spy_motion.position;

	Entity nearest_enemy = static_cast<Entity>(0);
	float min_distance = std::numeric_limits<float>::max();

	for (Entity enemy : registry.enemies.entities)
	{
		Health &enemy_health = registry.healths.get(enemy);
		if (enemy_health.is_dead)
		{
			continue;
		}

		Motion &enemy_motion = registry.motions.get(enemy);
		float distance = length(spy_position - enemy_motion.position);
		if (distance < min_distance)
		{
			min_distance = distance;
			nearest_enemy = enemy;
		}
	}

	return nearest_enemy;
}

void WorldSystem::saveProgress()
{
	// Define the file path for saving progress
	std::string filePath = PROJECT_SOURCE_DIR "/progress.json";

	nlohmann::json saveData;
	saveData["player_max_health"] = player_max_health;
	saveData["player_max_energy"] = player_max_energy;
	saveData["unlocked_stealth_ability"] = unlocked_stealth_ability;
	saveData["unlocked_teleport_stab_ability"] = unlocked_teleport_stab_ability;
	saveData["unlocked_rage_ability"] = unlocked_rage_ability;
	saveData["current_level"] = current_level;
	saveData["unlocked_weapons"] = nlohmann::json::array();
	for (const auto &weaponEntity : registry.weapons.entities)
	{
		if (registry.weapons.has(weaponEntity))
		{
			Weapon &weapon = registry.weapons.get(weaponEntity);
			nlohmann::json weaponData;
			weaponData["type"] = static_cast<int>(weapon.type);
			weaponData["level"] = static_cast<int>(weapon.level);
			weaponData["damage"] = weapon.damage;
			weaponData["attack_speed"] = weapon.attack_speed;

			saveData["unlocked_weapons"].push_back(weaponData);
		}
	}

	std::ofstream file(filePath);
	if (file.is_open())
	{
		file << saveData.dump(4); // indent value !
		file.close();
		std::cout << "Game progress saved successfully.\n";
	}
	else
	{
		std::cerr << "Failed to open file for saving progress.\n";
	}
}

void WorldSystem::loadProgress()
{
	std::string filePath = PROJECT_SOURCE_DIR "/progress.json";

	std::ifstream file(filePath);
	if (file.is_open())
	{
		nlohmann::json saveData;
		file >> saveData;
		file.close();

		player_max_health = saveData.value("player_max_health", 100.0f);
		player_max_energy = saveData.value("player_max_energy", 100.0f);
		unlocked_stealth_ability = saveData.value("unlocked_stealth_ability", false);
		unlocked_teleport_stab_ability = saveData.value("unlocked_teleport_stab_ability", false);
		unlocked_rage_ability = saveData.value("unlocked_rage_ability", false);
		current_level = saveData.value("current_level", 0);

		std::cout << "Progress loaded successfully.\n";
	}
	else
	{
		player_max_health = 100.0f;
		player_max_energy = 100.0f;
		unlocked_stealth_ability = false;
		unlocked_teleport_stab_ability = false;
		unlocked_rage_ability = false;
		current_level = 0;

		std::cout << "No progress file found. Initialized default progress.\n";
	}
}

void WorldSystem::updateBackgroundForLevel(int levelNumber)
{
	if (registry.backgrounds.entities.size() > 0)
	{
		Entity backgroundEntity = registry.backgrounds.entities[0];
		RenderRequest &renderRequest = registry.renderRequests.get(backgroundEntity);

		// Assign the correct texture based on levelNumber
		switch (levelNumber)
		{
		case 0:
			renderRequest.used_texture = TEXTURE_ASSET_ID::BACKGROUND_LEVEL_0;
			break;
		case 1:
			renderRequest.used_texture = TEXTURE_ASSET_ID::BACKGROUND_LEVEL_1;
			break;
		case 2:
			renderRequest.used_texture = TEXTURE_ASSET_ID::BACKGROUND_LEVEL_2;
			break;
		case 3:
			renderRequest.used_texture = TEXTURE_ASSET_ID::BACKGROUND_LEVEL_3;
			break;
		default:
			renderRequest.used_texture = TEXTURE_ASSET_ID::BACKGROUND_LEVEL_0;
			break;
		}
	}
}

void WorldSystem::initializeAbilityIcons(RenderSystem *renderer)
{
	// Create the backstab ability icon
	backstab_icon = createSprite(renderer, {window_width_px - 360.f, window_height_px - 100.f}, {100.f, 100.f}, TEXTURE_ASSET_ID::TELPORT_BACK_STAB);
	registry.cameraUI.emplace(backstab_icon);
	registry.opacities.emplace(backstab_icon, 0.0f); // Fully visible by default

	// Create the rage ability icon
	rage_icon = createSprite(renderer, {window_width_px - 480.f, window_height_px - 100.f}, {100.f, 100.f}, TEXTURE_ASSET_ID::RAGE);
	registry.cameraUI.emplace(rage_icon);
	registry.opacities.emplace(rage_icon, 0.0f); // Fully visible by default

	stealth_icon = createSprite(renderer, {window_width_px - 240.f, window_height_px - 100.f}, {100.f, 100.f}, TEXTURE_ASSET_ID::STEALTH);
	registry.cameraUI.emplace(stealth_icon);
	registry.opacities.emplace(stealth_icon, 0.0f);

	//// Initially hide the icons
	// registry.renderRequests.remove(backstab_icon);
	// registry.renderRequests.remove(rage_icon);
}
void WorldSystem::updateAbilityIcons(RenderSystem *renderer, float elapsed_ms)
{
	Player &player = registry.players.get(player_spy);

	// --- Backstab Ability ---
	if (unlocked_teleport_stab_ability)
	{
		float &opacity = registry.opacities.get(backstab_icon);

		if (player.teleport_back_stab_cooldown <= 0.0f)
		{
			// Fully visible when ready
			opacity = 1.0f;
		}
		// else if (player.stealth_mode)
		//{
		//	// Flashing effect when in use
		//	float time = static_cast<float>(glfwGetTime());
		//	opacity = (sin(time * 10.0f) + 1.0f) / 2.0f; // Alternates between 0 and 1
		// }
		else
		{
			// Reduced opacity when on cooldown
			opacity = 0.5f;
		}
	}

	// --- Rage Ability ---
	if (unlocked_rage_ability)
	{

		float &opacity = registry.opacities.get(rage_icon);

		if (player.rage_cooldown <= 0.0f && !player.rage_activate)
		{
			// Fully visible when ready
			opacity = 1.0f;
		}
		else if (player.rage_activate)
		{
			// Flashing effect when in use
			float time = static_cast<float>(glfwGetTime());
			opacity = (sin(time * 10.0f) + 1.0f) / 2.0f; // Alternates between 0 and 1
		}
		else
		{
			// Reduced opacity when on cooldown
			opacity = 0.5f;
		}
	}

	if (unlocked_stealth_ability)
	{
		float &opacity = registry.opacities.get(stealth_icon);

		if (player.dash_cooldown_remaining_ms <= 0.0f && !player.stealth_mode)
		{
			// Fully visible when ready
			opacity = 1.0f;
		}
		else if (player.stealth_mode)
		{
			// Flashing effect when in use
			float time = static_cast<float>(glfwGetTime());
			opacity = (sin(time * 10.0f) + 1.0f) / 2.0f; // Alternates between 0 and 1
		}
		else
		{
			// Reduced opacity when on cooldown
			opacity = 0.5f;
		}
	}
}
