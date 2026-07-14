#version 310 es

#include "base.glsl"

layout(location = POSITION) in vec3 a_position;
layout(location = BLENDWEIGHT) in vec4 a_blendWeight;
layout(location = BLENDINDICES) in vec4 a_blendIndex;
layout(location = TEXCOORD0) in vec2 a_texCoord;

layout(location = TEXCOORD0) out vec2 v_texCoord;

#define SKINNING_JOINT_COUNT 60

layout(std140, set = UNIFORM_SET, binding = VS_UB_BINDING) uniform vs_ub {
    vec4 u_matrixPalette[SKINNING_JOINT_COUNT * 3];
    mat4 u_MVPMatrix;
    vec4 u_stylizedUvTransform[2];
};

vec2 transformTexCoord(vec2 texCoord)
{
    // Alpha-cutout shadow sampling must use the same glTF/KTX2 origin as beauty.
    vec2 scaled = texCoord * u_stylizedUvTransform[0].zw;
    vec2 rotated = vec2(u_stylizedUvTransform[1].x * scaled.x - u_stylizedUvTransform[1].y * scaled.y,
                        u_stylizedUvTransform[1].y * scaled.x + u_stylizedUvTransform[1].x * scaled.y);
    vec2 transformed = u_stylizedUvTransform[0].xy + rotated;
    return transformed;
}

vec4 getSkinnedPosition()
{
    int matrixIndex = int(a_blendIndex[0]) * 3;
    vec4 matrixRow0 = u_matrixPalette[matrixIndex] * a_blendWeight[0];
    vec4 matrixRow1 = u_matrixPalette[matrixIndex + 1] * a_blendWeight[0];
    vec4 matrixRow2 = u_matrixPalette[matrixIndex + 2] * a_blendWeight[0];

    for (int influence = 1; influence < 4; ++influence)
    {
        float weight = a_blendWeight[influence];
        if (weight > 0.0)
        {
            matrixIndex = int(a_blendIndex[influence]) * 3;
            matrixRow0 += u_matrixPalette[matrixIndex] * weight;
            matrixRow1 += u_matrixPalette[matrixIndex + 1] * weight;
            matrixRow2 += u_matrixPalette[matrixIndex + 2] * weight;
        }
    }

    vec4 sourcePosition = vec4(a_position, 1.0);
    return vec4(dot(sourcePosition, matrixRow0),
                dot(sourcePosition, matrixRow1),
                dot(sourcePosition, matrixRow2),
                1.0);
}

void main()
{
    v_texCoord = transformTexCoord(a_texCoord);
    gl_Position = u_MVPMatrix * getSkinnedPosition();
}
