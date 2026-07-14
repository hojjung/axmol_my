#version 310 es

#include "base.glsl"

layout(location = POSITION) in vec3 a_position;
layout(location = TEXCOORD0) in vec2 a_texCoord;
layout(location = NORMAL) in vec3 a_normal;
layout(location = TEXCOORD1) in mat4 a_instance;

layout(location = TEXCOORD0) out vec2 v_texCoord;
layout(location = TEXCOORD1) out vec3 v_worldPosition;
layout(location = TEXCOORD2) out vec4 v_shadowCoord0;
layout(location = TEXCOORD3) out vec4 v_shadowCoord1;
layout(location = NORMAL) out vec3 v_worldNormal;

layout(std140, set = UNIFORM_SET, binding = VS_UB_BINDING) uniform vs_ub {
    mat4 u_MVPMatrix;
    mat4 u_MVMatrix;
    mat3 u_NormalMatrix;
    mat4 u_mainShadowMatrix[2];
    vec4 u_stylizedUvTransform[2];
};

vec2 transformTexCoord(vec2 texCoord)
{
    // glTF and KTX2 define (0, 0) at the logical upper-left. Texture upload
    // preserves row order, so applying the legacy Axmol 3D V flip is incorrect.
    vec2 scaled = texCoord * u_stylizedUvTransform[0].zw;
    vec2 rotated = vec2(u_stylizedUvTransform[1].x * scaled.x - u_stylizedUvTransform[1].y * scaled.y,
                        u_stylizedUvTransform[1].y * scaled.x + u_stylizedUvTransform[1].x * scaled.y);
    vec2 transformed = u_stylizedUvTransform[0].xy + rotated;
    return transformed;
}

vec3 transformInstanceNormal(vec3 normal)
{
    vec3 column0 = a_instance[0].xyz;
    vec3 column1 = a_instance[1].xyz;
    vec3 column2 = a_instance[2].xyz;
    vec3 cofactor0 = cross(column1, column2);
    vec3 cofactor1 = cross(column2, column0);
    vec3 cofactor2 = cross(column0, column1);
    float orientation = dot(column0, cofactor0) < 0.0 ? -1.0 : 1.0;
    return orientation * mat3(cofactor0, cofactor1, cofactor2) * normal;
}

void main()
{
    vec4 instancePosition = a_instance * vec4(a_position, 1.0);
    vec4 worldPosition = u_MVMatrix * instancePosition;
    v_texCoord = transformTexCoord(a_texCoord);
    v_worldPosition = worldPosition.xyz;
    v_worldNormal = normalize(u_NormalMatrix * transformInstanceNormal(a_normal));
    v_shadowCoord0 = u_mainShadowMatrix[0] * worldPosition;
    v_shadowCoord1 = u_mainShadowMatrix[1] * worldPosition;
    gl_Position = u_MVPMatrix * instancePosition;
}
