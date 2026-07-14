#version 310 es
precision highp float;
precision highp int;

#include "base.glsl"

#ifdef STYLIZED_ALPHA_CUTOUT
layout(location = TEXCOORD0) in vec2 v_texCoord;

layout(set = SAMPLER_SET, binding = 0) uniform sampler2D u_tex0;

layout(std140, set = UNIFORM_SET, binding = FS_UB_BINDING) uniform fs_ub {
    vec4 u_stylizedMaterial[7];
    vec4 u_color;
};
#endif

void main()
{
#ifdef STYLIZED_ALPHA_CUTOUT
    float alpha = texture(u_tex0, v_texCoord).a * u_stylizedMaterial[0].a * u_color.a;
    if (alpha < u_stylizedMaterial[6].y)
        discard;
#endif
}
