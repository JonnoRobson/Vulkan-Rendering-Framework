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
	mat4 viewProjMatrix;
};

// uniform buffers
layout(binding = 2) buffer LightingBuffer
{
	vec4 scene_data;
	vec4 camera_pos;
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
layout(binding = 5) uniform texture2D ambientMaps[512];
layout(binding = 6) uniform texture2D diffuseMaps[512];
layout(binding = 7) uniform texture2D specularMaps[512];
layout(binding = 8) uniform texture2D specularHighlightMaps[512];
layout(binding = 9) uniform texture2D emissiveMaps[512];
layout(binding = 10) uniform texture2D normalMaps[512];
layout(binding = 11) uniform texture2D alphaMaps[512];
layout(binding = 12) uniform texture2D reflectionMaps[512];
layout(binding = 13) uniform texture2D shadowMaps[16];

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

vec4 CalculateLighting(vec4 worldPosition, vec3 worldNormal, uint matIndex, uint lightIndex)
{
	MaterialData mat = material_data.materials[matIndex];
	LightData light = light_data.lights[lightIndex];

	vec4 lightPosition = light.lightPosition;
	vec4 lightDirection = light.lightDirection;
	vec4 lightColor = light.lightColor;
	float lightRange = light.lightRange;
	float lightIntensity = light.lightIntensity;
	float lightType = light.lightType;
	float shadowsEnabled = light.shadowsEnabled;
	mat4 lightViewProj = light.viewProjMatrix;

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

	// calculate specular lighting
	vec4 specularColor = material_data.materials[matIndex].specular;
	uint specular_map_index = material_data.materials[matIndex].specular_map_index;
	if(specular_map_index > 0)
	{
		specularColor = specularColor * texture(sampler2D(specularMaps[specular_map_index - 1], mapSampler), fragTexCoord);
	}

	// calculate eye vector
	vec3 eyeVec = worldPosition.xyz - light_data.camera_pos.xyz;

	// calculate reflected light vector
	vec3 reflectLight = reflect(rayDir, worldNormal);

	// calculate specular power
	float specularExponent = material_data.materials[matIndex].shininess;
	uint exponent_map_index = material_data.materials[matIndex].specular_highlight_map_index;
	if(exponent_map_index > 0)
	{
		specularExponent = specularExponent * texture(sampler2D(specularHighlightMaps[exponent_map_index - 1], mapSampler), fragTexCoord).x;
	}

	float specularPower = pow(clamp(dot(eyeVec, reflectLight), 0, 1), specularExponent);

	specularColor = specularColor * specularPower;

	// calculate shadow occlusion
	float occlusion = 0.0f;
	if(shadowsEnabled > 0)
	{
		// calculate pixel position in light space
		vec4 lightSpacePos = worldPosition * lightViewProj;
		lightSpacePos = lightSpacePos / lightSpacePos.w; 

		// calculate shadow map tex coords
		vec2 projTexCoord;
		projTexCoord.x = lightSpacePos.x / 2.0f + 0.5f;
		projTexCoord.y = -lightSpacePos.y / 2.0f + 0.5f;

		// test if the pixel falls inside the light map
		if((clamp(projTexCoord.x, 0, 1) == projTexCoord.x) && (clamp(projTexCoord.y, 0, 1) == projTexCoord.y))
		{
			// sample the shadow map
			float shadowMapValue = texture(sampler2D(shadowMaps[lightIndex], mapSampler), projTexCoord).x;
			
			// test if the pixel is the closest pixel to the light
			if(lightSpacePos.z - 0.001f <= shadowMapValue)
			{
				occlusion = 0.0f;
			}
			else
			{
				occlusion = 1.0f;
			}
		}
		else
		{
			occlusion = 1.0f;
		}
	}

	vec3 color = (lightColor.xyz + specularColor.xyz) * lightPower * lightIntensity * attenuation * (1.0f - occlusion);

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
		vec3 cameraVec = worldPosition.xyz - light_data.camera_pos.xyz;
		normal = PerturbNormal(normal, cameraVec, fragTexCoord, normal_map_index);
	}

	vec4 color = material_data.materials[matIndex].ambient * vec4(light_data.scene_data.xyz, 1.0f);

	// calculate lighting for all lights
	for(uint i = 0; i < light_data.scene_data.w; i++)
	{
		vec4 lighting = CalculateLighting(worldPosition, normal, matIndex, i);
		color = color + (diffuse * lighting);
	}

	// set alpha to material dissolve/alpha map value
	color.w = material_data.materials[matIndex].dissolve;
	uint alpha_map_index = material_data.materials[matIndex].alpha_map_index;
	if(alpha_map_index > 0)
	{
		color.w = color.w * texture(sampler2D(alphaMaps[alpha_map_index - 1], mapSampler), fragTexCoord).x;
	}

	outColor = color;
}