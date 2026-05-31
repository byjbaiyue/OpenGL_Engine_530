#version 330 core

in vec2 TexCoord;
in float ViewZ;

layout(location = 0) out vec4 Accumulation;

uniform sampler2D texture_base;

// WBOIT权重函数
float computeWeight(float z, float alpha) {
    float i = clamp(z, 0.0, 1.0);
    return alpha * pow(1.0 - i, 2);
}

void main()
{
    vec4 texColor = texture(texture_base, TexCoord);
    float alpha = texColor.a;
    
    if(alpha < 0.05) discard; // 忽略几乎透明的像素
    
    // 计算权重
    float weight = computeWeight(ViewZ, alpha);
    
    // 累加颜色（加权）
    Accumulation = vec4(texColor.rgb *alpha * weight, alpha * weight);
    
}