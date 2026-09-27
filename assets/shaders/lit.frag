#version 300 es
precision mediump float;
in vec2 fragUV;
in vec3 fragWorldNormal;
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
    vec3 N = normalize(fragWorldNormal);
    float lambert = max(dot(N, normalize(uLightDirAmbient.xyz)), 0.0);
    float light = uLightDirAmbient.w + (1.0 - uLightDirAmbient.w) * lambert;
    vec3 proj = fragShadowCoord.xyz / fragShadowCoord.w;
    proj = proj * 0.5 + 0.5;
    float shadow = texture(uShadowMap, vec3(clamp(proj.xy, 0.0, 1.0), proj.z - 0.0005));
    float finalLight = uLightDirAmbient.w + (1.0 - uLightDirAmbient.w) * lambert * shadow;
    vec4 tex = uHasTexture != 0 ? texture(uTexture, fragUV) : vec4(1.0);
    outColor = vec4(uColor.rgb * tex.rgb * finalLight, uColor.a * tex.a);
}
