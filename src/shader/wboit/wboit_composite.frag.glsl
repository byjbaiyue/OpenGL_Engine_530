#version 330 core
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D opaqueTexture;
uniform sampler2D accumulationTexture;
uniform sampler2D revealageTexture;


void main()
{
    vec3 opaqueColor = texture(opaqueTexture, TexCoord).rgb;

    vec4 accum = texture(accumulationTexture, TexCoord);

    float revealage = texture(revealageTexture, TexCoord).r;//越大越透明
    

    // revealage为1直接绘制底色
    if(revealage > 0.99) {
        FragColor = vec4(opaqueColor, 1.0);
    } else {

        vec3 transparentColor = accum.rgb / max(accum.a, 0.001);
        FragColor = vec4(mix(transparentColor,opaqueColor , revealage), 1.0);
    }
    
        // 调试：显示各个缓冲的值
    // 取消注释下面任意一行来调试
    //FragColor = vec4(opaqueColor, 1.0);        // 只显示不透明物体
    //FragColor = vec4(accum);          // 显示累加颜色
    //FragColor = vec4(accum.aaa, 1.0);          // 显示累加权重
    //FragColor = vec4(revealage, revealage, revealage, 1.0);  // 显示 revealage
}