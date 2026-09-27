#version 300 es
layout(location = 0) in vec3 inPosition;
uniform mat4 uLightVP;
uniform mat4 uModel;
void main() {
    gl_Position = uLightVP * uModel * vec4(inPosition, 1.0);
}
