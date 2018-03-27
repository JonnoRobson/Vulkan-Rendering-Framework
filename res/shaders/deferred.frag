#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(origin_upper_left) in vec4 gl_FragCoord;
layout(location = 0) in vec2 screenTexCoord;

struct LightData
{
	vec4 lightPosition;
	vec4 lightDirection;
	vec4 lightColor;
	float lightRange;
	float lightIntensity;
	float lightType;
	float shadowsEnabled;
	mat4 viewProjMatrices[6];
	uint shadowMapIndex;
	uint padding[3];
};

// uniform buffers
layout(binding = 0) buffer LightingBuffer
{
	vec4 scene_data;
	vec4 camera_data;
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

layout(binding = 1) uniform MaterialUberBuffer
{
	MaterialData materials[512];
} material_data;

// textures
layout(binding = 2) uniform sampler mapSampler;
layout(binding = 3) uniform texture2D ambientMaps[512];
layout(binding = 4) uniform texture2D diffuseMaps[512];
layout(binding = 5) uniform texture2D specularMaps[512];
layout(binding = 6) uniform texture2D specularHighlightMaps[512];
layout(binding = 7) uniform texture2D emissiveMaps[512];
layout(binding = 8) uniform texture2D normalMaps[512];
layout(binding = 9) uniform texture2D alphaMaps[512];
layout(binding = 10) uniform texture2D reflectionMaps[512];
layout(binding = 11) uniform texture2D shadowMaps[96];

layout(binding = 12) uniform texture2D gBuffer[2];
layout(binding = 13) uniform sampler bufferSampler;
layout(binding = 14) uniform sampler shadowMapSampler;

// outputs
layout(location = 0) out vec4 outColor;

float CalculateAttenuation(vec3 lightVector, vec4 lightDirection, float dist, float lightRange, float lightType)
{
	float attenuation = 1.0f;

	if(lightType == 1.0f || lightType == 2.0f)
	{	
		float dSquared = dot(lightVector * dist, lightVector * dist);
		attenuation = max(0, 1.0f - (dSquared / (lightRange * lightRange)));
	}

	
	if(lightType == 2.0f && attenuation > 0)
	{
		// add in spotlight attenuation factor
		vec3 lightVector2 = lightDirection.xyz;
		float rho = dot(lightVector, lightVector2);

		attenuation = max(0, attenuation * pow(rho, 8));
	}
	

	return attenuation;
}

float CalculateShadowOcclusion(vec4 worldPosition, vec3 rayDir, uint lightIndex, uint pcfSize)
{
	LightData light = light_data.lights[lightIndex];
	float shadowsEnabled = light.shadowsEnabled;

	if(shadowsEnabled > 0)
	{
		// determine the correct shadow map and matrices to use
		uint shadowMapIndex = 0;
		mat4 lightViewProj;
		if(light.lightType != 1.0f)
		{
			lightViewProj = light.viewProjMatrices[0];
			shadowMapIndex = light.shadowMapIndex;
		}
		else
		{
			// use the incoming ray dir to determine which shadow map to use
			float dir = max(max(abs(rayDir.y), abs(rayDir.z)), abs(rayDir.x));

			int indexOffset = 0;

			if(dir == abs(rayDir.x))
			{
				if(rayDir.x > 0)
					indexOffset= 0;
				else
					indexOffset = 1;	
			}
			else if (dir == abs(rayDir.y))
			{
				if(rayDir.y > 0)
					indexOffset= 2;
				else
					indexOffset = 3;	
			}
			else
			{
				if(rayDir.z > 0)
					indexOffset= 4;
				else
					indexOffset = 5;	
			}

			lightViewProj = light.viewProjMatrices[indexOffset];
			shadowMapIndex = light.shadowMapIndex + indexOffset;
		}

		// calculate pixel position in light space
		vec4 lightSpacePos = lightViewProj * worldPosition;
		lightSpacePos = lightSpacePos / lightSpacePos.w; 

		// calculate shadow map tex coords
		vec2 projTexCoord = lightSpacePos.xy * 0.5 + 0.5;

		// compute total number of samples to take from shadow map
		int pcfSizeMinus1 = int(pcfSize - 1);
		float kernelSize = 2.0 * pcfSizeMinus1 + 1.0;
		float numSamples = kernelSize * kernelSize;

		// counter for shadow map samples not in shadow
		float lightCount = 0.0;

		// sample the shadow map
		ivec2 textureDims = textureSize(sampler2D(shadowMaps[shadowMapIndex], shadowMapSampler), 0);
		float shadowMapTexelSize = 1.0 / textureDims.x;
		for(int x = -pcfSizeMinus1; x <= pcfSizeMinus1; x++)
		{
			for(int y = -pcfSizeMinus1; y <= pcfSizeMinus1; y++)
			{
				// compute coordinate for this pcf sample
				vec2 pcfCoord = projTexCoord + vec2(x, y) * shadowMapTexelSize;

				// test if the pixel falls inside the light map
				if((clamp(pcfCoord.x, 0, 1) == pcfCoord.x) && (clamp(pcfCoord.y, 0, 1) == pcfCoord.y))
				{
					// check if sample is in light
					float shadowMapValue = texture(sampler2D(shadowMaps[shadowMapIndex], shadowMapSampler), pcfCoord).x;
					if(lightSpacePos.z - 0.001 <= shadowMapValue)
						lightCount += 1.0;
				}
			}
		}

		return 1.0 - (lightCount / numSamples);
	}
	else
	{
		return 0.0f;
	}
}

vec4 CalculateLighting(vec4 worldPosition, vec3 worldNormal, vec2 fragTexCoord, vec4 specularColor, uint matIndex, uint lightIndex)
{
	MaterialData mat = material_data.materials[matIndex];
	LightData light = light_data.lights[lightIndex];

	vec4 lightPosition = light.lightPosition;
	vec4 lightDirection = light.lightDirection;
	vec4 lightColor = light.lightColor;
	float lightRange = light.lightRange;
	float lightIntensity = light.lightIntensity;
	float lightType = light.lightType;
		
	// calculate distance to the pixel from the camera and quality level
	float cameraDist = length(worldPosition.xyz - light_data.camera_data.xyz);
	float qualityLevel = light_data.camera_data.w / cameraDist;

	// immediatly return black if light intensity is zero or less
	if(lightIntensity <= 0.0f)
	{
		return vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}

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
	
	// calculate light power and return black if it is zero or less
	float lightPower = clamp(dot(worldNormal, rayDir), 0.0f, 1.0f);
	if(lightPower <= 0.0f)
	{
		return vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	// calculate attenuation and return black if fully attenuated
	float attenuation = CalculateAttenuation(rayDir, lightDirection, dist, lightRange, lightType);
	if(attenuation <= 0.0f)
	{
		return vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	// don't bother with specular lighting if quality level is low
	if(qualityLevel < 0.25f)
	{
		if(specularColor.x + specularColor.y + specularColor.z > 0)
		{
			// calculate eye vector
			vec3 eyeVec = worldPosition.xyz - light_data.camera_data.xyz;

			// calculate reflected light vector
			vec3 reflectLight = reflect(rayDir, worldNormal);

			float specularPower = pow(clamp(dot(reflectLight, eyeVec), 0, 1), specularColor.w);
			if(specularPower < 0.0f)
				specularPower = 0.0f;

			specularColor.xyz = specularColor.xyz * specularPower;
		}
	}

	uint shadowQuality = uint(max(1, min(4 * qualityLevel, 4)));
	
	// calculate shadow occlusion and return black if fully occluded
	float occlusion = CalculateShadowOcclusion(worldPosition, -rayDir, lightIndex, shadowQuality);
	if(occlusion >= 1.0f)
	{
		return vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
			
	vec3 color = (lightColor.xyz + specularColor.xyz) * lightPower * lightIntensity * attenuation * (1.0f - occlusion);
	
	if(color.x < 0.0f)
		color.x = 0.0f;

	if(color.y < 0.0f)
		color.y = 0.0f;

	if(color.z < 0.0f)
		color.z = 0.0f;

	return vec4(color, 1.0f);
}

mat3 CotangentFrame(vec3 normal, vec3 view, vec2 uv)
{
	// if the normal is 0,0,1 construct tangent and binormal in the other axis
	vec3 tangent, binormal;

	// get edge vectors of the pixel triangle
	vec3 dp1 = dFdx( view );
	vec3 dp2 = dFdy( view );
	vec2 duv1 = dFdx( uv );
	vec2 duv2 = dFdy( uv );
 
	// solve the linear system
	vec3 dp2perp = cross( dp2, normal );
	vec3 dp1perp = cross( normal, dp1 );
	tangent = dp2perp * duv1.x + dp1perp * duv2.x;
	binormal = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame 
    float invmax = inversesqrt( max( dot(tangent,tangent), dot(binormal,binormal) ) );
    return mat3( tangent * invmax, binormal * invmax, normal );
}

vec3 PerturbNormal(vec3 normal, vec3 view, vec2 texCoord, uint normal_map_index)
{
	// sample normal map
	vec3 map = texture(sampler2D(normalMaps[normal_map_index - 1], mapSampler), texCoord).xyz;
	map = map * 2.0f - 1.0f;
	mat3 cotangentFrame = CotangentFrame(normal, -view, texCoord);
	return normalize(cotangentFrame * map);
}

vec3 SphereMapDecode(vec2 encoded_normal)
{
	vec4 nn = vec4(encoded_normal, 0, 0) * vec4(2, 2, 0, 0) + vec4(-1, -1, 1, -1);
	float l = dot(nn.xyz, -nn.xyw);
	nn.z = l;
	nn.xy *= sqrt(l);
	return nn.xyz * 2.0f + vec3(0, 0, -1);
}

void ReadGBuffer(in vec4 g_buffer_1, in vec4 g_buffer_2, out vec3 world_position, out vec3 world_normal, out vec2 tex_coords, out uint material_index)
{
	world_position = g_buffer_1.xyz;
	material_index = uint(g_buffer_1.w) - 1;
	
	world_normal = SphereMapDecode(g_buffer_2.xy);
	tex_coords = g_buffer_2.zw;
}

void main()
{
	vec3 worldPosition = vec3(0.0f, 0.0f, 0.0f);
	vec3 worldNormal = vec3(0.0f, 0.0f, 0.0f);
	vec2 fragTexCoord = vec2(0.0f, 0.0f);
	uint matIndex = 0;

	// sample the g-buffer textures
	vec4 gBuffer1 = texelFetch(sampler2D(gBuffer[0], bufferSampler), ivec2(gl_FragCoord.xy), 0);
	vec4 gBuffer2 = texelFetch(sampler2D(gBuffer[1], bufferSampler), ivec2(gl_FragCoord.xy), 0);

	// discard pixel if it has an empty material index
	if(gBuffer1.w == 0)
	{
		discard;
	}

	// decode g buffer data
	ReadGBuffer(gBuffer1, gBuffer2, worldPosition, worldNormal, fragTexCoord, matIndex);
	
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
		vec3 cameraVec = light_data.camera_data.xyz - worldPosition.xyz;
		normal = PerturbNormal(normal, cameraVec, fragTexCoord, normal_map_index);
	}
	
	// if ambient map index is non-zero sample the ambient map
	vec4 ambient = material_data.materials[matIndex].ambient;
	uint ambient_map_index = material_data.materials[matIndex].ambient_map_index;
	if(ambient_map_index > 0)
	{
		ambient = ambient + texture(sampler2D(ambientMaps[ambient_map_index - 1], mapSampler), fragTexCoord);
	}
		
	vec4 color = ambient * vec4(light_data.scene_data.xyz, 1.0f);

	// calculate specular color
	vec4 specularColor = material_data.materials[matIndex].specular;
	uint specular_map_index = material_data.materials[matIndex].specular_map_index;
	if(specular_map_index > 0)
	{
		specularColor = specularColor * texture(sampler2D(specularMaps[specular_map_index - 1], mapSampler), fragTexCoord);
	}
	
	// calculate specular power
	specularColor.w = material_data.materials[matIndex].shininess;
	uint exponent_map_index = material_data.materials[matIndex].specular_highlight_map_index;
	if(exponent_map_index > 0)
	{
		specularColor.w = specularColor.w * texture(sampler2D(specularHighlightMaps[exponent_map_index - 1], mapSampler), fragTexCoord).x;
	}

	// calculate lighting for all lights
	for(uint i = 0; i < light_data.scene_data.w; i++)
	{
		vec4 lighting = CalculateLighting(vec4(worldPosition, 1.0f), normal, fragTexCoord, specularColor, matIndex, i);
		color = color + (diffuse * lighting);
	}

	color.w = 1.0f;
	
	outColor = color;
}