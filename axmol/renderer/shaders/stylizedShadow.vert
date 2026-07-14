#version 310 es

#include "base.glsl"

layout(location = POSITION) in vec3 a_position;
layout(location = TEXCOORD0) in vec2 a_texCoord;
layout(location = TEXCOORD1) in mat4 a_instance;

layout(location = TEXCOORD0) out vec2 v_texCoord;

layout(std140, set = UNIFORM_SET, binding = VS_UB_BINDING) uniform vs_ub {
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

void main()
{
    v_texCoord = transformTexCoord(a_texCoord);
    gl_Position = u_MVPMatrix * a_instance * vec4(a_position, 1.0);
}
