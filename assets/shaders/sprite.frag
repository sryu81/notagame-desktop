#version 300 es
precision mediump float;
in vec2 fragUV;
in highp vec4 fragShadowCoord;
uniform vec4 uColor;
uniform sampler2D uTexture;
uniform int uHasTexture;
uniform highp sampler2DShadow uShadowMap;
layout(std140) uniform FrameData {
    highp mat4 uViewProjection;
    highp vec4 uLightDirAmbient;
};
out vec4 outColor;
void main() {
    vec4 tex = uHasTexture != 0 ? texture(uTexture, fragUV) : vec4(1.0);
    if (tex.a < 0.5) discard;
    float sun = 0.6 + 0.4 * max(normalize(uLightDirAmbient.xyz).y, 0.0);
    vec3 proj = fragShadowCoord.xyz / fragShadowCoord.w * 0.5 + 0.5;
    float shadow = texture(uShadowMap, vec3(clamp(proj.xy, 0.0, 1.0), proj.z - 0.002));
    float light = uLightDirAmbient.w + (1.0 - uLightDirAmbient.w) * sun * shadow;
    outColor = vec4(uColor.rgb * tex.rgb * light, uColor.a);
}
