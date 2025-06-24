#version 320 es
precision highp float;

in vec2 v_texcoord;
out vec4 fragColor;
uniform sampler2D tex;
uniform sampler2D backgroundTex;
uniform vec2 windowPos;
uniform vec2 windowSize;
uniform vec2 monitorSize;
uniform vec4 borderColor;

void main() {
    vec4 originalColor = texture(tex, v_texcoord);

    vec2 p = (v_texcoord * monitorSize) - windowPos;
    vec2 R = windowSize;
    p.y = R.y - p.y; // y-flip for coordinate system match

    if (p.x < -0.0 || p.y < -0.0 || p.x > R.x + 0.0 || p.y > R.y + 0.0) {
        fragColor = originalColor;
        discard;
    }

    originalColor.rgb = vec3(1.) - vec3(.88, .9, .92) * originalColor.rgb;

    originalColor.rgb = -originalColor.rgb + dot(vec3(0.26312, 0.5283, 0.10488), originalColor.rgb) * 2.0;
    vec3 invertedColor = 1.0 - originalColor.rgb;

    fragColor = vec4(invertedColor, originalColor.a);
}