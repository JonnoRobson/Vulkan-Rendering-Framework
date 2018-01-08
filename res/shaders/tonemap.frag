#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;

// buffers
layout(binding = 0) uniform TonemapFactors
{
	float vignette_strength;
	float exposure_level;
	float gamma_level;
	float padding;
} tonemap_factors;

// textures
layout(binding = 1) uniform sampler bufferSampler;
layout(binding = 2) uniform texture2D originalTexture;
layout(binding = 3) uniform texture2D blurTexture;

// outputs
layout(location = 0) out vec4 outColor;

void main()
{
	vec4 original = texture(sampler2D(originalTexture, bufferSampler), fragTexCoord);
	vec4 blurred = texture(sampler2D(blurTexture, bufferSampler), fragTexCoord);

	vec4 color = lerp(original, blur, 0.4f);

	vec2 inTex = fragTexCoord - 0.5f;
	float vignette = 1.0f - dot(inTex, inTex);

	color *= pow(vignette, tonemap_factors.vignette_strength);

	color = pow(color, tonemap_factors.exposure_level);

	color = pow(color, tonemap_factors.gamma_level);

	outColor = color;
}