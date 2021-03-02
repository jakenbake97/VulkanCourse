#version 450

layout(location = 0) in vec3 fragCol;
layout(location = 1) in vec2 fragTex;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;

layout(location = 0) out vec4 outputColor; // Final output color (must also have location)

void main()
{
	outputColor = texture(textureSampler, fragTex);
}