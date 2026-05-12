varying mediump vec2 var_texcoord0;

uniform lowp sampler2D texture_sampler;
uniform lowp vec4 tint;
uniform mediump vec4 spectrum0;
uniform mediump vec4 spectrum1;
uniform mediump vec4 spectrum2;
uniform mediump vec4 spectrum3;

mediump float band_value(mediump float index)
{
    if (index < 1.0) return spectrum0.x;
    if (index < 2.0) return spectrum0.y;
    if (index < 3.0) return spectrum0.z;
    if (index < 4.0) return spectrum0.w;
    if (index < 5.0) return spectrum1.x;
    if (index < 6.0) return spectrum1.y;
    if (index < 7.0) return spectrum1.z;
    if (index < 8.0) return spectrum1.w;
    if (index < 9.0) return spectrum2.x;
    if (index < 10.0) return spectrum2.y;
    if (index < 11.0) return spectrum2.z;
    if (index < 12.0) return spectrum2.w;
    if (index < 13.0) return spectrum3.x;
    if (index < 14.0) return spectrum3.y;
    if (index < 15.0) return spectrum3.z;
    return spectrum3.w;
}

void main()
{
    lowp vec4 color = texture2D(texture_sampler, var_texcoord0) * tint;

    mediump float local_x = clamp((var_texcoord0.x - 0.00390625) / 0.65234375, 0.0, 0.999);
    mediump float local_y = clamp((var_texcoord0.y - 0.0078125) / 0.84375, 0.0, 1.0);
    mediump float band_index = floor(local_x * 16.0);
    mediump float band_x = fract(local_x * 16.0);
    mediump float level = band_value(band_index);
    mediump float bar_height = 0.08 + level * 0.24;
    mediump float inside_bar = step(local_y, bar_height) * step(0.12, band_x) * step(band_x, 0.88);
    mediump vec3 bar_color = mix(vec3(0.12, 0.75, 1.0), vec3(1.0, 0.82, 0.22), level);

    color.rgb = mix(color.rgb, bar_color, inside_bar * 0.85);
    color.a = max(color.a, inside_bar * 0.95);
    gl_FragColor = color;
}
