#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in uint inMatIndex;

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

void main()
{
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	fragTexCoord = inTexCoord;
	normal = normalize(inNormal * mat3(ubo.model));
	worldPosition = ubo.model * vec4(inPosition, 1.0);
	matIndex = inMatIndex;
}