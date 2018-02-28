#version 450
#extension GL_ARB_separate_shader_objects : enable

// outputs
layout(location = 0) out float minDepthBuffer;
layout(location = 1) out float maxDepthBuffer;

void main()
{
	minDepthBuffer = gl_FragCoord.z;
	maxDepthBuffer = gl_FragCoord.z;
}