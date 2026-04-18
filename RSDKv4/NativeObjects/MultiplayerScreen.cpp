#include "RetroEngine.hpp"

#if !RETRO_USE_ORIGINAL_CODE && RETRO_USE_NETWORKING

void MultiplayerScreen_Create(void *objPtr)
{
    if (skipStartMenu) {
        // code has been copied here from SegaSplash_Create due to the possibility of loading the 2P stage without the HW menus >:(
        ResetBitmapFonts();
        int heading = 0, labelTex = 0, textTex = 0;

        if (Engine.useHighResAssets)
            heading = LoadTexture("Data/Game/Menu/Heading_EN.png", TEXFMT_RGBA4444);
        else
            heading = LoadTexture("Data/Game/Menu/Heading_EN@1x.png", TEXFMT_RGBA4444);
        LoadBitmapFont("Data/Game/Menu/Heading_EN.fnt", FONT_HEADING, heading);

        if (Engine.useHighResAssets)
            labelTex = LoadTexture("Data/Game/Menu/Label_EN.png", TEXFMT_RGBA4444);
        else
            labelTex = LoadTexture("Data/Game/Menu/Label_EN@1x.png", TEXFMT_RGBA4444);
        LoadBitmapFont("Data/Game/Menu/Label_EN.fnt", FONT_LABEL, labelTex);

        textTex = LoadTexture("Data/Game/Menu/Text_EN.png", TEXFMT_RGBA4444);
        LoadBitmapFont("Data/Game/Menu/Text_EN.fnt", FONT_TEXT, textTex);

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
    }

    RSDK_THIS(MultiplayerScreen);

    self->state     = MULTIPLAYERSCREEN_STATE_ENTER;
    self->stateDraw = MULTIPLAYERSCREEN_STATEDRAW_MAIN;
    self->scale     = 0.0f;
    self->timer         = 0.0f;
    self->roomCode      = 0;
    self->requestedRoom = false;
    self->rotationY     = 0.0f;
    self->flipDir   = 0;
    vsPlayerID      = -1;

    self->label                  = CREATE_ENTITY(TextLabel);
    self->label->useRenderMatrix = true;
    self->label->fontID          = FONT_HEADING;
    self->label->scale           = 0.3;
    self->label->alpha           = 256;
    self->label->x               = 0.0;
    self->label->y               = 100.0;
    self->label->z               = 16.0;
    self->label->state           = TEXTLABEL_STATE_IDLE;
    SetStringToFont8(self->label->text, "MULTIPLAYER", FONT_HEADING);
    self->label->alignPtr(self->label, ALIGN_CENTER);

    self->meshPanel = LoadMesh("Data/Game/Models/Panel.bin", -2);
    SetMeshVertexColors(self->meshPanel, 0, 0, 0, 0xC0);
    self->textureArrows                                           = LoadTexture("Data/Game/Menu/ArrowButtons.png", TEXFMT_RGBA4444);
    self->buttons[MULTIPLAYERSCREEN_BUTTON_HOST]                  = CREATE_ENTITY(PushButton);
    self->buttons[MULTIPLAYERSCREEN_BUTTON_HOST]->useRenderMatrix = true;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_HOST]->x               = 0.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_HOST]->y               = 20.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_HOST]->z               = 0.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_HOST]->scale           = 0.25;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_HOST]->bgColor         = 0x00A048;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_HOST]->bgColorSelected = 0x00C060;
    SetStringToFont8(self->buttons[0]->text, "HOST", FONT_LABEL);

    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOIN]                  = CREATE_ENTITY(PushButton);
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOIN]->useRenderMatrix = true;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOIN]->x               = 0.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOIN]->y               = -20.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOIN]->z               = 0.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOIN]->scale           = 0.25;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOIN]->bgColor         = 0x00A048;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOIN]->bgColorSelected = 0x00C060;
    SetStringToFont8(self->buttons[1]->text, "JOIN", FONT_LABEL);

    self->buttons[MULTIPLAYERSCREEN_BUTTON_COPY]                  = CREATE_ENTITY(PushButton);
    self->buttons[MULTIPLAYERSCREEN_BUTTON_COPY]->useRenderMatrix = true;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_COPY]->x               = 0.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_COPY]->y               = -56.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_COPY]->z               = 0.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_COPY]->scale           = 0.175;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_COPY]->bgColor         = 0x00A048;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_COPY]->bgColorSelected = 0x00C060;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_COPY]->alpha           = 0;
    SetStringToFont8(self->buttons[2]->text, "COPY", FONT_LABEL);

    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]                  = CREATE_ENTITY(PushButton);
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->useRenderMatrix = true;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->x               = -56.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->y               = -56.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->z               = 0.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->scale           = 0.175;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->bgColor         = 0x00A048;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->bgColorSelected = 0x00C060;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->alpha           = 0;
    SetStringToFont8(self->buttons[3]->text, "JOIN ROOM", FONT_LABEL);

    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]                  = CREATE_ENTITY(PushButton);
    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->useRenderMatrix = true;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->x               = 64.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->y               = -56.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->z               = 0.0;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->scale           = 0.175;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->bgColor         = 0x00A048;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->bgColorSelected = 0x00C060;
    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->alpha           = 0;
    SetStringToFont8(self->buttons[4]->text, "PASTE", FONT_LABEL);

    for (int i = 0; i < 3; ++i) {
        self->codeLabel[i]                  = CREATE_ENTITY(TextLabel);
        self->codeLabel[i]->useRenderMatrix = true;
        self->codeLabel[i]->fontID          = FONT_LABEL;
        self->codeLabel[i]->scale           = 0.20;
        self->codeLabel[i]->alpha           = 0;
        self->codeLabel[i]->x               = 0;
        self->codeLabel[i]->y               = 0;
        self->codeLabel[i]->z               = 16.0;
        self->codeLabel[i]->state           = TEXTLABEL_STATE_IDLE;
    }

    SetStringToFont8(self->codeLabel[0]->text, "ROOM CODE", self->codeLabel[0]->fontID);
    self->codeLabel[0]->alignPtr(self->codeLabel[0], ALIGN_CENTER);
    self->codeLabel[0]->y = 40.0f;

    SetStringToFont8(self->codeLabel[1]->text, "UNKNOWN", self->codeLabel[1]->fontID);
    self->codeLabel[1]->alignPtr(self->codeLabel[1], ALIGN_CENTER);
    self->codeLabel[1]->y = 15.0f;

    self->codeLabel[2]->y     = -40.0f;
    SetStringToFont8(self->codeLabel[2]->text, "WAITING FOR 2P...", self->codeLabel[2]->fontID);
    self->codeLabel[2]->alignPtr(self->codeLabel[2], ALIGN_CENTER);

    char codeBuf[0x8];
    sprintf(codeBuf, "%X", 0);

    for (int i = 0; i < 8; ++i) {
        self->enterCodeLabel[i]                  = CREATE_ENTITY(TextLabel);
        self->enterCodeLabel[i]->useRenderMatrix = true;
        self->enterCodeLabel[i]->fontID          = FONT_LABEL;
        self->enterCodeLabel[i]->scale           = 0.2;
        self->enterCodeLabel[i]->alpha           = 0;
        self->enterCodeLabel[i]->x               = -56.0f + (i * 16.0f);
        self->enterCodeLabel[i]->y               = 0;
        self->enterCodeLabel[i]->z               = 16.0;
        self->enterCodeLabel[i]->state           = TEXTLABEL_STATE_IDLE;

        self->enterCodeLabel[i]->r = 0xFF;
        self->enterCodeLabel[i]->g = 0xFF;
        self->enterCodeLabel[i]->b = 0x00;

        SetStringToFont8(self->enterCodeLabel[i]->text, codeBuf, self->enterCodeLabel[i]->fontID);
        self->enterCodeLabel[i]->alignPtr(self->enterCodeLabel[i], ALIGN_CENTER);
    }
    for (int i = 0; i < 2; ++i) {
        self->enterCodeSlider[i]                  = CREATE_ENTITY(TextLabel);
        self->enterCodeSlider[i]->useRenderMatrix = true;
        self->enterCodeSlider[i]->fontID          = FONT_LABEL;
        self->enterCodeSlider[i]->scale           = 0.25;
        self->enterCodeSlider[i]->alpha           = 0;
        self->enterCodeSlider[i]->y               = (i == 0) ? 24.0 : 12.0;
        self->enterCodeSlider[i]->z               = 16.0;
        self->enterCodeSlider[i]->state           = TEXTLABEL_STATE_IDLE;
        SetStringToFont8(self->enterCodeSlider[i]->text, "V", self->enterCodeSlider[i]->fontID);
        self->enterCodeSlider[i]->alignPtr(self->enterCodeSlider[i], ALIGN_CENTER);
    }

    self->ipPrefixLabel                  = CREATE_ENTITY(TextLabel);
    self->ipPrefixLabel->useRenderMatrix = true;
    self->ipPrefixLabel->fontID          = FONT_LABEL;
    self->ipPrefixLabel->scale           = 0.2;
    self->ipPrefixLabel->alpha           = 0;
    self->ipPrefixLabel->x               = -125.0;
    self->ipPrefixLabel->y               = 0;
    self->ipPrefixLabel->z               = 16.0;
    self->ipPrefixLabel->state           = TEXTLABEL_STATE_IDLE;
    SetStringToFont8(self->ipPrefixLabel->text, "ID:", FONT_LABEL);
    self->ipPrefixLabel->alignPtr(self->ipPrefixLabel, ALIGN_CENTER);

    for (int i = 0; i < 12; ++i) {
        self->ipDigitLabel[i]                  = CREATE_ENTITY(TextLabel);
        self->ipDigitLabel[i]->useRenderMatrix = true;
        self->ipDigitLabel[i]->fontID          = FONT_LABEL;
        self->ipDigitLabel[i]->scale           = 0.2;
        self->ipDigitLabel[i]->alpha           = 0;

        self->ipDigitLabel[i]->x     = -88.0f + (i * 16.0f);
        self->ipDigitLabel[i]->y     = 0;
        self->ipDigitLabel[i]->z     = 16.0;
        self->ipDigitLabel[i]->state = TEXTLABEL_STATE_IDLE;
        self->ipDigitLabel[i]->r     = 255;
        self->ipDigitLabel[i]->g     = 255;
        self->ipDigitLabel[i]->b     = 0;

        SetStringToFont8(self->ipDigitLabel[i]->text, "0", FONT_LABEL);
        self->ipDigitLabel[i]->alignPtr(self->ipDigitLabel[i], ALIGN_CENTER);
    }
    for (int i = 0; i < 3; ++i) {
        self->ipDotLabel[i]                  = CREATE_ENTITY(TextLabel);
        self->ipDotLabel[i]->useRenderMatrix = true;
        self->ipDotLabel[i]->fontID          = FONT_LABEL;
        self->ipDotLabel[i]->scale           = 0.2;
        self->ipDotLabel[i]->alpha           = 0;
        self->ipDotLabel[i]->x               = -100.0f + (i * 64.0f) + 48.0f;
        self->ipDotLabel[i]->y               = 0;
        self->ipDotLabel[i]->z               = 16.0;
        self->ipDotLabel[i]->state           = TEXTLABEL_STATE_IDLE;
        SetStringToFont8(self->ipDotLabel[i]->text, ".", FONT_LABEL);
        self->ipDotLabel[i]->alignPtr(self->ipDotLabel[i], ALIGN_CENTER);
    }
}

void MultiplayerScreen_DrawJoinCode(void *objPtr, int v)
{
    RSDK_THIS(MultiplayerScreen);
    union {
        uint val;
        byte bytes[4];
    } u;
    u.val = self->roomCode;
#if RETRO_IS_BIG_ENDIAN
    SWAP_ENDIAN(u.val);
#endif

    for (int i = 0; i < 8; i += 2) {
        int n         = 7 - i;
        int nybbles[] = { u.bytes[n >> 1] & 0xF, ((u.bytes[n >> 1] & 0xF0) >> 4) & 0xF };

        self->enterCodeLabel[i + 0]->alpha = 0x100;
        self->enterCodeLabel[i + 1]->alpha = 0x100;

        self->enterCodeLabel[i + 0]->useColors = false;
        self->enterCodeLabel[i + 1]->useColors = false;

        char codeBuf[0x10];
        sprintf(codeBuf, "%X", nybbles[1]);
        SetStringToFont8(self->enterCodeLabel[i + 0]->text, codeBuf, self->enterCodeLabel[i + 0]->fontID);
        self->enterCodeLabel[i + 0]->alignPtr(self->enterCodeLabel[i + 0], ALIGN_CENTER);

        sprintf(codeBuf, "%X", nybbles[0]);
        SetStringToFont8(self->enterCodeLabel[i + 1]->text, codeBuf, self->enterCodeLabel[i + 1]->fontID);
        self->enterCodeLabel[i + 1]->alignPtr(self->enterCodeLabel[i + 1], ALIGN_CENTER);
    }
    self->enterCodeLabel[v]->useColors = true;
}

void MultiplayerScreen_Destroy(void *objPtr)
{
    RSDK_THIS(MultiplayerScreen);
    RemoveNativeObject(self->label);
    for (int i = 0; i < 3; ++i) RemoveNativeObject(self->codeLabel[i]);
    for (int i = 0; i < 8; ++i) RemoveNativeObject(self->enterCodeLabel[i]);
    for (int i = 0; i < 2; ++i) RemoveNativeObject(self->enterCodeSlider[i]);
    RemoveNativeObject(self->ipPrefixLabel);
    for (int i = 0; i < 12; ++i) RemoveNativeObject(self->ipDigitLabel[i]);
    for (int i = 0; i < 3; ++i) RemoveNativeObject(self->ipDotLabel[i]);
    for (int i = 0; i < MULTIPLAYERSCREEN_BUTTON_COUNT; ++i) RemoveNativeObject(self->buttons[i]);
    RemoveNativeObject(self->bg);
    RemoveNativeObject(self);
}

void MultiplayerScreen_Main(void *objPtr)
{
    RSDK_THIS(MultiplayerScreen);

    if (dcError && self->state == MULTIPLAYERSCREEN_STATE_HOSTSCR)
        CREATE_ENTITY(MultiplayerHandler);

    // Global rendering matrix setup
    if (self->state == MULTIPLAYERSCREEN_STATE_ENTER) {
        NewRenderState();
        MatrixScaleXYZF(&self->renderMatrix, self->scale, self->scale, 1.0);
        MatrixTranslateXYZF(&self->matrixTemp, 0.0, -8.0, 160.0);
        MatrixMultiplyF(&self->renderMatrix, &self->matrixTemp);
    }
    else if (self->state == MULTIPLAYERSCREEN_STATE_FLIP) {
        NewRenderState();
        MatrixRotateYF(&self->renderMatrix, self->rotationY);
        MatrixTranslateXYZF(&self->matrixTemp, 0.0, -8.0, 160.0);
        MatrixMultiplyF(&self->renderMatrix, &self->matrixTemp);
    }
    else if (self->state == MULTIPLAYERSCREEN_STATE_STARTGAME || self->state == MULTIPLAYERSCREEN_STATE_EXIT) {
        NewRenderState();
        MatrixScaleXYZF(&self->renderMatrix, self->scale, self->scale, 1.0);
        MatrixTranslateXYZF(&self->matrixTemp, 0.0, -8.0, 160.0);
        MatrixMultiplyF(&self->renderMatrix, &self->matrixTemp);
    }
    else {
        NewRenderState();
        MatrixTranslateXYZF(&self->renderMatrix, 0.0, -8.0, 160.0);
    }
    SetRenderMatrix(&self->renderMatrix);

    // Sync matrices for sub-entities
    if (self->label) memcpy(&self->label->renderMatrix, &self->renderMatrix, sizeof(MatrixF));
    for (int i = 0; i < 3; ++i) {
        if (self->codeLabel[i]) memcpy(&self->codeLabel[i]->renderMatrix, &self->renderMatrix, sizeof(MatrixF));
    }
    for (int i = 0; i < MULTIPLAYERSCREEN_BUTTON_COUNT; ++i) {
        if (self->buttons[i]) memcpy(&self->buttons[i]->renderMatrix, &self->renderMatrix, sizeof(MatrixF));
    }
    for (int i = 0; i < 8; ++i) {
        if (self->enterCodeLabel[i]) memcpy(&self->enterCodeLabel[i]->renderMatrix, &self->renderMatrix, sizeof(MatrixF));
    }
    for (int i = 0; i < 12; ++i) {
        if (self->ipDigitLabel[i]) memcpy(&self->ipDigitLabel[i]->renderMatrix, &self->renderMatrix, sizeof(MatrixF));
    }
    for (int i = 0; i < 3; ++i) {
        if (self->ipDotLabel[i]) memcpy(&self->ipDotLabel[i]->renderMatrix, &self->renderMatrix, sizeof(MatrixF));
    }
    if (self->ipPrefixLabel) memcpy(&self->ipPrefixLabel->renderMatrix, &self->renderMatrix, sizeof(MatrixF));
    if (self->enterCodeSlider[0]) memcpy(&self->enterCodeSlider[0]->renderMatrix, &self->renderMatrix, sizeof(MatrixF));
    if (self->enterCodeSlider[1]) {
        MatrixRotateZF(&self->enterCodeSlider[1]->renderMatrix, DegreesToRad(180));
        MatrixMultiplyF(&self->enterCodeSlider[1]->renderMatrix, &self->renderMatrix);
    }

    switch (self->state) {
        case MULTIPLAYERSCREEN_STATE_ENTER: {
            if (self->arrowAlpha < 0x100)
                self->arrowAlpha += 8;

            self->scale = fminf(self->scale + ((1.05 - self->scale) / ((60.0 * Engine.deltaTime) * 8.0)), 1.0f);

            self->timer += Engine.deltaTime;
            if (self->timer > 0.5) {
                self->timer = 0.0;
                self->state = MULTIPLAYERSCREEN_STATE_MAIN;
            }
            break;
        }
        case MULTIPLAYERSCREEN_STATE_MAIN: {
            CheckKeyDown(&keyDown);
            CheckKeyPress(&keyPress);

            if (usePhysicalControls) {
                if (keyPress.left || keyPress.right) {
                    PlaySfxByName("Menu Move", false);
                    self->selectedButton = (self->selectedButton == MULTIPLAYERSCREEN_BUTTON_HOST) ? MULTIPLAYERSCREEN_BUTTON_JOIN : MULTIPLAYERSCREEN_BUTTON_HOST;
                }
                if (touches > 0) {
                    usePhysicalControls = false;
                }
                else {
                    if (keyPress.up) {
                        PlaySfxByName("Menu Move", false);
                        self->selectedButton--;
                        if (self->selectedButton < MULTIPLAYERSCREEN_BUTTON_HOST)
                            self->selectedButton = MULTIPLAYERSCREEN_BUTTON_JOIN;
                    }
                    else if (keyPress.down) {
                        PlaySfxByName("Menu Move", false);
                        self->selectedButton++;
                        if (self->selectedButton > MULTIPLAYERSCREEN_BUTTON_JOIN)
                            self->selectedButton = MULTIPLAYERSCREEN_BUTTON_HOST;
                    }

                    self->buttons[MULTIPLAYERSCREEN_BUTTON_HOST]->state = PUSHBUTTON_STATE_UNSELECTED;
                    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOIN]->state = PUSHBUTTON_STATE_UNSELECTED;
                    self->buttons[self->selectedButton]->state          = PUSHBUTTON_STATE_SELECTED;

                    if (keyPress.start || keyPress.A) {
                        PlaySfxByName("Menu Select", false);
                        self->buttons[self->selectedButton]->state = PUSHBUTTON_STATE_FLASHING;
                        self->state                                = MULTIPLAYERSCREEN_STATE_ACTION;
                    }
                    else if (keyPress.B || self->backPressed) {
                        PlaySfxByName("Menu Back", false);
                        self->backPressed             = false;
                        self->state                   = MULTIPLAYERSCREEN_STATE_EXIT;
                        self->timer                   = 0;
                        NativeEntity_FadeScreen *fade = CREATE_ENTITY(FadeScreen);
                        fade->delay                   = 1.1f;
                        fade->state                   = FADESCREEN_STATE_FADEOUT;
                    }
                }
            }
            else {
                if (touches > 0) {
                    for (int i = 0; i < 2; ++i) {
                        self->buttons[i]->state =
                            CheckTouchRect(0, 20 - (i * 40), ((64.0 * self->buttons[i]->scale) + self->buttons[i]->textWidth) * 0.75, 20.4) >= 0;
                    }
                    self->backPressed = CheckTouchRect(128.0, -92.0, 32.0, 32.0) >= 0;
                    if (self->state == MULTIPLAYERSCREEN_STATE_MAIN) {
                        if (keyDown.left) {
                            self->selectedButton = MULTIPLAYERSCREEN_BUTTON_JOIN;
                            usePhysicalControls  = true;
                        }
                        if (keyDown.right) {
                            self->selectedButton = MULTIPLAYERSCREEN_BUTTON_HOST;
                            usePhysicalControls  = true;
                        }
                    }
                }
                else {
                    for (int i = 0; i < 2; ++i) {
                        if (self->buttons[i]->state == PUSHBUTTON_STATE_SELECTED) {
                            PlaySfxByName("Menu Select", false);
                            self->buttons[i]->state = PUSHBUTTON_STATE_FLASHING;
                            self->selectedButton    = i;
                            self->state             = MULTIPLAYERSCREEN_STATE_ACTION;
                            break;
                        }
                    }

                    if (keyPress.B || self->backPressed) {
                        PlaySfxByName("Menu Back", false);
                        self->backPressed             = false;
                        self->state                   = MULTIPLAYERSCREEN_STATE_EXIT;
                        self->timer                   = 0;
                        NativeEntity_FadeScreen *fade = CREATE_ENTITY(FadeScreen);
                        fade->delay                   = 1.0;
                        fade->state                   = FADESCREEN_STATE_FADEOUT;
                    }
                    else {
                        if (self->state == MULTIPLAYERSCREEN_STATE_MAIN) {
                            if (keyDown.down) {
                                self->selectedButton = MULTIPLAYERSCREEN_BUTTON_JOIN;
                                usePhysicalControls  = true;
                            }
                            if (keyDown.up) {
                                self->selectedButton = MULTIPLAYERSCREEN_BUTTON_HOST;
                                usePhysicalControls  = true;
                            }
                        }
                    }
                }
            }
            break;
        }
        case MULTIPLAYERSCREEN_STATE_ACTION: { // action
            CheckKeyDown(&keyDown);

            if (self->buttons[self->selectedButton]->state == PUSHBUTTON_STATE_UNSELECTED) {
                int selected = self->selectedButton;
                self->state  = MULTIPLAYERSCREEN_STATE_MAIN;
                 // PrintLog("MultiplayerScreen - Action confirm: %d", selected);
                switch (selected) {
                    default: break;
                    case MULTIPLAYERSCREEN_BUTTON_HOST:
                        useHostServer = false;
                        StrCopy(networkHost, "34.171.163.243");
                        WriteSettings();
                        RunNetwork();

                        self->state         = MULTIPLAYERSCREEN_STATE_FLIP;
                        self->nextState     = MULTIPLAYERSCREEN_STATE_HOSTSCR;
                        self->nextStateDraw = MULTIPLAYERSCREEN_STATEDRAW_HOST;
                        self->flipDir       = 1;
                        break;
                    case MULTIPLAYERSCREEN_BUTTON_JOIN:
                        useHostServer = false;
                        StrCopy(networkHost, "34.171.163.243");
                        WriteSettings();
                        RunNetwork();

                        self->state         = MULTIPLAYERSCREEN_STATE_FLIP;
                        self->nextState     = MULTIPLAYERSCREEN_STATE_JOINSCR;
                        self->nextStateDraw = MULTIPLAYERSCREEN_STATEDRAW_JOIN;
                        self->flipDir       = 1;
                        
                        availableRoomCount = 0;
                        self->requestedRoom = false; // Reset for reuse as list request flag
                        break;
                    case MULTIPLAYERSCREEN_BUTTON_JOINROOM: {
                        self->state                   = MULTIPLAYERSCREEN_STATE_STARTGAME;
                        NativeEntity_FadeScreen *fade = CREATE_ENTITY(FadeScreen);
                        fade->state                   = FADESCREEN_STATE_GAMEFADEOUT;
                        fade->delay                   = 1.1f;
                        break;
                    }
                }
            }
            break;
        }
        case MULTIPLAYERSCREEN_STATE_STARTGAME:
        case MULTIPLAYERSCREEN_STATE_EXIT: {
            if (self->dialog)
                self->dialog->state = DIALOGPANEL_STATE_IDLE;
            if (self->arrowAlpha > 0)
                self->arrowAlpha -= 8;

            self->timer += Engine.deltaTime;
            if (self->timer > 0.5) {
                if (self->state == MULTIPLAYERSCREEN_STATE_EXIT) {
                    if (skipStartMenu) {
                        StopMusic(true);
                        // is there a better way of removing the pop up message? :(
                        if (self->dialog) {
                            RemoveNativeObject(self->dialog);
                            RemoveNativeObject(self->label);
                            RemoveNativeObject(self->label);
                        }
                        SetGlobalVariableByName("options.stageSelectFlag", 0);
                        activeStageList   = 0;
                        stageMode         = STAGEMODE_LOAD;
                        Engine.gameMode   = ENGINE_MAINGAME;
                        stageListPosition = 0;
                    }
                    else
                        Engine.gameMode = ENGINE_RESETGAME;
                }
                else {
                    if (vsPlayerID == 1) {
                        SetRoomCode(self->roomCode);
                        ServerPacket send;
                        memset(&send, 0, sizeof(ServerPacket));
                        send.header = CL_JOIN;
                        send.room   = self->roomCode;
                         // PrintLog("MultiplayerScreen - Joining room 0x%08X", send.room);
                        SendServerPacket(send, true);
                    }
                }
                MultiplayerScreen_Destroy(self);
                NewRenderState();
                MatrixScaleXYZF(&self->renderMatrix, Engine.windowScale, Engine.windowScale, 1.0);
                MatrixTranslateXYZF(&self->matrixTemp, 0.0, 0.0, 160.0);
                MatrixMultiplyF(&self->renderMatrix, &self->matrixTemp);
                SetRenderMatrix(&self->renderMatrix);

                RenderRect(-SCREEN_CENTERX_F, SCREEN_CENTERY_F, 160.0, SCREEN_XSIZE_F, SCREEN_YSIZE_F, 0, 0, 0, 255);
                return;
            }
            break;
        }
        case MULTIPLAYERSCREEN_STATE_FLIP: { // panel flip
            if (self->flipDir) {
                self->rotationY += (10.0 * Engine.deltaTime);
                if (self->rotationY > M_PI_H && self->nextStateDraw != MULTIPLAYERSCREEN_STATEDRAW_NONE) {
                    self->stateDraw     = self->nextStateDraw;
                    self->nextStateDraw = MULTIPLAYERSCREEN_STATEDRAW_NONE;
                }

                if (self->rotationY > M_PI) {
                    self->state     = self->nextState;
                    self->timer     = 0.0f;
                    self->rotationY = 0.0;
                }
            }
            else {
                self->rotationY -= (10.0 * Engine.deltaTime);
                if (self->rotationY < -M_PI_H && self->nextStateDraw != MULTIPLAYERSCREEN_STATEDRAW_NONE) {
                    self->stateDraw     = self->nextStateDraw;
                    self->nextStateDraw = MULTIPLAYERSCREEN_STATEDRAW_NONE;
                }

                if (self->rotationY < -M_PI) {
                    self->state     = self->nextState;
                    self->timer     = 0.0f;
                    self->rotationY = 0.0;
                }
            }
            break;
        }
        case MULTIPLAYERSCREEN_STATE_HOSTSCR: {
            CheckKeyDown(&keyDown);
            CheckKeyPress(&keyPress);

            char ipBuf[0x40];
            int ip[4] = { 0, 0, 0, 0 };
            sscanf(publicIP, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);
            int digits[12];
            for (int i = 0; i < 4; ++i) {
                digits[i * 3 + 0] = (ip[i] / 100) % 10;
                digits[i * 3 + 1] = (ip[i] / 10) % 10;
                digits[i * 3 + 2] = ip[i] % 10;
            }

            // Enhanced obfuscation: reverse digits
            int obfuscated[12];
            for (int i = 0; i < 12; ++i) {
                obfuscated[i] = digits[11 - i];
            }

            sprintf(ipBuf, "ID: %c%c%c%c%c%c%c%c%c%c%c%c",
                    obfuscated[0] + 'A', obfuscated[1] + 'A', obfuscated[2] + 'A', obfuscated[3] + 'A',
                    obfuscated[4] + 'A', obfuscated[5] + 'A', obfuscated[6] + 'A', obfuscated[7] + 'A',
                    obfuscated[8] + 'A', obfuscated[9] + 'A', obfuscated[10] + 'A', obfuscated[11] + 'A');
            SetStringToFont8(self->codeLabel[2]->text, ipBuf, self->codeLabel[2]->fontID);
            self->codeLabel[2]->alignPtr(self->codeLabel[2], ALIGN_CENTER);

            if (!self->roomCode) {
                int code = GetRoomCode();
                if (code) {
                    char buffer[0x30];
                    sprintf(buffer, "%08X", code);
                    SetStringToFont8(self->codeLabel[1]->text, buffer, self->codeLabel[1]->fontID);
                    self->codeLabel[1]->alignPtr(self->codeLabel[1], ALIGN_CENTER);
                    self->roomCode = code;
                }
                else {
                    if (GetNetworkCode() && !self->requestedRoom) {
                        ServerPacket send;
                        memset(&send, 0, sizeof(ServerPacket));
                        send.header = CL_REQUEST_CODE;
                        if (!vsGameLength)
                            vsGameLength = 4;
                        if (!vsItemMode)
                            vsItemMode = 1;
                        send.data.multiData.type    = 0x00000FF0;
                        send.data.multiData.data[0] = (vsGameLength << 4) | (vsItemMode << 8);
                        SendServerPacket(send, true);
                        self->requestedRoom = true;
                    }
                }
            }
            else {
                // listen for room creation success (SV_CODES or SV_NEW_PLAYER)
                if (vsPlaying && vsPlayerID == 0) {
                     // PrintLog("MultiplayerScreen - Host session started, transitioning.");
                    self->selectedButton = MULTIPLAYERSCREEN_BUTTON_JOINROOM;
                    self->state          = MULTIPLAYERSCREEN_STATE_ACTION;
                }
            }

            if (usePhysicalControls) {
                if (touches > 0) {
                    usePhysicalControls = false;
                }
                else {
                    if (keyPress.B) {
                        self->dialog = CREATE_ENTITY(DialogPanel);
                        SetStringToFont8(self->dialog->text,
                                         "Are you sure you want to exit?\rThis will close the room,\rand you will return to the main menu.", 2);
                        self->state = MULTIPLAYERSCREEN_STATE_DIALOGWAIT;
                        PlaySfxByName("Resume", false);
                    }
                }
            }
            else {
                if (touches > 0) {
                    self->backPressed = CheckTouchRect(128.0, -92.0, 32.0, 32.0) >= 0;
                }
                else {
                    if (keyPress.B || self->backPressed) {
                        self->backPressed = false;
                        self->dialog      = CREATE_ENTITY(DialogPanel);
                        SetStringToFont8(self->dialog->text,
                                         "Are you sure you want to exit?\rThis will close the room,\rand you will return to the main menu.",
                                         FONT_TEXT);
                        self->state = MULTIPLAYERSCREEN_STATE_DIALOGWAIT;
                        PlaySfxByName("Resume", false);
                    }
                }
            }
            break;
        }
        case MULTIPLAYERSCREEN_STATE_JOINSCR: {
            CheckKeyDown(&keyDown);
            CheckKeyPress(&keyPress);

            if (!GetNetworkCode()) {
                self->codeLabel[1]->alpha = 0x100;
                self->codeLabel[1]->y     = 0;
                SetStringToFont8(self->codeLabel[1]->text, "CONNECTING...", FONT_LABEL);
                self->codeLabel[1]->alignPtr(self->codeLabel[1], ALIGN_CENTER);
                memcpy(&self->codeLabel[1]->renderMatrix, &self->renderMatrix, sizeof(MatrixF));
                return;
            }

            if (!self->requestedRoom) {
                ServerPacket send;
                memset(&send, 0, sizeof(ServerPacket));
                send.header = CL_LIST_ROOMS;
                SendServerPacket(send);
                self->requestedRoom = true;
                self->timer         = 0.0f;
            }

            self->timer += Engine.deltaTime;
            if (self->timer > 5.0) { // Refresh every 5 seconds
                self->requestedRoom = false;
                self->timer         = 0.0f;
            }

            if (availableRoomCount > 0) {
                if (keyPress.up) {
                    self->touchedUpID--;
                    if (self->touchedUpID < 0) self->touchedUpID = availableRoomCount - 1;
                    PlaySfxByName("Menu Move", false);
                }
                if (keyPress.down) {
                    self->touchedUpID++;
                    if (self->touchedUpID >= availableRoomCount) self->touchedUpID = 0;
                    PlaySfxByName("Menu Move", false);
                }
                if (keyPress.A || keyPress.start) {
                    self->roomCode = availableRooms[self->touchedUpID].code;
                    self->selectedButton = MULTIPLAYERSCREEN_BUTTON_JOINROOM;
                    self->state = MULTIPLAYERSCREEN_STATE_ACTION;
                    PlaySfxByName("Menu Select", false);
                }
                if (keyPress.B) {
                    self->state         = MULTIPLAYERSCREEN_STATE_FLIP;
                    self->nextState     = MULTIPLAYERSCREEN_STATE_MAIN;
                    self->nextStateDraw = MULTIPLAYERSCREEN_STATEDRAW_MAIN;
                    PlaySfxByName("Menu Back", false);
                }
                return;
            }

            if (usePhysicalControls) {
                if (touches > 0) {
                    usePhysicalControls = false;
                }
                else {
                    if (keyPress.left) {
                        PlaySfxByName("Menu Move", false);
                        self->selectedButton--;
                        if (self->selectedButton < 3)
                            self->selectedButton = 12;
                    }
                    else if (keyPress.right) {
                        PlaySfxByName("Menu Move", false);
                        self->selectedButton++;
                        if (self->selectedButton > 12)
                            self->selectedButton = 3;
                    }

                    if ((self->selectedButton != MULTIPLAYERSCREEN_BUTTON_JOINROOM && self->selectedButton != MULTIPLAYERSCREEN_BUTTON_PASTE)
                        && (keyPress.up || keyPress.down)) {
                        union {
                            int val;
                            byte bytes[4];
                        } u;
                        u.val         = self->roomCode;
#if RETRO_IS_BIG_ENDIAN
                        SWAP_ENDIAN(u.val);
#endif
                        int n         = 7 - (self->selectedButton - 5);
                        int nybbles[] = { u.bytes[n >> 1] & 0xF, ((u.bytes[n >> 1] & 0xF0) >> 4) & 0xF };

                        if (keyPress.up) {
                            PlaySfxByName("Menu Move", false);
                            nybbles[n & 1] = (nybbles[n & 1] + 1) & 0xF;
                        }
                        else if (keyPress.down) {
                            PlaySfxByName("Menu Move", false);
                            nybbles[n & 1] = (nybbles[n & 1] - 1) & 0xF;
                        }

                        u.bytes[n >> 1] = (nybbles[1] << 4) | (nybbles[0] & 0xF);
#if RETRO_IS_BIG_ENDIAN
                        SWAP_ENDIAN(u.val);
#endif
                        self->roomCode  = u.val;

                        MultiplayerScreen_DrawJoinCode(self, 7 - n);
                    }

                    for (int i = 0; i < 8; ++i) self->enterCodeLabel[i]->useColors = false;
                    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->state = PUSHBUTTON_STATE_UNSELECTED;
                    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->state    = PUSHBUTTON_STATE_UNSELECTED;
                    self->enterCodeSlider[0]->alpha                         = 0;
                    self->enterCodeSlider[1]->alpha                         = 0;

                    if (self->selectedButton == MULTIPLAYERSCREEN_BUTTON_JOINROOM)
                        self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->state = PUSHBUTTON_STATE_SELECTED;
                    else if (self->selectedButton == MULTIPLAYERSCREEN_BUTTON_PASTE)
                        self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->state = PUSHBUTTON_STATE_SELECTED;
                    else if (self->selectedButton > 4) {
                        self->enterCodeSlider[0]->x     = self->enterCodeLabel[self->selectedButton - 5]->x;
                        self->enterCodeSlider[1]->x     = -self->enterCodeLabel[self->selectedButton - 5]->x;
                        self->enterCodeSlider[0]->alpha = 0x100;
                        self->enterCodeSlider[1]->alpha = 0x100;

                        self->enterCodeLabel[self->selectedButton - 5]->useColors = true;
                    }

                    if (keyPress.start || keyPress.A) {
                        if (self->selectedButton == MULTIPLAYERSCREEN_BUTTON_JOINROOM) {
                            PlaySfxByName("Menu Select", false);
                            self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->state = PUSHBUTTON_STATE_UNSELECTED;
                            self->selectedButton                                    = MULTIPLAYERSCREEN_BUTTON_JOINROOM;
                            self->state                                             = MULTIPLAYERSCREEN_STATE_ACTION;
                        }
                        else if (self->selectedButton == MULTIPLAYERSCREEN_BUTTON_PASTE) {
                            self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->state = PUSHBUTTON_STATE_FLASHING;
#if RETRO_PLATFORM != RETRO_PS3
                            char buf[0x30];
                            char *txt = SDL_GetClipboardText(); // easier bc we must SDL free after
                            if (StrLength(txt) && StrLength(txt) < 0x30 - 2) {
                                StrCopy(buf, "0x");
                                StrAdd(buf, txt);
                                int before = self->roomCode;
                                if (ConvertStringToInteger(buf, &self->roomCode)) {
                                    MultiplayerScreen_DrawJoinCode(self, 0);
                                    self->enterCodeLabel[0]->useColors = false;
                                    self->selectedButton               = MULTIPLAYERSCREEN_BUTTON_JOINROOM;
                                    if (Engine.gameType == GAME_SONIC1) //??
                                        PlaySfxByName("Lamp Post", false);
                                    else
                                        PlaySfxByName("Star Post", false);
                                }
                                else {
                                    self->roomCode = before;
                                    PlaySfxByName("Hurt", false);
                                }
                            }
                            else
                                PlaySfxByName("Hurt", false);
                            SDL_free(txt);
#else
                            PlaySfxByName("Hurt", false);
#endif
                        }
                    }
                    else if (keyPress.B) {
                        PlaySfxByName("Menu Back", false);
                        self->state         = MULTIPLAYERSCREEN_STATE_FLIP;
                        self->nextState     = MULTIPLAYERSCREEN_STATE_MAIN;
                        self->nextStateDraw = MULTIPLAYERSCREEN_STATEDRAW_MAIN;
                    }
                }
            }
            else {
                if (touches > 0) {
                    float w = self->enterCodeLabel[1]->x - self->enterCodeLabel[0]->x;
                    for (int i = 0; i < 8; ++i) {
                        if (CheckTouchRect(self->enterCodeLabel[i]->x, 22.0f, w / 2, 16.0) >= 0)
                            self->touchedUpID = i;
                        if (CheckTouchRect(self->enterCodeLabel[i]->x, -22.0f, w / 2, 16.0) >= 0)
                            self->touchedDownID = i;
                    }

                    for (int i = 0; i < 8; ++i) self->enterCodeLabel[i]->useColors = false;

                    int id = self->touchedDownID;
                    if (self->touchedUpID >= 0)
                        id = self->touchedUpID;
                    if (id >= 0) {
                        self->selectedButton            = id + 5;
                        self->enterCodeSlider[0]->x     = self->enterCodeLabel[id]->x;
                        self->enterCodeSlider[1]->x     = -self->enterCodeLabel[id]->x;
                        self->enterCodeSlider[0]->alpha = 0x100;
                        self->enterCodeSlider[1]->alpha = 0x100;

                        self->enterCodeLabel[id]->useColors = true;
                    }

                    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->state =
                        CheckTouchRect(-56.0f, -64.0f,
                                       ((64.0 * self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->scale)
                                        + self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->textWidth)
                                           * 0.75,
                                       12.0)
                        >= 0;
                    self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->state =
                        CheckTouchRect(
                            64.0f, -64.0f,
                            ((64.0 * self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->scale) + self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->textWidth)
                                * 0.75,
                            12.0)
                        >= 0;

                    self->backPressed = CheckTouchRect(128.0, -92.0, 32.0, 32.0) >= 0;
                    if (keyDown.left || keyDown.right) {
                        usePhysicalControls = true;
                    }
                }
                else {
                    if (self->touchedUpID >= 0 || self->touchedDownID >= 0) {
                        int id = self->touchedDownID;
                        if (self->touchedUpID >= 0)
                            id = self->touchedUpID;

                        union {
                            int val;
                            byte bytes[4];
                        } u;
                        u.val         = self->roomCode;
#if RETRO_IS_BIG_ENDIAN
                        SWAP_ENDIAN(u.val);
#endif
                        int n         = 7 - id;
                        int nybbles[] = { u.bytes[n >> 1] & 0xF, ((u.bytes[n >> 1] & 0xF0) >> 4) & 0xF };

                        if (self->touchedUpID >= 0) {
                            PlaySfxByName("Menu Move", false);
                            nybbles[n & 1] = (nybbles[n & 1] + 1) & 0xF;
                        }
                        else if (self->touchedDownID >= 0) {
                            PlaySfxByName("Menu Move", false);
                            nybbles[n & 1] = (nybbles[n & 1] - 1) & 0xF;
                        }

                        u.bytes[n >> 1] = (nybbles[1] << 4) | (nybbles[0] & 0xF);
#if RETRO_IS_BIG_ENDIAN
                        SWAP_ENDIAN(u.val);
#endif
                        self->roomCode  = u.val;

                        u.val = self->roomCode;
#if RETRO_IS_BIG_ENDIAN
                        SWAP_ENDIAN(u.val);
#endif
                        for (int i = 0; i < 8; i += 2) {
                            int n         = 7 - i;
                            int nybbles[] = { u.bytes[n >> 1] & 0xF, ((u.bytes[n >> 1] & 0xF0) >> 4) & 0xF };

                            self->enterCodeLabel[i + 0]->alpha = 0x100;
                            self->enterCodeLabel[i + 1]->alpha = 0x100;

                            self->enterCodeLabel[i + 0]->useColors = false;
                            self->enterCodeLabel[i + 1]->useColors = false;

                            char codeBuf[0x10];
                            sprintf(codeBuf, "%X", nybbles[1]);
                            SetStringToFont8(self->enterCodeLabel[i + 0]->text, codeBuf, self->enterCodeLabel[i + 0]->fontID);
                            self->enterCodeLabel[i + 0]->alignPtr(self->enterCodeLabel[i + 0], ALIGN_CENTER);

                            sprintf(codeBuf, "%X", nybbles[0]);
                            SetStringToFont8(self->enterCodeLabel[i + 1]->text, codeBuf, self->enterCodeLabel[i + 1]->fontID);
                            self->enterCodeLabel[i + 1]->alignPtr(self->enterCodeLabel[i + 1], ALIGN_CENTER);
                        }
                        self->enterCodeLabel[id]->useColors = true;

                        self->touchedUpID   = -1;
                        self->touchedDownID = -1;
                    }

                    if (self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->state == PUSHBUTTON_STATE_SELECTED) {
                        PlaySfxByName("Menu Select", false);
                        self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->state = PUSHBUTTON_STATE_UNSELECTED;
                        self->selectedButton                                    = MULTIPLAYERSCREEN_BUTTON_JOINROOM;
                        self->state                                             = MULTIPLAYERSCREEN_STATE_ACTION;
                    }
                    else if (self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->state == PUSHBUTTON_STATE_SELECTED) {
                        self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->state = PUSHBUTTON_STATE_FLASHING;
#if RETRO_PLATFORM != RETRO_PS3
                        char buf[0x30];
                        char *txt = SDL_GetClipboardText(); // easier bc we must SDL free after
                        if (StrLength(txt) && StrLength(txt) < 0x30 - 2) {
                            StrCopy(buf, "0x");
                            StrAdd(buf, txt);
                            int before = self->roomCode;
                            if (ConvertStringToInteger(buf, &self->roomCode)) {
                                MultiplayerScreen_DrawJoinCode(self, 0);
                                self->enterCodeLabel[0]->useColors = false;
                                self->selectedButton               = 5;
                                if (Engine.gameType == GAME_SONIC1)
                                    PlaySfxByName("Lamp Post", false);
                                else
                                    PlaySfxByName("Star Post", false);
                            }
                            else {
                                self->roomCode = before;
                                PlaySfxByName("Hurt", false);
                            }
                        }
                        else
                            PlaySfxByName("Hurt", false);
                        SDL_free(txt);
#else
                        PlaySfxByName("Hurt", false);
#endif
                    }

                    if (keyPress.B || self->backPressed) {
                        PlaySfxByName("Menu Back", false);
                        self->backPressed   = false;
                        self->state         = MULTIPLAYERSCREEN_STATE_FLIP;
                        self->nextState     = MULTIPLAYERSCREEN_STATE_MAIN;
                        self->nextStateDraw = MULTIPLAYERSCREEN_STATEDRAW_MAIN;
                    }
                    else {
                        if (keyDown.left) {
                            self->selectedButton = 5;
                            usePhysicalControls  = true;
                        }
                        if (keyDown.right) {
                            self->selectedButton = 12;
                            usePhysicalControls  = true;
                        }
                    }
                }
            }
            break;
        }
        case MULTIPLAYERSCREEN_STATE_IPENTER: {
            CheckKeyDown(&keyDown);
            CheckKeyPress(&keyPress);

            if (usePhysicalControls) {
                if (touches > 0) {
                    usePhysicalControls = false;
                }
                else {
                    if (keyPress.left) {
                        PlaySfxByName("Menu Move", false);
                        if (self->selectedButton == 5)
                            self->selectedButton = MULTIPLAYERSCREEN_BUTTON_JOINROOM;
                        else if (self->selectedButton == MULTIPLAYERSCREEN_BUTTON_JOINROOM)
                            self->selectedButton = 16;
                        else
                            self->selectedButton--;
                    }
                    else if (keyPress.right) {
                        PlaySfxByName("Menu Move", false);
                        if (self->selectedButton == 16)
                            self->selectedButton = MULTIPLAYERSCREEN_BUTTON_JOINROOM;
                        else if (self->selectedButton == MULTIPLAYERSCREEN_BUTTON_JOINROOM)
                            self->selectedButton = 5;
                        else
                            self->selectedButton++;
                    }

                    if (self->selectedButton != MULTIPLAYERSCREEN_BUTTON_JOINROOM && (keyPress.up || keyPress.down)) {
                        int digitIdx = self->selectedButton - 5;
                        if (digitIdx >= 0 && digitIdx < 12) {
                            int val = self->ipDigits[digitIdx];
                            if (keyPress.up) {
                                PlaySfxByName("Menu Move", false);
                                val = (val + 1) % 10;
                            }
                            else if (keyPress.down) {
                                PlaySfxByName("Menu Move", false);
                                val = (val + 9) % 10;
                            }
                            self->ipDigits[digitIdx] = val;
                            char buf[2];
                            sprintf(buf, "%c", val + 'A');
                            SetStringToFont8(self->ipDigitLabel[digitIdx]->text, buf, self->ipDigitLabel[digitIdx]->fontID);
                        }
                    }

                    for (int i = 0; i < 12; ++i) self->ipDigitLabel[i]->useColors = false;
                    self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->state = PUSHBUTTON_STATE_UNSELECTED;
                    self->enterCodeSlider[0]->alpha                         = 0;
                    self->enterCodeSlider[1]->alpha                         = 0;

                    if (self->selectedButton == MULTIPLAYERSCREEN_BUTTON_JOINROOM)
                        self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->state = PUSHBUTTON_STATE_SELECTED;
                    else if (self->selectedButton >= 5 && self->selectedButton <= 16) {
                        int digitIdx                    = self->selectedButton - 5;
                        self->enterCodeSlider[0]->x     = self->ipDigitLabel[digitIdx]->x;
                        self->enterCodeSlider[1]->x     = -self->ipDigitLabel[digitIdx]->x;
                        self->enterCodeSlider[0]->alpha = 0x100;
                        self->enterCodeSlider[1]->alpha = 0x100;
                        self->ipDigitLabel[digitIdx]->useColors = true;
                        self->ipDigitLabel[digitIdx]->r         = 255;
                        self->ipDigitLabel[digitIdx]->g         = 255;
                        self->ipDigitLabel[digitIdx]->b         = 0;
                    }

                    if (keyPress.start || keyPress.A) {
                        if (self->selectedButton == MULTIPLAYERSCREEN_BUTTON_JOINROOM) {
                            PlaySfxByName("Menu Select", false);

                            // Revert enhanced obfuscation: reverse back
                            int digits[12];
                            for (int i = 0; i < 12; ++i) {
                                digits[11 - i] = self->ipDigits[i];
                            }

                            // Parse IP from digits
                            char newIP[64];
                            int offset = 0;
                            for (int i = 0; i < 4; ++i) {
                                int val = digits[i * 3] * 100 + digits[i * 3 + 1] * 10 + digits[i * 3 + 2];
                                sprintf(newIP + offset, "%d", val);
                                offset = strlen(newIP);
                                if (i < 3)
                                    newIP[offset++] = '.';
                            }
                            newIP[offset] = '\0';

                            StrCopy(networkHost, newIP);
                            WriteSettings();
                            RunNetwork();

                            self->state         = MULTIPLAYERSCREEN_STATE_FLIP;
                            self->nextState     = MULTIPLAYERSCREEN_STATE_JOINSCR;
                            self->nextStateDraw = MULTIPLAYERSCREEN_STATEDRAW_JOIN;
                            self->flipDir       = 1;
                        }
                    }
                    else if (keyPress.B) {
                        PlaySfxByName("Menu Back", false);
                        self->state         = MULTIPLAYERSCREEN_STATE_FLIP;
                        self->nextState     = MULTIPLAYERSCREEN_STATE_MAIN;
                        self->nextStateDraw = MULTIPLAYERSCREEN_STATEDRAW_MAIN;
                    }
                }
            }
            break;
        }
        case MULTIPLAYERSCREEN_STATE_DIALOGWAIT: {
            if (self->dialog->selection == DLG_NO || self->dialog->selection == DLG_OK) {
                self->state = MULTIPLAYERSCREEN_STATE_HOSTSCR;
            }
            else if (self->dialog->selection == DLG_YES) {
                PlaySfxByName("Menu Back", false);
                self->backPressed             = false;
                self->state                   = MULTIPLAYERSCREEN_STATE_EXIT;
                self->timer                   = 0;
                NativeEntity_FadeScreen *fade = CREATE_ENTITY(FadeScreen);
                fade->delay                   = 1.1f;
                fade->state                   = FADESCREEN_STATE_FADEOUT;
                DisconnectNetwork();
                InitNetwork();
            }
            break;
        }
    }

    switch (self->stateDraw) {
        default: break;
        case MULTIPLAYERSCREEN_STATEDRAW_MAIN:
             // PrintLog("MultiplayerScreen - DrawState: MAIN");
            for (int i = 0; i < MULTIPLAYERSCREEN_BUTTON_COUNT; ++i) self->buttons[i]->alpha = 0;
            self->buttons[MULTIPLAYERSCREEN_BUTTON_HOST]->alpha = 0x100;
            self->buttons[MULTIPLAYERSCREEN_BUTTON_JOIN]->alpha = 0x100;

            for (int i = 0; i < 3; ++i) self->codeLabel[i]->alpha = 0;
            for (int i = 0; i < 8; ++i) self->enterCodeLabel[i]->alpha = 0;
            for (int i = 0; i < 2; ++i) self->enterCodeSlider[i]->alpha = 0;
            if (self->ipPrefixLabel) self->ipPrefixLabel->alpha = 0;
            for (int i = 0; i < 12; ++i) self->ipDigitLabel[i]->alpha = 0;
            for (int i = 0; i < 3; ++i) self->ipDotLabel[i]->alpha = 0;

            self->selectedButton = MULTIPLAYERSCREEN_BUTTON_HOST;
            vsPlayerID           = -1;
            break;
        case MULTIPLAYERSCREEN_STATEDRAW_HOST: {
             // PrintLog("MultiplayerScreen - DrawState: HOST, vsPlayerID=%d", vsPlayerID);
            for (int i = 0; i < MULTIPLAYERSCREEN_BUTTON_COUNT; ++i) self->buttons[i]->alpha = 0;
            for (int i = 0; i < 3; ++i) self->codeLabel[i]->alpha = 0x100;
            for (int i = 0; i < 8; ++i) self->enterCodeLabel[i]->alpha = 0;
            for (int i = 0; i < 2; ++i) self->enterCodeSlider[i]->alpha = 0;
            if (self->ipPrefixLabel) self->ipPrefixLabel->alpha = 0;
            for (int i = 0; i < 12; ++i) self->ipDigitLabel[i]->alpha = 0;
            for (int i = 0; i < 3; ++i) self->ipDotLabel[i]->alpha = 0;

            self->selectedButton = -1;

            self->roomCode = 0;
            SetStringToFont8(self->codeLabel[1]->text, "FETCHING...", self->codeLabel[1]->fontID);
            self->codeLabel[1]->alignPtr(self->codeLabel[1], ALIGN_CENTER);

            ServerPacket send;
            memset(&send, 0, sizeof(ServerPacket));
            send.header = CL_REQUEST_CODE;
            // send over a preferred roomcode style
            if (!vsGameLength)
                vsGameLength = 4;
            if (!vsItemMode)
                vsItemMode = 1;
            send.data.multiData.type    = 0x00000FF0;
            send.data.multiData.data[0] = (vsGameLength << 4) | (vsItemMode << 8);
            vsPlayerID                  = 0; // we are... Big Host

            SendServerPacket(send, true);
            break;
        }
        case MULTIPLAYERSCREEN_STATEDRAW_IPENTER: {
             // PrintLog("MultiplayerScreen - DrawState: IPENTER");
            for (int i = 0; i < MULTIPLAYERSCREEN_BUTTON_COUNT; ++i) self->buttons[i]->alpha = 0;
            self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->alpha = 0x100;
            self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->x     = 0.0;
            self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->state = PUSHBUTTON_STATE_SCALED;
            SetStringToFont8(self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->text, "CONFIRM", FONT_LABEL);

            for (int i = 0; i < 3; ++i) self->codeLabel[i]->alpha = 0;
            for (int i = 0; i < 8; ++i) self->enterCodeLabel[i]->alpha = 0;

            // Initialize IP digits from current networkHost
            int ip[4] = { 0, 0, 0, 0 };
            sscanf(networkHost, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);
            int tempDigits[12];
            for (int i = 0; i < 4; ++i) {
                tempDigits[i * 3 + 0] = (ip[i] / 100) % 10;
                tempDigits[i * 3 + 1] = (ip[i] / 10) % 10;
                tempDigits[i * 3 + 2] = ip[i] % 10;
            }

            // Enhanced obfuscation: reverse digits
            for (int i = 0; i < 12; ++i) {
                self->ipDigits[i] = tempDigits[11 - i];
            }

            if (self->ipPrefixLabel) self->ipPrefixLabel->alpha = 0x100;
            for (int i = 0; i < 12; ++i) {
                self->ipDigitLabel[i]->alpha     = 0x100;
                self->ipDigitLabel[i]->useColors = false;
                char buf[2];
                sprintf(buf, "%c", self->ipDigits[i] + 'A');
                SetStringToFont8(self->ipDigitLabel[i]->text, buf, self->ipDigitLabel[i]->fontID);
                self->ipDigitLabel[i]->alignPtr(self->ipDigitLabel[i], ALIGN_CENTER);
            }
            for (int i = 0; i < 3; ++i) {
                self->ipDotLabel[i]->alpha = 0;
                self->ipDotLabel[i]->alignPtr(self->ipDotLabel[i], ALIGN_CENTER);
            }

            self->ipDigitLabel[0]->useColors = true;
            self->selectedButton             = 5;

            self->enterCodeSlider[0]->x     = self->ipDigitLabel[0]->x;
            self->enterCodeSlider[1]->x     = -self->ipDigitLabel[0]->x;
            self->enterCodeSlider[0]->alpha = 0x100;
            self->enterCodeSlider[1]->alpha = 0x100;
            break;
        }
        case MULTIPLAYERSCREEN_STATEDRAW_JOIN: {
             // PrintLog("MultiplayerScreen - DrawState: JOIN");
            self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->x     = -56.0;
            self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->state = PUSHBUTTON_STATE_SCALED;
            SetStringToFont8(self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->text, "JOIN ROOM", FONT_LABEL);
            self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->x     = 64.0;
            self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->state = PUSHBUTTON_STATE_SCALED;
            for (int i = 0; i < MULTIPLAYERSCREEN_BUTTON_COUNT; ++i) self->buttons[i]->alpha = 0;
            self->selectedButton = MULTIPLAYERSCREEN_BUTTON_COUNT;
            self->touchedUpID    = 0;
            self->touchedDownID  = -1;

            for (int i = 0; i < 3; ++i) self->codeLabel[i]->alpha = 0;

            self->roomCode  = 0;
            vsPlayerID      = 1; // we are... Little Guy
            for (int i = 0; i < 8; ++i) {
                self->enterCodeLabel[i]->alpha     = 0;
                self->enterCodeLabel[i]->useColors = false;
            }
            self->buttons[MULTIPLAYERSCREEN_BUTTON_JOINROOM]->alpha = 0;
            self->buttons[MULTIPLAYERSCREEN_BUTTON_PASTE]->alpha    = 0;
            if (self->ipPrefixLabel) self->ipPrefixLabel->alpha = 0;
            for (int i = 0; i < 12; ++i) self->ipDigitLabel[i]->alpha = 0;
            for (int i = 0; i < 3; ++i) self->ipDotLabel[i]->alpha = 0;

            break;
        }
    }
    self->stateDraw = MULTIPLAYERSCREEN_STATEDRAW_NONE;

    SetRenderBlendMode(RENDER_BLEND_ALPHA);
    RenderMesh(self->meshPanel, MESH_COLORS, false);
    SetRenderBlendMode(RENDER_BLEND_ALPHA);
    NewRenderState();
    SetRenderMatrix(NULL);

    if (self->backPressed)
        RenderImage(128.0, -92.0, 160.0, 0.3, 0.3, 64.0, 64.0, 128.0, 128.0, 128.0, 128.0, self->arrowAlpha, self->textureArrows);
    else
        RenderImage(128.0, -92.0, 160.0, 0.3, 0.3, 64.0, 64.0, 128.0, 128.0, 128.0, 0.0, self->arrowAlpha, self->textureArrows);

    if (self->state == MULTIPLAYERSCREEN_STATE_JOINSCR) {
        for (int i = 0; i < 8; ++i) self->enterCodeLabel[i]->alpha = 0;
        self->codeLabel[1]->alpha = 0;
        self->enterCodeSlider[0]->alpha = 0;
        self->enterCodeSlider[1]->alpha = 0;

        if (availableRoomCount > 0) {
            int count = availableRoomCount > 8 ? 8 : availableRoomCount;
            for (int i = 0; i < count; ++i) {
                float y = 40.0f - (i * 20.0f);
                
                self->enterCodeLabel[i]->alpha = 0x100;
                self->enterCodeLabel[i]->useColors = (i == self->touchedUpID);
                self->enterCodeLabel[i]->x = 0;
                self->enterCodeLabel[i]->y = y;
                SetStringToFont8(self->enterCodeLabel[i]->text, availableRooms[i].username, FONT_LABEL);
                self->enterCodeLabel[i]->alignPtr(self->enterCodeLabel[i], ALIGN_CENTER);
                memcpy(&self->enterCodeLabel[i]->renderMatrix, &self->renderMatrix, sizeof(MatrixF));

                if (i == self->touchedUpID) {
                    self->enterCodeSlider[0]->alpha = 0x100;
                    self->enterCodeSlider[0]->x = -80.0f;
                    self->enterCodeSlider[0]->y = y + 4.0f;
                    memcpy(&self->enterCodeSlider[0]->renderMatrix, &self->renderMatrix, sizeof(MatrixF));

                    self->enterCodeSlider[1]->alpha = 0x100;
                    self->enterCodeSlider[1]->x = 80.0f;
                    self->enterCodeSlider[1]->y = y - 4.0f;
                    MatrixRotateZF(&self->enterCodeSlider[1]->renderMatrix, DegreesToRad(180));
                    MatrixMultiplyF(&self->enterCodeSlider[1]->renderMatrix, &self->renderMatrix);
                }
            }
        } else {
            self->codeLabel[1]->alpha = 0x100;
            self->codeLabel[1]->y = 0;
            SetStringToFont8(self->codeLabel[1]->text, "NO ROOMS FOUND", FONT_LABEL);
            self->codeLabel[1]->alignPtr(self->codeLabel[1], ALIGN_CENTER);
            memcpy(&self->codeLabel[1]->renderMatrix, &self->renderMatrix, sizeof(MatrixF));
        }
    }
}
#endif
