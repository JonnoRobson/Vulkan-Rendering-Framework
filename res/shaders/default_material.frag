#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec4 worldPosition;
layout(location = 3) in vec3 eyeVec;
layout(location = 4) flat in uint matIndex;

struct LightData
{
	vec4 lightPosition;
	vec4 lightDirection;
	vec4 lightColor;
	float lightRange;
	float lightIntensity;
	float lightType;
	float shadowsEnabled;
};

// uniform buffers
layout(binding = 2) buffer LightingBuffer
{
	vec4 scene_data;
	LightData lights[];
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
	uint ambient_map_index;
	uint diffuse_map_index;
	uint specular_map_index;
	uint specular_highlight_map_index;
	uint emissive_map_index;
	uint bump_map_index;
	uint alpha_map_index;
	uint reflection_map_index;
};

layout(binding = 3) uniform MaterialUberBuffer
{
	MaterialData materials[512];
} material_data;

// textures
layout(binding = 4) uniform sampler mapSampler;
layout(binding = 5) uniform texture2D diffuseMaps[512];
layout(binding = 6) uniform texture2D normalMaps[512];

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

mat3 CotangentFrame(vec3 normal, vec3 view, vec2 uv)
{
	// get edge vectors of the pixel triangle
    vec3 dp1 = dFdx( view );
    vec3 dp2 = dFdy( view );
    vec2 duv1 = dFdx( uv );
    vec2 duv2 = dFdy( uv );
 
    // solve the linear system
    vec3 dp2perp = cross( dp2, normal );
    vec3 dp1perp = cross( normal, dp1 );
    vec3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 binormal = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = inversesqrt( max( dot(tangent,tangent), dot(binormal,binormal) ) );
    return mat3( tangent * invmax, binormal * invmax, normal );
}

vec3 PerturbNormal(vec3 normal, vec3 view, vec2 texCoord, uint normal_map_index)
{
	// sample normal map
	vec3 map = texture(sampler2D(normalMaps[normal_map_index - 1], mapSampler), fragTexCoord).xyz;
	map = map * 2.0f - 1.0f;
	mat3 cotangentFrame = CotangentFrame(normal, -view, texCoord);
	return normalize(cotangentFrame * map);
}

void main()
{
	vec4 diffuse = material_data.materials[matIndex].diffuse;
	
	// if diffuse map index is non-zero sample the diffuse map
	uint diffuse_map_index = material_data.materials[matIndex].diffuse_map_index;
	if(diffuse_map_index > 0)
	{
		diffuse = diffuse * texture(sampler2D(diffuseMaps[diffuse_map_index - 1], mapSampler), fragTexCoord);
	}

	// if normal map index is non-zero sample the normal map
	vec3 normal = worldNormal;
	uint normal_map_index = material_data.materials[matIndex].bump_map_index;
	if(normal_map_index > 0)
	{
		normal = PerturbNormal(normal, eyeVec, fragTexCoord, normal_map_index);
	}

	vec4 color = material_data.materials[matIndex].ambient * vec4(light_data.scene_data.xyz, 1.0f);

	// calculate lighting for all lights
	for(uint i = 0; i < light_data.scene_data.w; i++)
	{
		LightData light = light_data.lights[i];
		vec4 lighting = CalculateLighting(worldPosition, normal, light.lightPosition, light.lightDirection, light.lightColor, light.lightRange, light.lightIntensity, light.lightType, light.shadowsEnabled);
		color = color + (diffuse * lighting);
	}

	// set alpha to material dissolve value
	color.w = material_data.materials[matIndex].dissolve;

	outColor = color;
}