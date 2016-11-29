#version 450 core

layout (binding = 0, std140) uniform _u_bufferViewProjection {
        mat4 projectionMatrix;
        mat4 viewMatrix;
} u_bufferViewProjection;

layout (binding = 1, std140) uniform _u_bufferTransform {
        mat4 modelMatrix;
        mat3 normalMatrix;
} u_bufferTransform;

layout (location = 0) in vec4 a_vertex;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texCoord;

layout (location = 0) out vec3 v_f_normal;
layout (location = 3) out vec2 v_f_texCoord;
layout (location = 4) out vec3 v_f_incident;
layout (location = 5) out vec4 v_f_vertex;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main(void)
{
	v_f_normal = mat3(u_bufferViewProjection.viewMatrix) * u_bufferTransform.normalMatrix * a_normal;

	v_f_texCoord = a_texCoord;
	
	vec4 position = u_bufferViewProjection.viewMatrix * u_bufferTransform.modelMatrix * a_vertex;
	
	v_f_incident = normalize(position.xyz);
    
    v_f_vertex = u_bufferViewProjection.projectionMatrix * position;
    
    gl_Position = v_f_vertex;
}
