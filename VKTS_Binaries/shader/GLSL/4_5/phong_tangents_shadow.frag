#version 450 core

#define BIAS 0.001

#define NEIGHBOURS 1.0
#define RANGE (NEIGHBOURS * 2.0 + 1.0)
#define SAMPLES (RANGE * RANGE)

layout (binding = 2, std140) uniform _u_bufferFrag {
	vec3 light;
} u_bufferFrag;

layout (binding = 7) uniform sampler2D u_displaceTexture;
layout (binding = 8) uniform sampler2D u_emissiveTexture;
layout (binding = 9) uniform sampler2D u_alphaTexture;
layout (binding = 10) uniform sampler2D u_normalTexture;
layout (binding = 11) uniform sampler2D u_ambientTexture;
layout (binding = 12) uniform sampler2D u_diffuseTexture;
layout (binding = 13) uniform sampler2D u_specularTexture;
layout (binding = 14) uniform sampler2D u_specularShininessTexture;
layout (binding = 15) uniform sampler2D u_mirrorTexture;
layout (binding = 16) uniform sampler2D u_mirrorReflectivityTexture;

layout (binding = 4) uniform sampler2D u_shadowTexture;

layout (location = 0) in vec3 v_eye;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec3 v_bitangent;
layout (location = 3) in vec3 v_tangent;
layout (location = 4) in vec2 v_texCoord;
layout (location = 5) in vec4 v_projCoord;

layout (location = 0) out vec4 ob_fragColor;

float pcf()
{
    float sampleCount = 0.0;
    
    vec2 dimension = vec2(textureSize(u_shadowTexture, 0));
    
    float sStep = 1.0 / dimension.x;
    float tStep = 1.0 / dimension.y;
     
    for (float t = -NEIGHBOURS; t <= NEIGHBOURS; t += 1.0)
    {
        for (float s = -NEIGHBOURS; s <= NEIGHBOURS; s += 1.0)
        {
            if (v_projCoord.z <= texture(u_shadowTexture, v_projCoord.xy + vec2(sStep * s, tStep * t)).r + BIAS)
            {
                sampleCount += 1.0;
            }
        }
    }
        
    return sampleCount / SAMPLES;
}

void main(void)
{
	// Ambient and emissive color.
	vec4 color = texture(u_ambientTexture, v_texCoord) + texture(u_emissiveTexture, v_texCoord);
	
    mat3 objectToWorldMatrix = mat3(normalize(v_tangent), normalize(v_bitangent), normalize(v_normal));
    
    vec3 normal = objectToWorldMatrix * normalize((texture(u_normalTexture, v_texCoord) * 2.0 - 1.0).xyz);
	
	float nDotL = max(dot(u_bufferFrag.light, normal), 0.0);
	
	if (nDotL > 0.0)
	{
		// Diffuse color.
		color += texture(u_diffuseTexture, v_texCoord) * nDotL;
		
		vec3 eye = normalize(v_eye);
	
		// Incident vector is opposite light direction vector.
		vec3 reflection = reflect(-u_bufferFrag.light, normal);
		
		float eDotR = max(dot(eye, reflection), 0.0);
		
		if (eDotR > 0.0)
		{
			// Shininess is stored as 1.0/128.0.
			float shininess = texture(u_specularShininessTexture, v_texCoord).r * 128.0;
		
			// Specular color.
			color += texture(u_specularTexture, v_texCoord) * pow(eDotR, shininess);
		}
		
		color.rgb *= pcf(); 
	}
    else
    {
        color = vec4(0.0, 0.0, 0.0, 1.0);
    }
    
    color.a = texture(u_alphaTexture, v_texCoord).r;
	
	ob_fragColor = color;
}