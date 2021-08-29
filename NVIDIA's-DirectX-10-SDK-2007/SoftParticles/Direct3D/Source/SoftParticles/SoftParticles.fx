//----------------------------------------------------------------------------------
// File:   SoftParticles.fx
// Author: Tristan Lorach
// Email:  sdkfeedback@nvidia.com
// 
// Copyright (c) 2007 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA OR ITS SUPPLIERS
// BE  LIABLE  FOR  ANY  SPECIAL,  INCIDENTAL,  INDIRECT,  OR  CONSEQUENTIAL DAMAGES
// WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS OF BUSINESS PROFITS,
// BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS)
// ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
//
//----------------------------------------------------------------------------------

#define FAR_CLIP near_far.y
#define NEAR_CLIP near_far.x
#define ZFARMULTZNEAR near_far.z
#define ZFARMINUSZNEAR near_far.w
#define RATIOXY DepthRTSz.z
#define EPSILONZ zEpsilon
#define SLICES 30

cbuffer changingeveryframe
{
    float4x4 proj;
    float4x4 viewProj;
    float4x4 worldViewProj;
    float4x4 worldView;
    float4x4 worldViewI;
    float4x4 worldViewIT;
    float4x4 worldIT;
    float4x4 world;
    float4x4 viewIT;
    float4x4 viewI;
    float4x4 view;
    float offsetZ : OFFSETZ = 0.0;
};
// note : semantics used here to avoid to display them in the UI...
cbuffer almostneverchange
{
    float4 near_far : NEAR_FAR;
    float3 DepthRTSz : DEPTHRTSZ;
};
cbuffer tweakables
{
    float SoftParticleContrast <
      float uimin = 1.0;
      float uimax = 5.0;
      > = 2.0;
    float SoftParticleScale <
      float uimin = 0.0;
      float uimax = 5.0;
      > = 0.5;
    float intensity <
      float uimin = 0.0;
      float uimax = 5.0;
      > = 2.0;
    float maxSZ <
      float uimin = 0.0;
      float uimax = 10.0;
      > = 5.0;
    float zEpsilon <
      float uimin = 0.0;
      float uimax = 0.1;
      > = 0.0;
    float2 NearFadeout; // x = (NearFadeout * FAR_CLIP / ZFARMINUSZNEAR) and y = NearFadeout
    bool debugTex = false;
    float3 lightPos : LIGHTPOS              = {10.0, 20, -10.0};
    float3 ambiLightColor : AMBILIGHTCOLOR  = {0.07, 0.07, 0.07};
};

//
// Texture template to handle MSAA mode(s).
// the annoying details is that we need to recompile
// the shader if ever the amount of sample changed
//
#ifndef NUMSAMPLES
#define NUMSAMPLES 4 // Note: the compiler will also pass this amount of samples
#endif
Texture2DMS<float, NUMSAMPLES> texture_depthMS;

Texture2D texture_depth;
Texture3D texture_fog < string file="smokevol1.dds"; >;
Texture2D texture_ZBuffer;
Texture2D texture_mesh < string file="terrain.dds"; >;

SamplerState sampler_linear
{
    AddressU = Wrap;
    AddressV = Wrap;
    Filter = Min_Mag_Mip_Linear;
};
SamplerState sampler_depth
{
    AddressU = Clamp;
    AddressV = Clamp;
    Filter = Min_Mag_Mip_Point;
};

//////////////////////////////////////////////////////////////////////////////////
// STATES STATES STATES STATES STATES STATES STATES STATES STATES STATES STATES
//////////////////////////////////////////////////////////////////////////////////
DepthStencilState depthEnabled
{
    DepthEnable = true;
    DepthWriteMask = All;
    DepthFunc = Less;
};
DepthStencilState depthEnabledNoWrite
{
    DepthEnable = true;
    DepthWriteMask = Zero;
    DepthFunc = Less;
};
DepthStencilState depthDisabled
{
    DepthEnable = false;
    DepthWriteMask = Zero;
    DepthFunc = Always;
};
BlendState blendOFF
{
    BlendEnable[0] = false;
};
BlendState blendSrcAlphaInvSrcAlpha
{
    BlendEnable[0] = true;
    SrcBlend = Src_Alpha;
    DestBlend = Inv_Src_Alpha;
};
BlendState blendOneInvSrcAlpha
{
    BlendEnable[0] = true;
    SrcBlend = One;
    DestBlend = Inv_Src_Alpha;
};
RasterizerState cullDisabled
{
    CullMode = None;
    MultisampleEnable = TRUE;
};
////////////////////////////////////////////////////////////////////////////////////
// STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS STRUCTS
////////////////////////////////////////////////////////////////////////////////////
struct Mesh_VSIn
{
    float3 position : position;
    //float4 color    : color0; //NA in .X Reader for now...
    float3 normal   : normal;
    float2 tc       : texcoord0;
};
struct Mesh_VSOut
{
    float4 position : SV_Position;
    float4 color    : color;
    float  depth    : texcoord0;
    float3 tc       : texcoord1; // z used for a trick for fading out the model
};
struct Mesh_PSOut
{
    float4 color    : SV_Target0;
    float4 depth    : SV_Target1;
};

struct SoftParticles_VSIn
{
    float3 position : position;
    float2 parms    : texcoord0;
};

struct SoftParticles_VSOut
{
    float4 position : SV_Position;
    float2 parms    : texcoord0;
};

struct SoftParticles_GSOut
{
    float4 position : SV_Position;
    float  alpha    : color;
    float2 tc       : texcoord0;
    float  Z        : texcoord1;
};

struct SoftParticles_PSOut
{
    float4 color : SV_Target;
};
//////////////////////////////////////////////////////////////////////////////////
// Shared function to fetch the texture...
//////////////////////////////////////////////////////////////////////////////////
float4 GetFogTexture(float2 tc, const bool debug)
{
    if(debug)
    {
        // Draw a checked. For debugging
        float2 fp = step(frac(tc * 10.0), 0.5);
        return lerp(float4(1,0.2,0,1), float4(0,0.2,1,1), abs(fp.x - fp.y));
    } else {
        // get a texture for the Fog. Don't pay attention of the fact there's 2 tex fetch... 
        // just for adjusting the cycle of the texture for this effect...
        float4 tex = texture_fog.Sample(sampler_linear, float3(tc, offsetZ)).a;
        tex += texture_fog.Sample(sampler_linear, float3(tc, 1.0 - offsetZ)).a;
        return 0.5 * tex;
    }
}
//////////////////////////////////////////////////////////////////////////////////
// VTX SHADERS VTX SHADERS VTX SHADERS VTX SHADERS VTX SHADERS VTX SHADERS
//////////////////////////////////////////////////////////////////////////////////
SoftParticles_VSOut VSSoftParticles(SoftParticles_VSIn input)
{
    SoftParticles_VSOut output;
    float3 P = input.position;
    output.position = float4(P, 1);
    output.parms = input.parms;
    return output;
}
//////////////////////////////////////////////////////////////////////////////////
// GEOM. SHADERS GEOM. SHADERS GEOM. SHADERS GEOM. SHADERS GEOM. SHADERS
//
// We output Quads : SoftParticles_GSOut x 4 = of 32 floats
//
//////////////////////////////////////////////////////////////////////////////////
[maxvertexcount (4)]
void GSSoftParticles(inout TriangleStream<SoftParticles_GSOut> Stream, point SoftParticles_VSOut input[1] )
{
    SoftParticles_GSOut output;
    float alpha = input[0].parms.x;
    if(alpha > 0.0)
    {
        float4 Po = input[0].position;
        float4 Pw = mul(Po, worldView);
        //
        // fade out if close to the near plane
        //
        
        float d = 1.0 - saturate((NearFadeout.x - Pw.z) / NearFadeout.y);
        alpha *= d;

        float sz = maxSZ * input[0].parms.y;
        float3 V1 = float3(0,sz,0);
        float3 V2 = float3(sz,0,0);
        //
        // Projections
        //
        output.alpha = alpha;
        float4 Pt;
        Pt.w = 1.0;
        Pw.xyz += V2 - V1;
        output.position = mul(Pw, proj);
        output.Z = output.position.z;
        output.tc = float2(1,0);
        Stream.Append(output);
        Pw.xyz += 2.0*V1;
        output.position = mul(Pw, proj);
        output.tc = float2(1,1);
        Stream.Append(output);
        Pw.xyz -= 2.0*(V1 + V2);
        output.position = mul(Pw, proj);
        output.tc = float2(0,0);
        Stream.Append(output);
        Pw.xyz += 2.0*V1;
        output.position = mul(Pw, proj);
        output.tc = float2(0,1);
        Stream.Append(output);
    }
}
//////////////////////////////////////////////////////////////////////////////////
// PIXEL SHADERS PIXEL SHADERS PIXEL SHADERS PIXEL SHADERS PIXEL SHADERS
//////////////////////////////////////////////////////////////////////////////////
float Contrast(float Input, float ContrastPower)
{
#if 1
     //piecewise contrast function
     bool IsAboveHalf = Input > 0.5 ;
     float ToRaise = saturate(2*(IsAboveHalf ? 1-Input : Input));
     float Output = 0.5*pow(ToRaise, ContrastPower); 
     Output = IsAboveHalf ? 1-Output : Output;
     return Output;
#else
    // another solution to create a kind of contrast function
    return 1.0 - exp2(-2*pow(2.0*saturate(Input), ContrastPower));
#endif
}
//
// Using the ZBuffer (Depth Buffer) as a texture and compare the distance in Z
//
SoftParticles_PSOut PSSoftParticles_DepthBuffer(SoftParticles_GSOut input)
{
    SoftParticles_PSOut output;
    float zBuf = texture_ZBuffer.Load( int4(input.position.x, input.position.y,0,0)).r;
    float z = ZFARMULTZNEAR / ( FAR_CLIP - zBuf * ZFARMINUSZNEAR);
    float zdiff = (z - input.Z);
    float c = Contrast(zdiff * SoftParticleScale, SoftParticleContrast);
    if( c * zdiff <= EPSILONZ )
    {
        discard;
    }
    output.color = c * intensity * input.alpha * GetFogTexture(input.tc, debugTex);
    return output;
}

//////////////////////////////////////////////////////////////////////////////////
// Basic textured Mesh
//////////////////////////////////////////////////////////////////////////////////
Mesh_VSOut VSMesh(Mesh_VSIn input)
{
    Mesh_VSOut output;
    float3 P    = input.position;
    float3 Nw   = mul(float4(input.normal, 0), worldIT).xyz;
    float3 Pw   = mul(float4(P, 1), world).xyz;
    float3 Vw   = viewI[3];
    float3 Vdir = normalize(Vw - Pw);
    float3 L    = lightPos;//mul(float4(lightPos, 1), world).xyz;
    float3 Ldir = normalize(L - Pw);
    float3 Hvec = normalize(Ldir + Vdir);
    float4 l = lit(dot(Nw, Ldir), dot(Nw, Hvec), 100.0);

    output.color = float4(l.yy, 0.2*l.z, 0); //input.color.a); //color NA in .X Reader, yet...
    output.position = mul(float4(P, 1), worldViewProj);
    output.tc.xy = input.tc;
    output.tc.z = 0.05*dot(input.position.xz, input.position.xz); // distance of vertex : for fade-out effect
    //
    // This only used by the technique 'DepthFromSecondRT'
    //
    output.depth = output.position.z;

    return output;
}
float4 PSMeshNormal(Mesh_VSOut input) : SV_Target0
{
    float4 c;
    c.a = 1.0 - saturate(input.tc.z - 4.0); //input.color.a; // The intend would be to 'fade out' the mountain with horizon. But .X cannot for now...
    c.rgb = input.color.bbb + input.color.rrr * texture_mesh.Sample(sampler_linear, input.tc).rgb;
    return c;
}
//////////////////////////////////////////////////////////////////////////////////
// use the ZBuffer as a texture to compare the depth values
//////////////////////////////////////////////////////////////////////////////////

technique10 ZBufferAsTexture
{
    pass Mesh
    {
        SetBlendState(blendSrcAlphaInvSrcAlpha, float4(1.0, 1.0, 1.0, 1.0) ,0xffffffff);
        SetRasterizerState( cullDisabled );
        SetDepthStencilState( depthEnabled, 0 );
        SetVertexShader( CompileShader( vs_4_0, VSMesh() ) );
        SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_4_0, PSMeshNormal() ) );
    }
    pass SoftParticles
    {
        SetDepthStencilState( depthEnabledNoWrite, 0 );
        SetRasterizerState( cullDisabled );
        SetBlendState(blendOneInvSrcAlpha, float4(1.0, 1.0, 1.0, 1.0) ,0xffffffff);
        SetVertexShader( CompileShader( vs_4_0, VSSoftParticles() ) );
        SetGeometryShader( CompileShader( gs_4_0, GSSoftParticles() ) );
        SetPixelShader( CompileShader( ps_4_0, PSSoftParticles_DepthBuffer() ) );
    }
}
///////////////////////////////////////////////////////////////////
// USE a second render target to store Z 'by hand'
///////////////////////////////////////////////////////////////////
//
// In this case, The main scene must output a secont RT for Z
//
Mesh_PSOut PSMeshDepthToRT(Mesh_VSOut input)
{
    Mesh_PSOut output;
    float4 c;
    c.a = 1.0 - saturate(input.tc.z - 4.0); //input.color.a; // The intend would be to 'fade out' the mountain with horizon. But .X cannot for now...
    c.rgb = input.color.bbb + input.color.rrr * texture_mesh.Sample(sampler_linear, input.tc).rgb;
    output.color = c;
    output.depth = input.depth;
    return output;
}
//
// texture_depth was created by the 2nd RT (Mesh_PSOut::depth)
//
SoftParticles_PSOut PSSoftParticles_R32FloatDepth(SoftParticles_GSOut input)
{
    SoftParticles_PSOut output;
    float z = texture_depth.Load( int4(input.position.x, input.position.y, 0,0)).r;
    float zdiff = (z - input.Z);
    float c = Contrast(zdiff * SoftParticleScale, SoftParticleContrast);
    if( (zdiff * c) <= EPSILONZ )
    {
        discard;
    }
    output.color = c * intensity * input.alpha * GetFogTexture(input.tc, debugTex);
    return output;
}

technique10 MRT
{
    pass Mesh
    {
        SetBlendState(blendSrcAlphaInvSrcAlpha, float4(1.0, 1.0, 1.0, 1.0) ,0xffffffff);
        SetRasterizerState( cullDisabled );
        SetDepthStencilState( depthEnabled, 0 );
        SetVertexShader( CompileShader( vs_4_0, VSMesh() ) );
        SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_4_0, PSMeshDepthToRT() ) );
    }
    pass SoftParticles
    {
        SetDepthStencilState( depthEnabledNoWrite, 0 );
        SetRasterizerState( cullDisabled );
        SetBlendState(blendOneInvSrcAlpha, float4(1.0, 1.0, 1.0, 1.0) ,0xffffffff);
        SetVertexShader( CompileShader( vs_4_0, VSSoftParticles() ) );
        SetGeometryShader( CompileShader( gs_4_0, GSSoftParticles() ) );
        SetPixelShader( CompileShader( ps_4_0, PSSoftParticles_R32FloatDepth() ) );
    }
}

///////////////////////////////////////////////////////////////////
// Second Render Target as the depth buffer. Resolving MSAA in the
// Pixel shader... well, we just take only one sample and work on it
// This is largely enough for our soft particles.
// if you want to average depth by Loading all the samples :
// - Averaging a depth value isn't really meaningful
// - this will cost more for almost no improvement
///////////////////////////////////////////////////////////////////
SoftParticles_PSOut PSSoftParticles_R32FloatDepthMSAA(SoftParticles_GSOut input)
{
    SoftParticles_PSOut output;
    float z = texture_depthMS.Load( int2(input.position.x, input.position.y), 0).r;
    float zdiff = (z - input.Z);
    float c = Contrast(zdiff * SoftParticleScale, SoftParticleContrast);
    if( (zdiff * c) <= EPSILONZ )
    {
        discard;
    }
    output.color = c * intensity * input.alpha * GetFogTexture(input.tc, debugTex);
    return output;
}
technique10 MRT_MSAA
{
    pass Mesh
    {
        SetBlendState(blendSrcAlphaInvSrcAlpha, float4(1.0, 1.0, 1.0, 1.0) ,0xffffffff);
        SetRasterizerState( cullDisabled );
        SetDepthStencilState( depthEnabled, 0 );
        SetVertexShader( CompileShader( vs_4_0, VSMesh() ) );
        SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_4_0, PSMeshDepthToRT() ) );
    }
    pass SoftParticles
    {
        SetDepthStencilState( depthEnabledNoWrite, 0 );
        SetRasterizerState( cullDisabled );
        SetBlendState(blendOneInvSrcAlpha, float4(1.0, 1.0, 1.0, 1.0) ,0xffffffff);
        SetVertexShader( CompileShader( vs_4_0, VSSoftParticles() ) );
        SetGeometryShader( CompileShader( gs_4_0, GSSoftParticles() ) );
        SetPixelShader( CompileShader( ps_4_0, PSSoftParticles_R32FloatDepthMSAA() ) );
    }
}

///////////////////////////////////////////////////////////////////
// Reference Case : no soft effect at billboard intersection
///////////////////////////////////////////////////////////////////
SoftParticles_PSOut PSSoftParticles(SoftParticles_GSOut input)
{
    SoftParticles_PSOut output;
    output.color = intensity * input.alpha * GetFogTexture(input.tc, debugTex);
    return output;
}
technique10 Simple
{
    pass Mesh
    {
        SetBlendState(blendSrcAlphaInvSrcAlpha, float4(1.0, 1.0, 1.0, 1.0) ,0xffffffff);
        SetRasterizerState( cullDisabled );
        SetDepthStencilState( depthEnabled, 0 );
        SetVertexShader( CompileShader( vs_4_0, VSMesh() ) );
        SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_4_0, PSMeshNormal() ) );
    }
    pass SoftParticles
    {
        SetDepthStencilState( depthEnabledNoWrite, 0 );
        SetRasterizerState( cullDisabled );
        SetBlendState(blendOneInvSrcAlpha, float4(1.0, 1.0, 1.0, 1.0) ,0xffffffff);
        SetVertexShader( CompileShader( vs_4_0, VSSoftParticles() ) );
        SetGeometryShader( CompileShader( gs_4_0, GSSoftParticles() ) );
        SetPixelShader( CompileShader( ps_4_0, PSSoftParticles() ) );
    }
}


