#version 450
#extension GL_ARB_separate_shader_objects : enable

// defines
#define MSAA_COUNT 2

// inputs
layout(location = 0) in vec2 fragTexCoord;

// textures
layout(binding = 0) uniform sampler bufferSampler;
layout(binding = 1) uniform texture2DMS accumulationTexture;
layout(binding = 2) uniform texture2DMS revealageTexture;

// outputs
layout(location = 0) out vec4 outColor;

void main()
{
	vec4 totalColor = vec4(0.0, 0.0, 0.0, 0.0);
	ivec2 coord = ivec2(gl_FragCoord.xy);

	int samplesApplied = 0;

	for(int i = 0; i < MSAA_COUNT; i++)
	{
	
		// retrieve revealage
		float reveal = texelFetch(sampler2DMS(revealageTexture, bufferSampler), coord, i).r;
		if(reveal == 1.0f)
			continue;

		// retrieve accumulation
		vec4 accum = texelFetch(sampler2DMS(accumulationTexture, bufferSampler), coord, i);
		if(isinf(max(max(abs(accum.x), abs(accum.y)), abs(accum.z))))
		{
			accum.rgb = vec3(accum.a);
		}

		// calculate final combined color
		vec3 averageColor = accum.rgb / max(accum.a, 0.00001);

		totalColor += vec4(averageColor, 1.0f - reveal);

		samplesApplied++;
	}

	if(samplesApplied == 0)
		discard;

    outColor = totalColor / float(samplesApplied);
}