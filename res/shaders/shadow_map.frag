#version 450
#extension GL_ARB_separate_shader_objects : enable

// outputs
layout(location = 0) out vec4 outColor;


void main()
{
	float depthValue = gl_FragCoord.z;

	outColor = vec4(depthValue, depthValue, depthValue, 1.0f);
}