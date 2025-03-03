#pragma once

#include <array>
#include <utility>
#include <map>

#include "common.hpp"
#include "components.hpp"
#include "tiny_ecs.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H

// font character structure
struct Character
{
	unsigned int TextureID; // ID handle of the glyph texture
	glm::ivec2 Size;				// Size of glyph
	glm::ivec2 Bearing;			// Offset from baseline to left/top of glyph
	unsigned int Advance;		// Offset to advance to next glyph
	char character;
};

glm::mat3 get_transform(const Motion &motion);

// System responsible for setting up OpenGL and for rendering all the
// visual entities in the game
class RenderSystem
{
	/**
	 * The following arrays store the assets the game will use. They are loaded
	 * at initialization and are assumed to not be modified by the render loop.
	 *
	 * Whenever possible, add to these lists instead of creating dynamic state
	 * it is easier to debug and faster to execute for the computer.
	 */
	std::array<GLuint, texture_count> texture_gl_handles;
	std::array<ivec2, texture_count> texture_dimensions;

	// Make sure these paths remain in sync with the associated enumerators.
	// Associated id with .obj path
	const std::vector<std::pair<GEOMETRY_BUFFER_ID, std::string>> mesh_paths =
			{
					std::pair<GEOMETRY_BUFFER_ID, std::string>(GEOMETRY_BUFFER_ID::SALMON, mesh_path("salmon.obj"))
					// specify meshes of other assets here
	};

	// Make sure these paths remain in sync with the associated enumerators.
	const std::array<std::string, texture_count> texture_paths = {
			textures_path("green_fish.png"),
			textures_path("eel.png"),
			textures_path("floor_tile_square.png"),
			textures_path("wall.png"),
			textures_path("spy_no_weapon.png"),
			textures_path("enemy_small.png"),
			textures_path("sword_lvl2.png"),
			textures_path("flow_meter.png"),
			textures_path("enemy_corpse.png"),
			textures_path("enemy_attack.png"),
			textures_path("spy_corpse.png"),
			textures_path("chef.png"),
			textures_path("tomato.png"),
			textures_path("pan.png"),
			textures_path("sword_lvl1.png"),
			textures_path("sword_lvl2.png"),
			textures_path("sword_lvl3.png"),
			textures_path("dagger_lvl1.png"),
			textures_path("dagger_lvl2.png"),
			textures_path("dagger_lvl3.png"),
			textures_path("knight.png"),
			textures_path("prince.png"),
			textures_path("king.png"),
			textures_path("fountain.png"),
			textures_path("stealth.png"),
			textures_path("ability2.png"),
			textures_path("ability3.png"),
			textures_path("treasure_box.png"),
			textures_path("treasure_box_open.png"),
			textures_path("ui_frame.png"),
			textures_path("max_health.png"),
			textures_path("max_energy.png"),
			textures_path("arch_minion.png"),
			textures_path("arch_minion_attack.png"),
			textures_path("arrow.png"),
			textures_path("firerain.png"),
			textures_path("lasers.png"),
			textures_path("summon_soldier.png"),
			textures_path("dialogue_bg.png"),
			textures_path("level1_bg.png"),
			textures_path("level2_bg.png"),
			textures_path("level3_bg.png"),
			textures_path("level4_bg.png"),

			textures_path("/boss_animation/chef_1/chef_attack(1)0.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)1.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)2.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)3.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)4.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)5.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)6.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)7.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)8.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)9.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)10.png"),
			textures_path("/boss_animation/chef_1/chef_attack(1)11.png"),

			// chef attack 2
			textures_path("/boss_animation/chef_2/chef_attack(2)0000.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0001.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0002.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0003.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0004.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0005.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0006.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0007.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0008.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0009.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0010.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0011.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0012.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0013.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0014.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0015.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0016.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0017.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0018.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0019.png"),
			textures_path("/boss_animation/chef_2/chef_attack(2)0020.png"),

			// chef attack 3
			textures_path("/boss_animation/chef_3/chef_attack(3)0000.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0001.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0002.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0003.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0004.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0005.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0006.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0007.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0008.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0009.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0010.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0011.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0012.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0013.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0014.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0015.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0016.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0017.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0018.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0019.png"),
			textures_path("/boss_animation/chef_3/chef_attack(3)0020.png"),
	};

	std::array<GLuint, effect_count> effects;
	// Make sure these paths remain in sync with the associated enumerators.
	const std::array<std::string, effect_count> effect_paths = {
			shader_path("debug_line"),
			shader_path("skinned"),
			shader_path("textured"),
			shader_path("water"),
			shader_path("progress_bar"),
			shader_path("liquid_fill"),
			shader_path("text")};

	std::array<GLuint, geometry_count> vertex_buffers;
	std::array<GLuint, geometry_count> index_buffers;
	std::array<Mesh, geometry_count> meshes;
	std::array<TexturedMesh, geometry_count> textured_meshes;
	std::array<GLuint, geometry_count> bone_weights_vbo;
	std::array<GLuint, geometry_count> bone_indices_vbo;

public:
	std::array<SkinnedMesh, geometry_count> skinned_meshes;

	// Initialize the window
	bool init(GLFWwindow *window);

	template <class T>
	void bindVBOandIBO(GEOMETRY_BUFFER_ID gid, std::vector<T> vertices, std::vector<uint16_t> indices);

	void initializeGlTextures();

	void initializeGlEffects();

	void initializeGlMeshes();
	Mesh &getMesh(GEOMETRY_BUFFER_ID id) { return meshes[(int)id]; };
	TexturedMesh &getTexturedMesh(GEOMETRY_BUFFER_ID id) { return textured_meshes[(int)id]; };

	void initializeGlGeometryBuffers();
	// Initialize the screen texture used as intermediate render target
	// The draw loop first renders to this texture, then it is used for the wind
	// shader
	bool initScreenTexture();

	// Destroy resources associated to one or all entities created by the system
	~RenderSystem();

	// Draw all entities
	void draw();
	void renderText(const std::string &text, float x, float y, float scale, vec3 color);
	mat3 createProjectionMatrix();
	mat3 createCameraViewMatrix();
	void initTextRendering();
	void loadFont(const std::string &fontPath);
	void renderDialogueLine(const std::string &line);
	void renderPopup(const Popup &popup);
	void set_background_texture(TEXTURE_ASSET_ID background_texture);

	vec2 camera_position = {0.f, 0.f};

private:
	// Internal drawing functions for each entity type
	void drawTexturedMesh(Entity entity, const mat3 &view, const mat3 &projection);
	void drawToScreen();

	// Window handle
	GLFWwindow *window;

	// Screen texture handles
	GLuint frame_buffer;
	GLuint off_screen_render_buffer_color;
	GLuint off_screen_render_buffer_depth;

	GLuint vao;

	GLuint textVAO;
	GLuint textVBO;
	GLuint textProgram;
	Entity screen_state_entity;
	std::map<char, Character> characters;
	std::vector<std::string> gameInstructions = {
			"Controls:",
			"W / Up Arrow: Move Up",
			"A / Left Arrow: Move Left",
			"S / Down Arrow: Move Down",
			"D / Right Arrow: Move Right",
			"Shift (Hold): Sprint",
			"X: Stealth Dash (Unlocked after defeating the chef)",
			"Space Bar: Dodge (Requires energy)",
			"E: Interact (e.g., heal at fountain, open treasure box)",
			"Escape: Exit Game",
			"P: Toggle FPS Display",
			"O: Toggle Help Instructions",
			"B: Switch Weapon",
			"Left Mouse Click: Light Attack (Requires energy)",
			"Right Mouse Click (Hold): Charge Flow (Hold to charge)",
			"Right Mouse Click (Release with full flow): Heavy Attack",
			"Draw 'S' with Mouse: Enlarge Player (Temporary)",
	};
	std::vector<std::string> controls = {
			"WASD/Arrow Keys: Basic directional movement",
			"Shift and hold: Sprint (while holding directional keys)",
			"Space Bar: Dodge, (Requires energy)",
			"Left Mouse Click: Light Attack (Requires energy)",
			"Right Mouse Click/Hold: Charge Flow (Hold to charge)",
			"Right Mouse Click (Release with full flow): Heavy Attack",
			"Draw 'S' with Mouse: Enlarge Player (Temporary)",
			"1: Stealth Dash (Unlocked after defeating the chef)",
			"2: Teleport Backstab (Unlocked after defeating the knight)",
			"3: Rage Mode (Unlocked after defeating the prince)",
	};
	std::vector<std::string> UI_interactions = {
			"E: Interact with fountain/ treasure box",
			"Enter: Continue dialogue/ Exit interaction/help screen",
			"Escape: Exit Game",
			"P: Toggle FPS Display",
			"O: Open Help screen",
	};
	std::vector<std::string> mechanisms = {
			"Treasure box: New weapons/increase health/energy",
			"Fountains will heal you to full health",
			"Take on your journey to bring peace to Astraeth!",
	};
};

bool loadEffectFromFile(
		const std::string &vs_path, const std::string &fs_path, GLuint &out_program);
