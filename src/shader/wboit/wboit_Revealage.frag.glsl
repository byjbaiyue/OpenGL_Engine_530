#version 330 core

in vec2 TexCoord;
in float ViewZ;

layout(location = 0) out vec4 Revealage;

uniform sampler2D texture_base;

void main()
{
    vec4 texColor = texture(texture_base, TexCoord);
    float alpha = texColor.a;
    
    if(alpha < 0.05) discard;
    
    Revealage = vec4(1-alpha);
}