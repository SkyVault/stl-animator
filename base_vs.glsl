#version 330

in vec3 aPos;
in vec2 aUvs;
in vec3 aNormal;

uniform mat4 mvp;
uniform mat4 model;

out vec3 oFragPos;
out vec3 oNormal;

void main() {
    oNormal = aNormal;
    oFragPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = mvp * vec4(aPos, 1.0);
}
