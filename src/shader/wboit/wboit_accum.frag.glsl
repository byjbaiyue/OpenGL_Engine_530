#version 330 core

in vec2 TexCoord;
in float ViewZ;

layout(location = 0) out vec4 Accumulation;

uniform sampler2D texture_base;

// 改进的WBOIT权重函数
float computeWeight(float z, float alpha) {
    // 将视图空间深度映射到 [0,1] 范围（假设远平面为1000，近平面的值很小）
    // 注意：ViewZ是负值（OpenGL相机朝向-Z），需要取绝对值
    float maxZ = 1000.0;  // 可根据场景调整，或作为uniform传入
    float depthFactor = clamp(z / maxZ, 0.0, 1.0);
    
    // 方案1：标准WBOIT权重（推荐）
    // 近处透明物体权重更高，远处衰减更合理
    float weight = alpha * pow(1.0 - depthFactor, 3.0);
    
    // 方案2：更激进的近处权重（适合大多数透明效果）
    // return alpha * pow(1.0 - depthFactor, 2.0) * (1.0 + depthFactor);
    
    // 方案3：考虑alpha的平方，减少过度混合
    // return pow(alpha, 1.5) * pow(1.0 - depthFactor, 2.5);
    
    return weight;
}

void main()
{
    vec4 texColor = texture(texture_base, TexCoord);
    float alpha = texColor.a;
    
    // 保留更多细节，阈值降低
    if(alpha < 0.01) discard;
    
    // 预乘alpha处理
    vec3 premultipliedColor = texColor.rgb * alpha;
    
    // 使用ViewZ而不是gl_Position.z（gl_Position是裁剪空间坐标）
    float weight = computeWeight(ViewZ, alpha);
    
    // 累加颜色和权重
    // 输出格式：rgb累加值，alpha累加值
    Accumulation = vec4(premultipliedColor * weight, alpha * weight);
    
    // 可选：添加最大权重限制，避免数值溢出
    // Accumulation = min(Accumulation, 100.0);
}