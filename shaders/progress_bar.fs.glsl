#version 330

// Application data
uniform vec3 fcolor;
uniform float opacity;

// Output color
layout(location = 0) out vec4 out_color;

void main()
{
	out_color = vec4(fcolor, opacity);
}
