#version 300 es
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
out vec2 fragUV;
uniform mat4 uModel;
layout(std140) uniform FrameData {
    mat4 uViewProjection;
    vec4 uLightDirAmbient;
};
void main() {
    fragUV = inUV;
    gl_Position = uViewProjection * uModel * vec4(inPosition, 1.0);
}
