// Source Repository: https://github.com/waverso/waversomapstudio

float4 idc : register(c0);

float4 MaskPS() : COLOR
{
    return float4(idc.r, 1.0, 1.0, 1.0);
}

sampler2D maskSamp : register(s3);
sampler2D glowSamp : register(s4);
float4 prm : register(c0);
float4 col : register(c1);

float Has(float v)
{
    return step(0.002, v);
}

float IdEdge(float a, float b)
{
    return max(Has(a), Has(b)) * saturate((abs(a - b) - 0.0015) * 900.0);
}

float2 Fetch(float2 uv, float2 dir, float r, float s)
{
    return tex2D(maskSamp, uv + dir * prm.xy * (r * s)).rg;
}

void AddPair(float2 uv, float2 dir, float s, float w, float2 c, float r, inout float inA, inout float outA, inout float visAcc,
    inout float edgeAcc)
{
    float2 p = Fetch(uv, dir, r, s);
    float2 n = Fetch(uv, -dir, r, s);
    float inside = Has(c.r);

    float e = max(max(IdEdge(c.r, p.r), IdEdge(c.r, n.r)), IdEdge(p.r, n.r) * 0.7) * w;
    outA += e * (1.0 - inside);
    inA += e * inside;

    visAcc += e * max(Has(c.g), max(Has(p.g), Has(n.g)));
    edgeAcc += e;
}

float4 OutlinePS(float2 uv : TEXCOORD0) : COLOR
{
    float2 c = tex2D(maskSamp, uv).rg;
    float r = max(prm.z, 0.65);
    float inA = 0.0, outA = 0.0, visAcc = 0.0, edgeAcc = 0.0;

    AddPair(uv, float2(1, 0), 0.50, 1.00, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0, 1), 0.50, 1.00, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0.707, 0.707), 0.50, 0.92, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0.707, -0.707), 0.50, 0.92, c, r, inA, outA, visAcc, edgeAcc);

    AddPair(uv, float2(1, 0), 0.95, 0.72, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0, 1), 0.95, 0.72, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0.707, 0.707), 0.95, 0.64, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0.707, -0.707), 0.95, 0.64, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0.924, 0.383), 0.95, 0.42, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0.383, 0.924), 0.95, 0.42, c, r, inA, outA, visAcc, edgeAcc);

    AddPair(uv, float2(1, 0), 1.35, 0.24, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0, 1), 1.35, 0.24, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0.707, 0.707), 1.35, 0.20, c, r, inA, outA, visAcc, edgeAcc);
    AddPair(uv, float2(0.707, -0.707), 1.35, 0.20, c, r, inA, outA, visAcc, edgeAcc);

    outA = saturate(outA * 0.62);
    inA = saturate(inA * 0.62) * 0.92;
    float stroke = saturate(max(max(outA, inA), (outA + inA) * 0.55));
    float vis = max(Has(c.g), visAcc / max(edgeAcc, 0.0001));

    float innerMix = Has(c.r) * saturate(inA / (outA + inA + 0.0001));
    float3 visCol = lerp(saturate(col.rgb * 1.08), saturate(col.rgb * 0.85 + 0.16), innerMix);

    float2 offset = prm.xy * (r + 1.0);
    float2 glow = tex2D(glowSamp, uv + offset).bg + tex2D(glowSamp, uv - offset).bg
        + tex2D(glowSamp, uv + float2(offset.x, -offset.y)).bg
        + tex2D(glowSamp, uv + float2(-offset.x, offset.y)).bg;
    float strokeAlpha = stroke * lerp(prm.w, 1.0, vis);
    float glowAlpha = dot(glow, float2(prm.w, 1.0 - prm.w)) * 0.2 * (1.0 - Has(c.r)) * (1.0 - strokeAlpha);
    float alpha = strokeAlpha + glowAlpha;
    float3 color = lerp(float3(1.0, 1.0, 1.0), visCol, vis) * strokeAlpha + glowAlpha;
    return float4(color, alpha);
}

sampler2D image : register(s0);
float4 accent : register(c0);

float4 IconPS(float2 uv : TEXCOORD0, float4 tint : COLOR0) : COLOR0
{
    float4 pixel = tex2D(image, uv);
    float mask = max(0, pixel.r - max(pixel.g, pixel.b));
    return float4((pixel.rgb - float3(mask, 0, 0)) * tint.rgb + mask * accent.rgb, pixel.a * tint.a);
}
