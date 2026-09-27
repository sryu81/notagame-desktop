#version 300 es
precision highp float;
layout(location = 0) in vec3 aDir;
layout(location = 1) in float aMagnitude;
layout(location = 2) in vec3 aColor;
out vec3 vColor;
out float vBrightness;
layout(std140) uniform FrameData {
    mat4 uViewProjection;
    vec4 uLightDirAmbient;
};
uniform float uDayPhase;
uniform float uPointSize;
uniform float uBrightnessScale;
uniform float uBrightFloor;
uniform float uBrightCeil;
uniform vec3 uSkyAxis;
void main() {
    float angle = 6.2831853 * uDayPhase;
    float c = cos(angle);
    float s = sin(angle);
    vec3 axis = normalize(uSkyAxis);
    vec3 v = aDir;
    vec3 vrot = v * c + cross(axis, v) * s + axis * dot(axis, v) * (1.0 - c);
    vec4 worldPos = vec4(vrot * 1000.0, 1.0);
    vec4 clipPos = uViewProjection * worldPos;
    gl_Position = vec4(clipPos.x, clipPos.y, clipPos.w * 0.9999, clipPos.w);
    float b = clamp((3.0 - aMagnitude * 0.5) * uBrightnessScale, uBrightFloor, uBrightCeil);
    vBrightness = b;
    vColor = aColor;
    gl_PointSize = uPointSize;
}
