#include "RetroEngine.hpp"

void SegaSplash_Create(void *objPtr)
{
    RSDK_THIS(SegaSplash);
    self->state     = SEGAPLASH_STATE_ENTER;
    self->rectAlpha = 320.0;
    self->loadStep  = 0;
    self->textureID = LoadTexture("Data/Game/Menu/CWLogo.png", TEXFMT_RGBA8888);
    if (Engine.useHighResAssets) {
        if (Engine.language == RETRO_JP)
            self->textureID = LoadTexture("Data/Game/Menu/SegaJP@2x.png", TEXFMT_RGBA5551);
        else
            self->textureID = LoadTexture("Data/Game/Menu/Sega@2x.png", TEXFMT_RGBA5551);
    }
    else {
        if (Engine.language == RETRO_JP)
            self->textureID = LoadTexture("Data/Game/Menu/SegaJP.png", TEXFMT_RGBA5551);
        else
            self->textureID = LoadTexture("Data/Game/Menu/Sega.png", TEXFMT_RGBA5551);
    }
}

void SegaSplash_LoadStep(NativeEntity_SegaSplash *self)
{
    int package = 0;
    int heading = 0, labelTex = 0, textTex = 0;

    switch (self->loadStep) {
        case 0: LoadTexture("Data/Game/Menu/SonicLogo.png", TEXFMT_RGBA8888); break;
        case 1: {
            int introTex = LoadTexture("Data/Game/Menu/Intro.png", TEXFMT_RGBA5551);
            LoadMesh("Data/Game/Models/Intro.bin", introTex);
        } break;
        case 2: {
            package = LoadTexture("Data/Game/Models/Package_JP.png", TEXFMT_RGBA5551);
            LoadMesh("Data/Game/Models/JPBox.bin", package);
            LoadMesh("Data/Game/Models/JPCartridge.bin", package);
        } break;
        case 3: {
            package = LoadTexture("Data/Game/Models/Package_US.png", TEXFMT_RGBA5551);
            LoadMesh("Data/Game/Models/Box.bin", package);
            LoadMesh("Data/Game/Models/Cartridge.bin", package);
        } break;
        case 4: {
            package = LoadTexture("Data/Game/Models/Package_EU.png", TEXFMT_RGBA5551);
            LoadMesh("Data/Game/Models/Box.bin", package);
            LoadMesh("Data/Game/Models/Cartridge.bin", package);
        } break;
        case 5:
            LoadTexture("Data/Game/Menu/Circle.png", TEXFMT_RGBA4444);
            LoadTexture("Data/Game/Menu/BG1.png", TEXFMT_RGBA4444);
            break;
        case 6:
            LoadTexture("Data/Game/Menu/ArrowButtons.png", TEXFMT_RGBA4444);
            if (Engine.gameDeviceType == RETRO_MOBILE)
                LoadTexture("Data/Game/Menu/VirtualDPad.png", TEXFMT_RGBA8888);
            else
                LoadTexture("Data/Game/Menu/Generic.png", TEXFMT_RGBA8888);
            break;
        case 7:
            LoadTexture("Data/Game/Menu/PlayerSelect.png", TEXFMT_RGBA8888);
            LoadTexture("Data/Game/Menu/SegaID.png", TEXFMT_RGBA8888);
            break;
        case 8:
#if !RETRO_USE_ORIGINAL_CODE
            ResetBitmapFonts();
            if (Engine.useHighResAssets)
                heading = LoadTexture("Data/Game/Menu/Heading_EN.png", TEXFMT_RGBA4444);
            else
                heading = LoadTexture("Data/Game/Menu/Heading_EN@1x.png", TEXFMT_RGBA4444);
            LoadBitmapFont("Data/Game/Menu/Heading_EN.fnt", FONT_HEADING, heading);
#endif
            break;
        case 9:
#if !RETRO_USE_ORIGINAL_CODE
            if (Engine.useHighResAssets)
                labelTex = LoadTexture("Data/Game/Menu/Label_EN.png", TEXFMT_RGBA4444);
            else
                labelTex = LoadTexture("Data/Game/Menu/Label_EN@1x.png", TEXFMT_RGBA4444);
            LoadBitmapFont("Data/Game/Menu/Label_EN.fnt", FONT_LABEL, labelTex);
#endif
            break;
        case 10:
#if !RETRO_USE_ORIGINAL_CODE
            textTex = LoadTexture("Data/Game/Menu/Text_EN.png", TEXFMT_RGBA4444);
            LoadBitmapFont("Data/Game/Menu/Text_EN.fnt", FONT_TEXT, textTex);
#endif
            break;
        case 11:
#if !RETRO_USE_ORIGINAL_CODE
            switch (Engine.language) {
                case RETRO_JP:
                    heading = LoadTexture("Data/Game/Menu/Heading_JA@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Heading_JA.fnt", FONT_HEADING, heading);

                    labelTex = LoadTexture("Data/Game/Menu/Label_JA@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Label_JA.fnt", FONT_LABEL, labelTex);

                    textTex = LoadTexture("Data/Game/Menu/Text_JA@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Text_JA.fnt", FONT_TEXT, textTex);
                    break;
                case RETRO_RU:
                    if (Engine.useHighResAssets)
                        heading = LoadTexture("Data/Game/Menu/Heading_RU.png", TEXFMT_RGBA4444);
                    else
                        heading = LoadTexture("Data/Game/Menu/Heading_RU@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Heading_RU.fnt", FONT_HEADING, heading);

                    if (Engine.useHighResAssets)
                        labelTex = LoadTexture("Data/Game/Menu/Label_RU.png", TEXFMT_RGBA4444);
                    else
                        labelTex = LoadTexture("Data/Game/Menu/Label_RU@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Label_RU.fnt", FONT_LABEL, labelTex);
                    break;
                case RETRO_KO:
                    heading = LoadTexture("Data/Game/Menu/Heading_KO@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Heading_KO.fnt", FONT_HEADING, heading);

                    labelTex = LoadTexture("Data/Game/Menu/Label_KO@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Label_KO.fnt", FONT_LABEL, labelTex);

                    textTex = LoadTexture("Data/Game/Menu/Text_KO.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Text_KO.fnt", FONT_TEXT, textTex);
                    break;
                case RETRO_ZH:
                    heading = LoadTexture("Data/Game/Menu/Heading_ZH@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Heading_ZH.fnt", FONT_HEADING, heading);

                    labelTex = LoadTexture("Data/Game/Menu/Label_ZH@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Label_ZH.fnt", FONT_LABEL, labelTex);

                    textTex = LoadTexture("Data/Game/Menu/Text_ZH@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Text_ZH.fnt", FONT_TEXT, textTex);
                    break;
                case RETRO_ZS:
                    heading = LoadTexture("Data/Game/Menu/Heading_ZHS@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Heading_ZHS.fnt", FONT_HEADING, heading);
                    labelTex = LoadTexture("Data/Game/Menu/Label_ZHS@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Label_ZHS.fnt", FONT_LABEL, labelTex);
                    textTex = LoadTexture("Data/Game/Menu/Text_ZHS@1x.png", TEXFMT_RGBA4444);
                    LoadBitmapFont("Data/Game/Menu/Text_ZHS.fnt", FONT_TEXT, textTex);
                    break;
                default: break;
            }
#endif
            break;
        default: break;
    }

    self->loadStep++;
}

void SegaSplash_Main(void *objPtr)
{
    RSDK_THIS(SegaSplash);

    if (self->loadStep < 16) {
        SegaSplash_LoadStep(self);
    }

    switch (self->state) {
        case SEGAPLASH_STATE_ENTER:
            self->rectAlpha -= 300.0 * Engine.deltaTime;
            if (self->rectAlpha < -320.0)
                self->state = SEGAPLASH_STATE_EXIT;

            SetRenderBlendMode(RENDER_BLEND_ALPHA);
            RenderRect(-SCREEN_CENTERX_F, SCREEN_CENTERY_F, 160.0, SCREEN_XSIZE_F, SCREEN_YSIZE_F, 0xFF, 0xFF, 0xFF, 0xFF);
            SetRenderBlendMode(RENDER_BLEND_ALPHA);
            RenderImage(0.0, 0.0, 160.0, 0.4, 0.4, 256.0, 128.0, 512.0, 256.0, 0.0, 0.0, 255, self->textureID);
            RenderRect(-SCREEN_CENTERX_F, SCREEN_CENTERY_F, 160.0, SCREEN_XSIZE_F, SCREEN_YSIZE_F, 0, 0, 0, self->rectAlpha);
            break;

        case SEGAPLASH_STATE_EXIT:
            self->rectAlpha += 300.0 * Engine.deltaTime;
            if (self->rectAlpha > 512.0)
                self->state = SEGAPLASH_STATE_SPAWNCWSPLASH;
            SetRenderBlendMode(RENDER_BLEND_ALPHA);
            RenderRect(-SCREEN_CENTERX_F, SCREEN_CENTERY_F, 160.0, SCREEN_XSIZE_F, SCREEN_YSIZE_F, 0xFF, 0xFF, 0xFF, 0xFF);
            SetRenderBlendMode(RENDER_BLEND_ALPHA);
            RenderImage(0.0, 0.0, 160.0, 0.4, 0.4, 256.0, 128.0, 512.0, 256.0, 0.0, 0.0, 255, self->textureID);
            RenderRect(-SCREEN_CENTERX_F, SCREEN_CENTERY_F, 160.0, SCREEN_XSIZE_F, SCREEN_YSIZE_F, 0, 0, 0, self->rectAlpha);
            break;

        case SEGAPLASH_STATE_SPAWNCWSPLASH: ResetNativeObject(self, CWSplash_Create, CWSplash_Main); break;
    }
}
