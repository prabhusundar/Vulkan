#version 450 core

#define VKTS_MAX_LIGHTS 16

#define VKTS_PI 3.14159265

#define VKTS_FO_LINEAR_RANGE 0.08

#define VKTS_ONE_OVER_PI (1.0 / VKTS_PI)

#define VKTS_NORMAL_VALID_BIAS 0.1

layout (binding = 7, std140) uniform _u_bufferLights {
        vec4 L[VKTS_MAX_LIGHTS];
        vec4 color[VKTS_MAX_LIGHTS];
        int count;
} u_bufferLights;

layout (binding = 8, std140) uniform _u_bufferMatrices {
        mat4 inverseProjectionMatrix;
        mat4 inverseViewMatrix;
} u_bufferMatrices;

layout (binding = 6) uniform sampler2D u_lut;
layout (binding = 5) uniform samplerCube u_specularCubemap;
layout (binding = 4) uniform samplerCube u_diffuseCubemap;

layout (binding = 3) uniform sampler2D u_depth;                 // Depth.
layout (binding = 2) uniform sampler2D u_ambientOcclusionF0;    // Ambient occlusion and F0. GB channel not used.
layout (binding = 1) uniform sampler2D u_normalRoughness;       // Normal and roughness.
layout (binding = 0) uniform sampler2D u_colorMetallic;         // Color and metallic.

layout (location = 0) in vec2 v_texCoord;

layout (location = 0) out vec4 ob_fragColor;

float pow_5(float x)
{
    float x2 = x * x;
    float x4 = x2 * x2;
    
    return x * x4;
}

//

float ndfTrowbridgeReitzGGX(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    
    float alpha2 = alpha * alpha;
    
    float divisor = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
        
    return alpha2 / (VKTS_PI * divisor * divisor); 
}

//

float fresnel(float NdotV, float F0)
{
    return F0 + (1.0 - F0) * pow_5(1.0 - NdotV);
}

//

float geometricShadowingSchlick(float NdotV, float k)
{
    return NdotV / (NdotV * (1.0f - k) + k);
}

float geometricShadowingSmithSchlickGGX(float NdotL, float NdotV, float roughness)
{
    float k = roughness * roughness * 0.5f;

    return geometricShadowingSchlick(NdotL, k) * geometricShadowingSchlick(NdotV, k);
}

//

vec3 lambert(vec3 L, vec3 lightColor, vec3 N, vec3 baseColor)
{
    return lightColor * baseColor * max(dot(L, N), 0.0) * VKTS_ONE_OVER_PI;
}

vec3 iblLambert(vec3 N, vec3 baseColor)
{
    return baseColor * texture(u_diffuseCubemap, N).rgb;
}

vec3 cookTorrance(vec3 L, vec3 lightColor, vec3 N, vec3 V, float roughness, float F0)
{
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    
    // Lighted and visible
    if (NdotL >= 0.0 && NdotV >= 0.0)
    {
        vec3 H = normalize(L + V);
        
        float NdotH = dot(N, H);
        float VdotH = dot(V, H);
        
        float D = ndfTrowbridgeReitzGGX(NdotH, roughness);
        float F = fresnel(VdotH, F0);
        float G = geometricShadowingSmithSchlickGGX(NdotL, NdotV, roughness);
        
        float f =  D * F * G / (4.0 * NdotL * NdotV);
        
        return f * lightColor;
    }
    
    return vec3(0.0, 0.0, 0.0);
}

vec3 iblCookTorrance(vec3 N, vec3 V, float roughness, vec3 baseColor, float F0)
{
    // Note: reflect takes incident vector.
    // Note: Use N instead of H for approximation.
    vec3 L = reflect(-V, N);
    
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    
    // Lighted and visible
    if (NdotL > 0.0 && NdotV >= 0.0)
    {
        int levels = textureQueryLevels(u_specularCubemap); 
    
        float scaledRoughness = roughness * float(levels);
        
        float rLow = floor(scaledRoughness);
        float rHigh = ceil(scaledRoughness);    
        float rFraction = scaledRoughness - rLow;
        
        vec3 prefilteredColor = mix(textureLod(u_specularCubemap, L, rLow).rgb, textureLod(u_specularCubemap, L, rHigh).rgb, rFraction);

        vec2 envBRDF = texture(u_lut, vec2(NdotV, roughness)).rg;
        
        return baseColor * prefilteredColor * (F0 * envBRDF.x + envBRDF.y);
    }
    
    return vec3(0.0, 0.0, 0.0);
}

void main(void)
{
    float depth = texture(u_depth, v_texCoord).r;
    
    if (depth == 1.0)
    {
        discard;
    }
    
    //
    
    vec3 color = vec3(0.0, 0.0, 0.0);
    
    vec3 normal = texture(u_normalRoughness, v_texCoord).rgb * 2.0 - 1.0;
    float normalLength = length(normal);
    
    if (normalLength > VKTS_NORMAL_VALID_BIAS)
    {
        vec3 N = mat3(u_bufferMatrices.inverseViewMatrix) * normalize(normal);

        vec4 vertex = u_bufferMatrices.inverseProjectionMatrix * vec4(v_texCoord * 2.0 - 1.0, depth, 1.0);
        
        vec3 V = mat3(u_bufferMatrices.inverseViewMatrix) * -normalize(vertex.xyz);


        float NdotV = dot(N, V);
        
        if (NdotV < 0.0)
        {
            N = -N;
            
            NdotV = dot(N, V);
        }
        
        
        float roughness = texture(u_normalRoughness, v_texCoord).a;

        float metallic = texture(u_colorMetallic, v_texCoord).a;
        
        vec3 baseColor = texture(u_colorMetallic, v_texCoord).rgb;
        
        float ambientOcclusion = texture(u_ambientOcclusionF0, v_texCoord).r;
        
        float F0 = texture(u_ambientOcclusionF0, v_texCoord).a;
        
        //
        // Dielectric
        //
        
        vec3 colorLambert = vec3(0.0, 0.0, 0.0);

        if (metallic < 1.0)
        {
            // FIXME: Use for roughness > 0.0 Oren-Nayar.
            colorLambert = iblLambert(N, baseColor) * ambientOcclusion;

            //
            
            vec3 colorReflective = iblCookTorrance(N, V, roughness, vec3(1.0, 1.0, 1.0), F0 * VKTS_FO_LINEAR_RANGE);
            
            //

            colorLambert = mix(colorLambert, colorReflective, fresnel(NdotV, F0 * VKTS_FO_LINEAR_RANGE));
        }

        //
        // Metallic
        //
        
        vec3 colorCookTorrance = vec3(0.0, 0.0, 0.0);

        if (metallic > 0.0)
        {
            colorCookTorrance = iblCookTorrance(N, V, roughness, baseColor, F0);
        }
        
        // Dynamic lights.
        
        vec3 dynamicLight = vec3(0.0, 0.0, 0.0);
        
        // Bring vertex to world space.
        vertex = u_bufferMatrices.inverseViewMatrix * vertex; 
        
        for (int i = 0; i < min(u_bufferLights.count, VKTS_MAX_LIGHTS); i++)
        {
            vec3 light;
            
            if (u_bufferLights.L[i].w > 0.0)
            {
                light = normalize((u_bufferLights.L[i] - vertex).xyz);
            }
            else
            {
                light = u_bufferLights.L[i].xyz;
            }
        
            if (metallic < 1.0)
            {
                dynamicLight += lambert(light, u_bufferLights.color[i].xyz, N, baseColor) * (1.0 - metallic);
                
                dynamicLight += cookTorrance(light, u_bufferLights.color[i].xyz, N, V, roughness, F0 * VKTS_FO_LINEAR_RANGE);
            }

            if (metallic > 0.0)
            {
                dynamicLight += cookTorrance(light, u_bufferLights.color[i].xyz, N, V, roughness, F0);
            }
        }                
        
        //
        
        color = mix(colorLambert, colorCookTorrance, metallic);
        
        color += dynamicLight;
    }
    else
    {
        discard;
    }
    
    //

	ob_fragColor = vec4(color, 1.0);
}
