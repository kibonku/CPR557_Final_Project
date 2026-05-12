#version 450

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec3 fragNormalWorld;
layout (location = 3) in vec2 fragUV;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUBO
{
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightPosition;
    vec4 lightColor;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D potTex;

layout(push_constant) uniform Pushdata
{
    mat4 modelMatrix;
    mat4 normalMatrix;
} pushdata;


// Procedural leaf: three-stop gradient + curved tapering veins + sub-veins + micro texture.
// fragUV.x = axial u (0=root, 1=tip)
// fragUV.y = lateral (0=left, 0.5=midrib, 1=right)
vec3 proceduralLeaf()
{
    float u  = fragUV.x;
    float vc = fragUV.y * 2.0 - 1.0; // -1..1, 0=midrib

    // Three-stop axial gradient
    vec3 rootCol = vec3(0.06, 0.22, 0.04);
    vec3 midCol  = vec3(0.20, 0.55, 0.09);
    vec3 tipCol  = vec3(0.52, 0.90, 0.24);
    vec3 col     = mix(rootCol, midCol, smoothstep(0.0, 0.35, u));
    col          = mix(col,     tipCol, smoothstep(0.35, 1.0,  u));

    // Inter-vein brightening: slight bulge between veins
    float bulge = pow(1.0 - abs(vc), 2.2);
    col = mix(col, col * 1.12, bulge * 0.4);

    // Midrib: prominent, tapers root->tip
    float midribW = mix(0.13, 0.022, pow(u, 0.65));
    float midrib  = 1.0 - smoothstep(0.0, midribW, abs(vc));
    col = mix(col, rootCol * 0.45, midrib * 0.82);

    // Main lateral veins: curved (quadratic bend) + tapering width
    for (int i = 1; i <= 7; i++)
    {
        float uOrigin = float(i) / 8.0;
        if (u < uOrigin) continue;

        float progress = u - uOrigin;
        float spread   = 1.9 + 0.28 * float(i);
        float curve    = 1.2;
        float vcLine   = spread * progress + curve * progress * progress;

        float veinW = max(0.007, 0.016 * (1.0 - progress * 0.65));
        float dist  = min(abs(vc - vcLine), abs(vc + vcLine));
        float vein  = 1.0 - smoothstep(0.0, veinW, dist);
        col = mix(col, rootCol * 0.55, vein * 0.65);
    }

    // Sub-veins: shorter, thinner, between main veins
    for (int i = 1; i <= 8; i++)
    {
        float uOrigin = (float(i) - 0.5) / 8.0;
        if (u < uOrigin) continue;

        float progress = u - uOrigin;
        if (progress > 0.22) continue;

        float spread = 3.8 + 0.15 * float(i);
        float vcLine = spread * progress;
        float dist   = min(abs(vc - vcLine), abs(vc + vcLine));
        float vein   = 1.0 - smoothstep(0.0, 0.007, dist);
        col = mix(col, rootCol * 0.68, vein * 0.38);
    }

    // Micro surface variation via high-freq sin
    float micro = sin(u * 55.0) * sin((vc + 1.0) * 28.0) * 0.018;
    col.g += micro;

    return clamp(col, 0.0, 1.0);
}


// Procedural stem: strong gradient + vertical ridges + horizontal nodes.
// fragUV.x = axial (0=base, 1=top), fragUV.y = circumferential (0→1)
vec3 proceduralStem()
{
    float u = fragUV.x;
    float v = fragUV.y;

    // Strong gradient: dark brownish base -> bright green top
    vec3 baseCol = vec3(0.08, 0.22, 0.04);
    vec3 midCol  = vec3(0.14, 0.42, 0.08);
    vec3 topCol  = vec3(0.24, 0.68, 0.14);
    vec3 col = mix(baseCol, midCol, smoothstep(0.0, 0.4, u));
    col      = mix(col,     topCol, smoothstep(0.4, 1.0, u));

    // Vertical stripes: 8 stripes around circumference
    float stripe = 0.5 + 0.5 * sin(v * 3.14159 * 16.0);
    col = mix(col * 0.72, col * 1.10, stripe);

    // Horizontal nodes: distinct darker bands at stem segments
    float node = abs(fract(u * 4.5) - 0.5);
    float nodeBand = 1.0 - smoothstep(0.05, 0.12, node);
    col = mix(col, baseCol * 0.85, nodeBand * 0.55);

    // Bright highlight strip on one side
    float highlight = pow(max(0.0, sin(v * 3.14159)), 8.0);
    col = mix(col, topCol * 1.3, highlight * 0.35);

    return clamp(col, 0.0, 1.0);
}


void main()
{
    vec3 directionToLight = ubo.lightPosition - fragPosWorld;
    float attenuation = 0.8;

    vec3 lightColor   = ubo.lightColor.xyz * ubo.lightColor.w * attenuation;
    vec3 ambientLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
    vec3 diffuseLight = lightColor * max(dot(normalize(fragNormalWorld), normalize(directionToLight)), 0.0);

    // useTexture: 0=vertex color, 1=pot texture, 2=procedural leaf, 3=procedural stem
    float useTexture = pushdata.normalMatrix[3][2];
    vec3 baseColor;
    if (useTexture < 0.5)
        baseColor = fragColor;
    else if (useTexture < 1.5)
        baseColor = texture(potTex, fragUV).rgb;
    else if (useTexture < 2.5)
        baseColor = proceduralLeaf();
    else
        baseColor = proceduralStem();

    outColor = vec4((diffuseLight + ambientLight) * baseColor, 1.0);

    if (outColor.r < 0.001) outColor.r = 0.001;
    if (outColor.g < 0.001) outColor.g = 0.001;
    if (outColor.b < 0.001) outColor.b = 0.001;
}
