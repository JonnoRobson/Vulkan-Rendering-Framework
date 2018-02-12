#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inPositionMatIndex;
layout(location = 1) in vec4 inEncodedNormalTexCoord;

layout(binding = 0) uniform TransformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} transforms;

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out uint matIndex;

void main()
{
	gl_Position = transforms.proj * transforms.view * transforms.model * vec4(inPositionMatIndex.xyz, 1.0);
	fragTexCoord = inEncodedNormalTexCoord.zw;
	matIndex = uint(inPositionMatIndex.w);
}