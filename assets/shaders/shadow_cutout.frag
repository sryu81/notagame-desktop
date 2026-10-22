#version 300 es
precision mediump float;
in vec2 fragUV;
uniform sampler2D uTexture;
void main() {
    if (texture(uTexture, fragUV).a < 0.5) discard;
}
