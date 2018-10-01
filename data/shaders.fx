Texture2D texture0 : register( t0 );
SamplerState sampler0 : register( s0 );

struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = input.Pos;
    output.Tex = input.Tex;

    return output;
}

// ST.2084 (PQ)
float3 PQ_OETF(float3 color)
{
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;

    float3 Lm1 = pow(color, m1);
    float3 X = (c1 + c2 * Lm1) / (1 + c3 * Lm1);
    float3 E = pow(X, m2);
    return E;
}

float4 PS( PS_INPUT input) : SV_Target
{
    float4 sampledColor = texture0.Sample(sampler0, input.Tex);
    float4 linearColor = pow(sampledColor, 2.2);
    float4 pqColor;
    pqColor.rgb = PQ_OETF(linearColor.rgb);
    pqColor.a = linearColor.a;
    return pqColor;
}
