uniform sampler2D texture0;
uniform vec4 fc[1];

void main()
{
    vec4 Tex0 = texture2D(texture0,gl_TexCoord[0].xy);
    gl_FragColor = vec4(Tex0.a);
}