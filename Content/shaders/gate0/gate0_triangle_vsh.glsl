#version 460

const vec4 Positions[3] = vec4[](
vec4(0.0, 0.75, 0.0, 1.0),
vec4(-0.75, -0.75, 0.0, 1.0),
vec4(0.75, -0.75, 0.0, 1.0)
);

const vec4 Colors[3] = vec4[](
vec4(1.0, 0.2, 0.2, 1.0),
vec4(0.2, 1.0, 0.2, 1.0),
vec4(0.2, 0.4, 1.0, 1.0)
);

layout (location = 0) out vec4 outColor;

void main()
{
    gl_Position = Positions[gl_VertexID];
    outColor = Colors[gl_VertexID];
}
