#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 worldNormal;
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

float CalculateAttenuation(vec3 lightVector, vec4 lightDirection, float dist, float lightRange, float lightType)
{
	float attenuation = 1.0f;

	if(lightType == 1.0f || lightType == 2.0f)
	{
		attenuation = max(0, 1.0f - (dist / lightRange));
	}

	if(lightType == 2.0f)
	{
		// add in spotlight attenuation factor
		vec3 lightVector2 = lightDirection.xyz;
		float rho = dot(lightVector, lightVector2);
		attenuation *= pow(rho, 8);
	}

	return attenuation;
}

vec4 CalculateLighting(vec4 worldPosition, vec3 worldNormal, vec4 lightPosition, vec4 lightDirection, vec4 lightColor, float lightRange, float lightIntensity, float lightType, float shadowsEnabled)
{
	vec3 rayDir = vec3(0.0f, 0.0f, 0.0f);
	float dist = 0.0f;

	// calculate incoming light direction
	if(lightType == 1.0f || lightType == 2.0f)
	{
		rayDir = lightPosition.xyz - worldPosition.xyz;
		dist = length(rayDir);
		rayDir /= dist;
		rayDir = normalize(rayDir);
	}
	else if (lightType == 0.0f)
	{
		rayDir = -lightDirection.xyz;
	}

	float lightPower = clamp(dot(worldNormal, rayDir), 0.0f, 1.0f);

	float attenuation = CalculateAttenuation(rayDir, lightDirection, dist, lightRange, lightType);

	vec3 color = lightColor.xyz * lightPower * attenuation;

	return vec4(color, 1.0f);
}

void main()
{
	vec4 color = CalculateLighting(worldPosition, worldNormal, light_data.lightPosition, light_data.lightDirection, light_data.lightColor, light_data.lightRange, light_data.lightIntensity, light_data.lightType, light_data.shadowsEnabled);
	color = color + texture(texSampler, fragTexCoord);

	outColor = color;	
}