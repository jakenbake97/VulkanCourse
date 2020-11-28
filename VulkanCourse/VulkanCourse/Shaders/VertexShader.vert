#version 450

layout(location = 0) out vec3 fragColor; // output color for vertex (location is required)

// Triangle vertex positions
vec3 positions[3] = vec3[] (
	vec3( 0.0, -0.5, 0.0),
	vec3( 0.5,  0.5, 0.0),
	vec3(-0.5,  0.5, 0.0)
);

// Triangle vertex colors
vec3 colors[3] = vec3[] (
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);

void main()
{
	gl_Position = vec4(positions[gl_VertexIndex], 1.0);
	fragColor = colors[gl_VertexIndex];
}