#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;

// buffers
layout(binding = 0) uniform BlurFactors
{
	float radius;
	vec2 direction;
	float padding;
} blur_factors;

// textures
layout(binding = 1) uniform sampler bufferSampler;
layout(binding = 2) uniform texture2D sourceTexture;

// outputs
layout(location = 0) out vec4 outColor;

void main()
{
	// calculate texel dimensions for offsets
	ivec3 textureDims = textureSize(sampler2D(sourceTexture, bufferSampler), 0);

	float texelWidth = 1.0f / textureDims.x;
	float texelWdith = 1.0f / textureDims.y;
	
	// blur over the radius
	vec4 color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	float PixelWeight[8] = {0.2537, 0.2185, 0.0821, 0.0461, 0.0262, 0.0162, 0.0102, 0.0052 };
	for(int i = 0; i < blur_factors.radius; i++)
	{
		vec2 offset = vec2(blur_factors.direction.x * texeilWidth * i, blur_factors.direction.y * texelHeight * i);

		color += texture(sampler2D(sourceTexture, bufferSampler), fragTexCoord + offset) * PixelWeight[i];
		color += texture(sampler2D(sourceTexture, bufferSampler), fragTexCoord - offset) * PixelWeight[i];
	}

	outColor = color;
}