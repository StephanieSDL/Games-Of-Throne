#include "common.hpp"
#include <iostream>

// Note, we could also use the functions from GLM but we write the transformations here to show the uderlying math
void Transform::scale(vec2 scale)
{
	mat3 S = {{scale.x, 0.f, 0.f}, {0.f, scale.y, 0.f}, {0.f, 0.f, 1.f}};
	mat = mat * S;
}

void Transform::rotate(float radians)
{
	float c = cosf(radians);
	float s = sinf(radians);
	mat3 R = {{c, s, 0.f}, {-s, c, 0.f}, {0.f, 0.f, 1.f}};
	mat = mat * R;
}

void Transform::translate(vec2 offset)
{
	mat3 T = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {offset.x, offset.y, 1.f}};
	mat = mat * T;
}

bool gl_has_errors()
{
	GLenum error = glGetError();

	if (error == GL_NO_ERROR)
		return false;

	while (error != GL_NO_ERROR)
	{
		const char *error_str = "";
		switch (error)
		{
		case GL_INVALID_OPERATION:
			error_str = "INVALID_OPERATION";
			break;
		case GL_INVALID_ENUM:
			error_str = "INVALID_ENUM";
			break;
		case GL_INVALID_VALUE:
			error_str = "INVALID_VALUE";
			break;
		case GL_OUT_OF_MEMORY:
			error_str = "OUT_OF_MEMORY";
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			error_str = "INVALID_FRAMEBUFFER_OPERATION";
			break;
		}

		std::cerr << "OpenGL: " << error_str << std::endl;
		error = glGetError();
		assert(false);
	}

	return true;
}

glm::vec2 bezierInterpolation(float t, glm::vec2 P1, glm::vec2 P2, glm::vec2 P3)
{
	return (1 - t) * (1 - t) * P1 + 2 * (1 - t) * t * P2 + t * t * P3;
}

// helper to find mid-pint for bezier interp t-> 0.5 -> smoothing factor
glm::vec2 calculateControlPoint(glm::vec2 P1, glm::vec2 P3, glm::vec2 P, float t)
{
	return (P - (1.0f - t) * (1.0f - t) * P1 - (t * t * P3)) / (2.0f * (1.0f - t) * t);
}
