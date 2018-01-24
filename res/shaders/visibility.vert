#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in uint inMaterialIndex;

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
	gl_Position = transforms.proj * transforms.view * transforms.model * vec4(inPosition, 1.0);
	fragTexCoord = inTexCoord;
	matIndex = inMaterialIndex;
}