#version 450 core

layout (location = 0) in vec4 a_vertex;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_bitangent;
layout (location = 3) in vec3 a_tangent;
layout (location = 4) in vec2 a_texCoord;

layout (location = 0) out vec3 v_g_normal;
layout (location = 1) out vec3 v_g_bitangent;
layout (location = 2) out vec3 v_g_tangent;
layout (location = 3) out vec2 v_g_texCoord;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main(void)
{
    v_g_normal = a_normal;
    v_g_bitangent = a_bitangent;
    v_g_tangent = a_tangent;
	v_g_texCoord = a_texCoord;

	gl_Position = a_vertex;
}
