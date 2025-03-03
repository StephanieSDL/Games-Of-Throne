// internal
#include "render_system.hpp"

#include <array>
#include <fstream>

#include "../ext/stb_image/stb_image.h"
#include <ft2build.h>
#include FT_FREETYPE_H

// This creates circular header inclusion, that is quite bad.
#include "tiny_ecs_registry.hpp"

// stlib
#include <iostream>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// World initialization
bool RenderSystem::init(GLFWwindow *window_arg)
{
	this->window = window_arg;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // vsync

	// Load OpenGL function pointers
	const int is_fine = gl3w_init();
	assert(is_fine == 0);

	// Create a frame buffer
	frame_buffer = 0;
	glGenFramebuffers(1, &frame_buffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer);
	gl_has_errors();

	// For some high DPI displays (ex. Retina Display on Macbooks)
	// https://stackoverflow.com/questions/36672935/why-retina-screen-coordinate-value-is-twice-the-value-of-pixel-value
	int frame_buffer_width_px, frame_buffer_height_px;
	glfwGetFramebufferSize(window, &frame_buffer_width_px, &frame_buffer_height_px); // Note, this will be 2x the resolution given to glfwCreateWindow on retina displays
	if (frame_buffer_width_px != window_width_px)
	{
		printf("WARNING: retina display! https://stackoverflow.com/questions/36672935/why-retina-screen-coordinate-value-is-twice-the-value-of-pixel-value\n");
		printf("glfwGetFramebufferSize = %d,%d\n", frame_buffer_width_px, frame_buffer_height_px);
		printf("window width_height = %d,%d\n", window_width_px, window_height_px);
	}

	// Hint: Ask your TA for how to setup pretty OpenGL error callbacks.
	// This can not be done in macOS, so do not enable
	// it unless you are on Linux or Windows. You will need to change the window creation
	// code to use OpenGL 4.3 (not suported in macOS) and add additional .h and .cpp
	// glDebugMessageCallback((GLDEBUGPROC)errorCallback, nullptr);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	gl_has_errors();

	// Disable face culling
	glDisable(GL_CULL_FACE);
	glFrontFace(GL_CCW); // Counter clockwise front face

	initScreenTexture();
	initializeGlTextures();
	initializeGlEffects();

	gl_has_errors();

	initializeGlGeometryBuffers();
	initTextRendering();
	loadFont(font_path("Orange.ttf"));

	gl_has_errors();

	return true;
}

void RenderSystem::initializeGlTextures()
{
	glGenTextures((GLsizei)texture_gl_handles.size(), texture_gl_handles.data());

	for (uint i = 0; i < texture_paths.size(); i++)
	{
		const std::string &path = texture_paths[i];
		std::cout << "initializing" << path << std::endl;
		ivec2 &dimensions = texture_dimensions[i];

		stbi_uc *data;
		data = stbi_load(path.c_str(), &dimensions.x, &dimensions.y, NULL, 4);

		if (data == NULL)
		{
			const std::string message = "Could not load the file " + path + ".";
			fprintf(stderr, "%s", message.c_str());
			assert(false);
		}
		glBindTexture(GL_TEXTURE_2D, texture_gl_handles[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dimensions.x, dimensions.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		gl_has_errors();
		stbi_image_free(data);
	}
	gl_has_errors();
}

void RenderSystem::initializeGlEffects()
{
	for (uint i = 0; i < effect_paths.size(); i++)
	{
		const std::string vertex_shader_name = effect_paths[i] + ".vs.glsl";
		const std::string fragment_shader_name = effect_paths[i] + ".fs.glsl";

		bool is_valid = loadEffectFromFile(vertex_shader_name, fragment_shader_name, effects[i]);
		assert(is_valid && (GLuint)effects[i] != 0);
	}
}

// One could merge the following two functions as a template function...
template <class T>
void RenderSystem::bindVBOandIBO(GEOMETRY_BUFFER_ID gid, std::vector<T> vertices, std::vector<uint16_t> indices)
{
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[(uint)gid]);
	glBufferData(GL_ARRAY_BUFFER,
							 sizeof(vertices[0]) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
	gl_has_errors();

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffers[(uint)gid]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
							 sizeof(indices[0]) * indices.size(), indices.data(), GL_STATIC_DRAW);
	gl_has_errors();
}

void RenderSystem::initializeGlMeshes()
{
	for (uint i = 0; i < mesh_paths.size(); i++)
	{
		// Initialize meshes
		GEOMETRY_BUFFER_ID geom_index = mesh_paths[i].first;
		std::string name = mesh_paths[i].second;
		Mesh::loadFromOBJFile(name,
													meshes[(int)geom_index].vertices,
													meshes[(int)geom_index].vertex_indices,
													meshes[(int)geom_index].original_size);

		bindVBOandIBO(geom_index,
									meshes[(int)geom_index].vertices,
									meshes[(int)geom_index].vertex_indices);
	}
}

void RenderSystem::initializeGlGeometryBuffers()
{
	// Vertex Buffer creation.
	glGenBuffers((GLsizei)vertex_buffers.size(), vertex_buffers.data());
	// Index Buffer creation.
	glGenBuffers((GLsizei)index_buffers.size(), index_buffers.data());

	glGenBuffers((GLsizei)bone_weights_vbo.size(), bone_weights_vbo.data());
	glGenBuffers((GLsizei)bone_indices_vbo.size(), bone_indices_vbo.data());

	// Index and Vertex buffer data initialization.
	initializeGlMeshes();

	//////////////////////////
	// Initialize sprite
	// The position corresponds to the center of the texture.
	std::vector<TexturedVertex> textured_vertices(4);
	textured_vertices[0].position = {-1.f / 2, +1.f / 2, 0.f}; // top left
	textured_vertices[1].position = {+1.f / 2, +1.f / 2, 0.f}; // top right
	textured_vertices[2].position = {+1.f / 2, -1.f / 2, 0.f}; // bottom right
	textured_vertices[3].position = {-1.f / 2, -1.f / 2, 0.f}; // bottom left
	textured_vertices[0].texcoord = {0.f, 1.f};
	textured_vertices[1].texcoord = {1.f, 1.f};
	textured_vertices[2].texcoord = {1.f, 0.f};
	textured_vertices[3].texcoord = {0.f, 0.f};

	// Counterclockwise winding direction
	const std::vector<uint16_t> textured_indices = {0, 1, 3, 1, 2, 3};
	bindVBOandIBO(GEOMETRY_BUFFER_ID::SPRITE, textured_vertices, textured_indices);

	// //////////////////////
	// Initialize Weapon
	std::vector<TexturedVertex> weapon_vertices = {
			// lower
			{{-0.2f, 0.5f, 1.f}, {0.3f, 1.f}},
			{{0.2f, 0.5f, 1.f}, {0.7f, 1.f}},
			{{0.2f, 0.2f, 1.f}, {0.7f, 0.7f}},
			{{-0.2f, 0.2f, 1.f}, {0.3f, 0.7f}},
			// upper
			{{-0.35f, 0.15f, 1.f}, {0.15f, 0.65f}},
			{{0.35f, 0.15f, 1.f}, {0.85f, 0.65f}},
			{{0.35f, -0.35f, 1.f}, {0.85f, 0.15f}},
			{{-0.35f, -0.35f, 1.f}, {0.15f, 0.15f}},
			// tip
			{{0.f, -0.5f, 1.f}, {0.5f, 0.f}},				 // 8
			{{-0.35f, -0.35f, 1.f}, {0.15f, 0.15f}}, // 9
			{{0.35f, -0.35f, 1.f}, {0.85f, 0.15f}},	 // 10
			// middle
			{{-0.5f, 0.2f, 1.f}, {0.f, 0.7f}},	 // 11
			{{0.5f, 0.2f, 1.f}, {1.f, 0.7f}},		 // 12
			{{0.5f, 0.15f, 1.f}, {1.f, 0.65f}},	 // 13
			{{-0.5f, 0.15f, 1.f}, {0.f, 0.65f}}, // 14
	};
	std::vector<uint16_t> weapon_indices = {0, 1, 3, 1, 2, 3, 4, 5, 7, 5, 6, 7, 8, 9, 10, 11, 12, 14, 12, 13, 14};

	textured_meshes[(int)GEOMETRY_BUFFER_ID::WEAPON].vertices = weapon_vertices;
	textured_meshes[(int)GEOMETRY_BUFFER_ID::WEAPON].vertex_indices = weapon_indices;
	bindVBOandIBO(GEOMETRY_BUFFER_ID::WEAPON, weapon_vertices, weapon_indices);

	std::vector<TexturedVertex> dagger_vertices = {
			// lower
			{{-0.1f, 0.4f, 1.f}, {0.4f, .9f}},	// 0
			{{0.1f, 0.4f, 1.f}, {0.6f, .9f}},		// 1
			{{0.1f, 0.1f, 1.f}, {0.6f, 0.6f}},	// 2
			{{-0.1f, 0.1f, 1.f}, {0.4f, 0.6f}}, // 3
			// upper
			{{-0.25f, 0.05f, 1.f}, {0.25f, 0.55f}},	 // 4
			{{0.25f, 0.05f, 1.f}, {0.75f, 0.55f}},	 // 5
			{{0.25f, -0.25f, 1.f}, {0.75f, 0.25f}},	 // 6
			{{-0.25f, -0.25f, 1.f}, {0.25f, 0.25f}}, // 7
			// tip
			{{0.f, -0.4f, 1.f}, {0.5f, 0.1f}},			 // 8
			{{-0.25f, -0.25f, 1.f}, {0.25f, 0.25f}}, // 9
			{{0.25f, -0.25f, 1.f}, {0.75f, 0.25f}},	 // 10
	};

	std::vector<uint16_t> dagger_indices = {
			// Lower part
			0, 1, 3,
			1, 2, 3,
			// Upper part
			4, 5, 7,
			5, 6, 7,
			// Tip
			8, 9, 10,
			3, 5, 4,
			3, 2, 5};

	textured_meshes[(int)GEOMETRY_BUFFER_ID::DAGGER].vertices = dagger_vertices;
	textured_meshes[(int)GEOMETRY_BUFFER_ID::DAGGER].vertex_indices = dagger_indices;
	bindVBOandIBO(GEOMETRY_BUFFER_ID::DAGGER, dagger_vertices, dagger_indices);

	// Initialize progress bar
	std::vector<ColoredVertex> progress_vertices;
	std::vector<uint16_t> progress_indices;
	progress_vertices.push_back({{0.f, -0.5f, 0.f}, {0.f, 1.f, 0.f}});
	progress_vertices.push_back({{0.f, 0.5f, 0.f}, {0.f, 1.f, 0.f}});
	progress_vertices.push_back({{1.f, 0.5f, 0.f}, {0.f, 1.f, 0.f}});
	progress_vertices.push_back({{1.f, -0.5f, 0.f}, {0.f, 1.f, 0.f}});

	// Use two triangles to make a rectangle
	progress_indices = {0, 1, 2, 0, 2, 3};
	int geom_index = (int)GEOMETRY_BUFFER_ID::PROGRESS_BAR;
	meshes[geom_index].vertices = progress_vertices;
	meshes[geom_index].vertex_indices = progress_indices;
	bindVBOandIBO(GEOMETRY_BUFFER_ID::PROGRESS_BAR, meshes[geom_index].vertices, meshes[geom_index].vertex_indices);

	//////////////////////////////////
	// Initialize debug line
	std::vector<ColoredVertex> line_vertices;
	std::vector<uint16_t> line_indices;

	constexpr float depth = 0.5f;
	constexpr vec3 white = {1.f, 1.f, 1.f};

	// Corner points
	line_vertices = {
			{{-0.5, -0.5, depth}, white},
			{{-0.5, 0.5, depth}, white},
			{{0.5, 0.5, depth}, white},
			{{0.5, -0.5, depth}, white},
	};

	// Two triangles
	line_indices = {0, 1, 3, 1, 2, 3};

	geom_index = (int)GEOMETRY_BUFFER_ID::DEBUG_LINE;
	meshes[geom_index].vertices = line_vertices;
	meshes[geom_index].vertex_indices = line_indices;
	bindVBOandIBO(GEOMETRY_BUFFER_ID::DEBUG_LINE, line_vertices, line_indices);

	///////////////////////////
	// Initialize knight
	std::vector<TexturedVertex> knight_vertices = {
			// shield (0-6)
			{{-0.33f, 0.1f, 1.f}, {0.17f, 0.6f}},		 // 0 center
			{{-0.5f, -0.07f, 1.f}, {0.f, 0.43f}},		 // 1 top left
			{{-0.5f, 0.17f, 1.f}, {0.f, 0.67f}},		 // 2 middle left
			{{-0.33f, 0.43f, 1.f}, {0.17f, 0.93f}},	 // 3 bottom
			{{-0.16f, -0.07f, 1.f}, {0.34f, 0.43f}}, // 4 top right
			{{-0.16f, 0.15f, 1.f}, {0.34f, 0.65f}},	 // 5 middle right
			{{-0.33f, -0.12f, 1.f}, {0.17f, 0.38f}}, // 6 top
			// head (7-15)
			{{-0.01f, -0.35f, 1.f}, {0.49f, 0.15f}}, // 7 center
			{{-0.25f, -0.3f, 1.f}, {0.25f, 0.2f}},	 // 8 middle left
			{{-0.18f, -0.13f, 1.f}, {0.32f, 0.37f}}, // 9 bottom left
			{{-0.01f, -0.01f, 1.f}, {0.49f, 0.49f}}, // 10 bottom
			{{0.16f, -0.13f, 1.f}, {0.66f, 0.37f}},	 // 11 bottom right
			{{0.2f, -0.3f, 1.f}, {0.7f, 0.2f}},			 // 12 middle right
			{{-0.01f, -0.5f, 1.f}, {0.49f, 0.f}},		 // 13 top
			{{-0.2f, -0.5f, 1.f}, {0.3f, 0.f}},			 // 14 top left
			{{0.15f, -0.5f, 1.f}, {0.65f, 0.f}},		 // 15 top right
			// body (16-31)
			{{0.f, 0.2f, 1.f}, {0.5f, 0.7f}},				 // 16 center
			{{-0.25f, -0.22f, 1.f}, {0.25f, 0.28f}}, // 17 arm left top
			{{-0.33f, -0.12f, 1.f}, {0.17f, 0.38f}}, // 18 = 6 top
			{{-0.16f, -0.07f, 1.f}, {0.34f, 0.43f}}, // 19 = 4 top right
			{{-0.16f, 0.15f, 1.f}, {0.34f, 0.65f}},	 // 20 = 5 middle right
			{{-0.26f, 0.32f, 1.f}, {0.24f, 0.82f}},	 // 21 bottom left
			{{-0.18f, -0.13f, 1.f}, {0.32f, 0.37f}}, // 22 = 9 bottom left
			{{-0.01f, -0.01f, 1.f}, {0.49f, 0.49f}}, // 23 = 10 bottom
			{{-0.1f, 0.4f, 1.f}, {0.4f, 0.9f}},			 // 24 bottom middle left
			{{0.f, 0.48f, 1.f}, {0.5f, 0.98f}},			 // 25 bottom
			{{0.1f, 0.4f, 1.f}, {0.6f, 0.9f}},			 // 26 bottom middle right
			{{0.26f, 0.33f, 1.f}, {0.76f, 0.82f}},	 // 27 bottom right
			{{0.31f, 0.f, 1.f}, {0.81f, 0.5f}},			 // 28 middle right
			{{0.29f, -0.1f, 1.f}, {0.79f, 0.4f}},		 // 29 middle right upper
			{{0.18f, -0.19f, 1.f}, {0.68f, 0.31f}},	 // 30 arm right top
			{{0.16f, -0.13f, 1.f}, {0.66f, 0.37f}},	 // 31 = 11 bottom right
			// sword (32-43)
			{{0.3f, -0.48f, 1.f}, {0.8f, 0.02f}},		// 32 handle top left
			{{0.4f, -0.48f, 1.f}, {0.9f, 0.02f}},		// 33 handle top right
			{{0.3f, -0.24f, 1.f}, {0.8f, 0.26f}},		// 34 handle bottom left
			{{0.4f, -0.24f, 1.f}, {0.9f, 0.26f}},		// 35 handle bottom right
			{{0.2f, -0.24f, 1.f}, {0.7f, 0.26f}},		// 36 bar top left
			{{0.5f, -0.24f, 1.f}, {1.0f, 0.26f}},		// 37 bar top right
			{{0.22f, -0.15f, 1.f}, {0.72f, 0.35f}}, // 38 bar bottom left
			{{0.5f, -0.15f, 1.f}, {1.0f, 0.35f}},		// 39 bar bottom right
			{{0.3f, -0.15f, 1.f}, {0.8f, 0.35f}},		// 40 blade top left
			{{0.4f, -0.15f, 1.f}, {0.9f, 0.35f}},		// 41 blade top right
			{{0.3f, 0.45f, 1.f}, {0.8f, 0.95f}},		// 42 blade bottom left
			{{0.4f, 0.45f, 1.f}, {0.9f, 0.95f}},		// 43 blade bottom right
	};
	std::vector<uint16_t> knight_indices = {
			// head (0-7)
			9, 7, 8,
			10, 7, 9,
			10, 11, 7,
			11, 12, 7,
			12, 15, 7,
			15, 13, 7,
			7, 13, 14,
			7, 14, 8,
			// body (8-22)
			22, 17, 18,
			19, 22, 18,
			19, 23, 22,
			20, 23, 19,
			16, 23, 20,
			16, 20, 21,
			24, 16, 21,
			25, 16, 24,
			26, 16, 25,
			27, 16, 26,
			28, 16, 27,
			28, 29, 16,
			29, 30, 31,
			29, 31, 16,
			31, 23, 16,
			// shield (23-28)
			2, 0, 1,
			3, 0, 2,
			3, 5, 0,
			5, 4, 0,
			0, 6, 1,
			0, 4, 6,
			// sword (29-34)
			35, 32, 34,
			35, 33, 32,
			39, 36, 38,
			39, 37, 36,
			43, 40, 42,
			43, 41, 40};

	std::vector<MeshBone> knight_bones = {
			{-1}, // 0 body
			{0},	// 1 shield
			{0},	// 2 head
			{0},	// 3 sword
	};

	std::vector<glm::ivec4> knight_bone_indices = {
			// shield (0-6)
			{1, 0, 0, 0}, // 0
			{1, 0, 0, 0}, // 1
			{1, 0, 0, 0}, // 2
			{1, 0, 0, 0}, // 3
			{1, 0, 0, 0}, // 4
			{1, 0, 0, 0}, // 5
			{1, 0, 0, 0}, // 6
			// head (7-15)
			{2, 0, 0, 0}, // 7
			{2, 0, 0, 0}, // 8
			{2, 0, 0, 0}, // 9
			{2, 0, 0, 0}, // 10
			{2, 0, 0, 0}, // 11
			{2, 0, 0, 0}, // 12
			{2, 0, 0, 0}, // 13
			{2, 0, 0, 0}, // 14
			{2, 0, 0, 0}, // 15
			// body (16-31)
			{0, 0, 0, 0}, // 16
			{0, 0, 0, 0}, // 17
			{0, 0, 0, 0}, // 18
			{0, 0, 0, 0}, // 19
			{0, 0, 0, 0}, // 20
			{0, 0, 0, 0}, // 21
			{0, 0, 0, 0}, // 22
			{0, 0, 0, 0}, // 23
			{0, 0, 0, 0}, // 24
			{0, 0, 0, 0}, // 25
			{0, 0, 0, 0}, // 26
			{0, 0, 0, 0}, // 27
			{0, 0, 0, 0}, // 28
			{0, 0, 0, 0}, // 29
			{0, 0, 0, 0}, // 30
			{0, 0, 0, 0}, // 31
			// sword (32-43)
			{3, 0, 0, 0}, // 32
			{3, 0, 0, 0}, // 33
			{3, 0, 0, 0}, // 34
			{3, 0, 0, 0}, // 35
			{3, 0, 0, 0}, // 36
			{3, 0, 0, 0}, // 37
			{3, 0, 0, 0}, // 38
			{3, 0, 0, 0}, // 39
			{3, 0, 0, 0}, // 40
			{3, 0, 0, 0}, // 41
			{3, 0, 0, 0}, // 42
			{3, 0, 0, 0}, // 43
	};

	std::vector<glm::vec4> knight_bone_weights = {
			{1.f, 0.f, 0.f, 0.f}, // 0
			{1.f, 0.f, 0.f, 0.f}, // 1
			{1.f, 0.f, 0.f, 0.f}, // 2
			{1.f, 0.f, 0.f, 0.f}, // 3
			{1.f, 0.f, 0.f, 0.f}, // 4
			{1.f, 0.f, 0.f, 0.f}, // 5
			{1.f, 0.f, 0.f, 0.f}, // 6
			{1.f, 0.f, 0.f, 0.f}, // 7
			{1.f, 0.f, 0.f, 0.f}, // 8
			{1.f, 0.f, 0.f, 0.f}, // 9
			{1.f, 0.f, 0.f, 0.f}, // 10
			{1.f, 0.f, 0.f, 0.f}, // 11
			{1.f, 0.f, 0.f, 0.f}, // 12
			{1.f, 0.f, 0.f, 0.f}, // 13
			{1.f, 0.f, 0.f, 0.f}, // 14
			{1.f, 0.f, 0.f, 0.f}, // 15
			{1.f, 0.f, 0.f, 0.f}, // 16
			{1.f, 0.f, 0.f, 0.f}, // 17
			{1.f, 0.f, 0.f, 0.f}, // 18
			{1.f, 0.f, 0.f, 0.f}, // 19
			{1.f, 0.f, 0.f, 0.f}, // 20
			{1.f, 0.f, 0.f, 0.f}, // 21
			{1.f, 0.f, 0.f, 0.f}, // 22
			{1.f, 0.f, 0.f, 0.f}, // 23
			{1.f, 0.f, 0.f, 0.f}, // 24
			{1.f, 0.f, 0.f, 0.f}, // 25
			{1.f, 0.f, 0.f, 0.f}, // 26
			{1.f, 0.f, 0.f, 0.f}, // 27
			{1.f, 0.f, 0.f, 0.f}, // 28
			{1.f, 0.f, 0.f, 0.f}, // 29
			{1.f, 0.f, 0.f, 0.f}, // 30
			{1.f, 0.f, 0.f, 0.f}, // 31
			{1.f, 0.f, 0.f, 0.f}, // 32
			{1.f, 0.f, 0.f, 0.f}, // 33
			{1.f, 0.f, 0.f, 0.f}, // 34
			{1.f, 0.f, 0.f, 0.f}, // 35
			{1.f, 0.f, 0.f, 0.f}, // 36
			{1.f, 0.f, 0.f, 0.f}, // 37
			{1.f, 0.f, 0.f, 0.f}, // 38
			{1.f, 0.f, 0.f, 0.f}, // 39
			{1.f, 0.f, 0.f, 0.f}, // 40
			{1.f, 0.f, 0.f, 0.f}, // 41
			{1.f, 0.f, 0.f, 0.f}, // 42
			{1.f, 0.f, 0.f, 0.f}, // 43
	};

	textured_meshes[(int)GEOMETRY_BUFFER_ID::KNIGHT].vertices = knight_vertices;
	textured_meshes[(int)GEOMETRY_BUFFER_ID::KNIGHT].vertex_indices = knight_indices;
	bindVBOandIBO(GEOMETRY_BUFFER_ID::KNIGHT, knight_vertices, knight_indices);

	skinned_meshes[(int)GEOMETRY_BUFFER_ID::KNIGHT].bones = knight_bones;
	skinned_meshes[(int)GEOMETRY_BUFFER_ID::KNIGHT].bone_indices = knight_bone_indices;
	skinned_meshes[(int)GEOMETRY_BUFFER_ID::KNIGHT].bone_weights = knight_bone_weights;

	glBindBuffer(GL_ARRAY_BUFFER, bone_weights_vbo[(uint)GEOMETRY_BUFFER_ID::KNIGHT]);
	glBufferData(GL_ARRAY_BUFFER,
							 sizeof(knight_bone_weights[0]) * knight_bone_weights.size(), knight_bone_weights.data(), GL_STATIC_DRAW);
	gl_has_errors();

	glBindBuffer(GL_ARRAY_BUFFER, bone_indices_vbo[(uint)GEOMETRY_BUFFER_ID::KNIGHT]);
	glBufferData(GL_ARRAY_BUFFER,
							 sizeof(knight_bone_indices[0]) * knight_bone_indices.size(), knight_bone_indices.data(), GL_STATIC_DRAW);
	gl_has_errors();

	///////////////////////////
	// Initialize prince
	std::vector<TexturedVertex> prince_vertices = {
			// head (0-7)
			{{0.01f, -0.25f, 1.f}, {0.51f, 0.25f}},	 // 0 center
			{{0.1f, -0.5f, 1.f}, {0.6f, 0.f}},			 // 1 hat top middle
			{{-0.3f, -0.3f, 1.f}, {0.2f, 0.2f}},		 // 2 hat left
			{{0.32f, -0.28f, 1.f}, {0.82f, 0.22f}},	 // 3 hat right
			{{-0.06, -0.48f, 1.f}, {0.44f, 0.02f}},	 // 4 hat top left
			{{0.22f, -0.48f, 1.f}, {0.72f, 0.02f}},	 // 5 hat top right
			{{-0.25f, -0.08f, 1.f}, {0.25f, 0.42f}}, // 6 bottom left
			{{0.28, -0.07f, 1.f}, {0.78f, 0.43f}},	 // 7 bottom right
			// body (8-21)
			{{0.0f, 0.0f, 1.f}, {0.5f, 0.5f}},			 // 8 center
			{{0.01f, -0.25f, 1.f}, {0.51f, 0.25f}},	 // 9 = 0 top of body
			{{-0.25f, -0.08f, 1.f}, {0.25f, 0.42f}}, // 10 = 6 top left of body
			{{0.28, -0.07f, 1.f}, {0.78f, 0.43f}},	 // 11 = 7 top right of body
			{{-0.13f, -0.03f, 1.f}, {0.37f, 0.47f}}, // 12 middle left
			{{0.14f, -0.03f, 1.f}, {0.64f, 0.47f}},	 // 13 middle right
			{{-0.32f, 0.45f, 1.f}, {0.18f, 0.95f}},	 // 14 bottom left
			{{0.35f, 0.44f, 1.f}, {0.85f, 0.94f}},	 // 15 bottom right
			{{-0.37f, 0.49f, 1.f}, {0.13f, 0.99f}},	 // 16 bottom left tip
			{{0.43f, 0.49f, 1.f}, {0.93f, 0.99f}},	 // 17 bottom right tip
			{{0.01f, 0.31f, 1.f}, {0.51f, 0.81f}},	 // 18 lower center
			{{0.01f, 0.5f, 1.f}, {0.51f, 1.f}},			 // 19 bottom center
			{{0.21f, 0.21f, 1.f}, {0.71f, 0.71f}},	 // 20 right, beside hand
			{{0.23f, 0.2f, 1.f}, {0.73f, 0.70f}},		 // 21 right, below hand
			// hand on the right (22-25)
			{{0.21f, 0.21f, 1.f}, {0.71f, 0.71f}},	// 22 = 20 bottom left of hand
			{{0.4f, 0.19f, 1.f}, {0.9f, 0.69f}},		// 23 bottom right
			{{0.14f, -0.03f, 1.f}, {0.64f, 0.47f}}, // 24 = 13 top left
			{{0.23f, -0.05f, 1.f}, {0.73f, 0.45f}}, // 25 top right
			// arm on the left (26-30)
			{{-0.13f, -0.03f, 1.f}, {0.37f, 0.47f}}, // 26 = 12 right
			{{-0.25f, -0.08f, 1.f}, {0.25f, 0.42f}}, // 27 = 6 top right ish
			{{-0.16f, 0.08f, 1.f}, {0.34f, 0.58f}},	 // 28 bottom right ish
			{{-0.34f, 0.12f, 1.f}, {0.16f, 0.62f}},	 // 29 bottom left
			{{-0.34f, -0.07f, 1.f}, {0.16f, 0.43f}}, // 30 top left
			// wand (31-40)
			{{-0.5f, -0.47f, 1.f}, {0.f, 0.03f}},		 // 31 top left of top box
			{{-0.3f, -0.47f, 1.f}, {0.2f, 0.03f}},	 // 32 top right of top box
			{{-0.5f, -0.12f, 1.f}, {0.f, 0.38f}},		 // 33 bottom left of top box
			{{-0.3f, -0.12f, 1.f}, {0.2f, 0.38f}},	 // 34 bottom right of top box
			{{-0.45f, -0.12f, 1.f}, {0.05f, 0.38f}}, // 35 top left of bottom box
			{{-0.34f, -0.12f, 1.f}, {0.16f, 0.38f}}, // 36 top right of bottom box
			{{-0.45f, 0.47f, 1.f}, {0.05f, 0.97f}},	 // 37 bottom left of bottom box
			{{-0.34f, 0.47f, 1.f}, {0.16f, 0.97f}},	 // 38 bottom right of bottom box
			{{-0.25f, -0.08f, 1.f}, {0.25f, 0.42f}}, // 39 = 6 connector to body
			{{-0.3f, -0.3f, 1.f}, {0.2f, 0.2f}},		 // 40 = 2 hat left
	};
	std::vector<uint16_t> prince_indices = {
			// head
			1, 4, 0,
			4, 2, 0,
			5, 1, 0,
			3, 5, 0,
			0, 2, 6,
			7, 3, 0,
			// hand on the right
			23, 24, 22,
			25, 24, 23,
			// arm on the left
			26, 27, 28,
			28, 27, 30,
			28, 30, 29,
			// body
			9, 10, 8,
			11, 9, 8,
			8, 12, 14,
			8, 14, 18,
			18, 14, 19,
			14, 16, 19,
			13, 8, 20,
			20, 8, 18,
			21, 20, 18,
			21, 18, 15,
			15, 18, 19,
			17, 18, 19,
			// wand
			32, 31, 34,
			31, 33, 34,
			34, 39, 40,
			36, 35, 38,
			38, 35, 37
			//
	};

	std::vector<MeshBone> prince_bones = {
			{-1}, // 0 body
			{0},	// 1 head
			{0},	// 2 hand on the right
			{0},	// 3 arm on the left
			{3},	// 4 wand
	};

	std::vector<glm::ivec4> prince_bone_indices = {
			// head (0-7)
			{1, 0, 0, 0}, // 0
			{1, 0, 0, 0}, // 1
			{1, 0, 0, 0}, // 2
			{1, 0, 0, 0}, // 3
			{1, 0, 0, 0}, // 4
			{1, 0, 0, 0}, // 5
			{1, 0, 0, 0}, // 6
			{1, 0, 0, 0}, // 7
			// body (8-21)
			{0, 0, 0, 0}, // 8
			{0, 0, 0, 0}, // 9
			{0, 0, 0, 0}, // 10
			{0, 0, 0, 0}, // 11
			{0, 0, 0, 0}, // 12
			{0, 0, 0, 0}, // 13
			{0, 0, 0, 0}, // 14
			{0, 0, 0, 0}, // 15
			{0, 0, 0, 0}, // 16
			{0, 0, 0, 0}, // 17
			{0, 0, 0, 0}, // 18
			{0, 0, 0, 0}, // 19
			{0, 0, 0, 0}, // 20
			{0, 0, 0, 0}, // 21
			// hand on the right (22-25)
			{2, 0, 0, 0}, // 22
			{2, 0, 0, 0}, // 23
			{2, 0, 0, 0}, // 24
			{2, 0, 0, 0}, // 25
			// arm on the left (26-30)
			{3, 0, 0, 0}, // 26
			{3, 0, 0, 0}, // 27
			{3, 0, 0, 0}, // 28
			{3, 0, 0, 0}, // 29
			{3, 0, 0, 0}, // 30
			// wand (31-40)
			{4, 0, 0, 0}, // 31
			{4, 0, 0, 0}, // 32
			{4, 0, 0, 0}, // 33
			{4, 0, 0, 0}, // 34
			{4, 0, 0, 0}, // 35
			{4, 0, 0, 0}, // 36
			{4, 0, 0, 0}, // 37
			{4, 0, 0, 0}, // 38
			{4, 0, 0, 0}, // 39
			{4, 0, 0, 0}, // 40
	};

	std::vector<glm::vec4> prince_bone_weights = {
			{1.f, 0.f, 0.f, 0.f}, // 0
			{1.f, 0.f, 0.f, 0.f}, // 1
			{1.f, 0.f, 0.f, 0.f}, // 2
			{1.f, 0.f, 0.f, 0.f}, // 3
			{1.f, 0.f, 0.f, 0.f}, // 4
			{1.f, 0.f, 0.f, 0.f}, // 5
			{1.f, 0.f, 0.f, 0.f}, // 6
			{1.f, 0.f, 0.f, 0.f}, // 7
			{1.f, 0.f, 0.f, 0.f}, // 8
			{1.f, 0.f, 0.f, 0.f}, // 9
			{1.f, 0.f, 0.f, 0.f}, // 10
			{1.f, 0.f, 0.f, 0.f}, // 11
			{1.f, 0.f, 0.f, 0.f}, // 12
			{1.f, 0.f, 0.f, 0.f}, // 13
			{1.f, 0.f, 0.f, 0.f}, // 14
			{1.f, 0.f, 0.f, 0.f}, // 15
			{1.f, 0.f, 0.f, 0.f}, // 16
			{1.f, 0.f, 0.f, 0.f}, // 17
			{1.f, 0.f, 0.f, 0.f}, // 18
			{1.f, 0.f, 0.f, 0.f}, // 19
			{1.f, 0.f, 0.f, 0.f}, // 20
			{1.f, 0.f, 0.f, 0.f}, // 21
			{1.f, 0.f, 0.f, 0.f}, // 22
			{1.f, 0.f, 0.f, 0.f}, // 23
			{1.f, 0.f, 0.f, 0.f}, // 24
			{1.f, 0.f, 0.f, 0.f}, // 25
			{1.f, 0.f, 0.f, 0.f}, // 26
			{1.f, 0.f, 0.f, 0.f}, // 27
			{1.f, 0.f, 0.f, 0.f}, // 28
			{1.f, 0.f, 0.f, 0.f}, // 29
			{1.f, 0.f, 0.f, 0.f}, // 30
			{1.f, 0.f, 0.f, 0.f}, // 31
			{1.f, 0.f, 0.f, 0.f}, // 32
			{1.f, 0.f, 0.f, 0.f}, // 33
			{1.f, 0.f, 0.f, 0.f}, // 34
			{1.f, 0.f, 0.f, 0.f}, // 35
			{1.f, 0.f, 0.f, 0.f}, // 36
			{1.f, 0.f, 0.f, 0.f}, // 37
			{1.f, 0.f, 0.f, 0.f}, // 38
			{1.f, 0.f, 0.f, 0.f}, // 39
			{1.f, 0.f, 0.f, 0.f}, // 40
	};

	textured_meshes[(int)GEOMETRY_BUFFER_ID::PRINCE].vertices = prince_vertices;
	textured_meshes[(int)GEOMETRY_BUFFER_ID::PRINCE].vertex_indices = prince_indices;
	bindVBOandIBO(GEOMETRY_BUFFER_ID::PRINCE, prince_vertices, prince_indices);

	skinned_meshes[(int)GEOMETRY_BUFFER_ID::PRINCE].bones = prince_bones;
	skinned_meshes[(int)GEOMETRY_BUFFER_ID::PRINCE].bone_indices = prince_bone_indices;
	skinned_meshes[(int)GEOMETRY_BUFFER_ID::PRINCE].bone_weights = prince_bone_weights;

	glBindBuffer(GL_ARRAY_BUFFER, bone_weights_vbo[(uint)GEOMETRY_BUFFER_ID::PRINCE]);
	glBufferData(GL_ARRAY_BUFFER,
							 sizeof(prince_bone_weights[0]) * prince_bone_weights.size(), prince_bone_weights.data(), GL_STATIC_DRAW);
	gl_has_errors();

	glBindBuffer(GL_ARRAY_BUFFER, bone_indices_vbo[(uint)GEOMETRY_BUFFER_ID::PRINCE]);
	glBufferData(GL_ARRAY_BUFFER,
							 sizeof(prince_bone_indices[0]) * prince_bone_indices.size(), prince_bone_indices.data(), GL_STATIC_DRAW);
	gl_has_errors();

	///////////////////////////
	// Initialize king
	std::vector<TexturedVertex> king_vertices = {
			// head (0-8)
			{{0.07f, -0.18f, 1.f}, {0.57f, 0.32f}},	 // 0 center
			{{-0.28f, -0.48f, 1.f}, {0.22f, 0.02f}}, // 1 top left
			{{0.07f, -0.5f, 1.f}, {0.57f, 0.f}},		 // 2 top middle
			{{0.40f, -0.48f, 1.f}, {0.9f, 0.02f}},	 // 3 top right
			{{-0.2f, -0.23f, 1.f}, {0.3f, 0.27f}},	 // 4 middle left
			{{-0.29f, -0.08f, 1.f}, {0.21f, 0.42f}}, // 5 bottom left
			{{0.34f, -0.23f, 1.f}, {0.84f, 0.27f}},	 // 6 middle right
			{{0.44f, -0.08f, 1.f}, {0.94f, 0.42f}},	 // 7 bottom right
			{{0.07f, 0.05f, 1.f}, {0.57f, 0.55f}},	 // 8 beard center (bottom)
			// body (9-21)
			{{0.07f, 0.25f, 1.f}, {0.57f, 0.75f}},	 // 9 center
			{{-0.16f, 0.10f, 1.f}, {0.34, 0.60f}},	 // 10 beard bottom left
			{{0.27, 0.14f, 1.f}, {0.77f, 0.64f}},		 // 11 beard bottom right
			{{0.07f, 0.05f, 1.f}, {0.57f, 0.55f}},	 // 12 = 8 beard center
			{{-0.29f, -0.08f, 1.f}, {0.21f, 0.42f}}, // 13 = 5 beard middle left
			{{0.44f, -0.08f, 1.f}, {0.94f, 0.42f}},	 // 14 = 7 beard middle right
			{{-0.31f, 0.37f, 1.f}, {0.19f, 0.87f}},	 // 15 clothes bottom left
			{{0.46f, 0.38f, 1.f}, {0.96f, 0.88f}},	 // 16 clothes bottom right
			{{0.19f, 0.21f, 1.f}, {0.69f, 0.71f}},	 // 17 hand left
			{{0.22f, 0.26f, 1.f}, {0.72f, 0.76f}},	 // 18 hand left bottom
			{{0.40f, 0.30f, 1.f}, {0.90f, 0.80f}},	 // 19 hand bottom
			{{0.07f, 0.46f, 1.f}, {0.57f, 0.96f}},	 // 20 clothes bottom
			{{0.23f, 0.2f, 1.f}, {0.73f, 0.70f}},		 // 21 (unused)
			// hand on the right (22-28)
			{{0.44f, -0.08f, 1.f}, {0.94f, 0.42f}}, // 22 = 7 bottom right of face
			{{0.27, 0.14f, 1.f}, {0.77f, 0.64f}},		// 23 = 11 beard bottom right
			{{0.50f, 0.12f, 1.f}, {1.f, 0.62f}},		// 24 right middle
			{{0.50f, 0.21f, 1.f}, {1.f, 0.71f}},		// 25 right bottom
			{{0.19f, 0.21f, 1.f}, {0.69f, 0.71f}},	// 26 = 17 hand left
			{{0.22f, 0.26f, 1.f}, {0.72f, 0.76f}},	// 27 = 18 hand left bottom
			{{0.40f, 0.30f, 1.f}, {0.90f, 0.80f}},	// 28 = 19 hand bottom
			// arm and staff (29-37)
			{{-0.5f, -0.35f, 1.f}, {0.f, 0.15f}},		 // 29 top left
			{{-0.24f, -0.35f, 1.f}, {0.26f, 0.15f}}, // 30 top right
			{{-0.29f, -0.08f, 1.f}, {0.21f, 0.42f}}, // 31 = 5 beard middle left
			{{-0.16f, 0.10f, 1.f}, {0.34, 0.60f}},	 // 32 = 10 arm right
			{{-0.5f, -0.02f, 1.f}, {0.f, 0.52f}},		 // 33 left middle
			{{-0.5f, 0.47f, 1.f}, {0.f, 0.97f}},		 // 34 bottom left
			{{-0.29f, 0.47f, 1.f}, {0.21f, 0.97f}},	 // 35 bottom right
			{{-0.31f, 0.37f, 1.f}, {0.19f, 0.87f}},	 // 36 = 15 clothes bottom left
			{{-0.2f, -0.23f, 1.f}, {0.3f, 0.27f}},	 // 37 = 4 middle left of head
			// feet (38-42)
			{{-0.31f, 0.37f, 1.f}, {0.19f, 0.87f}}, // 38 = 15 clothes bottom left
			{{0.46f, 0.38f, 1.f}, {0.96f, 0.88f}},	// 39 = 16 clothes bottom right
			{{-0.27f, 0.48f, 1.f}, {0.23f, 0.98f}}, // 40 feet bottom left
			{{0.41f, 0.48f, 1.f}, {0.91f, 0.98f}},	// 41 feet bottom right
			{{0.07f, 0.46f, 1.f}, {0.57f, 0.96f}},	// 42 = 20 clothes bottom
	};
	std::vector<uint16_t> king_indices = {
			// head
			2, 1, 0,
			1, 4, 0,
			4, 5, 0,
			3, 2, 0,
			6, 3, 0,
			7, 6, 0,
			5, 8, 0,
			8, 7, 0,
			// arm and staff
			30, 29, 37,
			29, 31, 37,
			29, 33, 31,
			31, 33, 32,
			32, 33, 36,
			33, 34, 36,
			36, 34, 35,
			// hand on the right
			22, 23, 24,
			24, 23, 25,
			25, 23, 26,
			25, 26, 27,
			25, 27, 28,
			// feet
			38, 40, 42,
			39, 42, 41,
			42, 40, 41,
			// body
			12, 10, 13,
			12, 14, 11,
			12, 10, 9,
			11, 12, 9,
			10, 15, 9,
			15, 20, 9,
			9, 17, 11,
			9, 18, 17,
			9, 19, 18,
			9, 16, 19,
			9, 20, 16
			//
	};

	std::vector<MeshBone> king_bones = {
			{-1}, // 0 body
			{0},	// 1 head
			{0},	// 2 hand on the right
			{0},	// 3 arm and staff
			{0},	// 4 feet
	};

	std::vector<glm::ivec4> king_bone_indices = {
			// head (0-8)
			{1, 0, 0, 0}, // 0
			{1, 0, 0, 0}, // 1
			{1, 0, 0, 0}, // 2
			{1, 0, 0, 0}, // 3
			{1, 0, 0, 0}, // 4
			{1, 0, 0, 0}, // 5
			{1, 0, 0, 0}, // 6
			{1, 0, 0, 0}, // 7
			{1, 0, 0, 0}, // 8
			// body (9-21)
			{0, 0, 0, 0}, // 9
			{0, 0, 0, 0}, // 10
			{0, 0, 0, 0}, // 11
			{0, 0, 0, 0}, // 12
			{0, 0, 0, 0}, // 13
			{0, 0, 0, 0}, // 14
			{0, 0, 0, 0}, // 15
			{0, 0, 0, 0}, // 16
			{0, 0, 0, 0}, // 17
			{0, 0, 0, 0}, // 18
			{0, 0, 0, 0}, // 19
			{0, 0, 0, 0}, // 20
			{0, 0, 0, 0}, // 21
			// hand on the right (22-28)
			{2, 0, 0, 0}, // 22
			{2, 0, 0, 0}, // 23
			{2, 0, 0, 0}, // 24
			{2, 0, 0, 0}, // 25
			{2, 0, 0, 0}, // 26
			{2, 0, 0, 0}, // 27
			{2, 0, 0, 0}, // 28
			// arm and staff (29-37)
			{3, 0, 0, 0}, // 29
			{3, 0, 0, 0}, // 30
			{3, 0, 0, 0}, // 31
			{3, 0, 0, 0}, // 32
			{3, 0, 0, 0}, // 33
			{3, 0, 0, 0}, // 34
			{3, 0, 0, 0}, // 35
			{3, 0, 0, 0}, // 36
			{3, 0, 0, 0}, // 37
			// feet (38-42)
			{4, 0, 0, 0}, // 38
			{4, 0, 0, 0}, // 39
			{4, 0, 0, 0}, // 40
			{4, 0, 0, 0}, // 41
			{4, 0, 0, 0}, // 42
	};

	std::vector<glm::vec4> king_bone_weights = {
			{1.f, 0.f, 0.f, 0.f}, // 0
			{1.f, 0.f, 0.f, 0.f}, // 1
			{1.f, 0.f, 0.f, 0.f}, // 2
			{1.f, 0.f, 0.f, 0.f}, // 3
			{1.f, 0.f, 0.f, 0.f}, // 4
			{1.f, 0.f, 0.f, 0.f}, // 5
			{1.f, 0.f, 0.f, 0.f}, // 6
			{1.f, 0.f, 0.f, 0.f}, // 7
			{1.f, 0.f, 0.f, 0.f}, // 8
			{1.f, 0.f, 0.f, 0.f}, // 9
			{1.f, 0.f, 0.f, 0.f}, // 10
			{1.f, 0.f, 0.f, 0.f}, // 11
			{1.f, 0.f, 0.f, 0.f}, // 12
			{1.f, 0.f, 0.f, 0.f}, // 13
			{1.f, 0.f, 0.f, 0.f}, // 14
			{1.f, 0.f, 0.f, 0.f}, // 15
			{1.f, 0.f, 0.f, 0.f}, // 16
			{1.f, 0.f, 0.f, 0.f}, // 17
			{1.f, 0.f, 0.f, 0.f}, // 18
			{1.f, 0.f, 0.f, 0.f}, // 19
			{1.f, 0.f, 0.f, 0.f}, // 20
			{1.f, 0.f, 0.f, 0.f}, // 21
			{1.f, 0.f, 0.f, 0.f}, // 22
			{1.f, 0.f, 0.f, 0.f}, // 23
			{1.f, 0.f, 0.f, 0.f}, // 24
			{1.f, 0.f, 0.f, 0.f}, // 25
			{1.f, 0.f, 0.f, 0.f}, // 26
			{1.f, 0.f, 0.f, 0.f}, // 27
			{1.f, 0.f, 0.f, 0.f}, // 28
			{1.f, 0.f, 0.f, 0.f}, // 29
			{1.f, 0.f, 0.f, 0.f}, // 30
			{1.f, 0.f, 0.f, 0.f}, // 31
			{1.f, 0.f, 0.f, 0.f}, // 32
			{1.f, 0.f, 0.f, 0.f}, // 33
			{1.f, 0.f, 0.f, 0.f}, // 34
			{1.f, 0.f, 0.f, 0.f}, // 35
			{1.f, 0.f, 0.f, 0.f}, // 36
			{1.f, 0.f, 0.f, 0.f}, // 37
			{1.f, 0.f, 0.f, 0.f}, // 38
			{1.f, 0.f, 0.f, 0.f}, // 39
			{1.f, 0.f, 0.f, 0.f}, // 40
			{1.f, 0.f, 0.f, 0.f}, // 41
			{1.f, 0.f, 0.f, 0.f}, // 42
	};

	textured_meshes[(int)GEOMETRY_BUFFER_ID::KING].vertices = king_vertices;
	textured_meshes[(int)GEOMETRY_BUFFER_ID::KING].vertex_indices = king_indices;
	bindVBOandIBO(GEOMETRY_BUFFER_ID::KING, king_vertices, king_indices);

	skinned_meshes[(int)GEOMETRY_BUFFER_ID::KING].bones = king_bones;
	skinned_meshes[(int)GEOMETRY_BUFFER_ID::KING].bone_indices = king_bone_indices;
	skinned_meshes[(int)GEOMETRY_BUFFER_ID::KING].bone_weights = king_bone_weights;

	glBindBuffer(GL_ARRAY_BUFFER, bone_weights_vbo[(uint)GEOMETRY_BUFFER_ID::KING]);
	glBufferData(GL_ARRAY_BUFFER,
							 sizeof(king_bone_weights[0]) * king_bone_weights.size(), king_bone_weights.data(), GL_STATIC_DRAW);
	gl_has_errors();

	glBindBuffer(GL_ARRAY_BUFFER, bone_indices_vbo[(uint)GEOMETRY_BUFFER_ID::KING]);
	glBufferData(GL_ARRAY_BUFFER,
							 sizeof(king_bone_indices[0]) * king_bone_indices.size(), king_bone_indices.data(), GL_STATIC_DRAW);
	gl_has_errors();

	///////////////////////////////////////////////////////
	// Initialize screen triangle (yes, triangle, not quad; its more efficient).
	std::vector<vec3> screen_vertices(3);
	screen_vertices[0] = {-1, -6, 0.f};
	screen_vertices[1] = {6, -1, 0.f};
	screen_vertices[2] = {-1, 6, 0.f};

	// Counterclockwise as it's the default opengl front winding direction.
	const std::vector<uint16_t> screen_indices = {0, 1, 2};
	bindVBOandIBO(GEOMETRY_BUFFER_ID::SCREEN_TRIANGLE, screen_vertices, screen_indices);
}

RenderSystem::~RenderSystem()
{
	// Don't need to free gl resources since they last for as long as the program,
	// but it's polite to clean after yourself.
	glDeleteBuffers((GLsizei)vertex_buffers.size(), vertex_buffers.data());
	glDeleteBuffers((GLsizei)index_buffers.size(), index_buffers.data());
	glDeleteBuffers((GLsizei)bone_weights_vbo.size(), bone_weights_vbo.data());
	glDeleteBuffers((GLsizei)bone_indices_vbo.size(), bone_indices_vbo.data());
	glDeleteTextures((GLsizei)texture_gl_handles.size(), texture_gl_handles.data());
	glDeleteTextures(1, &off_screen_render_buffer_color);
	glDeleteRenderbuffers(1, &off_screen_render_buffer_depth);
	gl_has_errors();

	for (uint i = 0; i < effect_count; i++)
	{
		glDeleteProgram(effects[i]);
	}
	// delete allocated resources
	glDeleteFramebuffers(1, &frame_buffer);
	gl_has_errors();

	glDeleteVertexArrays(1, &textVAO);
	glDeleteBuffers(1, &textVBO);

	// Delete character textures
	for (auto &pair : characters)
	{
		glDeleteTextures(1, &pair.second.TextureID);
	}

	// remove all entities created by the render system
	while (registry.renderRequests.entities.size() > 0)
		registry.remove_all_components_of(registry.renderRequests.entities.back());
}

// Initialize the screen texture from a standard sprite
bool RenderSystem::initScreenTexture()
{
	registry.screenStates.emplace(screen_state_entity);

	int framebuffer_width, framebuffer_height;
	glfwGetFramebufferSize(const_cast<GLFWwindow *>(window), &framebuffer_width, &framebuffer_height); // Note, this will be 2x the resolution given to glfwCreateWindow on retina displays

	glGenTextures(1, &off_screen_render_buffer_color);
	glBindTexture(GL_TEXTURE_2D, off_screen_render_buffer_color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_width, framebuffer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl_has_errors();

	glGenRenderbuffers(1, &off_screen_render_buffer_depth);
	glBindRenderbuffer(GL_RENDERBUFFER, off_screen_render_buffer_depth);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, off_screen_render_buffer_color, 0);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, framebuffer_width, framebuffer_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, off_screen_render_buffer_depth);
	gl_has_errors();

	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	return true;
}

bool gl_compile_shader(GLuint shader)
{
	glCompileShader(shader);
	gl_has_errors();
	GLint success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE)
	{
		GLint log_len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
		std::vector<char> log(log_len);
		glGetShaderInfoLog(shader, log_len, &log_len, log.data());
		glDeleteShader(shader);

		gl_has_errors();

		fprintf(stderr, "GLSL: %s", log.data());
		return false;
	}

	return true;
}

bool loadEffectFromFile(
		const std::string &vs_path, const std::string &fs_path, GLuint &out_program)
{
	// Opening files
	std::ifstream vs_is(vs_path);
	std::ifstream fs_is(fs_path);
	if (!vs_is.good() || !fs_is.good())
	{
		fprintf(stderr, "Failed to load shader files %s, %s", vs_path.c_str(), fs_path.c_str());
		assert(false);
		return false;
	}

	// Reading sources
	std::stringstream vs_ss, fs_ss;
	vs_ss << vs_is.rdbuf();
	fs_ss << fs_is.rdbuf();
	std::string vs_str = vs_ss.str();
	std::string fs_str = fs_ss.str();
	const char *vs_src = vs_str.c_str();
	const char *fs_src = fs_str.c_str();
	GLsizei vs_len = (GLsizei)vs_str.size();
	GLsizei fs_len = (GLsizei)fs_str.size();

	GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vs_src, &vs_len);
	GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fs_src, &fs_len);
	gl_has_errors();

	// Compiling
	if (!gl_compile_shader(vertex))
	{
		fprintf(stderr, "Vertex compilation failed");
		assert(false);
		return false;
	}
	if (!gl_compile_shader(fragment))
	{
		fprintf(stderr, "Vertex compilation failed");
		assert(false);
		return false;
	}

	// Linking
	out_program = glCreateProgram();
	glAttachShader(out_program, vertex);
	glAttachShader(out_program, fragment);
	glLinkProgram(out_program);
	gl_has_errors();

	{
		GLint is_linked = GL_FALSE;
		glGetProgramiv(out_program, GL_LINK_STATUS, &is_linked);
		if (is_linked == GL_FALSE)
		{
			GLint log_len;
			glGetProgramiv(out_program, GL_INFO_LOG_LENGTH, &log_len);
			std::vector<char> log(log_len);
			glGetProgramInfoLog(out_program, log_len, &log_len, log.data());
			gl_has_errors();

			fprintf(stderr, "Link error: %s", log.data());
			assert(false);
			return false;
		}
	}

	// No need to carry this around. Keeping these objects is only useful if we recycle
	// the same shaders over and over, which we don't, so no need and this is simpler.
	glDetachShader(out_program, vertex);
	glDetachShader(out_program, fragment);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	gl_has_errors();
	return true;
}
