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

//layout(binding = 1) uniform sampler2D displacementSampler;

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec4 worldPosition;
layout(location = 4) out uint matIndex;

void ApproximateTangentVectors(in vec3 normal, out vec3 tangent, out vec3 binormal)
{
	vec3 c1 = cross(normal, vec3(0.0, 0.0, 1.0)); 
	vec3 c2 = cross(normal, vec3(0.0, 1.0, 0.0)); 
	if (length(c1) > length(c2))
		tangent = c1;	
	else
		tangent = c2;	
	
	tangent = normalize(tangent);
	binormal = normalize(cross(normal, tangent)); 
}

void main()
{
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	fragTexCoord = inTexCoord;
	normal = normalize(inNormal * mat3(ubo.model));
	worldPosition = ubo.model * vec4(inPosition, 1.0);
	matIndex = inMatIndex;
}