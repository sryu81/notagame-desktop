#version 300 es
precision mediump float;
in vec2 fragUV;
uniform vec4 uColor;
uniform sampler2D uTexture;
uniform int uHasTexture;
layout(std140) uniform FrameData {
    highp mat4 uViewProjection;
    highp vec4 uLightDirAmbient;
};
out vec4 outColor;
void main() {
    vec4 tex = uHasTexture != 0 ? texture(uTexture, fragUV) : vec4(1.0);
    outColor = vec4(uColor.rgb * tex.rgb, uColor.a * tex.a);
}
