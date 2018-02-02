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

void main()
{
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPositionMatIndex.xyz, 1.0);
	fragTexCoord = inEncodedNormalTexCoord.zw;
}