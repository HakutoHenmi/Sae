// Resources/shaders/PostProcessCommon.hlsli
#ifndef POST_PROCESS_COMMON_HLSLI
#define POST_PROCESS_COMMON_HLSLI

float Saturate1(float v)
{
    return saturate(v);
}
float2 Saturate2(float2 v)
{
    return saturate(v);
}
float3 Saturate3(float3 v)
{
    return saturate(v);
}

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// --------------------
// Hash / Noise
// --------------------
float Hash12(float2 p)
{
    float h = dot(p, float2(127.1, 311.7));
    return frac(sin(h) * 43758.5453123);
}

float Noise2D(float2 uv)
{
    float2 i = floor(uv);
    float2 f = frac(uv);

    float a = Hash12(i);
    float b = Hash12(i + float2(1, 0));
    float c = Hash12(i + float2(0, 1));
    float d = Hash12(i + float2(1, 1));

    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float FBM(float2 uv)
{
    float v = 0.0;
    float a = 0.5;
    float2 p = uv;
    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        v += a * Noise2D(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

// --------------------
// CRT/VHS helpers
// --------------------
float Vignette(float2 uv, float strength)
{
    float2 d = uv - 0.5;
    float r2 = dot(d, d);
    float v = 1.0 - r2 * (2.2 + strength * 4.0);
    return saturate(v);
}

float Scanline(float2 uv, float amount)
{
    float ln = sin(uv.y * 900.0) * 0.5 + 0.5;
    float v = lerp(1.0, 0.65 + 0.35 * ln, saturate(amount));
    return v;
}

float2 DistortCRT(float2 uv, float time, float amount)
{
    float w1 = sin(uv.y * 800.0 + time * 11.0);
    float w2 = sin(uv.y * 240.0 - time * 7.0);
    float wave = (w1 * 0.75 + w2 * 0.25) * amount;
    uv.x += wave;
    return uv;
}

float2 Barrel(float2 uv, float amount)
{
    float2 p = uv * 2.0 - 1.0;
    float r2 = dot(p, p);
    float k = -0.10 * amount;
    p *= (1.0 + k * r2);
    return (p * 0.5 + 0.5);
}

float3 SampleChroma(Texture2D tex, SamplerState smp, float2 uv, float2 shift)
{
    float r = tex.Sample(smp, uv + shift).r;
    float g = tex.Sample(smp, uv).g;
    float b = tex.Sample(smp, uv - shift).b;
    return float3(r, g, b);
}

float2 ClampUV(float2 uv)
{
    return saturate(uv);
}

// --------------------
// VHS
// --------------------
float VHS_VerticalJump(float time, float amount)
{
    float s = Hash12(float2(floor(time * 2.0), 9.13));
    float kick = step(0.965, s);
    float t = frac(time * 2.0);
    float relax = exp(-t * 8.0);
    return (kick * relax) * (0.03 * amount);
}

float VHS_LineWobble(float2 uv, float time, float amount)
{
    float y = floor(uv.y * 420.0);
    float n = Hash12(float2(y, floor(time * 30.0)));
    float s = (n - 0.5) * 2.0;
    float wob = sin(uv.y * 40.0 + time * 3.0) * 0.5 + s * 0.5;
    return wob * 0.010 * amount;
}

float VHS_TapeBand(float2 uv, float time, float amount)
{
    float bandY = frac(time * 0.08 + Hash12(float2(floor(time * 0.08), 1.2)));
    float d = abs(uv.y - bandY);
    float band = saturate(1.0 - d * 35.0);
    float n = FBM(uv * float2(30.0, 180.0) + time * 1.2);
    return (n - 0.5) * 0.25 * band * amount;
}

float3 VHS_ChromaSmear(Texture2D tex, SamplerState smp, float2 uv, float amount)
{
    float2 dx = float2(0.0015 * amount, 0.0);
    float3 c0 = tex.Sample(smp, uv).rgb;
    float3 c1 = tex.Sample(smp, uv - dx).rgb;
    float3 c2 = tex.Sample(smp, uv - dx * 2.0).rgb;

    float r = c0.r;
    float g = lerp(c0.g, c1.g, 0.65);
    float b = lerp(c0.b, c2.b, 0.75);
    return float3(r, g, b);
}

// --------------------
// [FIX] LineJitter を追加（CRTPost.hlsl が呼ぶため）
// --------------------
float LineJitter(float2 uv, float time, float strength)
{
    // 走査線単位でランダムな横ブレ
    float y = floor(uv.y * 600.0);
    float n = Hash12(float2(y, floor(time * 60.0))); // 0..1
    return (n - 0.5) * 0.02 * strength; // -..+
}

#endif // POST_PROCESS_COMMON_HLSLI
