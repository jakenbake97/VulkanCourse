#version 450

layout(location = 0) in vec3 fragCol;
layout(location = 0) out vec4 outputColor; // Final output color (must also have location)

void main()
{
	outputColor = vec4(fragCol, 1.0);
}