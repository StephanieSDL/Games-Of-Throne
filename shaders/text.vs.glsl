#version 330 core
/* simpleGL freetype font vertex shader */
layout (location = 0) in vec4 vertex;	// vec4 = vec2 pos (xy) + vec2 tex (zw)
out vec2 TexCoords;

uniform mat4 projection;
uniform mat3 transform; // TODO: may not be needed

void main()
{
	gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
	// vec3 pos = projection * transform * vec3(vertex.xy, 1.0);
	// gl_Position = vec4(pos.xy, 0.0, 1.0);
	TexCoords = vertex.zw;
}