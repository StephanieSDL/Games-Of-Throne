#version 330

// Input attributes
in vec3 in_position;

// Application data (no projection since it's UI element)
uniform mat3 transform;
uniform mat3 projection;
uniform mat3 view;

void main()
{
  vec3 pos = projection * view * transform * vec3(in_position.xy, 1.0);
	gl_Position = vec4(pos.xy, in_position.z, 1.0);
}
