uniform sampler2D texture0;
uniform sampler2D texture1;
uniform vec4 fc[1];

void main()
{
    vec4 Tex0 = texture2D(texture0,gl_TexCoord[0].xy);
    vec4 Tex1 = texture2D(texture1,gl_TexCoord[1].xy);
    
    vec4 r0 = gl_Color;
    vec4 r1;
    
    r1 = mix(r0, Tex0, Tex0.a);
    r1.a = r0.a;
    r0 = mix(r0 * Tex0, r1, fc[0].x);
    
    r1 = mix(r0, Tex1, Tex1.a);
    r1.a = r0.a;
    r0 = mix(r0 * Tex1, r1, fc[0].y);
    
    gl_FragColor = r0;
}