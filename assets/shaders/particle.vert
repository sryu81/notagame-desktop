#version 300 es
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
out vec4 fragColor;
layout(std140) uniform FrameData {
    mat4 uViewProjection;
    vec4 uLightDirAmbient;
};
void main() {
    fragColor = inColor;
    gl_Position = uViewProjection * vec4(inPosition, 1.0);
}
