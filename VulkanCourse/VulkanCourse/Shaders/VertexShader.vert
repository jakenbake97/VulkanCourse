#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 col;

layout (binding = 0) uniform MVP 
{
	mat4x4 projection;
	mat4x4 view;
	mat4x4 model;
}mvp;

layout (location = 0) out vec3 fragcol;

void main()
{
	gl_Position = mvp.projection * mvp.view * mvp.model * vec4(pos, 1.0);

	fragcol = col;
}