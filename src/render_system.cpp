// internal
#include "render_system.hpp"
#include <SDL.h>

#include "tiny_ecs_registry.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <sstream>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
// https://www.youtube.com/watch?app=desktop&v=BA6aR_5C_BM - fps source
extern bool show_fps;
extern bool show_help_text;
extern float fps;
extern bool dialogue_active;
extern int current_dialogue_line; // Tracks the current line of dialogue being shown
extern std::vector<std::string> dialogue_to_render;
// for DASH rendering
// extern bool unlocked_stealth_ability;
extern bool unlocked_teleport_stab_ability;
extern bool dashAvailable;
extern bool dashInUse;
extern bool has_popup;
extern Popup active_popup;

const float VIEW_CULLING_MARGIN = 200.f; // pixels in each direction to still consider in screen

glm::mat3 get_transform(const Motion &motion)
{
	Transform transform;
	transform.translate(motion.position);

	transform.translate(-motion.pivot_offset * motion.scale);
	transform.rotate(motion.angle);
	transform.translate(motion.pivot_offset * motion.scale);

	transform.scale(motion.scale);

	return transform.mat;
}

BoneTransform interpolate_bone_transform(const BoneTransform &a, const BoneTransform &b, float t)
{
	BoneTransform result;
	result.position = a.position * (1.f - t) + b.position * t;
	result.angle = a.angle * (1.f - t) + b.angle * t;
	result.scale = a.scale * (1.f - t) + b.scale * t;
	return result;
}

void RenderSystem::drawTexturedMesh(Entity entity, const mat3 &view, const mat3 &projection)
{
	Motion &motion = registry.motions.get(entity);
	// Transformation code, see Rendering and Transformation in the template
	// specification for more info Incrementally updates transformation matrix,
	// thus ORDER IS IMPORTANT
	glm::mat3 transform_mat = get_transform(motion);

	assert(registry.renderRequests.has(entity));
	const RenderRequest &render_request = registry.renderRequests.get(entity);

	const GLuint used_effect_enum = (GLuint)render_request.used_effect;
	assert(used_effect_enum != (GLuint)EFFECT_ASSET_ID::EFFECT_COUNT);
	const GLuint program = (GLuint)effects[used_effect_enum];

	// Setting shaders
	glUseProgram(program);
	gl_has_errors();

	assert(render_request.used_geometry != GEOMETRY_BUFFER_ID::GEOMETRY_COUNT);
	const GLuint vbo = vertex_buffers[(GLuint)render_request.used_geometry];
	const GLuint ibo = index_buffers[(GLuint)render_request.used_geometry];

	// Setting vertex and index buffers
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	gl_has_errors();

	// Input data location as in the vertex buffer
	if (render_request.used_effect == EFFECT_ASSET_ID::TEXTURED || render_request.used_effect == EFFECT_ASSET_ID::SKINNED)
	{
		GLint in_position_loc = glGetAttribLocation(program, "in_position");
		GLint in_texcoord_loc = glGetAttribLocation(program, "in_texcoord");
		gl_has_errors();
		assert(in_texcoord_loc >= 0);

		glEnableVertexAttribArray(in_position_loc);
		glVertexAttribPointer(in_position_loc, 3, GL_FLOAT, GL_FALSE,
													sizeof(TexturedVertex), (void *)0);
		gl_has_errors();

		glEnableVertexAttribArray(in_texcoord_loc);
		glVertexAttribPointer(
				in_texcoord_loc, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex),
				(void *)sizeof(
						vec3)); // note the stride to skip the preceeding vertex position

		// Enabling and binding texture to slot 0
		glActiveTexture(GL_TEXTURE0);
		gl_has_errors();

		assert(registry.renderRequests.has(entity));
		GLuint texture_id =
				texture_gl_handles[(GLuint)registry.renderRequests.get(entity).used_texture];

		glBindTexture(GL_TEXTURE_2D, texture_id);
		gl_has_errors();

		if (render_request.used_effect == EFFECT_ASSET_ID::SKINNED)
		{
			// use bone buffers
			const GLuint indices_vbo = bone_indices_vbo[(GLuint)render_request.used_geometry];
			const GLuint weights_vbo = bone_weights_vbo[(GLuint)render_request.used_geometry];

			// Set bones
			GLuint in_bone_indices_loc = glGetAttribLocation(program, "in_bone_indices");
			GLuint in_bone_weights_loc = glGetAttribLocation(program, "in_bone_weights");
			gl_has_errors();

			glBindBuffer(GL_ARRAY_BUFFER, indices_vbo);
			glEnableVertexAttribArray(in_bone_indices_loc);
			glVertexAttribIPointer(in_bone_indices_loc, 4, GL_INT, sizeof(glm::ivec4), (void *)0);
			gl_has_errors();

			glBindBuffer(GL_ARRAY_BUFFER, weights_vbo);
			glEnableVertexAttribArray(in_bone_weights_loc);
			glVertexAttribPointer(in_bone_weights_loc, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void *)0);
			gl_has_errors();

			// Set bone matrices
			std::vector<glm::mat3> bone_matrices;
			// std::vector<MeshBone> &bones = skinned_meshes[(int)render_request.used_geometry].bones;
			std::vector<MeshBone> &bones = registry.meshBones.get(entity).bones;
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

			GLuint bone_matrices_uloc = glGetUniformLocation(program, "bone_matrices");
			glUniformMatrix3fv(bone_matrices_uloc, bone_matrices.size(), GL_FALSE, glm::value_ptr(bone_matrices[0]));
			gl_has_errors();
		}
	}
	else if (render_request.used_effect == EFFECT_ASSET_ID::DEBUG_LINE)
	{
		GLint in_position_loc = glGetAttribLocation(program, "in_position");
		GLint in_color_loc = glGetAttribLocation(program, "in_color");
		gl_has_errors();

		glEnableVertexAttribArray(in_position_loc);
		glVertexAttribPointer(in_position_loc, 3, GL_FLOAT, GL_FALSE,
													sizeof(ColoredVertex), (void *)0);
		gl_has_errors();

		glEnableVertexAttribArray(in_color_loc);
		glVertexAttribPointer(in_color_loc, 3, GL_FLOAT, GL_FALSE,
													sizeof(ColoredVertex), (void *)sizeof(vec3));
		gl_has_errors();
	}
	else if (render_request.used_effect == EFFECT_ASSET_ID::PROGRESS_BAR)
	{
		GLint in_position_loc = glGetAttribLocation(program, "in_position");
		gl_has_errors();

		glEnableVertexAttribArray(in_position_loc);
		glVertexAttribPointer(in_position_loc, 3, GL_FLOAT, GL_FALSE,
													sizeof(ColoredVertex), (void *)0);
		gl_has_errors();
		// GLint in_position_loc = glGetAttribLocation(program, "in_position");
		// GLint in_color_loc = glGetAttribLocation(program, "in_color");
		// gl_has_errors();

		// glEnableVertexAttribArray(in_position_loc);
		// glVertexAttribPointer(in_position_loc, 3, GL_FLOAT, GL_FALSE,
		//                       sizeof(ColoredVertex), (void *)0);
		// gl_has_errors();

		// glEnableVertexAttribArray(in_color_loc);
		// glVertexAttribPointer(in_color_loc, 3, GL_FLOAT, GL_FALSE,
		//                       sizeof(ColoredVertex), (void *)sizeof(vec3));
		// gl_has_errors();
	}
	else if (render_request.used_effect == EFFECT_ASSET_ID::LIQUID_FILL)
	{
		GLint in_position_loc = glGetAttribLocation(program, "in_position");
		GLint in_texcoord_loc = glGetAttribLocation(program, "in_texcoord");
		gl_has_errors();

		glEnableVertexAttribArray(in_position_loc);
		glVertexAttribPointer(in_position_loc, 3, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), (void *)0);
		gl_has_errors();

		glEnableVertexAttribArray(in_texcoord_loc);
		glVertexAttribPointer(in_texcoord_loc, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), (void *)sizeof(vec3));
		gl_has_errors();

		// Enable and bind texture
		glActiveTexture(GL_TEXTURE0);
		GLuint texture_id = texture_gl_handles[(GLuint)registry.renderRequests.get(entity).used_texture];
		glBindTexture(GL_TEXTURE_2D, texture_id);
		gl_has_errors();

		// Set flowValue uniform
		Flow &flow = registry.flows.get(entity); // Assuming flow component
		GLint flowValue_uloc = glGetUniformLocation(program, "flowValue");
		glUniform1f(flowValue_uloc, flow.flowLevel / flow.maxFlowLevel);
		gl_has_errors();

		// Set color uniform
		GLint color_uloc = glGetUniformLocation(program, "liquidColor");
		vec3 color = vec3(0.0, 0.7, 1.0); // Customize the liquid color
		glUniform3fv(color_uloc, 1, (float *)&color);
		gl_has_errors();

		GLint outlineColor_uloc = glGetUniformLocation(program, "outlineColor");
		vec4 outlineColor = vec4(0.0, 0.0, 0.0, 1.0);								// Black outline
		glUniform4fv(outlineColor_uloc, 1, (float *)&outlineColor); // Pass outline color
		gl_has_errors();
	}
	else
	{
		assert(false && "Type of render request not supported");
	}

	// Getting uniform locations for glUniform* calls
	GLint color_uloc = glGetUniformLocation(program, "fcolor");
	const vec3 color = registry.colors.has(entity) ? registry.colors.get(entity) : vec3(1);
	glUniform3fv(color_uloc, 1, (float *)&color);
	gl_has_errors();

	GLuint opacity_uloc = glGetUniformLocation(program, "opacity");
	float opacity = registry.opacities.has(entity) ? registry.opacities.get(entity) : 1.f;
	glUniform1f(opacity_uloc, opacity);
	gl_has_errors();

	// Get number of indices from index buffer, which has elements uint16_t
	GLint size = 0;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
	gl_has_errors();

	GLsizei num_indices = size / sizeof(uint16_t);
	// GLsizei num_triangles = num_indices / 3;

	GLint currProgram;
	glGetIntegerv(GL_CURRENT_PROGRAM, &currProgram);
	// Setting uniform values to the currently bound program
	GLuint transform_loc = glGetUniformLocation(currProgram, "transform");
	glUniformMatrix3fv(transform_loc, 1, GL_FALSE, (float *)&transform_mat);
	GLuint projection_loc = glGetUniformLocation(currProgram, "projection");
	glUniformMatrix3fv(projection_loc, 1, GL_FALSE, (float *)&projection);
	GLuint view_loc = glGetUniformLocation(currProgram, "view");
	glUniformMatrix3fv(view_loc, 1, GL_FALSE, (float *)&view);
	gl_has_errors();
	// Drawing of num_indices/3 triangles specified in the index buffer
	glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, nullptr);
	gl_has_errors();
}

// draw the intermediate texture to the screen, with some distortion to simulate
// water
void RenderSystem::drawToScreen()
{
	// Setting shaders
	// get the water texture, sprite mesh, and program
	glUseProgram(effects[(GLuint)EFFECT_ASSET_ID::WATER]);
	gl_has_errors();
	// Clearing backbuffer
	int w, h;
	glfwGetFramebufferSize(window, &w, &h); // Note, this will be 2x the resolution given to glfwCreateWindow on retina displays
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, w, h);
	glDepthRange(0, 10);
	glClearColor(1.f, 0, 0, 1.0);
	glClearDepth(1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindVertexArray(vao); // ensure valid vao
	gl_has_errors();
	// Enabling alpha channel for textures
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	// Draw the screen texture on the quad geometry
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[(GLuint)GEOMETRY_BUFFER_ID::SCREEN_TRIANGLE]);
	glBindBuffer(
			GL_ELEMENT_ARRAY_BUFFER,
			index_buffers[(GLuint)GEOMETRY_BUFFER_ID::SCREEN_TRIANGLE]); // Note, GL_ELEMENT_ARRAY_BUFFER associates
																																	 // indices to the bound GL_ARRAY_BUFFER
	gl_has_errors();

	const GLuint water_program = effects[(GLuint)EFFECT_ASSET_ID::WATER];
	// Set clock
	GLuint time_uloc = glGetUniformLocation(water_program, "time");
	GLuint dead_timer_uloc = glGetUniformLocation(water_program, "darken_screen_factor");
	glUniform1f(time_uloc, (float)(glfwGetTime() * 10.0f));
	ScreenState &screen = registry.screenStates.get(screen_state_entity);
	glUniform1f(dead_timer_uloc, screen.darken_screen_factor);
	gl_has_errors();

	// Set the vertex position and vertex texture coordinates (both stored in the
	// same VBO)
	GLint in_position_loc = glGetAttribLocation(water_program, "in_position");
	glEnableVertexAttribArray(in_position_loc);
	glVertexAttribPointer(in_position_loc, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void *)0);
	gl_has_errors();

	// Bind our texture in Texture Unit 0
	glActiveTexture(GL_TEXTURE0);

	glBindTexture(GL_TEXTURE_2D, off_screen_render_buffer_color);
	gl_has_errors();

	// Draw
	glDrawElements(
			GL_TRIANGLES, 3, GL_UNSIGNED_SHORT,
			nullptr); // one triangle = 3 vertices; nullptr indicates that there is
								// no offset from the bound index buffer
	gl_has_errors();
}

// Render our game world
// http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/
void RenderSystem::draw()
{
	// Getting size of window
	int w, h;
	glfwGetFramebufferSize(window, &w, &h); // Note, this will be 2x the resolution given to glfwCreateWindow on retina displays

	// First render to the custom framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer);
	gl_has_errors();
	// Clearing backbuffer
	glViewport(0, 0, w, h);
	glDepthRange(0.00001, 10);
	glClearColor(GLfloat(150 / 255.0f), GLfloat(150 / 255.0f), GLfloat(150 / 255.0f), 1.0);
	glClearDepth(10.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST); // native OpenGL does not work with a depth buffer
														// and alpha blending, one would have to sort
														// sprites back to front
	glBindVertexArray(vao);		// ensure valid vao
	gl_has_errors();

	mat3 projection_2D = createProjectionMatrix();
	mat3 camera_view = createCameraViewMatrix();

	std::vector<Entity> entities_to_draw_first;
	std::vector<std::pair<Entity, std::pair<int, float>>> entities_to_draw;

	std::vector<Entity> ui_entities_to_draw_first;
	std::vector<std::pair<Entity, int>> ui_entities_to_draw;

	// Draw all textured meshes that have a position and size component
	for (Entity entity : registry.renderRequests.entities)
	{
		if (!registry.motions.has(entity))
			continue;

		if (registry.cameraUI.has(entity))
		{
			CameraUI &camera_ui = registry.cameraUI.get(entity);

			if (camera_ui.ignore_render_order)
			{
				ui_entities_to_draw_first.push_back(entity);
				continue;
			}

			ui_entities_to_draw.push_back(std::make_pair(entity, camera_ui.layer));
		}
		else
		{
			// view culling
			Motion &motion = registry.motions.get(entity);
			vec2 half_scale = {abs(motion.scale.x) / 2.f, abs(motion.scale.y) / 2.f};
			if ((motion.position.x + half_scale.x < camera_position.x - VIEW_CULLING_MARGIN || motion.position.x - half_scale.x > camera_position.x + window_width_px + VIEW_CULLING_MARGIN) || (motion.position.y + half_scale.y < camera_position.y - VIEW_CULLING_MARGIN || motion.position.y - half_scale.y > camera_position.y + window_height_px + VIEW_CULLING_MARGIN))
			{
				continue;
			}

			if (motion.ignore_render_order)
			{
				entities_to_draw_first.push_back(entity);
				continue;
			}

			entities_to_draw.push_back(std::make_pair(entity, std::make_pair(motion.layer, motion.position.y + motion.bb_offset.y)));
		}
	}

	// sort entities by y position
	std::sort(entities_to_draw.begin(), entities_to_draw.end(), [](const std::pair<Entity, std::pair<int, float>> &a, const std::pair<Entity, std::pair<int, float>> &b)
						{ return a.second.first < b.second.first || (a.second.first == b.second.first && a.second.second < b.second.second); });
	std::sort(ui_entities_to_draw.begin(), ui_entities_to_draw.end(), [](const std::pair<Entity, int> &a, const std::pair<Entity, int> &b)
						{ return a.second < b.second; });

	// std::cout << "Entities to draw: " << entities_to_draw.size() << std::endl;

	mat3 identity_view = mat3(1.0f); // Identity matrix

	for (auto &entity : ui_entities_to_draw_first)
	{
		drawTexturedMesh(entity, identity_view, projection_2D);
	}

	for (auto &entity : entities_to_draw_first)
	{
		drawTexturedMesh(entity, camera_view, projection_2D);
	}

	for (auto &entry : entities_to_draw)
	{
		drawTexturedMesh(entry.first, camera_view, projection_2D);
	}

	for (auto &entry : ui_entities_to_draw)
	{
		drawTexturedMesh(entry.first, identity_view, projection_2D);
	}

	for (Entity enemy : registry.enemies.entities)
	{
		auto &health = registry.healths.get(enemy);
		auto &render_request = registry.renderRequests.get(enemy);
		auto &motion = registry.motions.get(enemy);

		if (health.health <= 0)
		{
			if (!health.is_dead)
			{
				motion.scale.y *= 0.7;
				motion.scale.x *= -1;
				motion.scale *= 0.85;
				health.is_dead = true;
				PhysicsBody &enemy_physics = registry.physicsBodies.get(enemy);
				enemy_physics.body_type = BodyType::NONE;
			}
			Enemy &enemy_comp = registry.enemies.get(enemy);
			enemy_comp.state = EnemyState::DEAD;
			if (registry.motions.has(enemy))
			{
				Motion &enemy_motion = registry.motions.get(enemy);
				enemy_motion.velocity = {0.f, 0.f};
			}
			// Change texture to corpse
			render_request.used_texture = TEXTURE_ASSET_ID::ENEMY_CORPSE;
		}
		// if in attack state and not usign attack sprite, change sprite, does not include chef
		// auto& enemy_state = registry.enemies.get(enemy);
		// if(enemy_state.state == EnemyState::ATTACK && !registry.chef.has(enemy) && ! (render_request.used_texture == TEXTURE_ASSET_ID::ENEMY_ATTACK)){
		// 	render_request.used_texture = TEXTURE_ASSET_ID::ENEMY_ATTACK;
		// 	// get motion of the enemy
		// 	auto& enemy_motion = registry.motions.get(enemy);
		// 	enemy_motion.scale.x *= 1.2;
		// }
		// if(enemy_state.state == EnemyState::COMBAT && !registry.chef.has(enemy) && ! (render_request.used_texture == TEXTURE_ASSET_ID::ENEMY)){
		// 	render_request.used_texture = TEXTURE_ASSET_ID::ENEMY;
		// 	auto& enemy_motion = registry.motions.get(enemy);
		// }
	}

	Entity spy = registry.players.entities[0];
	auto &health = registry.healths.get(spy);
	auto &render_request = registry.renderRequests.get(spy);
	auto &motion = registry.motions.get(spy);
	if (health.health <= 0)
	{
		// Change texture to corpse
		if (!health.is_dead)
		{
			motion.scale.y *= 0.65;
			health.is_dead = true;
			// implementation only consider case with 1 weapon
			// assertion fails
			// if(registry.playerWeapons.has(spy)){
			// PlayerWeapon &player_weapon = registry.playerWeapons.get(entity);
			// registry.renderRequests.remove(player_weapon.weapon);
			// }
		}
		render_request.used_texture = TEXTURE_ASSET_ID::SPY_CORPSE;
	}

	// Render FPS in top-right corner
	if (show_fps)
	{
		int fpsInt = static_cast<int>(fps);
		if (fpsInt != 0)
		{
			std::stringstream fpsText;
			fpsText << "FPS: " << fpsInt;
			renderText(fpsText.str(), 5.f, window_height_px - 30.f, 1.0f, vec3(1.0, 0.0, 0.0));
		}
	}

	if (show_help_text)
	{
		// float x = 10.0f;										 // Starting x position
		// float y = window_height_px - 100.0f; // Starting y position (from top)
		// float lineSpacing = 25.0f;
		// float scale = 0.8f;
		// vec3 textColor = vec3(1.0f, 1.0f, 1.0f);

		// for (const std::string &line : gameInstructions)
		// {
		// 	renderText(line, x, y, scale, textColor);
		// 	y -= lineSpacing;
		// }
	}

	// bool renderD = false;
	// if (unlocked_stealth_ability)
	//{
	//	if (dashAvailable)
	//	{
	//		renderD = true;
	//	}
	//	else if (dashInUse)
	//	{
	//		float time = static_cast<float>(glfwGetTime());
	//		if (fmod(time, 1.0f) < 0.5f)
	//		{
	//			renderD = true;
	//		}
	//	}
	// }

	// if (renderD)
	//{
	//	float x = window_width_px - 220.0f;
	//	float y = 75.0f;
	//	float scale = 3.0f;
	//	vec3 color = vec3(1.0f, 1.0f, 1.0f);

	//	renderText("X", x, y, scale, color);
	//}

	if (dialogue_active)
	{
		renderDialogueLine(dialogue_to_render[current_dialogue_line]);
		renderText("Press Enter to continue", window_width_px - 270.f, 60.f, 1.0f, vec3(1.0f, 1.0f, 1.0f));
	}

	if (has_popup)
	{
		renderPopup(active_popup);
	}

	// Truely render to the screen
	drawToScreen();

	// flicker-free display with a double buffer
	glfwSwapBuffers(window);
	gl_has_errors();
}

mat3 RenderSystem::createProjectionMatrix()
{
	// Fake projection matrix, scales with respect to window coordinates
	float left = 0.f;
	float top = 0.f;

	gl_has_errors();
	float right = (float)window_width_px;
	float bottom = (float)window_height_px;

	float sx = 2.f / (right - left);
	float sy = 2.f / (top - bottom);
	float tx = -(right + left) / (right - left);
	float ty = -(top + bottom) / (top - bottom);
	return {{sx, 0.f, 0.f}, {0.f, sy, 0.f}, {tx, ty, 1.f}};
}

mat3 RenderSystem::createCameraViewMatrix()
{
	Transform transform;
	transform.translate(camera_position * -1.f);
	return transform.mat;
}

// boilerplate code generated with the help of gpt
// and https://learnopengl.com/code_viewer_gh.php?code=src/7.in_practice/2.text_rendering/text_rendering.cpp
void RenderSystem::renderText(const std::string &text, float x, float y, float scale, vec3 color)
{
	// Note: (0, 0) for renderText (x, y) is the bottom-left corner of the window (instead of top-left for the rest of the game)

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST); // Disable depth testing to ensure text renders on top - added

	GLuint textProgram = effects[(GLuint)EFFECT_ASSET_ID::TEXT];
	glUseProgram(textProgram);
	gl_has_errors();

	// Set the text color uniform
	GLint textColorLoc = glGetUniformLocation(textProgram, "textColor");
	if (textColorLoc == -1)
	{
		std::cerr << "Could not find 'textColor' uniform location." << std::endl;
		assert(false);
	}
	glUniform3f(textColorLoc, color.x, color.y, color.z);
	gl_has_errors();

	Transform transform;
	transform.translate({x, y});
	transform.scale({1.f, 1.f});
	GLint textTransformLoc = glGetUniformLocation(textProgram, "transform");
	glUniformMatrix3fv(textTransformLoc, 1, GL_FALSE, (float *)&transform.mat);

	// Set the sampler uniform to use texture unit 0
	GLint samplerLoc = glGetUniformLocation(textProgram, "text");
	if (samplerLoc == -1)
	{
		std::cerr << "Could not find 'text' sampler uniform location." << std::endl;
		assert(false);
	}
	glUniform1i(samplerLoc, 0); // Texture unit 0
	gl_has_errors();

	// Bind the VAO
	glBindVertexArray(textVAO);
	gl_has_errors();

	// Activate texture unit 0
	glActiveTexture(GL_TEXTURE0);
	gl_has_errors();

	// Iterate through all characters in the text string
	for (const char &c : text)
	{
		// Check if the character exists in the map
		if (characters.find(c) == characters.end())
		{
			std::cerr << "Character '" << c << "' not found in character map." << std::endl;
			assert(false);

			continue;
		}

		Character ch = characters[c];

		// std::cout << "Character '" << c << "' should be rendered" << std::endl;

		// Calculate the position and size of the character quad
		float xpos = x + ch.Bearing.x * scale;
		float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
		float w = ch.Size.x * scale;
		float h = ch.Size.y * scale;

		// Define the vertex data for the character quad
		float vertices[6][4] = {
				{xpos, ypos + h, 0.0f, 0.0f},
				{xpos, ypos, 0.0f, 1.0f},
				{xpos + w, ypos, 1.0f, 1.0f},

				{xpos, ypos + h, 0.0f, 0.0f},
				{xpos + w, ypos, 1.0f, 1.0f},
				{xpos + w, ypos + h, 1.0f, 0.0f}};

		// Bind the glyph texture
		glBindTexture(GL_TEXTURE_2D, ch.TextureID);
		gl_has_errors();

		// Update the content of the VBO with the character quad data
		glBindBuffer(GL_ARRAY_BUFFER, textVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
		gl_has_errors();

		// Render the character quad
		glDrawArrays(GL_TRIANGLES, 0, 6);

		gl_has_errors();

		// Advance the cursor to the next character position
		x += (ch.Advance >> 6) * scale; // Bitshift by 6 to get value in pixels (1/64)
	}

	// Unbind the VAO and texture
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);

	gl_has_errors();
}

void RenderSystem::initTextRendering()
{
	// Generate VAO and VBO
	glGenVertexArrays(1, &textVAO);
	glGenBuffers(1, &textVBO);

	// Bind VAO and VBO
	glBindVertexArray(textVAO);
	glBindBuffer(GL_ARRAY_BUFFER, textVBO);

	// Reserve buffer space but don't fill it yet
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

	// Configure vertex attributes
	glEnableVertexAttribArray(0); // Layout location 0 in shader
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

	// Unbind VAO and VBO to prevent accidental modification
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	gl_has_errors();

	std::cout << "initTextRendering() completed" << std::endl;
}

void RenderSystem::loadFont(const std::string &fontPath)
{

	textProgram = effects[(GLuint)EFFECT_ASSET_ID::TEXT];
	glUseProgram(textProgram);

	glm::mat4 projection = glm::ortho(
			0.0f, static_cast<float>(window_width_px),
			0.0f, static_cast<float>(window_height_px));

	GLint projLoc = glGetUniformLocation(textProgram, "projection");
	if (projLoc == -1)
	{
		std::cerr << "Failed to find 'projection' uniform in text shader." << std::endl;
		assert(false);
		return;
	}
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, (float *)&projection);
	std::cout << "projection being set properly." << std::endl;

	// Initialize FreeType library
	FT_Library ft;
	if (FT_Init_FreeType(&ft))
	{
		std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
		assert(false);
		return;
	}

	// Load font face
	FT_Face face;
	if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
	{
		std::cerr << "ERROR::FREETYPE: Failed to load font: " << fontPath << std::endl;
		assert(false);
		return;
	}
	else
	{
		std::cout << "Loading font from: " << fontPath << std::endl;
	}

	// Set font size
	FT_Set_Pixel_Sizes(face, 0, 24); // Adjust the font size as needed

	// Disable byte-alignment restriction
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Load the first 128 ASCII characters
	for (unsigned char c = 0; c < 128; c++)
	{
		// Load character glyph
		if (FT_Load_Char(face, c, FT_LOAD_RENDER))
		{
			std::cerr << "ERROR::FREETYPE: Failed to load Glyph for character " << c << std::endl;
			continue;
		}

		// Generate texture for the glyph
		GLuint texture;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		// Set texture parameters
		glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RED, // Use GL_RED since glyphs are grayscale
				face->glyph->bitmap.width,
				face->glyph->bitmap.rows,
				0,
				GL_RED,
				GL_UNSIGNED_BYTE,
				face->glyph->bitmap.buffer);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Prevent wrapping
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Prevent wrapping
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);		 // Linear filtering
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		Character character = {
				texture,
				glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
				glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
				static_cast<GLuint>(face->glyph->advance.x)};
		characters.insert(std::pair<char, Character>(c, character));
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	// Clean up FreeType resources
	FT_Done_Face(face);
	FT_Done_FreeType(ft);

	// bind buffers
	glBindVertexArray(textVAO);
	glBindBuffer(GL_ARRAY_BUFFER, textVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

	// Unbind texture
	glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderSystem::renderPopup(const Popup &popup)
{
	switch (popup.type)
	{
	case PopupType::ABILITY:
	{
		renderText("You have unlocked the " + popup.content_slot_1 + " ability", window_width_px / 2 - 300, window_height_px / 2 - 100, 1.3f, vec3(1.0f, 1.0f, 1.0f));
		renderText("Description: " + popup.content_slot_2, window_width_px / 2 - 300, window_height_px / 2 - 200, 0.8f, vec3(1.0f, 1.0f, 1.0f));
		break;
	}
	case PopupType::TREASURE_BOX:
	{
		renderText("You received " + popup.content_slot_1 + "!", window_width_px / 2 - 300, window_height_px / 2 - 100, 1.3f, vec3(1.0f, 1.0f, 1.0f));
		renderText(popup.content_slot_2, window_width_px / 2 - 300, window_height_px / 2 - 200, 0.8f, vec3(1.0f, 1.0f, 1.0f));
		break;
	}
	case PopupType::HELP:
	{
		renderText("HOW TO PLAY", window_width_px / 2 - 175, 630.f, 3.f, vec3(1.0f, 1.0f, 1.0f));
		renderText(popup.content_slot_1, window_width_px / 2 - 400, 550.f, 1.7f, vec3(1.0f, 1.0f, 1.0f));
		renderText(popup.content_slot_2, window_width_px / 2 + 200, 550.f, 1.7f, vec3(1.0f, 1.0f, 1.0f));
		float x = 50.0f;
		float x_2 = 700.f;
		float y = window_height_px - 225.0f;
		float y_2 = window_height_px - 225.0f;
		float y_3 = window_height_px - 500.f;
		float lineSpacing = 45.0f;
		float scale = 1.2f;
		vec3 textColor = vec3(1.0f, 1.0f, 1.0f);

		for (const std::string &line : controls)
		{
			renderText(line, x, y, scale, textColor);
			y -= lineSpacing;
		}
		for (const std::string &line : UI_interactions)
		{
			renderText(line, x_2, y_2, scale, textColor);
			y_2 -= lineSpacing;
		}
		for (const std::string &line : mechanisms)
		{
			renderText(line, x_2, y_3, scale, textColor);
			y_3 -= lineSpacing;
		}
		renderText("Press Enter to start!", window_width_px - 470.f, 60.f, 2.1f, vec3(1.0f, 1.0f, 1.0f));
		break;
	}
	default:
		break;
	}
}

void RenderSystem::renderDialogueLine(const std::string &line)
{
	float x = 100.f; // Starting x position for the text
	float y = 250.f; // Position inside the dialogue window
	float scale = 1.2f;
	vec3 textColor = vec3(1.0f, 1.0f, 1.0f); // Dark gray text color

	// std::cout << "Render text length: " << line.size() << std::endl;
	if (line.size() > 100)
	{
		// find last "space" before 100
		int lastSpace = 0;
		for (int i = 100; i > 0; i--)
		{
			if (line[i] == ' ')
			{
				lastSpace = i;
				break;
			}
		}
		if (lastSpace == 0)
		{
			// no space found, just render the line
			renderText(line, x, y, scale, textColor);
		}
		else
		{
			// render the line up to the last space
			renderText(line.substr(0, lastSpace), x, y, scale, textColor);
			// render the rest of the line
			renderText(line.substr(lastSpace + 1), x, y - 50, scale, textColor);
		}
	}
	else
	{
		renderText(line, x, y, scale, textColor);
	}
}
