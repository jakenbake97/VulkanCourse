#version 450

layout(location = 0) in vec3 fragColor; // Interpolated color from vertex (location must match vertex output)

layout(location = 0) out vec4 outputColor; // Final output color (must also have location)

void main()
{
	outputColor = vec4(fragColor, 1.0);
}