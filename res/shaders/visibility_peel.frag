#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(origin_upper_left) in vec4 gl_FragCoord;
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) flat in uint matIndex;
layout(location = 2) flat in uint shapeID;

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
layout(binding = 4, r32f) uniform image2D inMinDepthBuffer;
layout(binding = 5, r32f) uniform image2D inMaxDepthBuffer;

// outputs
layout(location = 0) out uint frontVisibilityBuffer;
layout(location = 1) out uint backVisibilityBuffer;
layout(location = 2) out float outMinDepthBuffer;
layout(location = 3) out float outMaxDepthBuffer;

#define SHAPE_ID_BITS 12
#define PASS_COUNT 2

void main()
{
	float fragDepth = gl_FragCoord.z;
	ivec2 depthBufferCoord = ivec2(gl_FragCoord.xy);
	
	float minDepth = imageLoad(inMinDepthBuffer, depthBufferCoord).x;
	float maxDepth = imageLoad(inMaxDepthBuffer, depthBufferCoord).x;
	
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
	if(fragDepth < minDepth || fragDepth > maxDepth)
	{
		outMinDepthBuffer = 1.0;
		outMaxDepthBuffer = 0.0;
		frontVisibilityBuffer = 0;
		backVisibilityBuffer = 0;
		return;
	}

	// fragment at this depth needs to be peeled again
	if(fragDepth > minDepth && fragDepth < maxDepth)
	{
		outMinDepthBuffer = fragDepth;
		outMaxDepthBuffer = 1.0 - fragDepth;
		frontVisibilityBuffer = 0;
		backVisibilityBuffer = 0;
		return;
	}

	// fragment is on peeled layer from last pass so add it to the peeled visibility buffer
	uint visibilityData = (gl_PrimitiveID << SHAPE_ID_BITS) | shapeID;
	outMinDepthBuffer = 1.0;
	outMaxDepthBuffer = 0.0;
	
	frontVisibilityBuffer = 0;
	backVisibilityBuffer = 0;

	if(fragDepth == minDepth)
		frontVisibilityBuffer = visibilityData;
	else
		backVisibilityBuffer = visibilityData;
	
}