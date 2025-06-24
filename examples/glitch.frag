#version 320 es
precision highp float;

// you might need to enable the "render-every-frame" plugin option for this to animate properly
in vec2 v_texcoord;
out vec4 fragColor;

uniform sampler2D tex;
uniform vec2 windowPos;
uniform vec2 windowSize;
uniform vec2 monitorSize;
uniform float time;

const float MAGNITUDE = 0.005;
const float SPEED = 4.0;

void main() {
    vec2 p = (v_texcoord * monitorSize) - windowPos;
    vec2 R = windowSize;
    p.y = R.y - p.y; // y-flip

    if (p.x < 0.0 || p.y < 0.0 || p.x > R.x || p.y > R.y) {
        discard;
    }

    vec4 windowPixel = texture(tex, v_texcoord);

    float offset_x = sin(time * SPEED) * MAGNITUDE;
    float offset_y = cos(time * SPEED * 0.7) * MAGNITUDE;
    vec2 offset = vec2(offset_x, offset_y);

    fragColor = vec4(texture(tex, v_texcoord + offset).r, windowPixel.g, texture(tex, v_texcoord - offset).b, windowPixel.a);
}