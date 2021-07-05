#ifndef INCLUDE_GUARD_LIGHT
#define INCLUDE_GUARD_LIGHT

struct LightParams {
    vec3 position;
    uint type;
    vec3 direction;
    float falloff_distance;
    vec3 color;
    uint shadow_map_index;
    vec2 penumbra_umbra_angles;
    vec2 dummy;
    mat4 view;
    mat4 proj;
};

bool is_spot(LightParams light) {
    return light.type == 0;
}

bool is_point(LightParams light) {
    return light.type == 1;
}

bool is_directional(LightParams light) {
    return light.type == 2;
}
#endif // #define INCLUDE_GUARD_LIGHT