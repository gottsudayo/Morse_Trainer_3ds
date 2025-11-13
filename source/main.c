#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

// 定数定義
#define MAX_MORSE 20
#define MAX_SENTENCE 200
#define MAX_HISTORY 50

// モールス信号記号
#define DOT_CHAR "."
#define DASH_CHAR "-"

// モールス信号辞書（Unicode使用）
typedef struct {
    const char* morse;
    const wchar_t* character;
} MorseEntry;

// 英語モールス信号
const MorseEntry morseAlpha[] = {
    {".-", L"A"}, {"-...", L"B"}, {"-.-.", L"C"}, {"-..", L"D"}, {".", L"E"},
    {"..-.", L"F"}, {"--.", L"G"}, {"....", L"H"}, {"..", L"I"}, {".---", L"J"},
    {"-.-", L"K"}, {".-..", L"L"}, {"--", L"M"}, {"-.", L"N"}, {"---", L"O"},
    {".--.", L"P"}, {"--.-", L"Q"}, {".-.", L"R"}, {"...", L"S"}, {"-", L"T"},
    {"..-", L"U"}, {"...-", L"V"}, {".--", L"W"}, {"-..-", L"X"}, {"-.--", L"Y"},
    {"--..", L"Z"},
    {".----", L"1"}, {"..---", L"2"}, {"...--", L"3"}, {"....-", L"4"}, {".....", L"5"},
    {"-....", L"6"}, {"--...", L"7"}, {"---..", L"8"}, {"----.", L"9"}, {"-----", L"0"},
    {NULL, NULL}
};

// 日本語モールス信号（Unicode使用）
const MorseEntry morseKana[] = {
    {".-", L"ア"}, {".-.-", L"イ"}, {"..-", L"ウ"}, {"-.---", L"エ"}, {".-...", L"オ"},
    {"-.-..", L"カ"}, {".-..", L"キ"}, {"...-", L"ク"}, {"-.--", L"ケ"}, {"----", L"コ"},
    {"-.-.", L"サ"}, {"--.-", L"シ"}, {"---.-", L"ス"}, {".---.", L"セ"}, {"---.", L"ソ"},
    {"-.", L"タ"}, {"..-.", L"チ"}, {".-..", L"ツ"}, {".-.--", L"テ"}, {"..-.-", L"ト"},
    {".-.", L"ナ"}, {"-.-.", L"ニ"}, {"....", L"ヌ"}, {"--.-", L"ネ"}, {"..--", L"ノ"},
    {"-...", L"ハ"}, {"--.--", L"ヒ"}, {"--..", L"フ"}, {".", L"ヘ"}, {"---", L"ホ"},
    {"--", L"マ"}, {"-..-", L"ミ"}, {"-", L"ム"}, {"-...-", L"メ"}, {"-..-.", L"モ"},
    {".--", L"ヤ"}, {".--.", L"ユ"}, {"--..", L"ヨ"},
    {"...", L"ラ"}, {"--.", L"リ"}, {".--.", L"ル"}, {"---", L"レ"}, {".---", L"ロ"},
    {".--.-", L"ワ"}, {".-..-", L"ヲ"}, {".--.", L"ン"},
    {NULL, NULL}
};

// グローバル変数
char currentMorse[MAX_MORSE] = "";
char sentence[MAX_SENTENCE][MAX_MORSE];
wchar_t sentenceChars[MAX_SENTENCE];
int sentenceLen = 0;

wchar_t history[MAX_HISTORY][256];
char historyMorse[MAX_HISTORY][1024];
int historyCount = 0;

int currentLanguage = 0; // 0=日本語, 1=英語
bool isPressed = false;
u64 pressStartTime = 0;
bool isListView = false;
u64 selectPressTime = 0;

int dotDurations[10] = {0};
int dashDurations[10] = {0};
int dotCount = 0;
int dashCount = 0;

u64 lastInputTime = 0;

// 画面とテキストバッファ
C3D_RenderTarget* top;
C3D_RenderTarget* bottom;
C2D_TextBuf dynBuf;
C2D_Text text;

// システムフォント
C2D_Font systemFont;

// 関数プロトタイプ
void initApp(void);
void drawUI(void);
void processInput(void);
const wchar_t* decodeMorse(const char* morse);
int getAvgDuration(int* arr, int count);
int getCharGap(void);
void addToSentence(const char* morse, const wchar_t* character);
void saveToHistory(void);
void clearCurrentInput(void);
void clearAll(void);
void drawTextWide(const wchar_t* wstr, float x, float y, float z, float scaleX, float scaleY, u32 color);

int main(int argc, char* argv[]) {
    // 初期化
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    
    top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    
    dynBuf = C2D_TextBufNew(4096);
    
    // システムフォントをロード
    systemFont = C2D_FontLoadSystem(CFG_REGION_JPN);
    
    initApp();
    
    // メインループ
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        
        if (kDown & KEY_START && !isListView) break;
        
        processInput();
        drawUI();
        
        C3D_FrameEnd(0);
    }
    
    // 終了処理
    C2D_FontFree(systemFont);
    C2D_TextBufDelete(dynBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}

void initApp(void) {
    memset(currentMorse, 0, sizeof(currentMorse));
    memset(sentence, 0, sizeof(sentence));
    memset(sentenceChars, 0, sizeof(sentenceChars));
    sentenceLen = 0;
    historyCount = 0;
    lastInputTime = osGetTime();
}

void drawTextWide(const wchar_t* wstr, float x, float y, float z, float scaleX, float scaleY, u32 color) {
    if (wstr == NULL || wcslen(wstr) == 0) return;
    
    C2D_TextBufClear(dynBuf);
    
    // UTF-16からテキストオブジェクトを作成
    C2D_TextFontParse(&text, systemFont, dynBuf, (const char*)wstr);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, x, y, z, scaleX, scaleY, color);
}

void drawUI(void) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    
    // 上画面描画
    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(top);
    
    // 翻訳ボックス（背景）
    C2D_DrawRectSolid(10, 10, 0.5f, 380, 85, C2D_Color32(236, 240, 241, 255));
    
    // モールス信号ボックス（背景）
    C2D_DrawRectSolid(10, 105, 0.5f, 380, 125, C2D_Color32(52, 73, 94, 255));
    
    // 翻訳テキスト表示（ワイド文字）
    if (wcslen(sentenceChars) > 0) {
        drawTextWide(sentenceChars, 20, 20, 0.5f, 0.6f, 0.6f, C2D_Color32(44, 62, 80, 255));
    }
    
    // モールス信号表示（ASCII）
    char morseDisplay[512] = "";
    for (int i = 0; i < sentenceLen; i++) {
        strcat(morseDisplay, sentence[i]);
        strcat(morseDisplay, " ");
    }
    if (strlen(currentMorse) > 0) {
        strcat(morseDisplay, currentMorse);
    }
    
    if (strlen(morseDisplay) > 0) {
        C2D_TextBufClear(dynBuf);
        C2D_TextFontParse(&text, systemFont, dynBuf, morseDisplay);
        C2D_TextOptimize(&text);
        C2D_DrawText(&text, C2D_WithColor, 20, 115, 0.5f, 0.7f, 0.7f, C2D_Color32(236, 240, 241, 255));
    }
    
    // 下画面描画
    C2D_TargetClear(bottom, C2D_Color32(44, 62, 80, 255));
    C2D_SceneBegin(bottom);
    
    if (!isListView) {
        // 言語切替ボタン
        u32 jaColor = (currentLanguage == 0) ? C2D_Color32(52, 152, 219, 255) : C2D_Color32(51, 51, 51, 255);
        u32 enColor = (currentLanguage == 1) ? C2D_Color32(52, 152, 219, 255) : C2D_Color32(51, 51, 51, 255);
        
        C2D_DrawRectSolid(10, 10, 0.5f, 145, 50, jaColor);
        C2D_DrawRectSolid(165, 10, 0.5f, 145, 50, enColor);
        
        // ボタンテキスト
        C2D_TextBufClear(dynBuf);
        C2D_TextFontParse(&text, systemFont, dynBuf, "JP");
        C2D_TextOptimize(&text);
        C2D_DrawText(&text, C2D_WithColor, 65, 25, 0.5f, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));
        
        C2D_TextFontParse(&text, systemFont, dynBuf, "EN");
        C2D_TextOptimize(&text);
        C2D_DrawText(&text, C2D_WithColor, 205, 25, 0.5f, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));
        
        // モールスボタン
        u32 btnColor = isPressed ? C2D_Color32(192, 57, 43, 255) : C2D_Color32(231, 76, 60, 255);
        C2D_DrawCircleSolid(160, 145, 0.5f, 65, btnColor);
        
        C2D_TextFontParse(&text, systemFont, dynBuf, "PRESS");
        C2D_TextOptimize(&text);
        C2D_DrawText(&text, C2D_WithColor, 130, 140, 0.5f, 0.5f, 0.5f, C2D_Color32(255, 255, 255, 255));
    } else {
        // リスト画面
        C2D_TextBufClear(dynBuf);
        C2D_TextFontParse(&text, systemFont, dynBuf, "History (START:Back)");
        C2D_TextOptimize(&text);
        C2D_DrawText(&text, C2D_WithColor, 50, 10, 0.5f, 0.4f, 0.4f, C2D_Color32(255, 255, 255, 255));
        
        // 履歴項目表示
        for (int i = 0; i < historyCount && i < 5; i++) {
            int yPos = 40 + (i * 35);
            C2D_DrawRectSolid(10, yPos, 0.5f, 300, 30, C2D_Color32(52, 73, 94, 255));
            
            if (wcslen(history[historyCount - 1 - i]) > 0) {
                drawTextWide(history[historyCount - 1 - i], 15, yPos + 5, 0.5f, 0.35f, 0.35f, C2D_Color32(52, 152, 219, 255));
            }
        }
    }
}

void processInput(void) {
    u32 kDown = hidKeysDown();
    u32 kUp = hidKeysUp();
    touchPosition touch;
    
    // タッチ入力
    if (kDown & KEY_TOUCH) {
        hidTouchRead(&touch);
        
        // 言語切替
        if (!isListView && touch.py >= 10 && touch.py <= 60) {
            if (touch.px >= 10 && touch.px <= 155) {
                currentLanguage = 0;
            } else if (touch.px >= 165 && touch.px <= 310) {
                currentLanguage = 1;
            }
        }
        
        // モールスボタン
        if (!isListView && touch.py >= 80 && touch.py <= 210) {
            int dx = touch.px - 160;
            int dy = touch.py - 145;
            if (dx*dx + dy*dy <= 65*65) {
                isPressed = true;
                pressStartTime = osGetTime();
            }
        }
    }
    
    if (kUp & KEY_TOUCH && isPressed) {
        u64 duration = osGetTime() - pressStartTime;
        
        if (duration > 200) {
            if (strlen(currentMorse) < MAX_MORSE - 2) {
                strcat(currentMorse, DASH_CHAR);
            }
            if (dashCount < 10) {
                dashDurations[dashCount++] = (int)duration;
            }
        } else {
            if (strlen(currentMorse) < MAX_MORSE - 2) {
                strcat(currentMorse, DOT_CHAR);
            }
            if (dotCount < 10) {
                dotDurations[dotCount++] = (int)duration;
            }
        }
        
        lastInputTime = osGetTime();
        isPressed = false;
    }
    
    // Aボタン
    if (kDown & KEY_A && !isListView) {
        isPressed = true;
        pressStartTime = osGetTime();
    }
    
    if (kUp & KEY_A && isPressed) {
        u64 duration = osGetTime() - pressStartTime;
        
        if (duration > 200) {
            if (strlen(currentMorse) < MAX_MORSE - 2) {
                strcat(currentMorse, DASH_CHAR);
            }
            if (dashCount < 10) {
                dashDurations[dashCount++] = (int)duration;
            }
        } else {
            if (strlen(currentMorse) < MAX_MORSE - 2) {
                strcat(currentMorse, DOT_CHAR);
            }
            if (dotCount < 10) {
                dotDurations[dotCount++] = (int)duration;
            }
        }
        
        lastInputTime = osGetTime();
        isPressed = false;
    }
    
    // 文字認識タイミング
    if (strlen(currentMorse) > 0 && !isPressed) {
        u64 elapsed = osGetTime() - lastInputTime;
        int charGap = getCharGap();
        
        if (elapsed > charGap) {
            const wchar_t* decoded = decodeMorse(currentMorse);
            if (decoded) {
                addToSentence(currentMorse, decoded);
            }
            memset(currentMorse, 0, sizeof(currentMorse));
        }
    }
    
    // 十字キー下
    if (kDown & KEY_DOWN && !isListView) {
        clearCurrentInput();
    }
    
    // Xボタン
    if (kDown & KEY_X && !isListView) {
        clearAll();
    }
    
    // Bボタン
    if (kDown & KEY_B && !isListView) {
        saveToHistory();
    }
    
    // STARTボタン
    if (kDown & KEY_START) {
        isListView = !isListView;
    }
    
    // SELECTボタン（リスト画面で2回押下で全削除）
    if (kDown & KEY_SELECT && isListView) {
        u64 now = osGetTime();
        if (now - selectPressTime < 500) {
            historyCount = 0;
            selectPressTime = 0;
        } else {
            selectPressTime = now;
        }
    }
}

const wchar_t* decodeMorse(const char* morse) {
    const MorseEntry* dict = (currentLanguage == 0) ? morseKana : morseAlpha;
    
    for (int i = 0; dict[i].morse != NULL; i++) {
        if (strcmp(morse, dict[i].morse) == 0) {
            return dict[i].character;
        }
    }
    
    return NULL;
}

int getAvgDuration(int* arr, int count) {
    if (count == 0) return 0;
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += arr[i];
    }
    return sum / count;
}

int getCharGap(void) {
    int avgDot = getAvgDuration(dotDurations, dotCount);
    int avgDash = getAvgDuration(dashDurations, dashCount);
    
    if (avgDot == 0) avgDot = 150;
    if (avgDash == 0) avgDash = 450;
    
    int avgUnit = (avgDot + avgDash / 3) / 2;
    int gap = (int)(avgUnit * 2.5);
    
    return (gap > 400) ? gap : 400;
}

void addToSentence(const char* morse, const wchar_t* character) {
    if (sentenceLen < MAX_SENTENCE - 1 && character != NULL) {
        strncpy(sentence[sentenceLen], morse, MAX_MORSE - 1);
        sentence[sentenceLen][MAX_MORSE - 1] = '\0';
        
        // ワイド文字を追加
        int len = wcslen(sentenceChars);
        if (len < MAX_SENTENCE - 1) {
            sentenceChars[len] = character[0];
            sentenceChars[len + 1] = L'\0';
        }
        
        sentenceLen++;
    }
}

void saveToHistory(void) {
    if (sentenceLen == 0) return;
    if (historyCount >= MAX_HISTORY) return;
    
    // 翻訳を履歴に保存
    wcsncpy(history[historyCount], sentenceChars, 255);
    history[historyCount][255] = L'\0';
    
    // モールス信号を履歴に保存
    strcpy(historyMorse[historyCount], "");
    for (int i = 0; i < sentenceLen; i++) {
        strcat(historyMorse[historyCount], sentence[i]);
        strcat(historyMorse[historyCount], " ");
    }
    
    historyCount++;
    
    // 現在の入力をクリア
    clearCurrentInput();
}

void clearCurrentInput(void) {
    memset(currentMorse, 0, sizeof(currentMorse));
    memset(sentence, 0, sizeof(sentence));
    memset(sentenceChars, 0, sizeof(sentenceChars));
    sentenceLen = 0;
}

void clearAll(void) {
    clearCurrentInput();
    historyCount = 0;
}