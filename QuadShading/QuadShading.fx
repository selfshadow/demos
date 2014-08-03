
//--------------------------------------------------------------------------------------
// File: PixelQuad.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

RWTexture2D<uint> lockUAV      : register(u0);
RWTexture2D<uint> overdrawUAV  : register(u1);
RWTexture2D<uint> liveCountUAV : register(u2);
RWTexture1D<uint> liveStatsUAV : register(u3);

Texture2D<uint> overdrawSRV  : register(t0);
Texture1D<uint> liveStatsSRV : register(t1);


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------

cbuffer cbChangeOnResize : register(b0)
{
    matrix Projection;
};

cbuffer cbChangesEveryFrame : register(b1)
{
    matrix View;
};


//--------------------------------------------------------------------------------------
float4 SceneVS(float4 pos : POSITION) : SV_POSITION
{
    float4 cpos;
    cpos = mul( pos, View);
    cpos = mul(cpos, Projection);

    return cpos;
}

void SceneDepthPS()
{
}


[earlydepthstencil]
void ScenePS(float4 vpos : SV_Position, uint id : SV_PrimitiveID)
{
    uint2 quad = vpos.xy*0.5;
    uint  prevID;

    uint unlockedID = 0xffffffff;
    bool processed  = false;
    int  lockCount  = 0;
    int  pixelCount = 0;

    for (int i = 0; i < 64; i++)
    {
        if (!processed)
            InterlockedCompareExchange(lockUAV[quad], unlockedID, id, prevID);

        [branch]
        if (prevID == unlockedID)
        {
            if (++lockCount == 4)
            {
                // Retrieve live pixel count (minus 1) in quad
                InterlockedAnd(liveCountUAV[quad], 0, pixelCount);

                // Unlock for other quads
                InterlockedExchange(lockUAV[quad], unlockedID, prevID);
            }
            processed = true;
        }

        if (prevID == id && !processed)
        {
            InterlockedAdd(liveCountUAV[quad], 1);
            processed = true;
        }
    }

    if (lockCount)
    {
        InterlockedAdd(overdrawUAV[quad], 1);
        InterlockedAdd(liveStatsUAV[pixelCount], 1);
    }
}


//--------------------------------------------------------------------------------------
float4 VisVS(float4 pos : POSITION) : SV_Position
{
    return pos;
}


float4 ToColour(uint v)
{
    const uint nbColours = 10;
    const float4 colours[nbColours] =
    {
       float4(0,     0,   0, 255),
       float4(2,    25, 147, 255),
       float4(0,   149, 255, 255),
       float4(0,   253, 255, 255),
       float4(142, 250,   0, 255),
       float4(255, 251,   0, 255),
       float4(255, 147,   0, 255),
       float4(255,  38,   0, 255),
       float4(148,  17,   0, 255),
       float4(255,   0, 255, 255)
    };

    return colours[min(v, nbColours - 1)]/255.0;
}


float4 PieChart(float2 quad)
{
    float t4 = liveStatsSRV[3];
    float t3 = liveStatsSRV[2] + t4;
    float t2 = liveStatsSRV[1] + t3;
    float t1 = liveStatsSRV[0] + t2;

    float4 colour = 0;

    float2 p = quad*0.5 - float2(72, 72);
    if ((p.x*p.x + p.y*p.y) < 64*64)
    {
        float a = t1*(atan2(p.y, p.x + 0.0001)/(2*3.14159265) + 0.5);

             if (a <= t4) colour = ToColour(5);
        else if (a <= t3) colour = ToColour(6);
        else if (a <= t2) colour = ToColour(7);
        else if (a <= t1) colour = ToColour(8);
    }

    return colour;
}


float4 VisPS(float4 vpos : SV_POSITION) : SV_Target
{
    uint2 quad = vpos.xy*0.5;

    uint overdraw = overdrawSRV[quad];
    float4 colour = ToColour(overdraw);

    colour += PieChart(vpos.xy + float2(0.0, 0.0))*0.25;
    colour += PieChart(vpos.xy + float2(0.5, 0.0))*0.25;
    colour += PieChart(vpos.xy + float2(0.0, 0.5))*0.25;
    colour += PieChart(vpos.xy + float2(0.5, 0.5))*0.25;

    return colour;
}