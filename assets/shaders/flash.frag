#version 300 es
precision mediump float;
uniform vec3 uColor;
uniform float uIntensity;
out vec4 outColor;
void main() {
    outColor = vec4(uColor * uIntensity, uIntensity * 0.5);
}
