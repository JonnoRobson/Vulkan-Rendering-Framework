#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec4 worldPosition;
layout(location = 3) flat in uint matIndex;

// uniform buffers
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

struct MaterialData
{
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	vec4 transmittance;
	vec4 emissive;
	float shininess;
	float ior;
	float dissolve;
	float illum;
};

layout(binding = 3) buffer MaterialUberBuffer
{
	MaterialData mat_data[];
} material_buffer;

// textures
//layout(binding = 4) uniform sampler2D ambientSampler;
layout(binding = 5) uniform sampler2D diffuseSampler;
//layout(binding = 6) uniform sampler2D specularSampler;
//layout(binding = 7) uniform sampler2D specularHighlightSampler;
//layout(binding = 8) uniform sampler2D emissiveSampler;
//layout(binding = 9) uniform sampler2D bumpSampler;
//layout(binding = 10) uniform sampler2D alphaSampler;
//layout(binding = 11) uniform sampler2D reflectiveSampler;

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
	vec4 diffuse = material_buffer.mat_data[matIndex].diffuse * texture(diffuseSampler, fragTexCoord);
	vec4 color = CalculateLighting(worldPosition, worldNormal, light_data.lightPosition, light_data.lightDirection, light_data.lightColor, light_data.lightRange, light_data.lightIntensity, light_data.lightType, light_data.shadowsEnabled);
	color =  diffuse;

	outColor = color;
}