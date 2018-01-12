#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;


// textures
layout(binding = 0) uniform sampler bufferSampler;
layout(binding = 1) uniform texture2D accumulationTexture;
layout(binding = 2) uniform texture2D revealageTexture;

// outputs
layout(location = 0) out vec4 outColor;

void main()
{
	ivec2 coord = ivec2(gl_FragCoord.xy);
	
	// retrieve revealage
	float reveal = texelFetch(sampler2D(revealageTexture, bufferSampler), coord, 0).r;
	if(reveal == 1.0f)
		discard;

	// retrieve accumulation
	vec4 accum = texelFetch(sampler2D(accumulationTexture, bufferSampler), coord, 0);
	if(isinf(max(max(abs(accum.x), abs(accum.y)), abs(accum.z))))
	{
        accum.rgb = vec3(accum.a);
    }

	// calculate final combined color
	vec3 averageColor = accum.rgb / max(accum.a, 0.00001);
    outColor = vec4(averageColor, 1.0f - reveal);
}