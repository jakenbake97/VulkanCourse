#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputColor; // Color output from subpass 1
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inputDepth; // depth output from subpass 1

layout (location = 0) out vec4 color;

void main()
{
	int xHalf = 1280 / 2;
	if (gl_FragCoord.x > xHalf)
	{
		float lowerBound = 0.98;
		float upperBound = 1;
		float depth = subpassLoad(inputDepth).r;
		float depthColorScaled = 1.0 - ((depth - lowerBound) / (upperBound - lowerBound));

		color = vec4(subpassLoad(inputColor).rgb * depthColorScaled, 1.0f);
	}
	else
	{
		color = subpassLoad(inputColor).rgba;
	}
}