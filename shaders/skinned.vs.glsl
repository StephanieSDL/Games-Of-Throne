#version 330

#define MAX_BONES_PER_VERTEX 4
#define MAX_BONES 100

// Input attributes
in vec3 in_position;
in vec2 in_texcoord;
in ivec4 in_bone_indices;
in vec4 in_bone_weights;

// Passed to fragment shader
out vec2 texcoord;

// Application data
uniform mat3 transform;
uniform mat3 projection;
uniform mat3 view;
uniform mat3 bone_matrices[MAX_BONES];

void main()
{
	texcoord = in_texcoord;

	vec3 pos = vec3(0.0);
	vec3 original_pos = vec3(in_position.xy, 1.0);
	for (int i = 0; i < MAX_BONES_PER_VERTEX; i++) {
		int bone_index = in_bone_indices[i];
		float weight = in_bone_weights[i];
		if (weight == 0.0) {
			continue;
		}
		pos += weight * (bone_matrices[bone_index] * original_pos);
	}

	vec3 final_pos = projection * view * transform * pos;
	gl_Position = vec4(final_pos.xy, in_position.z, 1.0);
}
