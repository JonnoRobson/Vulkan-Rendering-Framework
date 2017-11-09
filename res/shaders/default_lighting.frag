#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 worldPosition;

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 2) uniform LightingBufferObject
{
	vec4 lightPosition;
	vec4 lightDirection;
	vec4 lightColor;
	float lightRange;
	float lightIntensity;
	float lightType;
	float shadowsEnabled;
} light_data;

// outputs
layout(location = 0) out vec4 outColor;

float CalculateAttenuation()
{
}

vec4 CalculateLighting()
{
}

void main()
{
	outColor = texture(texSampler, fragTexCoord);	
}