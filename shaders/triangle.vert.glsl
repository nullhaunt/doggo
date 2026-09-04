#version 460

const vec2 positions[3] = vec2[](
vec2(0.0, 0.7),
vec2(-0.7, -0.7),
vec2(0.7, -0.7)
);

const vec3 colors[3] = vec3[](
vec3(1.0, 0.2, 0.2),
vec3(0.2, 1.0, 0.2),
vec3(0.2, 0.4, 1.0)
);

layout (location = 0) out vec3 outColor;

void main()
{
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    outColor = colors[gl_VertexID];
}