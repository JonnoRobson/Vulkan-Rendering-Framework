#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;


// textures
layout(binding = 1) uniform sampler mapSampler;
layout(binding = 2) uniform texture2D diffuseMap;

// outputs
layout(location = 0) out vec4 outColor;

void main()
{
	vec4 color = texture(sampler2D(diffuseMap, mapSampler), fragTexCoord);
	outColor = color;
}