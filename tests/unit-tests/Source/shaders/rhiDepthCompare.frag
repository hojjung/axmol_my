#version 310 es
precision highp float;
precision highp sampler2DShadow;

#include "base.glsl"

layout(location = TEXCOORD0) in vec2 v_texCoord;
layout(set = SAMPLER_SET, binding = 0, sampler_slot = ShadowCmpClamp) uniform sampler2DShadow u_depth;

layout(location = SV_Target0) out vec4 FragColor;

void main()
{
    vec2 shadowCoord = vec2(0.5, v_texCoord.x < 0.5 ? 0.75 : 0.25);
    shadowCoord.y = TEXCOORD_Y(shadowCoord);
    float visibility = texture(u_depth, vec3(shadowCoord, 0.9));
    FragColor = vec4(vec3(visibility), 1.0);
}
