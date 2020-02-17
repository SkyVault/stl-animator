#version 330

out vec4 result;

in vec3 oFragPos;
in vec3 oNormal;

uniform vec3 light_pos = vec3(0, 0, 0);
uniform vec3 light_color = vec3(1, 1, 1);
uniform vec3 diffuse = vec3(1, 1, 1);

void main() {
    // Ambient
    float ambient_strength = 0.1f;
    vec3 ambient_result = ambient_strength * light_color;

    // Diffuse
    vec3 normal = normalize(oNormal);
    vec3 light_dir = normalize(light_pos - oFragPos);

    float diff = max(dot(normal, light_dir), 0.0);
    vec3 diffuse_result = diff * light_color;

    result = vec4((ambient_result + diffuse_result) * diffuse, 1.0);
}
