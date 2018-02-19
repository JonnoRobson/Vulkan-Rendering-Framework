#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) flat in uint matIndex;
layout(location = 2) flat in uint drawID;

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

layout(push_constant) uniform PushConstants
{
	uint shapeID;
} push_constants;

// textures
layout(binding = 2) uniform sampler mapSampler;
layout(binding = 3) uniform texture2D alphaMaps[512];

// outputs
layout(location = 0) out uint visibilityBuffer;

#define SHAPE_ID_BITS 12

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

	visibilityBuffer = (gl_PrimitiveID << SHAPE_ID_BITS) | push_constants.shapeID;
}