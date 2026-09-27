#version 300 es
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
out vec2 fragUV;
out vec3 fragWorldNormal;
out highp vec4 fragShadowCoord;
uniform mat4 uModel;
uniform highp mat4 uLightVP;
layout(std140) uniform FrameData {
    mat4 uViewProjection;
    vec4 uLightDirAmbient;
};
void main() {
    fragUV = inUV;
    fragWorldNormal = mat3(uModel) * inNormal;
    fragShadowCoord = uLightVP * uModel * vec4(inPosition, 1.0);
    gl_Position = uViewProjection * uModel * vec4(inPosition, 1.0);
}
