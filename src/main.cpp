
#define GL3W_IMPLEMENTATION
#include <gl3w.h>
// stlib
#include <chrono>
#include <iostream>

// internal
#include "physics_system.hpp"
#include "render_system.hpp"
#include "world_system.hpp"
#include "ai_system.hpp"

using Clock = std::chrono::high_resolution_clock;

// Entry point
int main()
{
	// Global systems
	WorldSystem world;
	RenderSystem renderer;
	PhysicsSystem physics;
	AISystem ai;

	// Initializing window
	GLFWwindow *window = world.create_window();
	if (!window)
	{
		// Time to read the error message
		printf("Press any key to exit");
		getchar();
		return EXIT_FAILURE;
	}

	// initialize the main systems
	renderer.init(window);
	world.init(&renderer);
	ai.init(&renderer);

	// variable timestep loop
	auto t = Clock::now();
	while (!world.is_over())
	{
		// Processes system messages, if this wasn't present the window would become unresponsive
		glfwPollEvents();

		// Calculating elapsed times in milliseconds from the previous iteration
		auto now = Clock::now();
		float elapsed_ms =
				(float)(std::chrono::duration_cast<std::chrono::microseconds>(now - t)).count() / 1000;
		t = now;

		if (!world.is_paused)
		{
			world.step(elapsed_ms);
			ai.step(elapsed_ms, world.levelMap);
			physics.step(elapsed_ms);
			world.handle_collisions();
		}

		renderer.draw();
	}

	return EXIT_SUCCESS;
}
