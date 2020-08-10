float fresnel_dielectric_cos(float cosi, float eta) {
  /* compute fresnel reflectance without explicitly computing
   * the refracted direction */
  float c = abs(cosi);
  float g = eta * eta - 1 + c * c;
  float result;

  if (g > 0) {
    g = sqrt(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1) / (c * (g - c) + 1);
    result = 0.5 * A * A * (1 + B * B);
  }
  else
    result = 1.0; /* TIR (no refracted component) */

  return result;
}

vec3 fresnel_conductor(float cosi, vec3 eta, vec3 k) {
  vec3 cosi2 = vec3(cosi * cosi);
  vec3 one = vec3(1, 1, 1);
  vec3 tmp_f = eta * eta + k * k;
  vec3 tmp = tmp_f * cosi2;
  vec3 Rparl2 = (tmp - (2.0 * eta * cosi) + one) / (tmp + (2.0 * eta * cosi) + one);
  vec3 Rperp2 = (tmp_f - (2.0 * eta * cosi) + cosi2) / (tmp_f + (2.0 * eta * cosi) + cosi2);
  return (Rparl2 + Rperp2) * 0.5;
}

vec3 F_Schlick(float HdotV, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}

vec3 F_CookTorrance(float HdotV, vec3 F0) {
    vec3 n = (1 + sqrt(F0)) / (1 - sqrt(F0));
    vec3 g = sqrt( (n*n) + pow(HdotV, 2) - 1);
    return 0.5 * ((g-HdotV)/(g+HdotV)*(g-HdotV)/(g+HdotV)) *
                (1 + ( ((g+HdotV)*HdotV-1) / ((g-HdotV)*HdotV+1)*((g+HdotV)*HdotV-1) / ((g-HdotV)*HdotV+1)) );
}