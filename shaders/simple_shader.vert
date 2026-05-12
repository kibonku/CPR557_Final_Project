#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;
layout(location = 3) out vec2 fragUV;

// Set and binding should match the descriptor set layout
layout(set = 0, binding = 0) uniform GlobalUBO  
{
    mat4 projectionViewMatrix;
    vec4 ambientLightColor; // w is intensity
    vec3 lightPosition;
    vec4 lightColor;
    vec2 windPos;           // Phase 5: GPU vertex deformation (scaled wind vector) > Wind direction
} ubo;

// Note 128 bytes can only contain 2 4x4 matrices, so we run into the limitation
layout(push_constant) uniform Pushdata
{
    mat4 modelMatrix;
    mat4 normalMatrix;
} pushdata;


void main()
{
    // deformScale: 1.0 for leaf nodes, 0.0 for pot/stem.
    // Stored in normalMatrix[3][3] — unused by mat3(normalMatrix) in the lighting calc.
    float deformScale = pushdata.normalMatrix[3][3];

    // t: normalized axial position along leaf (0=root, 1=tip)
    float t = clamp(position.x, 0.0, 1.0);

    // Leaf world-space axis (local X transformed to world).
    vec3 leafAxis  = normalize(vec3(pushdata.modelMatrix[0]));
    vec3 windForce = vec3(ubo.windPos.x, ubo.windPos.y, 0.0);

    // Remove axial component — lateral bend only, no axial stretch.
    vec3 bendForce = windForce - dot(windForce, leafAxis) * leafAxis;
    float bendMag  = length(bendForce);

    vec4 positionWorld = pushdata.modelMatrix * vec4(position, 1.0);
    vec3 normalWorld   = normalize(mat3(pushdata.normalMatrix) * normal);

    // Rotation-based deformation: rotate each vertex around the leaf root.
    // Preserves distance from root → no mesh stretching.
    // Guard: skip when wind is negligible or node is not a leaf.
    if (bendMag > 0.001 && deformScale > 0.5)
    {
        // Root of this leaf in world space (local origin).
        vec3 rootWorld = vec3(pushdata.modelMatrix * vec4(0.0, 0.0, 0.0, 1.0));
        vec3 relPos    = positionWorld.xyz - rootWorld;

        // Rotation axis ⊥ to both leafAxis and bendForce (safe: bendForce ⊥ leafAxis by construction).
        vec3 axis  = normalize(cross(leafAxis, bendForce));

        // Rotation angle scales linearly root→tip (cantilever beam approximation).
        float theta = bendMag * t;

        // Rodrigues' rotation formula
        float c = cos(theta);
        float s = sin(theta);
        vec3 rotated = relPos * c
                     + cross(axis, relPos) * s
                     + axis * dot(axis, relPos) * (1.0 - c);

        positionWorld = vec4(rootWorld + rotated, 1.0);

        // Rotate normal by the same rotation so lighting stays correct.
        normalWorld = normalize(normalWorld * c
                              + cross(axis, normalWorld) * s
                              + axis * dot(axis, normalWorld) * (1.0 - c));
    }

    gl_Position     = ubo.projectionViewMatrix * positionWorld;
    fragNormalWorld = normalWorld;
    fragPosWorld    = positionWorld.xyz;
    fragColor       = color;

    fragUV = uv;
}
