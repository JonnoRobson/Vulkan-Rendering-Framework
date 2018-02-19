#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec4 worldPosition;
layout(location = 3) flat in uint matIndex;

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
layout(binding = 3) uniform texture2D alphaMaps[512];

// outputs
layout(location = 0) out vec4 gBufferPosMat;
layout(location = 1) out vec4 gBufferNormTex;

vec2 SphereMapEncode(vec3 normal)
{
	vec2 enc = (normal.x == 0 && normal.y == 0) ? vec2(0.0f, 0.0f) : normalize(normal.xy);
	enc = enc * (sqrt(-normal.z * 0.5f + 0.5f));
	enc = enc * 0.5f + vec2(0.5f, 0.5f);
	return enc;
}

void main()
{
	// discard pixel if alpha is less than 1
	float alpha = material_data.materials[matIndex].dissolve;
	uint alpha_map_index = material_data.materials[matIndex].alpha_map_index;
	if(alpha_map_index > 0)
	{
		alpha = alpha * texture(sampler2D(alphaMaps[alpha_map_index - 1], mapSampler), fragTexCoord).r;
	}

	if(alpha < 1.0f)
		discard;

	gBufferPosMat = vec4(worldPosition.xyz, float(matIndex + 1));
	gBufferNormTex = vec4(SphereMapEncode(normalize(worldNormal)), fragTexCoord);
}