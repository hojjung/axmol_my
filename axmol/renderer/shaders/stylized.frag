#version 310 es
precision highp float;
precision highp int;
precision highp sampler2DShadow;

#include "base.glsl"

layout(location = TEXCOORD0) in vec2 v_texCoord;
layout(location = TEXCOORD1) in vec3 v_worldPosition;
layout(location = TEXCOORD2) in vec4 v_shadowCoord0;
layout(location = TEXCOORD3) in vec4 v_shadowCoord1;
layout(location = NORMAL) in vec3 v_worldNormal;

layout(set = SAMPLER_SET, binding = 0) uniform sampler2D u_tex0;
layout(set = SAMPLER_SET, binding = 1, sampler_slot = ShadowCmpClamp) uniform sampler2DShadow u_mainShadowMap;

layout(std140, set = UNIFORM_SET, binding = FS_UB_BINDING) uniform fs_ub {
    vec4 u_stylizedMaterial[7];
    vec4 u_stylizedLightData[9];
    vec2 u_shadowTexelSize;
    float u_shadowBias;
    float u_shadowEnabled;
    vec4 u_shadowParams;
    vec4 u_color;
};

vec3 surfaceNormal()
{
    vec3 normal = normalize(v_worldNormal);
    return gl_FrontFacing ? normal : -normal;
}

float sampleMainShadow()
{
    if (u_shadowEnabled < 0.5)
        return 1.0;

    float cameraDepth = dot(v_worldPosition - u_stylizedLightData[3].xyz,
                            u_stylizedLightData[4].xyz);
    int cascadeIndex = u_shadowParams.x > 1.5 && cameraDepth > u_shadowParams.y ? 1 : 0;
    vec4 projectedCoord = cascadeIndex == 0 ? v_shadowCoord0 : v_shadowCoord1;
    if (projectedCoord.w <= 0.0)
        return 1.0;

    vec3 shadowCoord = projectedCoord.xyz / projectedCoord.w;
    float inverseCascadeCount = 1.0 / max(u_shadowParams.x, 1.0);
    float atlasMinimumX = float(cascadeIndex) * inverseCascadeCount;
    float atlasMaximumX = atlasMinimumX + inverseCascadeCount;
    if (shadowCoord.x < atlasMinimumX || shadowCoord.x > atlasMaximumX ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 || shadowCoord.z < 0.0 || shadowCoord.z > 1.0)
        return 1.0;

    vec3 receiverNormal = surfaceNormal();
    vec3 receiverLightDirection = normalize(-u_stylizedLightData[0].xyz);
    float receiverFacing = max(dot(receiverNormal, receiverLightDirection), 0.0);
    float referenceDepth = shadowCoord.z - u_shadowBias -
                           u_shadowParams.w * (1.0 - receiverFacing) * u_shadowTexelSize.y;
    vec2 atlasMinimum = vec2(atlasMinimumX, 0.0) + u_shadowTexelSize * 0.5;
    vec2 atlasMaximum = vec2(atlasMaximumX, 1.0) - u_shadowTexelSize * 0.5;
    float visibility = 0.0;
    if (u_shadowParams.z > 4.5)
    {
        visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + u_shadowTexelSize * vec2(-1.0, -1.0), atlasMinimum, atlasMaximum), referenceDepth));
        visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + u_shadowTexelSize * vec2( 0.0, -1.0), atlasMinimum, atlasMaximum), referenceDepth));
        visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + u_shadowTexelSize * vec2( 1.0, -1.0), atlasMinimum, atlasMaximum), referenceDepth));
        visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + u_shadowTexelSize * vec2(-1.0,  0.0), atlasMinimum, atlasMaximum), referenceDepth));
        visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy, atlasMinimum, atlasMaximum), referenceDepth));
        visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + u_shadowTexelSize * vec2( 1.0,  0.0), atlasMinimum, atlasMaximum), referenceDepth));
        visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + u_shadowTexelSize * vec2(-1.0,  1.0), atlasMinimum, atlasMaximum), referenceDepth));
        visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + u_shadowTexelSize * vec2( 0.0,  1.0), atlasMinimum, atlasMaximum), referenceDepth));
        visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + u_shadowTexelSize * vec2( 1.0,  1.0), atlasMinimum, atlasMaximum), referenceDepth));
        return visibility / 9.0;
    }

    vec2 halfTexel = u_shadowTexelSize * 0.5;
    visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + vec2(-halfTexel.x, -halfTexel.y), atlasMinimum, atlasMaximum), referenceDepth));
    visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + vec2( halfTexel.x, -halfTexel.y), atlasMinimum, atlasMaximum), referenceDepth));
    visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + vec2(-halfTexel.x,  halfTexel.y), atlasMinimum, atlasMaximum), referenceDepth));
    visibility += texture(u_mainShadowMap, vec3(clamp(shadowCoord.xy + vec2( halfTexel.x,  halfTexel.y), atlasMinimum, atlasMaximum), referenceDepth));
    return visibility * 0.25;
}

float pointAttenuation(vec3 toLight, float inverseRange)
{
    float normalizedDistanceSquared = dot(toLight, toLight) * inverseRange * inverseRange;
    float attenuation = max(1.0 - normalizedDistanceSquared, 0.0);
    return attenuation * attenuation;
}

vec3 linearToSrgb(vec3 linearColor)
{
    // Axmol swapchains and the stylized intermediate are UNORM targets. Keep
    // debug masks raw and encode only the final linear-lit beauty color.
    vec3 color = max(linearColor, vec3(0.0));
    vec3 low   = color * 12.92;
    vec3 high  = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), color));
}

layout(location = SV_Target0) out vec4 FragColor;

void main()
{
    vec4 albedo = texture(u_tex0, v_texCoord) * u_stylizedMaterial[0] * u_color;

#ifdef STYLIZED_ALPHA_CUTOUT
    if (albedo.a < u_stylizedMaterial[6].y)
        discard;
#endif

    vec3 normal = surfaceNormal();
    vec3 mainLightDirection = normalize(-u_stylizedLightData[0].xyz);
    vec3 mainLightColor = u_stylizedLightData[1].xyz;
    float mainLightEnabled = u_stylizedLightData[0].w;

    float signedNdotL = clamp(dot(normal, mainLightDirection), -1.0, 1.0);
    float lightFacing = max(signedNdotL, 0.0) * mainLightEnabled;
    float halfLambert = signedNdotL * 0.5 + 0.5;
    float bandSoftness = max(max(u_stylizedMaterial[5].y, fwidth(halfLambert)), 0.0001);
    float toonBand = smoothstep(u_stylizedMaterial[5].x - bandSoftness,
                                u_stylizedMaterial[5].x + bandSoftness,
                                halfLambert);
    float shadowVisibility = sampleMainShadow();
    float litBand = toonBand * shadowVisibility * mainLightEnabled;

    vec3 highlight = u_stylizedMaterial[1].rgb * mainLightColor;
    vec3 directTint = mix(u_stylizedMaterial[2].rgb, highlight, litBand);
    vec3 pointDiffuse = vec3(0.0);

    for (int pointIndex = 0; pointIndex < 2; ++pointIndex)
    {
        int dataIndex = 5 + pointIndex * 2;
        vec3 toLight = u_stylizedLightData[dataIndex].xyz - v_worldPosition;
        float inverseRange = u_stylizedLightData[dataIndex].w;
        float distanceSquared = dot(toLight, toLight);
        if (distanceSquared > 0.0 && inverseRange > 0.0)
        {
            float pointNdotL = max(dot(normal, toLight * inversesqrt(distanceSquared)), 0.0);
            pointDiffuse += u_stylizedLightData[dataIndex + 1].rgb *
                            pointNdotL * pointAttenuation(toLight, inverseRange);
        }
    }

    vec3 viewDirection = normalize(u_stylizedLightData[3].xyz - v_worldPosition);
    float fresnel = 1.0 - max(dot(normal, viewDirection), 0.0);
    float rim = smoothstep(u_stylizedMaterial[5].z, u_stylizedMaterial[5].w, fresnel);
    float mainLuminance = dot(mainLightColor, vec3(0.2126, 0.7152, 0.0722));
    rim *= lightFacing * shadowVisibility * mainLuminance * u_stylizedMaterial[6].x;

    int debugView = int(u_stylizedMaterial[6].z + 0.5);
    if (debugView == 1)
    {
        FragColor = vec4(vec3(toonBand), 1.0);
        return;
    }
    if (debugView == 2)
    {
        FragColor = vec4(vec3(shadowVisibility), 1.0);
        return;
    }
    if (debugView == 3)
    {
        FragColor = vec4(vec3(rim), 1.0);
        return;
    }

    vec3 lighting = directTint + u_stylizedLightData[2].rgb + pointDiffuse;
    vec3 color = albedo.rgb * u_stylizedMaterial[3].rgb * lighting;
    color += u_stylizedMaterial[4].rgb * rim;
    FragColor = vec4(linearToSrgb(color), albedo.a);
}
