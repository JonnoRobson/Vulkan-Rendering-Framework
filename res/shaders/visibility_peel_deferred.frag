#version 450
#extension GL_ARB_separate_shader_objects : enable

#define PEEL_COUNT 4

// inputs
layout(origin_upper_left) in vec4 gl_FragCoord;
layout(location = 0) in vec2 screenTexCoord;

// required structs
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

struct StorageVertex
{
	vec4 pos_mat_index;
	vec4 encoded_normal_tex_coord;
};

struct Vertex
{
	vec3 pos;
	vec2 tex_coord;
	vec3 normal;
	uint mat_index;
};

struct Shape
{
	uvec4 offsets;
	vec4 min_vertex;
	vec4 max_vertex;
};

// uniform buffers
layout(binding = 0) buffer LightingBuffer
{
	vec4 scene_data;
	vec4 camera_data;
	LightData lights[];
} light_data;

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

layout(binding = 12) uniform utexture2D visibilityBuffers[PEEL_COUNT * 2];
layout(binding = 13) uniform texture2D depthBuffers[PEEL_COUNT * 2];
layout(binding = 14) uniform sampler bufferSampler;

// vertex, index and shape buffers
layout(binding = 15) buffer VertexBuffer
{
	StorageVertex _vertices[];
};

layout(binding = 16) buffer IndexBuffer
{
	uint _indices[];
};

layout(binding = 17) buffer ShapeBuffer
{
	Shape _shapes[];
};

layout(binding = 18) uniform MatrixBuffer
{
	vec4 screenDimensions;
	mat4 invView;
	mat4 invProj;
} matrix_data;

layout(binding = 19) uniform sampler shadowMapSampler;

// outputs
layout(location = 0) out vec4 outColor;

#define SHAPE_ID_BITS 12
#define SHAPE_ID_MASK 4095

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
		ivec2 textureDims = textureSize(sampler2D(shadowMaps[shadowMapIndex], mapSampler), 0);
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

vec3 Intersect(vec3 p, vec3 v0, vec3 v1, vec3 v2)
{
	vec3 weights = vec3(0, 0, 0);

	vec3 v0v1 = v1-v0;
	vec3 v0v2 = v2-v0;

	vec3 n = cross(v0v1, v0v2);
	float area = length(n) / 2;

	vec3 c = vec3(0.0f, 0.0f, 0.0f);

	vec3 edge1 = v2 - v1;
	vec3 vp1 = p - v1;
	c = cross(edge1, vp1);
	weights.x = (length(c) / 2) / area;

	vec3 edge2 = v0 - v2;
	vec3 vp2 = p - v2;
	c = cross(edge2, vp2);
	weights.y = (length(c) / 2) / area;

	weights.z = 1.0f - weights.x - weights.y;

	return weights;
}

Vertex LoadVertex(uint index)
{
	Vertex vertex;

	StorageVertex storageVertex = _vertices[index];

	vertex.pos = storageVertex.pos_mat_index.xyz;
	vertex.tex_coord = storageVertex.encoded_normal_tex_coord.zw;
	vertex.normal = SphereMapDecode(storageVertex.encoded_normal_tex_coord.xy);
	vertex.mat_index = uint(storageVertex.pos_mat_index.w);
	
	return vertex;   
}

vec3 PositionFromDepth(float depth, vec2 uv)
{
	vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 viewPos = matrix_data.invProj * clipPos;

	viewPos /= viewPos.w;

	vec4 worldPos = matrix_data.invView * viewPos;

	return worldPos.xyz;
}

Vertex LoadAndInterpolateVertex(uint vertexOffset, uint indexOffset, uint triID, float depth, vec2 pixelCoord)
{
	uint indexLoc0 = indexOffset + (triID * 3) + 0;
	uint indexLoc1 = indexOffset + (triID * 3) + 1;
	uint indexLoc2 = indexOffset + (triID * 3) + 2;
	
	uint vIndices[] = 
	{
		_indices[indexLoc0] + vertexOffset,
		_indices[indexLoc1] + vertexOffset,
		_indices[indexLoc2] + vertexOffset
	};
	
	Vertex v0 = LoadVertex(vIndices[0]);
	Vertex v1 = LoadVertex(vIndices[1]);
	Vertex v2 = LoadVertex(vIndices[2]);
	
	vec4 p0 = vec4(v0.pos, 1.0f);
	vec4 p1 = vec4(v1.pos, 1.0f);
	vec4 p2 = vec4(v2.pos, 1.0f);
	
	// calculate the barycentric coordinates of the pixel
	vec3 worldPos = PositionFromDepth(depth, screenTexCoord);
	vec3 weights = Intersect(worldPos.xyz, p0.xyz, p1.xyz, p2.xyz);

	Vertex vertex;
	vertex.pos = v0.pos * weights.x + (v1.pos * weights.y + (v2.pos * weights.z));
	vertex.tex_coord = v0.tex_coord * weights.x + (v1.tex_coord * weights.y + (v2.tex_coord * weights.z));
	vertex.normal = v0.normal * weights.x + (v1.normal * weights.y + (v2.normal * weights.z));
	vertex.mat_index = v0.mat_index;

	return vertex;
}

void main()
{
	vec3 worldPosition = vec3(0.0f, 0.0f, 0.0f);
	vec3 worldNormal = vec3(0.0f, 0.0f, 0.0f);
	vec2 fragTexCoord = vec2(0.0f, 0.0f);
	uint matIndex = 0;

	vec3 accumColor = vec3(0.0, 0.0, 0.0);
	float accumAlpha = 0.0;
	float accumCount = 0.0;
	ivec2 visBufferCoord = ivec2(gl_FragCoord.xy);

	// blend layers from back to front
	for(int i = 0; i < PEEL_COUNT; i++)
	{
		// read from the visibility buffer texture
		uint visibilityData = texelFetch(usampler2D(visibilityBuffers[i], bufferSampler), ivec2(gl_FragCoord.xy), 0).r;
		uint triID = visibilityData >> SHAPE_ID_BITS;
		uint shapeID = (visibilityData & SHAPE_ID_MASK);
		uvec2 offsets = _shapes[shapeID].offsets.xy;

		if(visibilityData == 0)
			break;
			
		// load depth
		float depth = texelFetch(sampler2D(depthBuffers[i], bufferSampler), ivec2(gl_FragCoord.xy), 0).r;
		Vertex vertex = LoadAndInterpolateVertex(offsets.x, offsets.y, triID, depth, gl_FragCoord.xy);
		worldPosition = vertex.pos;
		worldNormal = vertex.normal;
		fragTexCoord = vertex.tex_coord;
		matIndex = vertex.mat_index;

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

		// calculate sample opacity
		float alpha = material_data.materials[matIndex].dissolve;
		uint alpha_map_index = material_data.materials[matIndex].alpha_map_index;
		if(alpha_map_index > 0)
		{
			alpha = alpha * texture(sampler2D(alphaMaps[alpha_map_index - 1], mapSampler), fragTexCoord).x;
		}

		// calculate lighting for all lights
		for(uint l = 0; l < light_data.scene_data.w; l++)
		{
			vec4 lighting = CalculateLighting(vec4(worldPosition, 1.0f), normal, fragTexCoord, specularColor, matIndex, l);
			color = color + (diffuse * lighting);
		}

		color.w = alpha;
		
		// blend layer
		//accumColor = (accumColor * (1.0 - color.w)) + (color.xyz * color.w); 
		accumColor = accumColor + (color.xyz * color.w * clamp(1.0 - accumAlpha, 0.0, 1.0));
		accumAlpha = accumAlpha + color.w;

		if(accumAlpha >= 1.0)
			break;
	}

	if(accumAlpha <= 0)
		discard;

	outColor = vec4(accumColor, clamp(accumAlpha, 0, 1));
}