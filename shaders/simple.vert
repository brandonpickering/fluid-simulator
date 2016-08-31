#version 330


in layout(location = 0) vec2 inPosition;

out vec2 vPosition;


void main() {
    gl_Position = vec4(inPosition, 0, 1);
    vPosition = inPosition;
}
