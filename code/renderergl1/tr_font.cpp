/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// tr_font.cpp -- font rendering

#include "tr_local.h"

#define MAX_LOADED_FONTS 255

static fontheader_sgl_t s_loadedFonts_sgl[MAX_LOADED_FONTS];
static int s_numLoadedFonts_sgl = 0;
static fontheader_t s_loadedFonts[MAX_LOADED_FONTS];

cvar_t *vr_traceFont;
static int s_numLoadedFonts = 0;
static float s_fontHeightScale = 1.0;
static float s_fontGeneralScale = 1.0;
static float s_fontZ = 0.0;

void R_ShutdownFont() {
    int i;
    fontheader_t *header;
    fontheader_sgl_t *header_sgl;

    for (i = 0; i < s_numLoadedFonts; i++)
    {
        header = &s_loadedFonts[i];
        if (header->charTable) {
            ri.Free(header->charTable);
            header->charTable = NULL;
        }

        memset(header, 0, sizeof(*header));
    }

    for (i = 0; i < s_numLoadedFonts_sgl; i++)
    {
        header_sgl = &s_loadedFonts_sgl[i];
        memset(header_sgl, 0, sizeof(*header_sgl));
    }
}

void R_SetFontHeightScale(float scale)
{
    s_fontHeightScale = scale;
}

void R_SetFontScale(float scale)
{
    s_fontGeneralScale = scale;
}

void R_SetFontZ(float zed)
{
    s_fontZ = zed;
}

static int CodeSearch(const fontheader_t* font, unsigned short uch) {
    int mid;
    int l, r;

    r = font->charTableLength;
    l = 0;
    while (l < r) {
        mid = (l + r) / 2;
        
        if (font->charTable[mid].cp > uch) {
            r = (l + r) / 2;
            continue;
        }

        if (uch == font->charTable[mid].cp) {
            return (l + r) / 2;
        }

        l = mid + 1;
    }

    if (uch != font->charTable[l].cp) {
        return -1;
    }

    return l;
}

static qboolean DBCSIsLeadByte(const fontheader_t* font, unsigned short uch) {
    // Byte ranges found in Wikipedia articles with relevant search strings in each case
    switch (font->codePage) {
    case 932:
        // Shift_jis
        return ((uch >= 0x81) && (uch <= 0x9F)) ||
            ((uch >= 0xE0) && (uch <= 0xFC));
        // Lead bytes F0 to FC may be a Microsoft addition.
    case 936:
        // GBK
        return (uch >= 0x81) && (uch <= 0xFE);
    case 949:
        // Korean Wansung KS C-5601-1987
        return (uch >= 0x81) && (uch <= 0xFE);
    case 950:
        // Big5
        return (uch >= 0x81) && (uch <= 0xFE);
    case 1361:
        // Korean Johab KS C-5601-1992
        return
            ((uch >= 0x84) && (uch <= 0xD3)) ||
            ((uch >= 0xD8) && (uch <= 0xDE)) ||
            ((uch >= 0xE0) && (uch <= 0xF9));
    }
    return false;
}

fontheader_sgl_t* R_LoadFont_sgl(const char* name)
{
    int i;
    char* theFile;
    fontheader_sgl_t* header;
    char* ref;
    const char* token;
    qboolean error;

    error = qfalse;

    for (i = 0; i < s_numLoadedFonts_sgl; i++)
    {
        header = &s_loadedFonts_sgl[i];
        if (!Q_stricmp(name, header->name)) {
            return header;
        }
    }

    if (s_numLoadedFonts >= MAX_LOADED_FONTS)
    {
        ri.Printf(PRINT_WARNING, "LoadFont: Too many fonts loaded!  Couldn't load %s\n", name);
        return NULL;
    }

    if (ri.FS_ReadFile(va("fonts/%s.RitualFont", name), (void**)&theFile) == -1)
    {
        ri.Printf(PRINT_WARNING, "LoadFont: Couldn't load font %s\n", name);
        return NULL;
    }

    header = &s_loadedFonts_sgl[s_numLoadedFonts_sgl];
    header->height = 0.0;
    header->aspectRatio = 0.0;
    Q_strncpyz(header->name, name, sizeof(header->name));

    ref = theFile;
    while (ref && !error)
    {
        token = COM_Parse(&ref);
        if (!Q_stricmp(token, "RitFont"))
        {
            // ignore this token
            continue;
        }

        if (!Q_stricmp(token, "indirections"))
        {
            token = COM_Parse(&ref);
            if (Q_stricmp(token, "{"))
            {
                error = qtrue;
                break;
            }

            for (i = 0; i < 256; i++)
            {
                token = COM_Parse(&ref);
                if (!token[0]) {
                    error = qtrue;
                    break;
                }

                header->indirection[i] = atoi(token);
            }

            if (error) {
                break;
            }

            token = COM_Parse(&ref);
            if (Q_stricmp(token, "}"))
            {
                error = qtrue;
                break;
            }
        }
        else if (!Q_stricmp(token, "locations"))
        {
            token = COM_Parse(&ref);
            if (Q_stricmp(token, "{"))
            {
                error = qtrue;
                break;
            }

            for (i = 0; i < 256; i++)
            {
                token = COM_Parse(&ref);
                if (Q_stricmp(token, "{")) {
                    error = qtrue;
                    break;
                }

                if (header->aspectRatio == 0.0)
                {
                    ri.Printf(PRINT_WARNING, "WARNING: aspect decl must be before locations in font '%s'", name);
                    break;
                }

                header->locations[i].pos[0] = atof(COM_Parse(&ref)) / 256.0;
                header->locations[i].pos[1] = atof(COM_Parse(&ref)) * header->aspectRatio / 256.0;
                header->locations[i].size[0] = atof(COM_Parse(&ref)) / 256.0;
                header->locations[i].size[1] = atof(COM_Parse(&ref)) * header->aspectRatio / 256.0;
            
                token = COM_Parse(&ref);
                if (Q_stricmp(token, "}"))
                {
                    error = qtrue;
                    break;
                }
            }

            if (error) {
                break;
            }

            token = COM_Parse(&ref);
            if (Q_stricmp(token, "}"))
            {
                error = qtrue;
                break;
            }
        }
        else if (!Q_stricmp(token, "height"))
        {
            // parse the header height
            token = COM_Parse(&ref);
            header->height = atof(token);
        }
        else if (!Q_stricmp(token, "aspect"))
        {
            // parse the aspect ratio
            token = COM_Parse(&ref);
            header->aspectRatio = atof(token);
        }
        else
        {
            // unknown token
            break;
        }
    }

    if (token[0])
    {
        ri.Printf(PRINT_WARNING, "WARNING: Unknown token '%s' parsing font '%s'\n", token, name);
        error = qtrue;
    }

    R_LoadFontShader(header);
    if (!header->height || !header->aspectRatio) {
        // invalid height or aspect ratio
        error = qtrue;
    }

    ri.FS_FreeFile(theFile);
    if (error)
    {
        ri.Printf(3, "WARNING: Error parsing font %s.\n", name);
        return NULL;
    }
    else
    {
        s_numLoadedFonts_sgl++;
        return header;
    }

    return NULL;
}

fontheader_t* R_LoadFont(const char* name) {
    int i;
    char* theFile;
    fontheader_t* header;
    char* ref;
    const char* token;
    qboolean error;
    char* pRitFontNames[32];

    error = qfalse;

    for (i = 0; i < s_numLoadedFonts; i++)
    {
        header = &s_loadedFonts[i];
        if (!Q_stricmp(name, header->name)) {
            return header;
        }
    }

    if (s_numLoadedFonts >= MAX_LOADED_FONTS)
    {
        ri.Printf(PRINT_WARNING, "LoadFont: Too many fonts loaded!  Couldn't load %s\n", name);
        return NULL;
    }

    if (ri.FS_ReadFile(va("fonts/%s.RitualFont", name), (void**)&theFile) == -1)
    {
        ri.Printf(PRINT_WARNING, "LoadFont: Couldn't load font %s\n", name);
        return NULL;
    }

    memset(pRitFontNames, 0, sizeof(pRitFontNames));
    ref = theFile;
    header = &s_loadedFonts[s_numLoadedFonts];

    token = COM_Parse(&ref);
    if (Q_stricmp(token, "RitFontList"))
    {
        if (Q_stricmp(token, "RitFont"))
        {
            ri.Printf(PRINT_WARNING, "LoadFont: Not actual font %s\n", name);
            return NULL;
        }
        header->numPages = 0;
    }
    else
    {
        while (ref && !error)
        {
            token = COM_Parse(&ref);
            if (!Q_stricmp(token, "CodePage"))
            {
                header->codePage = atoi(COM_Parse(&ref));
            }
            else if (!Q_stricmp(token, "Chars"))
            {
                header->charTableLength = atoi(COM_Parse(&ref));
                header->charTable = (fontchartable_t*)ri.Malloc(header->charTableLength * sizeof(fontchartable_t));
                if (!header->charTable)
                {
                    ri.Printf(PRINT_WARNING, "LoadFont: Couldn't alloc mem %s\n", name);
                    error = qtrue;
                    break;
                }
            }
            else if (!Q_stricmp(token, "Pages"))
            {
                header->numPages = atoi(COM_Parse(&ref));
            }
            else if (!Q_stricmp(token, "RitFontName"))
            {
                token = COM_Parse(&ref);
                if (Q_stricmp(token, "{"))
                {
                    ri.Printf(PRINT_WARNING, "LoadFont: Bad Format %s\n", name);
                    error = qtrue;
                    break;
                }

                for (i = 0; i < header->numPages; i++)
                {
                    token = COM_Parse(&ref);
                    if (!token[0])
                    {
                        ri.Printf(PRINT_WARNING, "LoadFont: Bad Token %s\n", name);
                        error = qtrue;
                        break;
                    }

                    pRitFontNames[i] = (char*)ri.Malloc(strlen(token) + 1);
                    if (!pRitFontNames[i])
                    {
                        ri.Printf(PRINT_WARNING, "LoadFont: Couldn't alloc mem %s\n", name);
                        error = qtrue;
                        break;
                    }

                    strcpy(pRitFontNames[i], token);
                }

                if (error) {
                    break;
                }

                token = COM_Parse(&ref);
                if (Q_stricmp(token, "}"))
                {
                    ri.Printf(PRINT_WARNING, "LoadFont: Bad Format %s\n", name);
                    error = qtrue;
                    break;
                }
            }
            else if (!Q_stricmp(token, "CharTable"))
            {
                token = COM_Parse(&ref);
                if (Q_stricmp(token, "{"))
                {
                    ri.Printf(PRINT_WARNING, "LoadFont: Bad Format %s\n", name);
                    error = qtrue;
                    break;
                }

                for (i = 0; i < header->charTableLength; i++) {
                    token = COM_Parse(&ref);
                    if (Q_stricmp(token, "{"))
                    {
                        ri.Printf(PRINT_WARNING, "LoadFont: Bad Token %s\n", name);
                        error = qtrue;
                        break;
                    }

                    header->charTable[i].cp = atoi(COM_Parse(&ref));
                    header->charTable[i].index = atoi(COM_Parse(&ref));
                    header->charTable[i].loc = atoi(COM_Parse(&ref));
                    atoi(COM_Parse(&ref));

                    token = COM_Parse(&ref);
                    if (Q_stricmp(token, "}"))
                    {
                        ri.Printf(PRINT_WARNING, "LoadFont: Bad Format %s\n", name);
                        error = qtrue;
                        break;
                    }
                }

                if (error) {
                    break;
                }

                token = COM_Parse(&ref);
                if (Q_stricmp(token, "}"))
                {
                    ri.Printf(PRINT_WARNING, "LoadFont: Bad Format %s\n", name);
                    error = qtrue;
                    break;
                }
            }
            else
            {
                ri.Printf(PRINT_WARNING, "LoadFont: Bad Token %s\n", name);
                error = qtrue;
                break;
            }
        }
    }

    ri.FS_FreeFile(theFile);

    if (!header->numPages)
    {
        header->charTableLength = 0;
        header->charTable = NULL;
        header->sgl[0] = R_LoadFont_sgl(name);
        if (!header->sgl[0])
        {
            ri.Printf(PRINT_WARNING, "LoadFont: failed %s\n", name);
            return NULL;
        }
    }
    else
    {
        for (i = 0; i < header->numPages; i++)
        {
            header->sgl[i] = R_LoadFont_sgl(pRitFontNames[i]);
            if (!header->sgl[i])
            {
                ri.Printf(PRINT_WARNING, "LoadFont: failed %s(%s)\n", pRitFontNames[i], name);
                error = qtrue;
                break;
            }
        }

        // Free all allocated strings
        for (i = 0; i < header->numPages; i++) {
            ri.Free(pRitFontNames[i]);
        }

        if (error)
        {
            if (header->charTable) {
                ri.Free(header->charTable);
            }

            header->numPages = 0;
            header->charTableLength = 0;
            header->charTable = NULL;
        }
    }

    strcpy(header->name, name);
    s_numLoadedFonts++;
    return header;
}

void R_LoadFontShader(fontheader_sgl_t* font)
{
    int i;
    int save;
    char filename[64];
    shader_t* fontshader;

    save = r_sequencenumber;
    r_sequencenumber = -1;
    Com_sprintf(filename, sizeof(filename), "gfx/fonts/%s", font->name);
    font->shader = R_FindShader(filename, -1, qfalse, qfalse, qfalse, qfalse);
    r_sequencenumber = save;

    if (!font->shader) {
        ri.Error(ERR_DROP, "Could not load font shader for %s\n", filename);
    }

    fontshader = (shader_t*)font->shader;
    if (fontshader->numUnfoggedPasses > 0)
    {
        for (i = 0; i < fontshader->numUnfoggedPasses; i++)
        {
            if (fontshader->unfoggedStages[i] != NULL && fontshader->unfoggedStages[i]->active)
            {
                fontshader->unfoggedStages[i]->rgbGen = CGEN_GLOBAL_COLOR;
                fontshader->unfoggedStages[i]->alphaGen = AGEN_GLOBAL_ALPHA;
            }
        }

        font->trhandle = r_sequencenumber;
    }
    else
    {
        font->trhandle = r_sequencenumber;
    }

    //
    // Fixed in OPM
    //  Keep the atlas alive, and fetch it again if it has already gone.
    //
    //  Every string in the game drew as a solid black rectangle - not a black
    //  box with faint glyphs, which is the missing-alpha symptom, but flat
    //  0,0,0 with no glyph structure in it at all. Measured at the draw call,
    //  the shader's own stage reported texnum 0: no texture. Sampling texture
    //  zero returns opaque black, and opaque black is exactly what the screen
    //  had.
    //
    //  The lifetime is the cause. R_FreeUnusedImages runs at the end of every
    //  registration - RE_EndRegistration, so once per map load - and frees
    //  every image whose r_sequence is neither the current one nor -1, where
    //  freeing means memset of the whole image_t. The font atlas is loaded
    //  once, early, by whoever draws text first; the shader that refers to it
    //  is then found in the shader hash on every later call to this function
    //  and handed straight back, images untouched. So the shader survives a map
    //  load and the image it points at does not, and the pointer is left aimed
    //  at a zeroed slot.
    //
    //  The trick above of forcing r_sequencenumber to -1 across R_FindShader is
    //  meant to prevent exactly this, and it does - but only for the load that
    //  actually creates the image. It does nothing for an atlas that some other
    //  path brought in first at an ordinary sequence number, which is the usual
    //  case: the menus draw text long before any of this runs.
    //
    //  So mark it here, every time, where the font is known to still want it.
    //  And if it has already been taken, ask for it again by name - the name is
    //  gone from the image_t along with everything else, but it is recoverable,
    //  because the font's own name is what built the shader's name to begin
    //  with.
    //
    for (i = 0; i < fontshader->numUnfoggedPasses; i++)
    {
        shaderStage_t *st = fontshader->unfoggedStages[i];
        image_t       *img;

        if (!st || !st->active) {
            continue;
        }

        img = st->bundle[0].image[0];

        if (!img || !img->texnum) {
            char lost[MAX_QPATH];

            //
            //  Asking for it by name is not enough on its own, and that was the
            //  first attempt. R_FindImageFileOld looks in the hash before it
            //  looks on disk, and the entry in the hash is the very image that
            //  has no texture - so it comes straight back, still broken, and
            //  nothing has happened. Measured: name intact, r_sequence pinned,
            //  texnum still zero.
            //
            //  So take it out of the hash first. R_FreeImage unhooks it and
            //  clears the slot, which is what makes the next lookup miss and go
            //  to disk, where a fresh image_t gets a texture of its own. The
            //  name has to be copied out before that, because clearing the slot
            //  takes the name with it.
            //
            if (img && img->imgName[0]) {
                Q_strncpyz(lost, img->imgName, sizeof(lost));
            } else {
                Com_sprintf(lost, sizeof(lost), "gfx/fonts/%s.tga", font->name);
            }

            if (img) {
                R_FreeImage(img);
            }

            img = R_FindImageFileOld(lost, qfalse, qfalse, qfalse, GL_REPEAT, GL_REPEAT);

            if (img) {
                st->bundle[0].image[0] = img;
                ri.Printf(PRINT_ALL, "R_LoadFontShader: reloaded the atlas '%s' for '%s', texnum %d\n",
                    lost, font->name, (int)img->texnum);
            } else {
                ri.Printf(PRINT_WARNING, "R_LoadFontShader: lost the atlas '%s' for '%s' and could not fetch it back\n",
                    lost, font->name);
            }
        }

        if (img) {
            // Never a candidate for R_FreeUnusedImages again. A font outlives
            // any one map, so its atlas has to as well.
            img->r_sequence = -1;
        }
    }

    //
    // Added in OPM, for the port.
    //
    //  Every string in the UI draws as an opaque black box with the glyphs
    //  faintly visible in it, while every image beside it is correct. Both go
    //  through RB_BeginSurface/RB_EndSurface on the same immediate path, so the
    //  difference has to be the shader, and there are exactly two ways it can
    //  come out opaque:
    //
    //    - the lookup missed the script and fell back to a default shader,
    //      which is GLS_DEFAULT and therefore opaque. "default 1" says so;
    //    - the script was found but the atlas lost its alpha on upload, so
    //      GL_SRC_ALPHA blends against 1 everywhere and covers the background.
    //      "internal 0x1907" is GL_RGB and means exactly that; 0x1908 is
    //      GL_RGBA and means the alpha survived.
    //
    //  gfx/fonts/facfont-20.tga is greyscale in RGB - 0 where alpha is 0, about
    //  144 on a glyph - so drawn without a working alpha it is a black box with
    //  grey letters, which is what the device shows.
    //
    //  The stage's blend bits are printed raw. 0x65 is
    //  GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA, what the
    //  script asks for. 0 is GLS_DEFAULT, which is opaque.
    //
    if (vr_traceFont && vr_traceFont->integer) {
        const shaderStage_t *st = fontshader->numUnfoggedPasses ? fontshader->unfoggedStages[0] : NULL;

        ri.Printf(
            PRINT_ALL,
            "font: \"%s\" default %d passes %d | blend 0x%x rgbGen %d alphaGen %d | image \"%s\" internal 0x%x"
            " texnum %d\n",
            fontshader->name,
            (int)fontshader->defaultShader,
            fontshader->numUnfoggedPasses,
            st ? (unsigned int)(st->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) : 0u,
            st ? (int)st->rgbGen : -1,
            st ? (int)st->alphaGen : -1,
            (st && st->bundle[0].image[0]) ? st->bundle[0].image[0]->imgName : "none",
            (st && st->bundle[0].image[0]) ? (unsigned int)st->bundle[0].image[0]->internalFormat : 0u,
            (st && st->bundle[0].image[0]) ? (int)st->bundle[0].image[0]->texnum : -1
        );
    }
}

void R_DrawString_sgl(fontheader_sgl_t* font, const char* text, float x, float y, int maxlen, const float *pvVirtualScreen) {
    float charHeight;
    float startx, starty;
    int i;
    float fWidthScale, fHeightScale;

    i = 0;
    startx = x;
    starty = y;
    if (pvVirtualScreen) {
        if (pvVirtualScreen[0]) {
            fWidthScale = pvVirtualScreen[0];
        } else {
            fWidthScale = (double)glConfig.vidWidth / 640.0;
        }

        if (pvVirtualScreen[1]) {
            fHeightScale = pvVirtualScreen[1];
        } else {
            fHeightScale = (double)glConfig.vidHeight / 480.0;
        }
    }

    if (!font) {
        return;
    }

    R_IssuePendingRenderCommands();

    if (font->trhandle != r_sequencenumber) {
        font->shader = NULL;
    }

    if (!font->shader) {
        R_LoadFontShader(font);
    }

    //
    // Added in OPM
    //  What the GL is actually set to at the moment a string is about to be
    //  drawn, once a second while vr_traceFont is on.
    //
    //  Every string in the eye buffers comes out as a solid black rectangle -
    //  measured off a real eye dump, not a compositor screenshot, so it is not
    //  a compression artefact: the box is 0,0,0 with no glyph structure in it
    //  at all. Opaque and black together rules out most of the candidates. The
    //  atlas is a 32 bit TGA with a real alpha channel, min 0 max 255, and the
    //  loader supports that; the shader is rgbGen global_color, alphaGen
    //  global_alpha over SRC_ALPHA/ONE_MINUS_SRC_ALPHA, which is right; and
    //  color2D cannot be black, because the bars drawn either side of the text
    //  through the same field come out green and amber.
    //
    //  What is left is the texture not reaching the fragments, and that is a
    //  GL state question that no amount of reading the engine will answer -
    //  the state the engine believes it set and the state gl4es actually has
    //  are exactly the two things that come apart here. So this asks the
    //  driver instead of the renderer: what is bound, what is enabled, and
    //  what the blend is really doing.
    //
    if (vr_traceFont && vr_traceFont->integer) {
        static int lastReport;
        int        now = ri.Milliseconds();

        if (now - lastReport > 1000) {
            const shader_t      *sh = (const shader_t *)font->shader;
            const shaderStage_t *st = (sh && sh->numUnfoggedPasses) ? sh->unfoggedStages[0] : NULL;
            GLint                boundTex = 0, srcBlend = 0, dstBlend = 0;
            GLboolean            blendOn = 0, texOn = 0;

            lastReport = now;

            qglGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTex);
            qglGetIntegerv(GL_BLEND_SRC, &srcBlend);
            qglGetIntegerv(GL_BLEND_DST, &dstBlend);
            blendOn = glIsEnabled(GL_BLEND);
            texOn   = glIsEnabled(GL_TEXTURE_2D);

            ri.Printf(
                PRINT_ALL,
                "font draw: \"%s\" color2D %d %d %d %d | blend %d src 0x%x dst 0x%x | tex2d %d bound %d"
                " want %d | img \"%s\" seq %d of %d | scale %.2f\n",
                sh ? sh->name : "none",
                backEnd.color2D[0], backEnd.color2D[1], backEnd.color2D[2], backEnd.color2D[3],
                (int)blendOn, (unsigned int)srcBlend, (unsigned int)dstBlend,
                (int)texOn, (int)boundTex,
                (st && st->bundle[0].image[0]) ? st->bundle[0].image[0]->texnum : -1,
                (st && st->bundle[0].image[0]) ? st->bundle[0].image[0]->imgName : "none",
                (st && st->bundle[0].image[0]) ? st->bundle[0].image[0]->r_sequence : -999,
                r_sequencenumber,
                s_fontGeneralScale
            );
        }
    }

    charHeight = s_fontHeightScale * font->height * s_fontGeneralScale;
    RB_BeginSurface((shader_t*)font->shader);

    for (i = 0; text[i]; i++) {
        unsigned char c;
        int indirected;
        letterloc_t* loc;

        c = text[i];

        if (maxlen != -1 && i >= maxlen) {
            break;
        }

        switch (c)
        {
        case '\t':
            indirected = font->indirection[32];
            if (indirected == -1) {
                ri.Printf(PRINT_DEVELOPER, "R_DrawString: no space-character in font!\n");
            } else {
                x = s_fontGeneralScale * font->locations[indirected].size[0] * 256.0 * 3.0 + x;
            }
            break;

        case '\n':
            starty = charHeight + starty;
            x = startx;
            y = starty;
            break;

        case '\r':
            x = startx;
            break;

        default:
            indirected = font->indirection[c];
            if (indirected == -1)
            {
                ri.Printf(PRINT_DEVELOPER, "R_DrawString: no 0x%02x-character in font!\n", c);
                indirected = font->indirection['?'];
                if (indirected == -1) {
                    ri.Printf(PRINT_DEVELOPER, "R_DrawString: no '?' character in font!\n");
                    break;
				}
                // set the indirection for the next time
				font->indirection[c] = indirected;
            }

            RB_CHECKOVERFLOW(4, 6);

            loc = &font->locations[indirected];

            // texture coordinates
            tess.texCoords[tess.numVertexes][0][0] = loc->pos[0];
            tess.texCoords[tess.numVertexes][0][1] = loc->pos[1];
            tess.texCoords[tess.numVertexes + 1][0][0] = loc->size[0] + loc->pos[0];
            tess.texCoords[tess.numVertexes + 1][0][1] = loc->pos[1];
            tess.texCoords[tess.numVertexes + 2][0][0] = loc->pos[0];
            tess.texCoords[tess.numVertexes + 2][0][1] = loc->size[1] + loc->pos[1];
            tess.texCoords[tess.numVertexes + 3][0][0] = loc->size[0] + loc->pos[0];
            tess.texCoords[tess.numVertexes + 3][0][1] = loc->size[1] + loc->pos[1];

            // vertices position
            tess.xyz[tess.numVertexes][0] = x;
            tess.xyz[tess.numVertexes][1] = y;
            tess.xyz[tess.numVertexes][2] = s_fontZ;
            tess.xyz[tess.numVertexes + 1][0] = x + s_fontGeneralScale * loc->size[0] * 256.0;
            tess.xyz[tess.numVertexes + 1][1] = y;
            tess.xyz[tess.numVertexes + 1][2] = s_fontZ;
            tess.xyz[tess.numVertexes + 2][0] = x;
            tess.xyz[tess.numVertexes + 2][1] = y + charHeight;
            tess.xyz[tess.numVertexes + 2][2] = s_fontZ;
            tess.xyz[tess.numVertexes + 3][0] = x + s_fontGeneralScale * loc->size[0] * 256.0;
            tess.xyz[tess.numVertexes + 3][1] = y + charHeight;
            tess.xyz[tess.numVertexes + 3][2] = s_fontZ;

            // indices
            tess.indexes[tess.numIndexes] = tess.numVertexes;
            tess.indexes[tess.numIndexes + 1] = tess.numVertexes + 1;
            tess.indexes[tess.numIndexes + 2] = tess.numVertexes + 2;
            tess.indexes[tess.numIndexes + 3] = tess.numVertexes + 1;
            tess.indexes[tess.numIndexes + 4] = tess.numVertexes + 3;
            tess.indexes[tess.numIndexes + 5] = tess.numVertexes + 2;

            if (pvVirtualScreen)
            {
                // scale the string properly if virtual screen
                tess.xyz[tess.numVertexes][0] *= fWidthScale;
                tess.xyz[tess.numVertexes][1] *= fHeightScale;
                tess.xyz[tess.numVertexes + 1][0] *= fWidthScale;
                tess.xyz[tess.numVertexes + 1][1] *= fHeightScale;
                tess.xyz[tess.numVertexes + 2][0] *= fWidthScale;
                tess.xyz[tess.numVertexes + 2][1] *= fHeightScale;
                tess.xyz[tess.numVertexes + 3][0] *= fWidthScale;
                tess.xyz[tess.numVertexes + 3][1] *= fHeightScale;
            }

            x += s_fontGeneralScale * loc->size[0] * 256.0;
            tess.numVertexes += 4;
            tess.numIndexes += 6;
            break;
        }
    }

    RB_EndSurface();
}

void R_DrawString(fontheader_t* font, const char* text, float x, float y, int maxlen, const float *pvVirtualScreen) {
    int i;
    int code;
    unsigned short uch;
    char buffer[512];
    size_t buflen;
    int cursgl;
    float curX, curY;
    float curHeight;
    
    if (!font->numPages) {
        if (font->sgl[0]) {
            R_DrawString_sgl(font->sgl[0], text, x, y, maxlen, pvVirtualScreen);
        }
        return;
    }

    if (maxlen < 0) {
        maxlen = strlen(text);
    }
    
    curX = x;
    curY = y;
    curHeight = 0.f;
    cursgl = -1;
    buflen = 0;

    i = 0;
    while(i < maxlen) {
        fontchartable_t* ct;

        uch = text[i];
        i++;

        if (DBCSIsLeadByte(font, uch)) {
            uch = (uch << 8) | text[i];
            i++;
        }

        if (!uch) {
            break;
        }

        if (uch == '\n' || uch == '\r') {
            buffer[buflen] = 0;
            R_DrawString_sgl(font->sgl[cursgl], buffer, curX, curY, maxlen, pvVirtualScreen);
            
            curX = x;
            curHeight = 0.f;
            buflen = 0;
            if (uch == '\n') {
                curY += font->sgl[0]->height * s_fontGeneralScale * s_fontHeightScale;
            }

            continue;
        }

        code = CodeSearch(font, uch);
        if (code < 0) {
            continue;
        }

        if (cursgl == -1) {
            cursgl = font->charTable[code].index;
        }

        ct = &font->charTable[code];
        if (cursgl != ct->index || buflen >= ARRAY_LEN(buffer) - 2) {
            buffer[buflen] = 0;
            R_DrawString_sgl(font->sgl[cursgl], buffer, curX, curY, maxlen, pvVirtualScreen);

            curX += curHeight;
            curHeight = 0.f;
            buflen = 0;
            ct = &font->charTable[code];
            cursgl = ct->index;
        }

        buffer[buflen++] = ct->loc;
        curHeight += font->sgl[cursgl]->locations[ct->loc].size[0] * 256.0f;
    }

    if (buflen)
    {
        buffer[buflen] = 0;
        R_DrawString_sgl(font->sgl[cursgl], buffer, curX, y, buflen, pvVirtualScreen);
    }
}

void R_DrawFloatingString_sgl(fontheader_sgl_t* font, const char* text, const vec3_t org, const vec4_t color, float scale, int maxlen) {
    shader_t* fontshader;
    qhandle_t fsh;
    float charWidth, charHeight;
    int i;
    vec3_t pos;
    polyVert_t verts[4];

    if (!font) {
        return;
    }

    R_IssuePendingRenderCommands();
    if (font->trhandle != r_sequencenumber) {
        font->shader = NULL;
    }

    if (!font->shader) {
        R_LoadFontShader(font);
    }

    i = 0;
    fontshader = (shader_t*)font->shader;
    fsh = 0;

    for (i = 0; i < tr.numShaders; i++)
    {
        if (fontshader == tr.shaders[i])
        {
            fsh = i;
            break;
        }
    }

    i = 0;
    charHeight = font->height * s_fontHeightScale * s_fontGeneralScale * scale;
    VectorCopy(org, pos);

    for (i = 0; text[i]; i++) {
        unsigned char c;
        int indirected;
        letterloc_t* loc;
        
        c = text[i];
        indirected = font->indirection[c];
        if (indirected == -1)
        {
            ri.Printf(PRINT_ALL, "R_DrawFloatingString: no 0x%02x-character in font!\n", c);
            continue;
        }

        loc = &font->locations[indirected];

        // vertices color
        verts[0].modulate[0] = (int)(color[0] * 255.0);
        verts[0].modulate[1] = (int)(color[1] * 255.0);
        verts[0].modulate[2] = (int)(color[2] * 255.0);
        verts[0].modulate[3] = (int)(color[3] * 255.0);
        verts[1].modulate[0] = verts[0].modulate[0];
        verts[1].modulate[1] = verts[0].modulate[1];
        verts[1].modulate[2] = verts[0].modulate[2];
        verts[1].modulate[3] = verts[0].modulate[3];
        verts[2].modulate[0] = verts[0].modulate[0];
        verts[2].modulate[1] = verts[0].modulate[1];
        verts[2].modulate[2] = verts[0].modulate[2];
        verts[2].modulate[3] = verts[0].modulate[3];
        verts[3].modulate[0] = verts[0].modulate[0];
        verts[3].modulate[1] = verts[0].modulate[1];
        verts[3].modulate[2] = verts[0].modulate[2];
        verts[3].modulate[3] = verts[0].modulate[3];

        // texture coordinates
        verts[0].st[0] = loc->pos[0];
        verts[0].st[1] = loc->pos[1];
        verts[1].st[0] = loc->pos[0] + font->locations[indirected].size[0];
        verts[1].st[1] = loc->pos[1];
        verts[2].st[0] = verts[1].st[0];
        verts[2].st[1] = loc->pos[1] + loc->size[1];
        verts[3].st[0] = loc->pos[0];
        verts[3].st[1] = verts[2].st[1];
        VectorCopy(pos, verts[3].xyz);

        charWidth = font->locations[indirected].size[0] * 256.0 * s_fontGeneralScale * scale;
        verts[2].xyz[0] = pos[0] + tr.refdef.viewaxis[1][0] * -charWidth;
        verts[2].xyz[1] = pos[1] + tr.refdef.viewaxis[1][1] * -charWidth;
        verts[2].xyz[2] = pos[2] + tr.refdef.viewaxis[1][2] * -charWidth;
        verts[1].xyz[0] = verts[2].xyz[0] + charHeight * tr.refdef.viewaxis[2][0];
        verts[1].xyz[1] = verts[2].xyz[1] + charHeight * tr.refdef.viewaxis[2][1];
        verts[1].xyz[2] = verts[2].xyz[2] + charHeight * tr.refdef.viewaxis[2][2];
        verts[0].xyz[1] = verts[1].xyz[1] + tr.refdef.viewaxis[1][1] * charWidth;
        verts[0].xyz[2] = verts[1].xyz[2] + tr.refdef.viewaxis[1][2] * charWidth;
        verts[0].xyz[0] = verts[1].xyz[0] + tr.refdef.viewaxis[1][0] * charWidth;
    
        if (RE_AddPolyToScene(fsh, 4, verts, 0)) {
            ++tr.refdef.numPolys;
        }

        pos[0] = verts[2].xyz[0];
        pos[1] = verts[2].xyz[1];
        pos[2] = verts[2].xyz[2];
    }
}

void R_DrawFloatingString(fontheader_t* font, const char* text, const vec3_t org, const vec4_t color, float scale, int maxlen) {
    return R_DrawFloatingString_sgl(font->sgl[0], text, org, color, scale, maxlen);
}

float R_GetFontHeight(const fontheader_t* font)
{
    if (!font || !font->sgl[0]) {
        return 0.0;
    }

    return font->sgl[0]->height * s_fontGeneralScale * s_fontHeightScale;
}

float R_GetFontStringWidth_sgl(const fontheader_sgl_t* font, const char* s)
{
    float widths;
    int i;

    widths = 0.0;

    if (!font) {
        return 0.0;
    }

    for (i = 0; s[i]; i++)
    {
        int indirected;
        char c = *s;

        if (c == '\t')
        {
            indirected = font->indirection[32];
            if (indirected != -1) {
                widths += font->locations[indirected].size[0] * 3.0;
            } else {
                ri.Printf(PRINT_ALL, "R_GetFontStringWidth: no space-character in font!\n");
            }
        }
        else
        {
            indirected = font->indirection[c];
            if (indirected != -1) {
                widths += font->locations[indirected].size[0];
            } else {
                ri.Printf(PRINT_ALL, "R_GetFontStringWidth: no 0x%02x-character in font!\n", c);
            }
        }
    }

    return widths * s_fontGeneralScale * 256.0;
}

float R_GetFontStringWidth(const fontheader_t* font, const char* s)
{
    int i;
    int code;
    fontchartable_t* ct;
    float width = 0.f;

    if (!font->numPages) {
        return R_GetFontStringWidth_sgl(font->sgl[0], s);
    }

    i = 0;
    while(s[i]) {
        unsigned char uch = s[i++];

        if (DBCSIsLeadByte(font, uch)) {
            uch = (uch << 8) | s[i++];
            if (!uch) {
                break;
            }
        }

        if (uch == '\t') {
            code = CodeSearch(font, ' ');
            if (code >= 0) {
                ct = &font->charTable[code];
                width += font->sgl[ct->index]->locations[ct->loc].size[0] * 3.f;
            }
        }
        else {
            code = CodeSearch(font, uch);
            if (code >= 0) {
                ct = &font->charTable[code];
                width += font->sgl[ct->index]->locations[ct->loc].size[0];
            }
        }
    }

    return width;
}
