#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) flat in uint matIndex;

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
layout(binding = 4, rg32f) uniform image2D minMaxDepthBuffer;
layout(binding = 5) uniform PeelDataBuffer
{
	uint pass_number;
	vec2 screen_dimensions;
	float padding;
};

// outputs
layout(location = 0) out uint frontVisibilityBuffer;
layout(location = 1) out uint backVisibilityBuffer;

#define SHAPE_ID_BITS 12
#define PASS_COUNT 2

void main()
{
	float fragDepth = gl_FragCoord.z;
	vec2 minMaxDepth = imageLoad(minMaxDepthBuffer, ivec2(gl_FragCoord.xy * screen_dimensions.xy)).xy;
	float nearestDepth = minMaxDepth.x;
	float furthestDepth = minMaxDepth.y;

	// sample alpha of this pixel
	float alpha = material_data.materials[matIndex].dissolve;
	uint alpha_map_index = material_data.materials[matIndex].alpha_map_index;
	if(alpha_map_index > 0)
	{
		alpha = alpha * texture(sampler2D(alphaMaps[alpha_map_index - 1], mapSampler), fragTexCoord).r;
	}

	// if alpha for this pixel is zero simply discard it
	if(alpha <= 0.0f)
		discard;

	// fragment at this depth has already been peeled
	if(fragDepth < nearestDepth || fragDepth > furthestDepth)
	{
		imageStore(minMaxDepthBuffer, ivec2(gl_FragCoord.xy * screen_dimensions), vec4(0.0, 0.0, 0.0, 0.0));
		discard;
	}

	// fragment at this depth needs to be peeled again
	if(fragDepth > nearestDepth && fragDepth < furthestDepth)
	{
		imageStore(minMaxDepthBuffer, ivec2(gl_FragCoord.xy * screen_dimensions), vec4(vec2(fragDepth, 1.0f - fragDepth), 0, 0));
		discard;
	}

	// fragment is on peeled layer from last past so add it to the peeled visibility buffer
	uint visibilityData = (gl_PrimitiveID << SHAPE_ID_BITS) | push_constants.shapeID;
	imageStore(minMaxDepthBuffer, ivec2(gl_FragCoord.xy * screen_dimensions), vec4(0.0, 0.0, 0.0, 0.0));
	
	frontVisibilityBuffer = 0;
	backVisibilityBuffer = 0;

	if(fragDepth == nearestDepth)
		frontVisibilityBuffer = visibilityData;
	else
		backVisibilityBuffer = visibilityData;
}