#version 300 es
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
out vec2 fragUV;
out highp vec4 fragShadowCoord;
uniform mat4 uModel;
uniform highp mat4 uLightVP;
uniform vec4 uUVRect;
layout(std140) uniform FrameData {
    mat4 uViewProjection;
    vec4 uLightDirAmbient;
};
void main() {
    fragUV = mix(uUVRect.xy, uUVRect.zw, inUV);
    vec4 world = uModel * vec4(inPosition, 1.0);
    fragShadowCoord = uLightVP * world;
    gl_Position = uViewProjection * world;
}
