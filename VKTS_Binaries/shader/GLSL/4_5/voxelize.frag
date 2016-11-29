#version 450 core

// As 1 byte for the counter.
#define MAX_VALUES 256.0

layout (r32ui, binding = 15) uniform coherent volatile uimage3D u_voxelGrid[3];

layout (binding = 8) uniform sampler2D u_emissiveTexture;
layout (binding = 10) uniform sampler2D u_normalTexture;
layout (binding = 12) uniform sampler2D u_diffuseTexture;

layout (location = 0) in vec4 v_aabb;
layout (location = 1) in flat float v_orientation;
layout (location = 2) in vec3 v_normal;
layout (location = 3) in vec3 v_bitangent;
layout (location = 4) in vec3 v_tangent;
layout (location = 5) in vec2 v_texCoord;

// see "Octree-Based Sparse Voxelization Using the GPU Hardware Rasterizer" in OpenGL Insights
void imageAtomicAverage(ivec3 pos, vec4 addingValue, int index, bool isNormal)
{
    // New value to store in the image.
    uint newValue = packUnorm4x8(addingValue);
    // Expected value, that data can be stored in the image.
    uint expectedValue = 0;
    // Actual data in the image.
    uint actualValue;

    // Average/sum of the voxel.
    vec4 tempValue; 
    
    // Try to store ...
    actualValue = imageAtomicCompSwap(u_voxelGrid[index], pos, expectedValue, newValue);
    
    while (actualValue != expectedValue)
    {
        // ... but did not work out, as the expected value did not match the actual value.
        expectedValue = actualValue; 
    
    
        // Unpack the current average ...
        tempValue = unpackUnorm4x8(actualValue);
        // ... and convert back to the sum.
        tempValue.a *= MAX_VALUES;
        tempValue.rgb *= tempValue.a;
        
        if (isNormal)
        {
            tempValue.rgb = tempValue.rgb * 2.0 - 1.0;
            
            tempValue.rgb = normalize(tempValue.rgb + addingValue.rgb * 2.0 - 1.0);

            tempValue.rgb = tempValue.rgb * 0.5 + 0.5;
        }
        else
        {        
            // Add the current color ...        
            tempValue.rgb += addingValue.rgb;
        }
        tempValue.a += 1.0;
        
        // ... and normalize back.
        tempValue.rgb /= tempValue.a;
        tempValue.a /= MAX_VALUES;
     
     
        // Pack and ...
        newValue = packUnorm4x8(tempValue);
        
        
        // ... try to store again ...
        actualValue = imageAtomicCompSwap(u_voxelGrid[index], pos, expectedValue, newValue);
    }
}

// see "Object-Order Ray Tracing for Fully Dynamic Scenes" in GPU Pro 5
void main(void)
{
    ivec3 gridSize = imageSize(u_voxelGrid[0]);

    // Get xy-NDC of fragment.
    vec2 pos = (gl_FragCoord.xy / vec2(gridSize.xy)) * 2.0 - 1.0;

	// Discard pixels, which are not inside the AABB.
	if (pos.x < v_aabb.x || pos.y < v_aabb.y || pos.x > v_aabb.z || pos.y > v_aabb.w)
	{
		discard;
	}
	
	//
		
	ivec3 gridPosition;
	
    // Sample point location addresses the correct voxel grid entry ...
	gridPosition.xy = ivec2(gl_FragCoord.xy);
	
	// ... but has to be adjusted for z, as it is in the range of 0 and 1. 
    float depth = float(gridSize.z - 1);
	gridPosition.z = int(depth * gl_FragCoord.z);

	// Conservative depth rasterization preparation: Get half range, how much depth pixels are affected.    
    float halfDepthRange = abs(fwidth(gl_FragCoord.z) * 0.5);
	
	// Because of conservative depth rasterization, possibly a range has to be updated in the voxel grid. 
	ivec3 gridPositionStart = ivec3(gridPosition.xy, int(depth * clamp(gl_FragCoord.z - halfDepthRange, 0.0, 1.0)));
	ivec3 gridPositionEnd = ivec3(gridPosition.xy, int(depth * clamp(gl_FragCoord.z + halfDepthRange, 0.0, 1.0)));
	
	ivec3 gridPositionStep = ivec3(0, 0, 1);
	
	int gridRange = gridPositionEnd.z - gridPositionStart.z + 1;  
	
	ivec3 temp;
	
	// Swizzle back.
	if (v_orientation == 1.0)
    {
        temp.y = gridSize.y - gridPositionStart.z;
        temp.z = gridPositionStart.y;
    
        gridPositionStart.yz = temp.yz; 
        
        gridPositionStep = ivec3(0, -1, 0); 
    }
    else if (v_orientation == 2.0)
    {
        temp.x = gridSize.x - gridPositionStart.z;
        temp.z = gridPositionStart.x;
    
        gridPositionStart.xz = temp.xz; 
        
        gridPositionStep = ivec3(-1, 0, 0);
    }
    
    //

    mat3 objectToWorldMatrix = mat3(normalize(v_tangent), normalize(v_bitangent), normalize(v_normal));
    
    vec3 normal = objectToWorldMatrix * normalize((texture(u_normalTexture, v_texCoord) * 2.0 - 1.0).xyz);
    
    //

    for (int i = 0; i < gridRange; i++)
    {
        // Note: Alpha channel contains the normalized counter.
	    imageAtomicAverage(gridPositionStart, vec4(texture(u_emissiveTexture, v_texCoord).rgb, 1.0 / MAX_VALUES), 0, false);
        imageAtomicAverage(gridPositionStart, vec4(texture(u_diffuseTexture, v_texCoord).rgb, 1.0 / MAX_VALUES), 1, false);
        
        imageAtomicAverage(gridPositionStart, vec4(normal * 0.5 + 0.5, 1.0 / MAX_VALUES), 2, true);
	    
        gridPositionStart += gridPositionStep; 
    }	
}
