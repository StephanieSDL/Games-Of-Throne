#version 330 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;

out vec2 TexCoord;

uniform mat3 transform;
uniform mat3 projection;

void main()
{
    vec3 world_pos = transform * vec3(in_position.xy, 1.0);
    vec3 proj_pos = projection * world_pos;
    gl_Position = vec4(proj_pos.xy, 0.0, 1.0);

    TexCoord = in_texcoord;
}
