#version 300 es
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
out vec2 fragUV;
uniform mat4 uLightVP;
uniform mat4 uModel;
uniform vec4 uUVRect;
void main() {
    fragUV = mix(uUVRect.xy, uUVRect.zw, inUV);
    gl_Position = uLightVP * uModel * vec4(inPosition, 1.0);
}
