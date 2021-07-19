#ifndef INCLUDE_GUARD_LIGHTING_HELPER
#define INCLUDE_GUARD_LIGHTING_HELPER

vec3 get_light_radiance(LightParams light, vec3 frag_pos, vec3 L_vec) {
    vec3 radiance = light.color;
    if (is_spot(light)) {
        float theta_s = acos(dot(light.direction, -L_vec));
        float t = clamp((theta_s - light.penumbra_umbra_angles.y) / (light.penumbra_umbra_angles.x - light.penumbra_umbra_angles.y), 0.0, 1.0);
        radiance *= pow(t, 2.0);
    }

    if (light.falloff_distance > 0.0) {
        float dist = length(light.position - frag_pos);
        radiance *= pow(max(1-pow(dist/light.falloff_distance, 2.0f), 0.0f), 2.0f);
    }

    return radiance;
}

// Shadow Helping functions
float ChebyshevUpperBound(vec2 Moments, float t) {
    // One-tailed inequality valid if t > Moments.x
    float p = float(t > Moments.x);
    // Compute variance.
    float Variance = Moments.y - (Moments.x*Moments.x);
    Variance = max(Variance, 0.0001);
    // Compute probabilistic upper bound.
    float d = t - Moments.x;
    float p_max = Variance / (Variance + d*d);
    return max(p, p_max);
}
float ReduceLightBleeding(float p_max, float amount) {
    // using a linear step function
    return clamp((p_max - amount) / (1.0 - amount), 0, 1);
}
float get_shadow_component(LightParams light, sampler2D shadow_map, vec2 shadow_map_coordinates, float light_frag_depth) {
    vec2 moments;
    // if the texture coordinates are outside we give the moments a really far away depth so no shadowing occurs within reasonable limits
    if (clamp(shadow_map_coordinates, 0.0, 1.0) != shadow_map_coordinates) {
        moments = vec2(-40, 1600);
    }
    else {
        // get the moments from the texture using the normalized xy coordinates
        moments = texture(shadow_map, shadow_map_coordinates).rg;
    }
    // apply the chebyshev variance equation to give a probability that the fragment is in shadow
    float p_max = ChebyshevUpperBound(moments, light_frag_depth);
    // We apply a fix to the light bleeding problem
    return ReduceLightBleeding(p_max, 0.6);
}

vec3 get_L_vec(LightParams light, vec3 frag_pos) {
    if(is_point(light) || is_spot(light)) {
        return light.position - frag_pos;
    }
    else if (is_directional(light)) {
        return -light.direction;
    }
    return vec3(1.0);
}

#endif // #define INCLUDE_GUARD_LIGHTING_HELPER