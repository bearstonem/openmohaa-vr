/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

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

// DESCRIPTION:
// The heads up display, gathered into one group for VR.
//
// Added in OPM.
//
// The retail HUD is five separate menus loaded from ui/hud_*.urc - health,
// ammo, compass, weapons, items - each aligned to a different corner of a 4:3
// screen. On a monitor those corners are a glance away. In a headset they are
// the far edge of the lens: technically visible, practically not, and no amount
// of moving them solves it, because they are five widgets that each carry their
// own frame, their own alignment, and their own 2D window.
//
// So this does not move them. It draws the same information again, in one
// place, from the same source they read from.
//
// That source turns out to be almost nothing: every one of those widgets is a
// formatter over cl.snap.ps.stats[], a flat array of thirty-two integers, and
// the .urc files are pure layout. hud_health.urc is "playerstat 0, maxplayerstat
// 1, draw it as a vertical bar with this shader". cgame has the same array as
// cg.snap->ps.stats[], so nothing has to be read back out of the UI at all.
//
// Drawn from cgame rather than as another widget for one reason beyond taste:
// cgame's 2D goes through the single full screen window that UIView3D::set2D
// sets up, and R_VRAdjust2DRect converges that window as a whole. A widget
// would arrive with its own window and land back in the bug that made the HUD
// impossible to fuse in the first place.
//
// The compass was an upright ring with letters orbiting it to begin with,
// because turning the retail card needs rotated geometry and cgame had no
// import that could draw any. It read as a blank plate, which is what a compass
// is without its dial, so R_DrawTrianglePic was added to the cgame imports - the
// UI has always had it, as re.DrawTrianglePic, and it is what the retail compass
// turns its own face with - and the three real layers are drawn here now.
//
// Still not the whole of the retail compass: the damage direction ring is not
// drawn, and the objective is a pip rather than compassobjarrow. And the one
// worth having is a different thing altogether - a compass the player pulls out
// of a pocket and holds.

#include "cg_local.h"

//
// The layout, in design units. The cluster is authored in a box this size and
// then scaled to the display, so the numbers below can be read as a picture
// rather than as a resolution.
//
//     +------------------------------------------+
//     |  (compass)   Mission Objectives:         |
//     |    ring      - take the bunker           |
//     |              - find the radio            |
//     |  HEALTH [====----]   AMMO [======--] 8|64|
//     |            M1 GARAND                     |
//     |            [pistol][rifle][smg][mg]      |
//     +------------------------------------------+
//
#define VRHUD_DESIGN_W    420.0f
#define VRHUD_DESIGN_H    190.0f

// The design box is authored against a 1080 line display and scaled from there,
// so it keeps the same angular size on any eye resolution.
#define VRHUD_DESIGN_REF  1080.0f

#define VRHUD_RING_R      50.0f  // compass ring radius
#define VRHUD_RING_CX     -150.0f
#define VRHUD_RING_CY     -50.0f

#define VRHUD_OBJ_X       -92.0f // objectives column, left edge
#define VRHUD_OBJ_TOP     -92.0f

#define VRHUD_BAR_H       14.0f
#define VRHUD_BAR_W       104.0f
#define VRHUD_BAR_Y       26.0f
#define VRHUD_HEALTH_X    -196.0f
#define VRHUD_AMMO_X      36.0f

#define VRHUD_NAME_Y      54.0f
#define VRHUD_WEAP_Y      72.0f
#define VRHUD_WEAP_W      42.0f
#define VRHUD_WEAP_H      21.0f

// Text is drawn by the font at its own size, so it is scaled separately from
// the boxes and bars - a 20 pixel font shrunk to fit a design box is not
// legible through a lens.
#define VRHUD_TEXT_SCALE  1.15f

typedef struct {
    qboolean  registered;
    qhandle_t compassBack;
    qhandle_t compassFace;
    qhandle_t compassNeedle;
    qhandle_t weapPistol;
    qhandle_t weapRifle;
    qhandle_t weapSmg;
    qhandle_t weapMg;
} vrHudMedia_t;

static vrHudMedia_t vrHud;

/*
================
CG_VRHudRegister

Deliberately lazy rather than in CG_RegisterGraphics. This draws nothing until
the player is in a map with a snapshot, by which time the shaders are loadable;
registering from the media pass would put these in front of every level load
whether the HUD is switched on or not.

The handles are dropped whenever the renderer's sequence changes underneath
them, which a map change does, so this re-registers on the first frame of each
map rather than holding a handle into the previous one.
================
*/
static void CG_VRHudRegister(void)
{
    static int registeredMap = -1;

    if (vrHud.registered && registeredMap == cgs.levelStartTime) {
        return;
    }

    registeredMap = cgs.levelStartTime;

    vrHud.compassBack   = cgi.R_RegisterShaderNoMip("textures/hud/compassback");
    vrHud.compassFace   = cgi.R_RegisterShaderNoMip("textures/hud/compassface");
    vrHud.compassNeedle = cgi.R_RegisterShaderNoMip("textures/hud/compassneedle");
    vrHud.weapPistol  = cgi.R_RegisterShaderNoMip("textures/hud/weap_pistol");
    vrHud.weapRifle   = cgi.R_RegisterShaderNoMip("textures/hud/weap_rifle");
    vrHud.weapSmg     = cgi.R_RegisterShaderNoMip("textures/hud/weap_smg");
    vrHud.weapMg      = cgi.R_RegisterShaderNoMip("textures/hud/weap_mg");

    vrHud.registered = qtrue;
}

//
// The mapping from design units to pixels for this frame. Held in file statics
// because every helper below needs them and threading them through each call
// would be four more arguments saying the same thing.
//
static float vrHudScale;    // pixels per design unit
static float vrHudOriginX;  // pixels, centre of the cluster
static float vrHudOriginY;

static float CG_VRHudX(float designX)
{
    return vrHudOriginX + designX * vrHudScale;
}

static float CG_VRHudY(float designY)
{
    return vrHudOriginY + designY * vrHudScale;
}

/*
================
CG_VRHudFill

A rectangle in design units, in a colour. Everything here that is not a piece of
retail art is one of these - bars, pips, the marker on the compass, the backing
plate - because a flat quad needs no asset to go looking for and stays legible
at any size.

Drawn with R_DrawBox, which is untextured and takes the colour straight from
R_SetColor, rather than by stretching a white shader.

That was the first attempt and it was wrong twice over. gfx/2d/blank - the
obvious candidate, and what cgame already registers for the lagometer - is
declared in scripts/gfx.shader as

    alphaGen oneMinusVertex
    rgbGen vertex

and that alphaGen is not a detail. It inverts the alpha: a bar asked for at
alpha 1 comes out at 0 and a backing plate asked for at 0.35 comes out at 0.65,
so the things meant to be solid were the faintest and the thing meant to be
faint was the most solid. Anything else built to be tinted has its own opinion
of the same sort. R_DrawBox has none - it is qglColor4ubv and a quad.
================
*/
static void CG_VRHudFill(float x, float y, float w, float h, float r, float g, float b, float a)
{
    vec4_t color;

    color[0] = r;
    color[1] = g;
    color[2] = b;
    color[3] = a;

    cgi.R_SetColor(color);
    cgi.R_DrawBox(CG_VRHudX(x), CG_VRHudY(y), w * vrHudScale, h * vrHudScale);
}

/*
================
CG_VRHudPic

A shader stretched over a rectangle in design units, at full brightness or
dimmed. Dimming rather than hiding is what lets the weapon strip show which
slots exist as well as which are filled.
================
*/
static void CG_VRHudPic(float x, float y, float w, float h, qhandle_t shader, float bright, float alpha)
{
    vec4_t color;

    if (!shader) {
        return;
    }

    color[0] = bright;
    color[1] = bright;
    color[2] = bright;
    color[3] = alpha;

    cgi.R_SetColor(color);
    cgi.R_DrawStretchPic(
        CG_VRHudX(x), CG_VRHudY(y), w * vrHudScale, h * vrHudScale, 0.0, 0.0, 1.0, 1.0, shader
    );
}

/*
================
CG_VRHudRotatedPic

A square of art centred on a point in design units, turned clockwise by an angle
in degrees.

R_DrawStretchPic cannot do this - it takes a rectangle and draws it square to
the screen - so the quad is built by hand as two triangles and handed to
R_DrawTrianglePic, which is the same call the retail compass uses to turn its
own face.

Screen space has y running down, so a clockwise turn is

    x' =  dx cos - dy sin
    y' =  dx sin + dy cos

which sends the top corner to the right, as a compass card should when the
player turns to the left.
================
*/
static void CG_VRHudRotatedPic(float cx, float cy, float radius, float degrees, qhandle_t shader, float alpha)
{
    static const float corner[4][2] = {
        {-1.0f, -1.0f},
        { 1.0f, -1.0f},
        {-1.0f,  1.0f},
        { 1.0f,  1.0f}
    };
    static const float uv[4][2] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f}
    };
    vec2_t pts[4];
    vec2_t tri[3], triUV[3];
    vec4_t color;
    float  s, c;
    int    i;

    if (!shader) {
        return;
    }

    s = sin(DEG2RAD(degrees));
    c = cos(DEG2RAD(degrees));

    for (i = 0; i < 4; i++) {
        const float dx = corner[i][0] * radius;
        const float dy = corner[i][1] * radius;

        pts[i][0] = CG_VRHudX(cx + (dx * c - dy * s));
        pts[i][1] = CG_VRHudY(cy + (dx * s + dy * c));
    }

    color[0] = color[1] = color[2] = 1.0f;
    color[3] = alpha;
    cgi.R_SetColor(color);

    // Two triangles, wound the same way round: 0-1-2 and 1-3-2.
    for (i = 0; i < 2; i++) {
        static const int index[2][3] = {
            {0, 1, 2},
            {1, 3, 2}
        };
        int k;

        for (k = 0; k < 3; k++) {
            tri[k][0]   = pts[index[i][k]][0];
            tri[k][1]   = pts[index[i][k]][1];
            triUV[k][0] = uv[index[i][k]][0];
            triUV[k][1] = uv[index[i][k]][1];
        }

        cgi.R_DrawTrianglePic(tri, triUV, shader);
    }
}

/*
================
CG_VRHudText

A string with its top left at a point in design units.

R_DrawString multiplies both the glyph size and the position by the scale vector
it is handed, so the position has to be divided back out by it - which is why
the coordinates given here are not the coordinates it receives. The same idiom
is in CG_DrawObjectives.
================
*/
static void CG_VRHudText(float x, float y, const char *text, float r, float g, float b, float a)
{
    vec4_t color;
    vec2_t textScale;
    float  s;

    if (!text || !text[0] || !cgs.media.objectiveFont) {
        return;
    }

    s            = vrHudScale * VRHUD_TEXT_SCALE;
    textScale[0] = s;
    textScale[1] = s;

    color[0] = r;
    color[1] = g;
    color[2] = b;
    color[3] = a;

    cgi.R_SetColor(color);
    cgi.R_DrawString(cgs.media.objectiveFont, text, CG_VRHudX(x) / s, CG_VRHudY(y) / s, -1, textScale);
}

/*
================
CG_VRHudBar

A meter: a dark trough with a coloured fill and a thin outline, in design units.

Drawn as flat quads rather than with textures/hud/healthmeter because the retail
meters are tall narrow art built to stand on end at the edge of a screen, and
this cluster wants them lying down next to each other.
================
*/
static void CG_VRHudBar(float x, float y, float frac, float r, float g, float b, float alpha)
{
    if (frac < 0.0f) {
        frac = 0.0f;
    } else if (frac > 1.0f) {
        frac = 1.0f;
    }

    CG_VRHudFill(x - 1.0f, y - 1.0f, VRHUD_BAR_W + 2.0f, VRHUD_BAR_H + 2.0f, 0.0f, 0.0f, 0.0f, alpha * 0.55f);
    CG_VRHudFill(x, y, VRHUD_BAR_W, VRHUD_BAR_H, 0.12f, 0.12f, 0.10f, alpha * 0.85f);

    if (frac > 0.0f) {
        CG_VRHudFill(x, y, VRHUD_BAR_W * frac, VRHUD_BAR_H, r, g, b, alpha);
    }
}

/*
================
CG_VRHudCompass

An upright ring with the cardinal letters orbiting it and a pip for the current
objective.

Two angles do all of it, and both are decoded exactly the way the retail widgets
decode them, so they agree with the game rather than with a guess:

  - Heading. UIFakkLabel::DrawStatCompass takes the eye angles and subtracts
    stats[STAT_COMPASSNORTH] / 182, which is a sixteen bit angle - 65536 over
    360. That difference is how far the player is turned from the map's north.
  - The objective. hud_compass.urc draws it as "statbar spinner 0 3600" with
    end angles -180 to 180, and DrawStatSpinner turns that into
    stat / 3600 * 360 - 180 degrees. It is a plain spinner and not a
    headingspinner, which is the variant that subtracts the view angles - so
    the game has already made it relative to where the player is looking, and
    it can be used as a screen angle directly.

Screen angles here are measured clockwise from the top of the ring. A direction
whose world yaw is a sits at yaw - a, because yaw increases anticlockwise in
this engine and clockwise is what a compass face reads.
================
*/
static void CG_VRHudCompass(float alpha)
{
    vec3_t eyeAngles;
    float  heading;

    //
    // Three layers, the same three and in the same order as hud_compass.urc:
    // the brass plate, the card turning on it, and the needle over the top.
    //
    CG_VRHudPic(
        VRHUD_RING_CX - VRHUD_RING_R,
        VRHUD_RING_CY - VRHUD_RING_R,
        VRHUD_RING_R * 2.0f,
        VRHUD_RING_R * 2.0f,
        vrHud.compassBack,
        1.0f,
        alpha
    );

    CG_EyeAngles(&eyeAngles);
    heading = eyeAngles[YAW] - cg.snap->ps.stats[STAT_COMPASSNORTH] / 182.0f;

    //
    // The card, turned clockwise by the heading.
    //
    // North is at the top of the texture, so turning the whole face by the
    // heading puts north wherever north actually is - and because the engine's
    // yaw increases anticlockwise while a compass reads clockwise, turning the
    // player to the left walks the card to the right, which is what a compass
    // held in the hand does.
    //
    CG_VRHudRotatedPic(
        VRHUD_RING_CX, VRHUD_RING_CY, VRHUD_RING_R * 0.94f, heading, vrHud.compassFace, alpha
    );

    //
    // The objective, before the needle so the needle stays on top of it.
    //
    // stats[STAT_OBJECTIVECENTER] is already relative to where the player is
    // looking - hud_compass.urc draws it as a plain spinner rather than the
    // headingspinner variant, and it is the headingspinner that subtracts the
    // view angles - so this is a screen angle as it stands. The decode is the
    // one DrawStatSpinner does for "statbar spinner 0 3600" with end angles
    // -180 to 180.
    //
    if (cg.snap->ps.stats[STAT_OBJECTIVECENTER]) {
        const float theta = DEG2RAD(cg.snap->ps.stats[STAT_OBJECTIVECENTER] / 10.0f - 180.0f);
        const float px    = VRHUD_RING_CX + sin(theta) * VRHUD_RING_R * 0.78f;
        const float py    = VRHUD_RING_CY - cos(theta) * VRHUD_RING_R * 0.78f;

        CG_VRHudFill(px - 3.5f, py - 3.5f, 7.0f, 7.0f, 0.1f, 0.1f, 0.1f, alpha);
        CG_VRHudFill(px - 2.5f, py - 2.5f, 5.0f, 5.0f, 1.0f, 0.85f, 0.1f, alpha);
    }

    //
    // The needle, which does not move. It marks the way the player is facing,
    // and everything else turns underneath it.
    //
    CG_VRHudPic(
        VRHUD_RING_CX - VRHUD_RING_R,
        VRHUD_RING_CY - VRHUD_RING_R,
        VRHUD_RING_R * 2.0f,
        VRHUD_RING_R * 2.0f,
        vrHud.compassNeedle,
        1.0f,
        alpha
    );
}

/*
================
CG_VRHudWeapons

The four weapon classes, dim for a slot the player does not have and lit for one
they do, with the equipped class boxed.

stats[STAT_WEAPONS] is a bitmask of WEAPON_CLASS_*, and stats[STAT_EQUIPPED_WEAPON]
is the same bitmask with the class in hand set - which is how the retail bar in
cl_ui.cpp decides what to enable, only it does it by passing enable and disable
events to named widgets in hud_weapons.urc.
================
*/
static void CG_VRHudWeapons(float alpha)
{
    const int owned    = cg.snap->ps.stats[STAT_WEAPONS];
    const int equipped = cg.snap->ps.stats[STAT_EQUIPPED_WEAPON];
    int       i;

    struct {
        int       bit;
        qhandle_t shader;
    } slots[4] = {
        {WEAPON_CLASS_PISTOL, vrHud.weapPistol},
        {WEAPON_CLASS_RIFLE,  vrHud.weapRifle },
        {WEAPON_CLASS_SMG,    vrHud.weapSmg   },
        {WEAPON_CLASS_MG,     vrHud.weapMg    },
    };

    for (i = 0; i < 4; i++) {
        const float x    = (i - 2) * (VRHUD_WEAP_W + 6.0f) + 3.0f;
        const qboolean have = (owned & slots[i].bit) ? qtrue : qfalse;

        if (equipped & slots[i].bit) {
            CG_VRHudFill(
                x - 2.0f, VRHUD_WEAP_Y - 2.0f, VRHUD_WEAP_W + 4.0f, VRHUD_WEAP_H + 4.0f,
                1.0f, 0.85f, 0.2f, alpha * 0.35f
            );
        }

        CG_VRHudPic(
            x, VRHUD_WEAP_Y, VRHUD_WEAP_W, VRHUD_WEAP_H, slots[i].shader,
            have ? 1.0f : 0.35f, have ? alpha : alpha * 0.4f
        );
    }
}

/*
================
CG_DrawVRHud

One cluster, in the middle of the view, carrying everything the corners used to.

Placed and sized entirely from cvars so it can be dialled in from autoexec.cfg
with a push and a force-stop: there is no console in a headset, and a HUD is
exactly the kind of thing that can only be judged with it on.
================
*/
void CG_DrawVRHud(void)
{
    float alpha;
    float health, maxHealth;
    float clip, maxClip;
    int   i;
    float objY;

    if (!vr_hud || !vr_hud->integer) {
        return;
    }

    if (!cg.snap || (cg.snap->ps.pm_flags & PMF_INTERMISSION)) {
        return;
    }

    // Nothing to report before there is a player to report on, and dead men
    // read no compasses.
    if (cg.snap->ps.stats[STAT_HEALTH] <= 0) {
        return;
    }

    CG_VRHudRegister();

    alpha = vr_hudAlpha ? vr_hudAlpha->value : 1.0f;
    if (alpha <= 0.01f) {
        return;
    } else if (alpha > 1.0f) {
        alpha = 1.0f;
    }

    //
    // Design units to pixels. The reference height keeps the cluster the same
    // angular size whatever the eye buffer turns out to be; the anchor is a
    // fraction of the display so it means the same thing there too.
    //
    vrHudScale = (cgs.glconfig.vidHeight / VRHUD_DESIGN_REF)
               * (vr_hudGroupScale ? vr_hudGroupScale->value : 1.0f);
    if (vrHudScale < 0.1f) {
        vrHudScale = 0.1f;
    }

    vrHudOriginX = cgs.glconfig.vidWidth * (vr_hudGroupX ? vr_hudGroupX->value : 0.5f);
    vrHudOriginY = cgs.glconfig.vidHeight * (vr_hudGroupY ? vr_hudGroupY->value : 0.62f);

    //
    // A backing plate, so the whole group reads as one object against whatever
    // the world happens to be doing behind it. Without it the text disappears
    // over snow and the bars disappear over mud.
    //
    CG_VRHudFill(
        -VRHUD_DESIGN_W * 0.5f,
        -VRHUD_DESIGN_H * 0.5f,
        VRHUD_DESIGN_W,
        VRHUD_DESIGN_H,
        0.0f,
        0.0f,
        0.0f,
        alpha * 0.35f
    );

    CG_VRHudCompass(alpha);

    //
    // Objectives. The text is already gathered by CG_DrawObjectives from
    // cg.Objectives[], which the configstrings fill; this only lays it out
    // narrower and next to everything else.
    //
    objY = VRHUD_OBJ_TOP;
    CG_VRHudText(VRHUD_OBJ_X, objY, cgi.LV_ConvertString("Mission Objectives:"), 0.85f, 0.75f, 0.2f, alpha);
    objY += 20.0f;

    for (i = 0; i < MAX_OBJECTIVES; i++) {
        if (cg.Objectives[i].flags == OBJ_FLAG_NONE || (cg.Objectives[i].flags & OBJ_FLAG_HIDDEN)) {
            continue;
        }

        if (cg.Objectives[i].flags & OBJ_FLAG_COMPLETED) {
            CG_VRHudText(VRHUD_OBJ_X, objY, "x", 0.6f, 0.6f, 0.6f, alpha);
            CG_VRHudText(
                VRHUD_OBJ_X + 14.0f, objY, cgi.LV_ConvertString(cg.Objectives[i].text), 0.6f, 0.6f, 0.6f, alpha
            );
        } else {
            CG_VRHudText(VRHUD_OBJ_X, objY, "-", 1.0f, 1.0f, 1.0f, alpha);
            CG_VRHudText(
                VRHUD_OBJ_X + 14.0f, objY, cgi.LV_ConvertString(cg.Objectives[i].text), 1.0f, 1.0f, 1.0f, alpha
            );
        }

        objY += 17.0f;

        // The cluster is a fixed box and a level can set a great many
        // objectives; the ones that do not fit are better dropped than drawn
        // through the health bar.
        if (objY > VRHUD_BAR_Y - 22.0f) {
            break;
        }
    }

    //
    // Health. stats[0] over stats[1], which is what hud_health.urc reads.
    //
    health    = (float)cg.snap->ps.stats[STAT_HEALTH];
    maxHealth = (float)cg.snap->ps.stats[STAT_MAXHEALTH];
    if (maxHealth < 1.0f) {
        maxHealth = 100.0f;
    }

    {
        const float frac = health / maxHealth;

        CG_VRHudText(VRHUD_HEALTH_X, VRHUD_BAR_Y - 17.0f, "HEALTH", 0.7f, 0.6f, 0.05f, alpha);
        // Green through amber to red, so the colour says as much as the length.
        CG_VRHudBar(
            VRHUD_HEALTH_X, VRHUD_BAR_Y, frac, frac > 0.5f ? (2.0f - frac * 2.0f) : 1.0f, frac * 1.6f, 0.12f, alpha
        );
        CG_VRHudText(
            VRHUD_HEALTH_X + VRHUD_BAR_W + 8.0f, VRHUD_BAR_Y - 2.0f, va("%d", (int)health), 1.0f, 1.0f, 1.0f, alpha
        );
    }

    //
    // Ammo. The clip bar is stats[6] over stats[7]; the numbers beside it are
    // the clip and the reserve, which is how every hud_ammo_*.urc reads them.
    //
    clip    = (float)cg.snap->ps.stats[STAT_CLIPAMMO];
    maxClip = (float)cg.snap->ps.stats[STAT_MAXCLIPAMMO];

    if (maxClip >= 1.0f) {
        CG_VRHudText(VRHUD_AMMO_X, VRHUD_BAR_Y - 17.0f, "AMMO", 0.7f, 0.6f, 0.05f, alpha);
        CG_VRHudBar(VRHUD_AMMO_X, VRHUD_BAR_Y, clip / maxClip, 0.95f, 0.8f, 0.25f, alpha);
        CG_VRHudText(
            VRHUD_AMMO_X + VRHUD_BAR_W + 8.0f,
            VRHUD_BAR_Y - 2.0f,
            va("%d|%d", (int)clip, cg.snap->ps.stats[STAT_AMMO]),
            1.0f,
            1.0f,
            1.0f,
            alpha
        );
    }

    //
    // What is in the player's hands. activeItems[ITEM_WEAPON] indexes the
    // weapon configstrings, which is where cg_viewmodelanim.c gets the same
    // name from.
    //
    if (cg.snap->ps.activeItems[ITEM_WEAPON] >= 0) {
        const char *name = CG_ConfigString(CS_WEAPONS + cg.snap->ps.activeItems[ITEM_WEAPON]);

        if (name && name[0]) {
            CG_VRHudText(-40.0f, VRHUD_NAME_Y, name, 0.7f, 0.6f, 0.05f, alpha);
        }
    }

    CG_VRHudWeapons(alpha);

    cgi.R_SetColor(NULL);
}
