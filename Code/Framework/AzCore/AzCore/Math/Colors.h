/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/ColorSwatch.h>

namespace AZ
{
    //! Named colors, from the CSS specification:
    //! https://www.w3.org/TR/2011/REC-SVG11-20110816/types.html#ColorKeywords
    //!
    //! These are ColorSwatch rather than Color so that they are usable in constant expressions and
    //! convert implicitly, and exactly, to every AzCore color type:
    //!
    //!     constexpr AZ::Color32 white = AZ::Colors::White;
    //!     constexpr AZ::Color64 red   = AZ::Colors::Red;
    //!     AZ::Color             blue  = AZ::Colors::Blue;
    //!
    //! Because they are `inline constexpr` they have external linkage and a single definition, and
    //! live in read-only data -- there is no per-translation-unit copy and no static initializer.
    namespace Colors
    {
        // Basic Colors
        inline constexpr ColorSwatch White                { 255, 255, 255 };
        inline constexpr ColorSwatch Silver               { 192, 192, 192 };
        inline constexpr ColorSwatch Gray                 { 128, 128, 128 };
        inline constexpr ColorSwatch Black                {   0,   0,   0 };
        inline constexpr ColorSwatch Red                  { 255,   0,   0 };
        inline constexpr ColorSwatch Maroon               { 128,   0,   0 };
        inline constexpr ColorSwatch Lime                 {   0, 255,   0 };
        inline constexpr ColorSwatch Green                {   0, 128,   0 };
        inline constexpr ColorSwatch Blue                 {   0,   0, 255 };
        inline constexpr ColorSwatch Navy                 {   0,   0, 128 };
        inline constexpr ColorSwatch Yellow               { 255, 255,   0 };
        inline constexpr ColorSwatch Orange               { 255, 165,   0 };
        inline constexpr ColorSwatch Olive                { 128, 128,   0 };
        inline constexpr ColorSwatch Purple               { 128,   0, 128 };
        inline constexpr ColorSwatch Fuchsia              { 255,   0, 255 };
        inline constexpr ColorSwatch Teal                 {   0, 128, 128 };
        inline constexpr ColorSwatch Aqua                 {   0, 255, 255 };

        // Reds
        inline constexpr ColorSwatch IndianRed            { 205,  92,  92 };
        inline constexpr ColorSwatch LightCoral           { 240, 128, 128 };
        inline constexpr ColorSwatch Salmon               { 250, 128, 114 };
        inline constexpr ColorSwatch DarkSalmon           { 233, 150, 122 };
        inline constexpr ColorSwatch LightSalmon          { 255, 160, 122 };
        inline constexpr ColorSwatch Crimson              { 220,  20,  60 };
        inline constexpr ColorSwatch FireBrick            { 178,  34,  34 };
        inline constexpr ColorSwatch DarkRed              { 139,   0,   0 };

        // Pinks
        inline constexpr ColorSwatch Pink                 { 255, 192, 203 };
        inline constexpr ColorSwatch LightPink            { 255, 182, 193 };
        inline constexpr ColorSwatch HotPink              { 255, 105, 180 };
        inline constexpr ColorSwatch DeepPink             { 255,  20, 147 };
        inline constexpr ColorSwatch MediumVioletRed      { 199,  21, 133 };
        inline constexpr ColorSwatch PaleVioletRed        { 219, 112, 147 };

        // Oranges
        inline constexpr ColorSwatch Coral                { 255, 127,  80 };
        inline constexpr ColorSwatch Tomato               { 255,  99,  71 };
        inline constexpr ColorSwatch OrangeRed            { 255,  69,   0 };
        inline constexpr ColorSwatch DarkOrange           { 255, 140,   0 };

        // Yellows
        inline constexpr ColorSwatch Gold                 { 255, 215,   0 };
        inline constexpr ColorSwatch LightYellow          { 255, 255, 224 };
        inline constexpr ColorSwatch LemonChiffon         { 255, 250, 205 };
        inline constexpr ColorSwatch LightGoldenrodYellow { 250, 250, 210 };
        inline constexpr ColorSwatch PapayaWhip           { 255, 239, 213 };
        inline constexpr ColorSwatch Moccasin             { 255, 228, 181 };
        inline constexpr ColorSwatch PeachPuff            { 255, 218, 185 };
        inline constexpr ColorSwatch PaleGoldenrod        { 238, 232, 170 };
        inline constexpr ColorSwatch Khaki                { 240, 230, 140 };
        inline constexpr ColorSwatch DarkKhaki            { 189, 183, 107 };

        // Purples
        inline constexpr ColorSwatch Lavender             { 230, 230, 250 };
        inline constexpr ColorSwatch Thistle              { 216, 191, 216 };
        inline constexpr ColorSwatch Plum                 { 221, 160, 221 };
        inline constexpr ColorSwatch Violet               { 238, 130, 238 };
        inline constexpr ColorSwatch Orchid               { 218, 112, 214 };
        inline constexpr ColorSwatch Magenta              { 255,   0, 255 };
        inline constexpr ColorSwatch MediumOrchid         { 186,  85, 211 };
        inline constexpr ColorSwatch MediumPurple         { 147, 112, 219 };
        inline constexpr ColorSwatch BlueViolet           { 138,  43, 226 };
        inline constexpr ColorSwatch DarkViolet           { 148,   0, 211 };
        inline constexpr ColorSwatch DarkOrchid           { 153,  50, 204 };
        inline constexpr ColorSwatch DarkMagenta          { 139,   0, 139 };
        inline constexpr ColorSwatch RebeccaPurple        { 102,  51, 153 };
        inline constexpr ColorSwatch Indigo               {  75,   0, 130 };
        inline constexpr ColorSwatch MediumSlateBlue      { 123, 104, 238 };
        inline constexpr ColorSwatch SlateBlue            { 106,  90, 205 };
        inline constexpr ColorSwatch DarkSlateBlue        {  72,  61, 139 };

        // Greens
        inline constexpr ColorSwatch GreenYellow          { 173, 255,  47 };
        inline constexpr ColorSwatch Chartreuse           { 127, 255,   0 };
        inline constexpr ColorSwatch LawnGreen            { 124, 252,   0 };
        inline constexpr ColorSwatch LimeGreen            {  50, 205,  50 };
        inline constexpr ColorSwatch PaleGreen            { 152, 251, 152 };
        inline constexpr ColorSwatch LightGreen           { 144, 238, 144 };
        inline constexpr ColorSwatch MediumSpringGreen    {   0, 250, 154 };
        inline constexpr ColorSwatch SpringGreen          {   0, 255, 127 };
        inline constexpr ColorSwatch MediumSeaGreen       {  60, 179, 113 };
        inline constexpr ColorSwatch SeaGreen             {  46, 139,  87 };
        inline constexpr ColorSwatch ForestGreen          {  34, 139,  34 };
        inline constexpr ColorSwatch DarkGreen            {   0, 100,   0 };
        inline constexpr ColorSwatch YellowGreen          { 154, 205,  50 };
        inline constexpr ColorSwatch OliveDrab            { 107, 142,  35 };
        inline constexpr ColorSwatch DarkOliveGreen       {  85, 107,  47 };
        inline constexpr ColorSwatch MediumAquamarine     { 102, 205, 170 };
        inline constexpr ColorSwatch DarkSeaGreen         { 143, 188, 143 };
        inline constexpr ColorSwatch LightSeaGreen        {  32, 178, 170 };
        inline constexpr ColorSwatch DarkCyan             {   0, 139, 139 };

        // Blues
        inline constexpr ColorSwatch Cyan                 {   0, 255, 255 };
        inline constexpr ColorSwatch LightCyan            { 224, 255, 255 };
        inline constexpr ColorSwatch PaleTurquoise        { 175, 238, 238 };
        inline constexpr ColorSwatch Aquamarine           { 127, 255, 212 };
        inline constexpr ColorSwatch Turquoise            {  64, 224, 208 };
        inline constexpr ColorSwatch MediumTurquoise      {  72, 209, 204 };
        inline constexpr ColorSwatch DarkTurquoise        {   0, 206, 209 };
        inline constexpr ColorSwatch CadetBlue            {  95, 158, 160 };
        inline constexpr ColorSwatch SteelBlue            {  70, 130, 180 };
        inline constexpr ColorSwatch LightSteelBlue       { 176, 196, 222 };
        inline constexpr ColorSwatch PowderBlue           { 176, 224, 230 };
        inline constexpr ColorSwatch LightBlue            { 173, 216, 230 };
        inline constexpr ColorSwatch SkyBlue              { 135, 206, 235 };
        inline constexpr ColorSwatch LightSkyBlue         { 135, 206, 250 };
        inline constexpr ColorSwatch DeepSkyBlue          {   0, 191, 255 };
        inline constexpr ColorSwatch DodgerBlue           {  30, 144, 255 };
        inline constexpr ColorSwatch CornflowerBlue       { 100, 149, 237 };
        inline constexpr ColorSwatch RoyalBlue            {  65, 105, 225 };
        inline constexpr ColorSwatch MediumBlue           {   0,   0, 205 };
        inline constexpr ColorSwatch DarkBlue             {   0,   0, 139 };
        inline constexpr ColorSwatch MidnightBlue         {  25,  25, 112 };

        // Browns
        inline constexpr ColorSwatch Cornsilk             { 255, 248, 220 };
        inline constexpr ColorSwatch BlanchedAlmond       { 255, 235, 205 };
        inline constexpr ColorSwatch Bisque               { 255, 228, 196 };
        inline constexpr ColorSwatch NavajoWhite          { 255, 222, 173 };
        inline constexpr ColorSwatch Wheat                { 245, 222, 179 };
        inline constexpr ColorSwatch BurlyWood            { 222, 184, 135 };
        inline constexpr ColorSwatch Tan                  { 210, 180, 140 };
        inline constexpr ColorSwatch RosyBrown            { 188, 143, 143 };
        inline constexpr ColorSwatch SandyBrown           { 244, 164,  96 };
        inline constexpr ColorSwatch Goldenrod            { 218, 165,  32 };
        inline constexpr ColorSwatch DarkGoldenrod        { 184, 134,  11 };
        inline constexpr ColorSwatch Peru                 { 205, 133,  63 };
        inline constexpr ColorSwatch Chocolate            { 210, 105,  30 };
        inline constexpr ColorSwatch SaddleBrown          { 139,  69,  19 };
        inline constexpr ColorSwatch Sienna               { 160,  82,  45 };
        inline constexpr ColorSwatch Brown                { 165,  42,  42 };

        // Whites
        inline constexpr ColorSwatch Snow                 { 255, 250, 250 };
        inline constexpr ColorSwatch Honeydew             { 240, 255, 240 };
        inline constexpr ColorSwatch MintCream            { 245, 255, 250 };
        inline constexpr ColorSwatch Azure                { 240, 255, 255 };
        inline constexpr ColorSwatch AliceBlue            { 240, 248, 255 };
        inline constexpr ColorSwatch GhostWhite           { 248, 248, 255 };
        inline constexpr ColorSwatch WhiteSmoke           { 245, 245, 245 };
        inline constexpr ColorSwatch Seashell             { 255, 245, 238 };
        inline constexpr ColorSwatch Beige                { 245, 245, 220 };
        inline constexpr ColorSwatch OldLace              { 253, 245, 230 };
        inline constexpr ColorSwatch FloralWhite          { 255, 250, 240 };
        inline constexpr ColorSwatch Ivory                { 255, 255, 240 };
        inline constexpr ColorSwatch AntiqueWhite         { 250, 235, 215 };
        inline constexpr ColorSwatch Linen                { 250, 240, 230 };
        inline constexpr ColorSwatch LavenderBlush        { 255, 240, 245 };
        inline constexpr ColorSwatch MistyRose            { 255, 228, 225 };

        // Grays
        inline constexpr ColorSwatch Gainsboro            { 220, 220, 220 };
        inline constexpr ColorSwatch LightGray            { 211, 211, 211 };
        inline constexpr ColorSwatch LightGrey            { 211, 211, 211 };
        inline constexpr ColorSwatch DarkGray             { 169, 169, 169 };
        inline constexpr ColorSwatch DarkGrey             { 169, 169, 169 };
        inline constexpr ColorSwatch Grey                 { 128, 128, 128 };
        inline constexpr ColorSwatch DimGray              { 105, 105, 105 };
        inline constexpr ColorSwatch DimGrey              { 105, 105, 105 };
        inline constexpr ColorSwatch LightSlateGray       { 119, 136, 153 };
        inline constexpr ColorSwatch LightSlateGrey       { 119, 136, 153 };
        inline constexpr ColorSwatch SlateGray            { 112, 128, 144 };
        inline constexpr ColorSwatch SlateGrey            { 112, 128, 144 };
        inline constexpr ColorSwatch DarkSlateGray        {  47,  79,  79 };
        inline constexpr ColorSwatch DarkSlateGrey        {  47,  79,  79 };
    } // namespace Colors
} // namespace AZ
