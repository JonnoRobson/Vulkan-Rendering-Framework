#version 450
#extension GL_ARB_separate_shader_objects : enable

// inputs
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec4 worldPosition;
layout(location = 4) flat in uint matIndex;

// outputs
layout(location = 0) out vec4 gBufferPosMat;
layout(location = 1) out vec4 gBufferNormTex;

vec2 SphereMapEncode(vec3 normal)
{
	if(normal.z > 0.999f)
	{
		return vec2(256.0f, 256.0f);
	}

	vec2 enc = normalize(normal.xy) * (sqrt(-normal.z * 0.5f + 0.5f));
	enc = enc * 0.5f + vec2(0.5f, 0.5f);
	return enc;
}

void main()
{
	gBufferPosMat = vec4(worldPosition.xyz, float(matIndex + 1));
	gBufferNormTex = vec4(SphereMapEncode(normalize(worldNormal)), fragTexCoord);
}