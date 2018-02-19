#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inPositionMatIndex;
layout(location = 1) in vec4 inEncodedNormalTexCoord;

layout(binding = 0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec4 worldPosition;
layout(location = 3) out uint matIndex;

vec3 SphereMapDecode(vec2 encoded_normal)
{
	vec4 nn = vec4(encoded_normal, 0, 0) * vec4(2, 2, 0, 0) + vec4(-1, -1, 1, -1);
	float l = dot(nn.xyz, -nn.xyw);
	nn.z = l;
	nn.xy *= sqrt(l);
	return nn.xyz * 2.0f + vec3(0, 0, -1);
}

void main()
{
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPositionMatIndex.xyz, 1.0);
	fragTexCoord = inEncodedNormalTexCoord.zw;
	normal = normalize(SphereMapDecode(inEncodedNormalTexCoord.xy) * mat3(ubo.model));
	worldPosition = ubo.model * vec4(inPositionMatIndex.xyz, 1.0);
	matIndex = uint(inPositionMatIndex.w);
}