#version 330 core
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D opaqueTexture;
uniform sampler2D accumulationTexture;
uniform sampler2D revealageTexture;


void main()
{
    // 1. 不透明背景
    vec3 opaqueColor = texture(opaqueTexture, TexCoord).rgb;
    
    // 2. 透明物体的累加数据
    vec4 accum = texture(accumulationTexture, TexCoord);
    
    // 3. 透射率（revealage = 剩余透明度）
    float revealage = texture(revealageTexture, TexCoord).r;
    
    // 4. 合成
    vec3 transparentColor;
    if (accum.a > 0.001) {
        // 恢复平均颜色
        transparentColor = accum.rgb / accum.a;
    } else {
        transparentColor = vec3(0.0);
    }
    
    // 5. 最终混合（注意：revealage 应该是剩余的不透明度比例）
    // 标准公式：最终颜色 = 透明颜色 + 不透明颜色 * 剩余透明度
    vec3 finalColor = transparentColor + opaqueColor * revealage;
    
    FragColor = vec4(finalColor, 1.0);
    
        // 调试：显示各个缓冲的值
    // 取消注释下面任意一行来调试
    //FragColor = vec4(opaqueColor, 1.0);        // 只显示不透明物体
    //FragColor = vec4(accum);          // 显示累加颜色
    //FragColor = vec4(accum.aaa, 1.0);          // 显示累加权重
    //FragColor = vec4(revealage, revealage, revealage, 1.0);  // 显示 revealage
}