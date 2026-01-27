#version 450

// SModel v1 vertex layout (VertexPNTT):
// location 0: vec3 position
// location 1: vec3 normal
// location 2: vec2 uv0
// location 3: vec4 tangent
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV0;
layout(location = 3) in vec4 inTangent;

// Per-instance world matrix (mat4 consumes 4 locations)
layout(location = 4) in vec4 inInstanceCol0;
layout(location = 5) in vec4 inInstanceCol1;
layout(location = 6) in vec4 inInstanceCol2;
layout(location = 7) in vec4 inInstanceCol3;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

// Flattened node globals: [instance][node]
layout(set = 0, binding = 1, std430) readonly buffer NodePalette
{
    mat4 nodeGlobals[];
} palette;

layout(push_constant) uniform PushConstants
{
    mat4 model;
    vec4 baseColorFactor;
    vec4 materialParams;
    uvec4 nodeInfo; // x=nodeIndex, y=nodeCount
} pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV0;

void main()
{
    mat4 instanceWorld = mat4(inInstanceCol0, inInstanceCol1, inInstanceCol2, inInstanceCol3);
    uint instanceIndex = uint(gl_InstanceIndex);
    uint nodeIndex = pc.nodeInfo.x;
    uint nodeCount = max(pc.nodeInfo.y, 1u);
    mat4 nodeM = palette.nodeGlobals[instanceIndex * nodeCount + nodeIndex];
    mat4 M = instanceWorld * pc.model * nodeM;

    vec4 worldPos = M * vec4(inPosition, 1.0);
    mat3 normalMat = mat3(transpose(inverse(M)));
    vNormal = normalize(normalMat * inNormal);
    vUV0 = inUV0;
    gl_Position = cam.proj * cam.view * worldPos;
}
