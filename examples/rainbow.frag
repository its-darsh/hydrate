#version 320 es
precision highp float;

// you might need to enable the "render-every-frame" plugin option for this to animate properly
in vec2 v_texcoord;
out vec4 fragColor;

uniform sampler2D tex;
uniform vec2 windowPos;
uniform vec2 windowSize;
uniform vec2 monitorSize;
uniform vec4 borderColor;
uniform float borderWidth;
uniform float time;

const float RAINBOW_SPEED = 0.5;
const float SATURATION = 0.8;
const float BRIGHTNESS = 0.9;

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec2 p = (v_texcoord * monitorSize) - windowPos;
    vec2 R = windowSize;
    p.y = R.y - p.y;

    vec4 windowPixel = texture(tex, v_texcoord);

    vec2 center_p = p - R / 2.0;
    float angle = atan(center_p.y, center_p.x);
    float hue = angle / (2.0 * 3.14159) + time * RAINBOW_SPEED;
    vec3 rainbowColor = hsv2rgb(vec3(fract(hue), SATURATION, BRIGHTNESS));

    rainbowColor *= borderColor.rgb;

    fragColor = mix(vec4(rainbowColor, 1.0), windowPixel, windowPixel.a);
}