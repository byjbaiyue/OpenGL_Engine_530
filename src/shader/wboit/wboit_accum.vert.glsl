#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoord;


out vec2 TexCoord;
out float ViewZ;

uniform mat4 projection;
uniform mat4 view; //这玩意仅需要传入3x3矩阵(去除平移)
uniform vec3 chunkRelativePos;



void main()
{
    vec3 finalPos = aPos + chunkRelativePos;
    vec4 ViewPos = view * vec4(finalPos, 1.0);
    
    gl_Position = projection * ViewPos;

    TexCoord = aTexCoord;



    ViewZ = ( -ViewPos.z ) ; //-ViewPos.z越远值越大(ViewZ范围0~1)
}