#version 330 core

in vec2 TexCoord;  // The texture coordinates for the fragment

out vec4 FragColor;  // The output color of the fragment

uniform sampler2D flowTexture;  // The texture (for the circle boundary or texture)
uniform float flowValue;        // The current flow level, from 0.0 (empty) to 1.0 (full)
uniform vec3 liquidColor;       // The color of the liquid
uniform vec4 outlineColor;      // The color of the outline

// Circle parameters
const vec2 circleCenter = vec2(0.5, 0.5);  // The center of the circle (normalized)
const float radius = 0.5;                  // The radius of the circle (normalized)
const float outlineThickness = 0.03;       // Thickness of the outline

void main()
{
    // Calculate the distance of the fragment from the center of the circle
    float distFromCenter = distance(TexCoord, circleCenter);

    // Render the outline (pixels between radius and radius - outlineThickness)
    if (distFromCenter > radius - outlineThickness && distFromCenter <= radius) {
        FragColor = outlineColor;  // Render the outline color
        return;
    }

    // Check if the fragment is inside the circle
    if (distFromCenter > radius) {
        discard;  // Discard pixels outside the circle
    }

    // Fill the circle from the bottom up based on flowValue
    if (TexCoord.y < (1.0 - flowValue)) {
        discard;  // Discard pixels above the flow level
    }

    // Sample the texture (if you have a texture for the flow meter background)
    vec4 textureColor = texture(flowTexture, TexCoord);

    // Blend the liquid color with the texture (or just use the liquid color)
    FragColor = vec4(liquidColor, 1.0) * textureColor;
}
