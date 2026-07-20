/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "GlyphData.slang"

// This class allows procedural printing of characters during a fullscreen triangle pass
public struct GlyphRender
{
    // public:

    [mutating] public void Init(uint2 resolution)
    {
        m_backbufferResolution = resolution;       
        m_glyphColor = 1;
        m_dropShadowColor = float4(0,0,0,1);        
    }

    [mutating] public float4 DrawNumberScreenSpace(const float value, const int2 screenCoord, const int2 origin, const int scale)
    {
        m_screenSpaceScale = scale;
        const uint mask = PrintInsideTile(value, origin, screenCoord);
        if( (mask & MaskForeground) != 0 )
        {
            return m_glyphColor;   
        }   
        else
        {   
            const uint maskDropShadow = PrintInsideTile(value, origin, screenCoord - int2(1,1)); 
            if( (maskDropShadow & MaskForeground) != 0 )
            {
                return m_dropShadowColor;
            }
            else
            {
                return 0; 
            }
        }    
    }
    
    [mutating] public float4 DrawNumberUvSpace(const float value, const float2 uv, const float2 origin, const float scale)
    {
        return DrawNumberScreenSpace(
            value, 
            uv * m_backbufferResolution, 
            origin * m_backbufferResolution, 
            scale * m_backbufferResolution.x * 0.01);  // multiply by a scale factor so that the user can use call this function with values in the 0-1 range for reasonably sized text
    }
    
    // private:
       
    // Using a macro because passing arrays as const uint glyphArray[] doesn't compile in azsl at this time.
#define SAMPLE_GLYPH(index, glyphArray)  \
    { \
        m_localPosition.x += m_localSize.x; \
        if( m_localPosition.x < m_localSize.x ) \
        { \
            m_localPosition.xy /= m_screenSpaceScale; \
            mask = (glyphArray[m_localPosition.y + index * GLYPH_HEIGHT] & (1u << m_localPosition.x)) != 0 ? MaskForeground : MaskBackground; \
        } \
    }       
       
    [mutating] public void InitPrintCoords(uint2 uv, uint2 origin)
    {        
        m_localPosition = uv - origin;
        m_localSize = uint2(GLYPH_CHAR_WIDTH + 1, GLYPH_HEIGHT) * m_screenSpaceScale;
    }    
    
    [mutating] public uint PrintInsideTile(const float numberToPrint, const uint2 origin, const uint2 uv)
    {
        InitPrintCoords(uv, origin);
        uint mask = MaskUninitialized;
           
        if( m_localPosition.y < m_localSize.y )
        {
            mask = PrintInsideTileImpl(numberToPrint);    
        } 
        return mask; 
    }
    
    public float RoundToTensPlace(float value)
    {
        value *= 10;
        value = round(value);
        value /= 10;
        return value;
    }
       
    public uint PrintInsideTileImpl(float value)
    {         
        value = RoundToTensPlace(value);
              
        uint mask = MaskUninitialized;
        uint valueAbs = abs(value);

        const float fractional = frac(value);       
        if (fractional != 0)
        {
            int digit = round(fractional * 10);
            SAMPLE_GLYPH(digit, NumberGlyphs);            
            SAMPLE_GLYPH(0, DotGlyph);         
        }
                       
        while( valueAbs > 0 && mask == MaskUninitialized )
        {
            const uint n = valueAbs % 10;
            valueAbs /= 10;

            SAMPLE_GLYPH(n, NumberGlyphs);
        }

        if (value < 0)
        {
            SAMPLE_GLYPH(0, MinusGlyph); 
        }
        else if (value == 0)
        {
            SAMPLE_GLYPH(0, NumberGlyphs);         
        }

        return mask;
    }       
  
    public enum MaskState
    {
        MaskUninitialized = 0,
        MaskBackground = 0x1,
        MaskForeground = 0x2
    };
  
    public float4 m_glyphColor;
    public float4 m_dropShadowColor;
    public uint2 m_localPosition;
    public uint2 m_localSize;
    public uint m_screenSpaceScale;
    public uint2 m_backbufferResolution;
};
