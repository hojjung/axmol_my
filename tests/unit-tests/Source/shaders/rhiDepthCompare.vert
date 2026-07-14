#version 310 es

#include "base.glsl"

layout(location = POSITION) in vec3 a_position;
layout(location = TEXCOORD0) out vec2 v_texCoord;

void main()
{
    gl_Position = vec4(a_position, 1.0);
    v_texCoord = a_position.xy * 0.5 + 0.5;
}
