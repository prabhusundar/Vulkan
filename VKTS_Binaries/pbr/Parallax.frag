#version 450 core

#define VKTS_PARALLAX_SCALE 0.002



layout (location = 0) in vec3 v_f_normal;
layout (location = 1) in vec3 v_f_bitangent;
layout (location = 2) in vec3 v_f_tangent;
layout (location = 3) in vec2 v_f_texCoord;

layout (location = 4) in vec3 v_f_incident;
layout (location = 5) in vec4 v_f_vertex;



layout (location = 2) out vec4 ob_ambientOcclusionF0;   // Ambient occlusion and F0. GB not used.
layout (location = 1) out vec4 ob_normalRoughness;      // Normal and roughness.
layout (location = 0) out vec4 ob_colorMetallic;        // Color and metallic.

mat4 translate(vec3 t)
{
    return mat4(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, t.x, t.y, t.z, 1.0);
}

mat4 rotateRzRyRx(vec3 rotate)
{
    if (rotate.x == 0.0 && rotate.y == 0.0 && rotate.z == 0.0)
    {
        return mat4(1.0);
    }

    float rz = radians(rotate.z);
    float ry = radians(rotate.y);
    float rx = radians(rotate.x);
    float sx = sin(rx);
    float cx = cos(rx);
    float sy = sin(ry);
    float cy = cos(ry);
    float sz = sin(rz);
    float cz = cos(rz);

    return mat4(cy * cz, cy * sz, -sy, 0.0, -cx * sz + cz * sx * sy, cx * cz + sx * sy * sz, cy * sx, 0.0, sz * sx + cx * cz * sy, -cz * sx + cx * sy * sz, cx * cy, 0.0, 0.0, 0.0, 0.0, 1.0);
}

mat4 scale(vec3 s)
{
    return mat4(s.x, 0.0, 0.0, 0.0, 0.0, s.y, 0.0, 0.0, 0.0, 0.0, s.z, 0.0, 0.0, 0.0, 0.0, 1.0);
}

float fresnelNode(float eta, float theta)
{
    float c = abs(theta);
    float g = eta * eta - 1 + c * c;
    
    if(g > 0)
    {
        g = sqrt(g);
        
        float A = (g - c)/(g + c);
        float B = (c * (g + c) - 1)/(c * (g - c) + 1);
        
        return 0.5 * A * A * (1 + B * B);
    }
	
    return 1.0;
}

vec2 parallaxMappingNode(vec2 texCoord, vec3 view, float height)
{
    return texCoord - view.xy * height * VKTS_PARALLAX_SCALE;
}

vec3 bumpMappingNode(vec3 normal, float height, float distance, float strength)
{
    float finalHeight = height * distance; 

    vec3 x = normalize(vec3(1.0, 0.0, dFdx(finalHeight) * strength));
    vec3 y = normalize(vec3(0.0, 1.0, dFdy(finalHeight) * strength));
    vec3 z = cross(x, y);

    return mat3(x, y, z) * normal;
}

void main()
{ 
    vec3 normal = normalize(v_f_normal);
    vec3 incident = normalize(v_f_incident);
    vec3 bitangent = normalize(v_f_bitangent);
    vec3 tangent = normalize(v_f_tangent);
    
    mat3 objectToWorldMatrix = mat3(tangent, bitangent, normal);
    vec3 texCoord = vec3(v_f_texCoord, 0.0);
    
    
    // UV map start

    // Out
    vec3 UV_Map_001_UV = texCoord;
    
    // UV map end
    
    // Checker texture start

    // In
    vec3 Vector_1 = UV_Map_001_UV;
    vec4 Color1_3 = vec4(0.800, 0.800, 0.800, 1.000);
    vec4 Color2_4 = vec4(0.200, 0.200, 0.200, 1.000);
    float Scale_1 = 5.000;

    bool TempBool_2 = mod(floor(Vector_1.s * Scale_1), 2.0) == 1.0;
    bool TempBool_3 = mod(floor(Vector_1.t * Scale_1), 2.0) == 1.0;
    
    // Out
    vec4 Checker_Texture_001_Color = Color2_4;
    float Checker_Texture_001_Fac = 0.0;
        
    if ((TempBool_2 && !TempBool_3) || (!TempBool_2 && TempBool_3))
    {
        Checker_Texture_001_Color = Color1_3;
        Checker_Texture_001_Fac = 1.0;
    }
    
    // Checker texture end

    mat3 worldToTangentMatrix = transpose(mat3(tangent, bitangent, normal));
    texCoord = vec3(parallaxMappingNode(texCoord.xy, worldToTangentMatrix * incident, Checker_Texture_001_Fac), 0.0);
    
    // UV map start

    // Out
    vec3 UV_Map_UV = texCoord;
    
    // UV map end
    
    // Checker texture start

    // In
    vec3 Vector_0 = UV_Map_UV;
    vec4 Color1_1 = vec4(0.800, 0.800, 0.800, 1.000);
    vec4 Color2_2 = vec4(0.200, 0.200, 0.200, 1.000);
    float Scale_0 = 5.000;

    bool TempBool_0 = mod(floor(Vector_0.s * Scale_0), 2.0) == 1.0;
    bool TempBool_1 = mod(floor(Vector_0.t * Scale_0), 2.0) == 1.0;
    
    // Out
    vec4 Checker_Texture_Color = Color2_2;
    float Checker_Texture_Fac = 0.0;
        
    if ((TempBool_0 && !TempBool_1) || (!TempBool_0 && TempBool_1))
    {
        Checker_Texture_Color = Color1_1;
        Checker_Texture_Fac = 1.0;
    }
    
    // Checker texture end
    
    // PBR start

    // In
    float F0_0 = 0.500;
    float AmbientOcclusion_0 = 1.000;
    float Roughness_0 = 0.200;
    float Metallic_0 = 0.000;
    float Mask_0 = 0.000;
    vec3 Normal_0 = normal;
    vec4 Color_0 = Checker_Texture_Color;
    
    // PBR end

    if (round(Mask_0) == 1.0)
    {
        discard;
    }

    ob_ambientOcclusionF0 = vec4(AmbientOcclusion_0, 0.0, 0.0, F0_0);
    ob_normalRoughness = vec4(Normal_0.xyz * 0.5 + 0.5, Roughness_0);
    ob_colorMetallic = vec4(Color_0.rgb, Metallic_0);
}
