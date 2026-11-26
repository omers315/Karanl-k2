/*
 * MALIKANE 3D - raylib ile birinci sahis korku oyunu (C)
 *
 * Kontroller (Ayarlar > Tus atama ile degistirilebilir):
 *   W A S D / OKLAR : hareket          FARE : bakis (fener yonu)
 *   NUMPAD 4/6/8/2  : kamerayi dondur (NumLock acik)   NUMPAD 5 : bakisi ortala
 *   SHIFT           : kosu (gurultu yapar)   C / CTRL : egil (sessiz, yavas; kapiyi yavas acar)
 *   F / Sol Tik     : fener       Q : kibrit yak (zayif, sicak isik)
 *   E               : etkilesim (kapi ac/kapat, dolap/cekmece/sandik ac, esyayi al, merdiven, jenerator, sigorta kutusu)
 *   BOSLUK          : dolapta nefes tut     G / Sag Tik : sise at (dikkat dagit)
 *   FARE TEKERLEGI  : fener odagi (dar-uzun / genis-kisa)
 *   V               : guvenlik kameralari (fare: tikla/tekerlek/numara kutulari ile kamera degistir, ESC/V: kapat)
 *   TAB             : not defteri            M : mini harita
 *   ESC / Start     : duraklat (devam et / menuye don / cikis)    F11 : tam ekran    F9 : dokular ac/kapat
 *   Oyun kolu: sol cubuk yuru, sag cubuk bak, A etkilesim, X fener, Y sise, B egil, RT kos, RB kibrit, LB not, LT nefes, Geri kamera
 *
 * Amac: Iki katli malikanede 6 sayfayi bul, sigortayi tak ve jeneratoru calistir (elektrik),
 * sonra zemin kattaki yesil cikis kapisina ulas. Tum notlari bulursan gizli sona ulasirsin.
 * Dusmanlar: Bekci (ust kat, kor ama duyar),
 * Heykel (sen ona bakarken donar, bakmazken yaklasir; ilk sayfadan sonra uyanir).
 *
 * Istege bagli dosyalar (exe ile ayni klasore):
 *   textures/ klasoru, PNG (wall, wainscot, floor_wood, floor_tile, floor_stone, ceiling, door, wood, fabric, rug,
 *                   warden, statue)   - dokular / canavar resimleri
 *   models/ klasoru, GLB (bookshelf, bed, ... fusebox; warden, statue) - 3B modeller
 *   sounds/ klasoru, wav/ogg/mp3 (step, heart, drone, creak, slam, ... scream, pad, pulse, musicbox, thunder ...)
 *   intro/intro.mp4 : kendi baslangic videon (FFmpeg gerekir; kareler otomatik hazirlanir)
 *   menu_full.png : yazilari hazir menu gorseli      menu_bg.png : yazisiz menu arka plani
 *   font.ttf      : Turkce karakterli yazi tipi
 *
 * Derleme (Windows):
 *   gcc karanlik_malikane_3d.c -o malikane3d.exe -IC:\raylib\w64devkit\include -LC:\raylib\w64devkit\lib -lraylib -lopengl32 -lgdi32 -lwinmm
 * Linux:
 *   gcc karanlik_malikane_3d.c -o malikane3d -lraylib -lm
 */

#include "raylib.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIN_W 1280
#define WIN_H 720
#define SCREEN_W GetScreenWidth()
#define SCREEN_H GetScreenHeight()
#define MAP_W 60
#define MAP_H 40
#define MAX_ROOMS 12
#define MAX_ITEMS 40
#define MAX_PROPS 160
#define MAX_DOORS 48
#define MAX_LIGHTS 72
#define NFLOORS 2
#define NMON 2
#define MAX_KEYS 4
#define MAX_CAMS 6
#define PAGES_NEEDED 6
#define WALL_H 2.4f
#define VIEW_R 14
#define DOOR_T 0.5f
#define JUMP_LUNGE 0.2f            /* jumpscare: canavarin one atilma suresi (sn) */
#define PAN_SIGN 1.0f           /* ses sag/sol ters geliyorsa -1.0f yap */
#define CFG_FILE "malikane.cfg"
#define TAU 6.2831853f

typedef struct { int x, y, w, h; } Room;
enum { IT_PAGE, IT_BATTERY, IT_KEY, IT_BOTTLE, IT_FUSE, IT_FUEL, IT_MATCH, IT_NOTE };
typedef struct { Vector2 pos; int type, id; bool taken; int prop; float rot; } Item;     /* prop >= 0: mobilyanin icinde */
typedef struct { int tx, ty; bool alongX; float open; bool want; int lock, room; bool slow; } Door;
enum { LK_LAMP, LK_FIRE, LK_MOON };
typedef struct { float x, z, y, rad, inten, warm; int kind, room; } Light;
typedef struct { float x, y, z, tx, tz; int room; } Cam;      /* guvenlik kamerasi (tavan kosesi) */

enum { P_BOOKSHELF, P_BED, P_WARDROBE, P_NIGHTSTAND, P_COUCH, P_ARMCHAIR, P_TABLE, P_DESK, P_CHAIR,
       P_FRIDGE, P_COUNTER, P_CRATE, P_BARREL, P_CLOCK, P_PLANT, P_PIANO, P_FIREPLACE, P_LAMP, P_GENERATOR, P_FUSEBOX, NPROPTYPES };
typedef struct { int type, tx, ty, fx, fz, len; float cx, cz; bool lit; signed char loot; bool done; float open; bool want; } Prop;   /* loot: -1 acilmaz, 0 bos, 1 pil, 2 sise, 3 kibrit, 4 korku */   /* (fx,fz): on yuz yonu */
enum { TH_LIVING, TH_LIBRARY, TH_BEDROOM, TH_KITCHEN, TH_STORAGE, TH_STUDY, TH_CORRIDOR, NTHEMES };
typedef struct { Color wall, panel, trim, floorA, floorB, ceil; int floorType; } Theme;  /* 0 ahsap, 1 fayans, 2 tas */
#define C3(r, g, b) {r, g, b, 255}
static const Theme themes[NTHEMES] = {
    /* salon        */ {C3(92,66,62),  C3(56,36,30), C3(34,22,18), C3(74,50,34),  C3(74,50,34),  C3(58,50,48), 0},
    /* kutuphane    */ {C3(64,74,64),  C3(46,32,24), C3(30,20,14), C3(64,44,30),  C3(64,44,30),  C3(50,52,50), 0},
    /* yatak odasi  */ {C3(86,82,98),  C3(66,56,62), C3(40,34,38), C3(80,60,44),  C3(80,60,44),  C3(62,60,68), 0},
    /* mutfak       */ {C3(104,104,96),C3(72,84,82), C3(50,56,54), C3(120,120,114),C3(70,74,74), C3(70,70,66), 1},
    /* depo         */ {C3(62,60,58),  C3(46,44,43), C3(30,29,28), C3(64,62,60),  C3(56,55,54),  C3(44,43,42), 2},
    /* calisma      */ {C3(76,66,52),  C3(44,30,22), C3(28,18,12), C3(70,48,32),  C3(70,48,32),  C3(54,48,42), 0},
    /* hol/koridor  */ {C3(70,68,76),  C3(42,38,40), C3(28,26,28), C3(56,56,62),  C3(48,48,54),  C3(40,40,46), 2},
};
static const char *thName[NTHEMES] = {"Salon", "Kütüphane", "Yatak Odası", "Mutfak", "Depo", "Çalışma Odası", "Hol"};
typedef struct { int type, len, count; } Want;
static const Want w_living[]  = {{P_FIREPLACE,2,1},{P_COUCH,2,1},{P_ARMCHAIR,1,2},{P_PIANO,2,1},{P_CLOCK,1,1},{P_PLANT,1,2},{P_LAMP,1,1},{-1,0,0}};
static const Want w_library[] = {{P_BOOKSHELF,2,3},{P_BOOKSHELF,1,2},{P_DESK,2,1},{P_ARMCHAIR,1,1},{P_PLANT,1,1},{P_LAMP,1,1},{-1,0,0}};
static const Want w_bedroom[] = {{P_BED,2,1},{P_NIGHTSTAND,1,2},{P_WARDROBE,2,1},{P_PLANT,1,1},{P_LAMP,1,1},{-1,0,0}};
static const Want w_kitchen[] = {{P_COUNTER,2,2},{P_COUNTER,1,1},{P_FRIDGE,1,1},{P_TABLE,2,1},{P_BARREL,1,1},{-1,0,0}};
static const Want w_storage[] = {{P_CRATE,1,5},{P_BARREL,1,3},{P_BOOKSHELF,2,1},{P_WARDROBE,1,1},{-1,0,0}};
static const Want w_study[]   = {{P_DESK,2,1},{P_BOOKSHELF,2,2},{P_CLOCK,1,1},{P_ARMCHAIR,1,1},{P_PLANT,1,1},{P_LAMP,1,1},{-1,0,0}};
static const Want *const wants[NTHEMES] = {w_living, w_library, w_bedroom, w_kitchen, w_storage, w_study, NULL};

typedef struct {
    char grid[MAP_H][MAP_W];
    unsigned char occg[MAP_H][MAP_W];      /* 1: alcak mobilya, 2: yuksek mobilya (gorusu keser) */
    signed char roomg[MAP_H][MAP_W];       /* karonun ait oldugu oda (-1: koridor/duvar) */
    signed char dmapg[MAP_H][MAP_W];       /* kapi indeksi (-1: yok) */
    bool seeng[MAP_H][MAP_W];              /* mini harita: kesfedildi mi */
    unsigned char lkc[MAP_H][MAP_W];       /* dolap kullanim sayisi (canavar ogrenir) */
    unsigned char picChg[MAP_H][MAP_W];    /* tablo degisimi: 0 normal, 1 bos cerceve, 2 kanli */
    Room rm[MAX_ROOMS]; int nrm;
    int th[MAX_ROOMS];
    bool lampg[MAX_ROOMS], lockedg[MAX_ROOMS];
    Prop pr[MAX_PROPS]; int npr;
    Item it[MAX_ITEMS]; int nit;
    Door dr[MAX_DOORS]; int ndr;
    Light lt[MAX_LIGHTS]; int nlt;
    Cam cm[MAX_CAMS]; int ncm;
} Floor;
static Floor FL[NFLOORS];
static Floor *cf = &FL[0];                 /* aktif kat */
#define map (cf->grid)
#define occ (cf->occg)
#define roomOf (cf->roomg)
#define dmap (cf->dmapg)
#define rooms (cf->rm)
#define nrooms (cf->nrm)
#define roomTheme (cf->th)
#define props (cf->pr)
#define nprops (cf->npr)
#define items (cf->it)
#define nitems (cf->nit)
#define doors (cf->dr)
#define ndoors (cf->ndr)
#define lights (cf->lt)
#define nlights (cf->nlt)

typedef enum { ST_MENU, ST_PLAY, ST_PAUSE, ST_DEAD, ST_WIN, ST_SETTINGS, ST_CREDITS, ST_CALIB, ST_KEYS, ST_CINE } GState;
typedef enum { M_PATROL, M_CHASE, M_SEARCH } MState;
enum { K_WARDEN, K_STATUE };

typedef struct {
    Vector2 pos;
    float yaw, pitch, battery, stamina, sanity, bob, stepT, noiseT, eye, focus, breath, matchT;
    bool torch, exhausted, sprinting, hidden, moving, crouch, holding, hasFuse, hasFuel;
    int pages, lockTx, lockTy, keys, bottles, lvl, matches, notes;
} Player;

typedef struct {
    Vector2 pos, target, lastSeen;
    MState state;
    float timer, face, hunchT, swapT, stepT, anim, spd;
    bool seesPlayer, knows, checked, toStairs, active;
    int kind, lvl, pts;                    /* kind: K_WARDEN (Bekci), K_STATUE (Heykel) */
} Monster;

typedef struct { Vector3 p, v; bool on; } Proj;

static struct { float sens, bright, vol; bool fog, fx; int diff; bool jump; float fov; bool post, cine; } cfg = {1.0f, 0.45f, 0.8f, true, true, 1, true, 70.0f, true, true};
static float bestTime = 0;
static bool firstRun;

/* tus atama + oyun kolu */
enum { A_FWD, A_BACK, A_LEFT, A_RIGHT, A_SPRINT, A_CROUCH, A_USE, A_TORCH, A_THROW, A_MATCH, A_NOTES, A_MAP, A_CCTV, A_HOLD, A_COUNT };
static int keymap[A_COUNT] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_LEFT_SHIFT, KEY_C, KEY_E, KEY_F, KEY_G, KEY_Q, KEY_TAB, KEY_M, KEY_V, KEY_SPACE};
static const int gpMap[A_COUNT] = {-1, -1, -1, -1, GAMEPAD_BUTTON_RIGHT_TRIGGER_2, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, GAMEPAD_BUTTON_RIGHT_FACE_DOWN,
    GAMEPAD_BUTTON_RIGHT_FACE_LEFT, GAMEPAD_BUTTON_RIGHT_FACE_UP, GAMEPAD_BUTTON_RIGHT_TRIGGER_1, GAMEPAD_BUTTON_LEFT_TRIGGER_1,
    GAMEPAD_BUTTON_LEFT_THUMB, GAMEPAD_BUTTON_MIDDLE_LEFT, GAMEPAD_BUTTON_LEFT_TRIGGER_2};
static const char *actName[A_COUNT] = {"İleri", "Geri", "Sol", "Sağ", "Koş", "Eğil", "Etkileşim", "Fener", "Şişe At", "Kibrit Yak", "Not Defteri", "Harita", "Kameralar", "Nefes Tut"};
static bool kdown(int a) { return IsKeyDown(keymap[a]) || (gpMap[a] >= 0 && IsGamepadAvailable(0) && IsGamepadButtonDown(0, gpMap[a])); }
static bool kpress(int a) { return IsKeyPressed(keymap[a]) || (gpMap[a] >= 0 && IsGamepadAvailable(0) && IsGamepadButtonPressed(0, gpMap[a])); }
static float gp_axis(int a) {
    if (!IsGamepadAvailable(0)) return 0;
    float v = GetGamepadAxisMovement(0, a);
    return fabsf(v) < 0.2f ? 0.0f : v;
}
static const char *key_name(int k) {
    static char b[16];
    if (k >= KEY_A && k <= KEY_Z) { b[0] = (char)('A' + k - KEY_A); b[1] = 0; return b; }
    if (k >= KEY_ZERO && k <= KEY_NINE) { b[0] = (char)('0' + k - KEY_ZERO); b[1] = 0; return b; }
    if (k >= KEY_F1 && k <= KEY_F12) { snprintf(b, sizeof b, "F%d", k - KEY_F1 + 1); return b; }
    switch (k) {
    case KEY_SPACE: return "Boşluk"; case KEY_LEFT_SHIFT: return "Sol Shift"; case KEY_RIGHT_SHIFT: return "Sağ Shift";
    case KEY_LEFT_CONTROL: return "Sol Ctrl"; case KEY_LEFT_ALT: return "Sol Alt"; case KEY_TAB: return "Tab";
    case KEY_ENTER: return "Enter"; case KEY_BACKSPACE: return "Geri Sil";
    }
    snprintf(b, sizeof b, "Tuş %d", k);
    return b;
}

/* ---------- global durum ---------- */
static int df[MAP_H][MAP_W];
static bool gIgnoreLocks, gNoDoors;
static int exitRoom, stairX, stairY, nkeys, nkeysF0;
static int resvX = -1, resvY = -1;
static Room stairRect;
static int keyLvl[MAX_KEYS], keyRoom[MAX_KEYS];
static Player P;
static Monster mons[NMON];
static Proj pj[4];
static GState gstate = ST_MENU, setReturn = ST_MENU;
static bool hasGame, showNotes, showMap = true, skipMouse, wantQuit;
static float gtime, runTime, msgT, hallT, hallCd, flickerT, lightMul = 1.0f, gProx, gMonD = 99, trans, scareT, heartT, pianoT = 12;
static int transTo, killer, pageGot[PAGES_NEEDED];
static char msg[160], deathMsg[96];
static Vector2 hallPos, jumpSrc, fakePos;
static float jumpT, fakeT, scareEvtT = 70;
static Camera3D cam3;
static float lg[MAP_H][MAP_W], lw[MAP_H][MAP_W], ls[MAP_H][MAP_W];   /* isik, sicaklik, sabit isik */
static float gShadeD, gWarm;
static bool gCamMode, showCctv;                     /* kamera gorunumu: gece gorusu */
static Vector2 camPlayerPos;
static float camPlayerYaw;
static int camPlayerLvl, camSel;
static Camera3D cctvCam;
static int ia_type, ia_x, ia_y;
static int dirPhase, ending, genIdx = -1;
static float tension, dirT = 40, ambT = 30, powerFlashT, cineT;
static bool dirHunch, fuseIn, genOn;
/* kendi videon: intro/ klasorundeki kare dizisi (frame_0001.jpg ...) + ses */
static bool introMode, introJpg, introAudio, introStream, introStreamEnded;
static int introCount;
static float introFps = 60.0f, introFrameTimer;
static Music introMusic;
static FILE *introPipe;
static unsigned char *introPixels;
#define INTRO_W 1280
#define INTRO_H 720
#define INTRO_FRAME_BYTES (INTRO_W * INTRO_H * 3)
#define POWER_ON (fuseIn && genOn)
/* gorsel: son islem (shader), modeller */
static RenderTexture2D postRT;
static Shader postSh;
static bool postOK;
static int locTime = -1, locStress = -1, locRes = -1;
static Model propModel[NPROPTYPES + 2], monModel[2];
static bool hasPM[NPROPTYPES + 2], hasMM[2];
enum { IA_NONE, IA_LOCKER, IA_DOOR, IA_STAIRS, IA_PROP, IA_ITEM };

static const char *noteText[8] = {
    "Elif'in günlüğünden: Dedem Aldemir'in saatini bulmak için malikaneye girdim. Kapı arkamdan kendiliğinden kapandı.",
    "Elif: Bodrumdaki jeneratör hâlâ çalışıyor ama yakıtı bitmiş. Elektrik gidince ev bambaşka bir yere dönüşüyor.",
    "Doktor Aldemir'in notu: Evi korusun diye mekanik bir nöbetçi yaptım. Ona bakan olursa durur, bakmayan olursa yaklaşır. Bunu bir oyun sanmıştım.",
    "Elif: Üst kattaki Bekçi gözlerini çuvalla örtmüş. Kör ama duyuyor. Eğilirsem beni fark etmiyor.",
    "Doktor Aldemir: Bekçi yıllardır sadık kâhyam. Kimse evden çıkmasın diye emir verdim, şimdi kendi emrimin tutsağıyım.",
    "Elif: Sayfalar dedemin deney defterinden. Altısı bir araya gelince çıkış kapısı açılıyor ama elektrik de gerekiyor.",
    "Elif: Heykel'e baktığım sürece bana dokunmuyor. Gözümü kırpmadan geri geri yürüdüm. Sanki tek istediği kimsenin onu görmemesi.",
    "Elif'in son notu: Beni arıyorsan vazgeç. Bütün notları bulursan anlarsın: ben çok önce çıktım. Sana bıraktığım mektup beni aramak için değil, bu evi bitirmen içindi."
};
static const char *pageLore[PAGES_NEEDED] = {
    "Bu evi dedem yaptırdı. Her gece koridorlarda ayak sesleri duyuyorum.",
    "Üst kattaki Bekçi kördür ama en küçük sesi bile duyar. Eğil ve sessiz yürü.",
    "Zemin katta dolaşan Heykel, sen ona bakarken kıpırdamaz. Bakmadığın an yaklaşır.",
    "Kilitli odaların anahtarları evin içine dağıldı. Biri bunu bilerek yaptı.",
    "Dolaplar güvenli değil. Aynı dolaba tekrar tekrar saklanma, onlar hatırlar.",
    "Çıkış kapısı ancak altı sayfa tamamlanınca açılır. Kapının ardında ne var, bilmiyorum."
};

/* ---------- yardimcilar ---------- */
static float vdist(Vector2 a, Vector2 b) { float dx = a.x - b.x, dy = a.y - b.y; return sqrtf(dx * dx + dy * dy); }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float angdiff(float a, float b) {
    float d = fmodf(a - b + PI, 2 * PI);
    if (d < 0) d += 2 * PI;
    return fabsf(d - PI);
}
static void set_msg(const char *s) { snprintf(msg, sizeof msg, "%s", s); msgT = 3.0f; }
static Vector2 tile_center(int tx, int ty) { return (Vector2){tx + 0.5f, ty + 0.5f}; }
static int tile_of(float v) { return (int)floorf(v); }
static unsigned hsh(int x, int y) {
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static float frnd(void) { return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f; }
static float diff_mul(void) { return cfg.diff == 0 ? 0.88f : (cfg.diff == 2 ? 1.14f : 1.0f); }

/* ---------- yazi (Turkce karakter destegi) ---------- */
static Font uiFont;
static bool trFont = false;

static char tr1(unsigned char lead, unsigned char c) {
    if (lead == 0xC3) {
        switch (c) { case 0x87: return 'C'; case 0xA7: return 'c'; case 0x96: return 'O'; case 0xB6: return 'o'; case 0x9C: return 'U'; case 0xBC: return 'u'; }
    } else if (lead == 0xC4) {
        switch (c) { case 0x9E: return 'G'; case 0x9F: return 'g'; case 0xB0: return 'I'; case 0xB1: return 'i'; }
    } else if (lead == 0xC5) {
        switch (c) { case 0x9E: return 'S'; case 0x9F: return 's'; }
    }
    return '?';
}
/* Turkce fontu yoksa ozel harfleri ASCII'ye cevirir */
static const char *U(const char *s) {
    if (trFont) return s;
    static char buf[8][320];
    static int bi;
    char *o = buf[bi = (bi + 1) & 7];
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && n < 316; p++) {
        if ((*p == 0xC3 || *p == 0xC4 || *p == 0xC5) && p[1]) { o[n++] = tr1(*p, p[1]); p++; }
        else o[n++] = (char)*p;
    }
    o[n] = 0;
    return o;
}
static void load_ui_font(void) {
    static const char *paths[] = {"font.ttf", "C:/Windows/Fonts/georgia.ttf", "C:/Windows/Fonts/times.ttf",
        "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSerif.ttf", "/System/Library/Fonts/Supplemental/Georgia.ttf", NULL};
    static int cps[200];
    static const int extra[] = {0xC7, 0xE7, 0x11E, 0x11F, 0x130, 0x131, 0xD6, 0xF6, 0x15E, 0x15F, 0xDC, 0xFC, 0x2019, 0x2014};
    int n = 0;
    for (int c = 32; c < 127; c++) cps[n++] = c;
    for (int i = 0; i < (int)(sizeof extra / sizeof extra[0]); i++) cps[n++] = extra[i];
    uiFont = GetFontDefault();
    for (int i = 0; paths[i]; i++) {
        if (!FileExists(paths[i])) continue;
        Font f = LoadFontEx(paths[i], 64, cps, n);
        if (f.texture.id > 0 && f.glyphCount > 0) {
            uiFont = f; SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR); trFont = true;
            return;
        }
    }
}
static float tsp(float size) { return trFont ? size * 0.05f : size / 10.0f; }
static void dtext(const char *s, float x, float y, float size, Color c) {
    DrawTextEx(uiFont, U(s), (Vector2){x, y}, size, tsp(size), c);
}
static float twidth(const char *s, float size) { return MeasureTextEx(uiFont, U(s), size, tsp(size)).x; }
static void dcenter(const char *s, float cx, float y, float size, Color c) { dtext(s, cx - twidth(s, size) / 2, y, size, c); }
static void dtext_sp(const char *s, float x, float y, float size, float sp, Color c) {
    DrawTextEx(uiFont, U(s), (Vector2){x, y}, size, sp, c);
}

/* ---------- ayar dosyasi ---------- */
static void save_cfg(void) {
    FILE *f = fopen(CFG_FILE, "w");
    if (!f) return;
    fprintf(f, "sens=%.3f\nbright=%.3f\nvol=%.3f\nfog=%d\nfx=%d\ndiff=%d\njump=%d\nfov=%.1f\npost=%d\ncine=%d\nbest=%.2f\n",
            cfg.sens, cfg.bright, cfg.vol, cfg.fog, cfg.fx, cfg.diff, cfg.jump, cfg.fov, cfg.post, cfg.cine, bestTime);
    for (int i = 0; i < A_COUNT; i++) fprintf(f, "key%d=%d\n", i, keymap[i]);
    fclose(f);
}
static void load_cfg(void) {
    FILE *f = fopen(CFG_FILE, "r");
    if (!f) { firstRun = true; return; }
    char line[96];
    while (fgets(line, sizeof line, f)) {
        char k[24];
        float v;
        if (sscanf(line, "%23[^=]=%f", k, &v) != 2) continue;
        if (!strcmp(k, "sens")) cfg.sens = clampf(v, 0.3f, 2.5f);
        else if (!strcmp(k, "bright")) cfg.bright = clampf(v, 0, 1);
        else if (!strcmp(k, "vol")) cfg.vol = clampf(v, 0, 1);
        else if (!strcmp(k, "fog")) cfg.fog = v != 0;
        else if (!strcmp(k, "fx")) cfg.fx = v != 0;
        else if (!strcmp(k, "diff")) cfg.diff = v < 0 ? 0 : (v > 2 ? 2 : (int)v);
        else if (!strcmp(k, "jump")) cfg.jump = v != 0;
        else if (!strcmp(k, "fov")) cfg.fov = clampf(v, 60, 100);
        else if (!strcmp(k, "post")) cfg.post = v != 0;
        else if (!strcmp(k, "cine")) cfg.cine = v != 0;
        else if (!strcmp(k, "best")) bestTime = v;
        else if (!strncmp(k, "key", 3)) { int i = atoi(k + 3); if (i >= 0 && i < A_COUNT && v > 0) keymap[i] = (int)v; }
    }
    fclose(f);
}

/* ---------- renklendirme (isik + sis + sicaklik) ---------- */
static unsigned char cb(float v) { return (unsigned char)clampf(v, 0, 255); }
static float amb(void) { return 0.010f + 0.052f * cfg.bright; }
static Color shade(Color c, float b, float emis) {
    float k = amb() + b + (gCamMode ? 0.3f : 0.0f);
    if (k < emis) k = emis;
    if (k > 1.25f) k = 1.25f;
    float r = c.r * k * 0.88f, g = c.g * k * 1.02f, bl = c.b * k * 0.78f;
    if (gWarm > 0) { r *= 1.0f + 0.35f * gWarm; bl *= 1.0f - 0.3f * gWarm; }
    else if (gWarm < -0.25f) {
        /* Malikane lambalari: yumusak, soluk, tekinsiz yesil renk yayilimi */
        float gh = clampf((-gWarm - 0.25f) / 0.75f, 0.0f, 1.0f);
        r *= 1.0f - 0.42f * gh;
        g *= 1.0f + 0.30f * gh;
        bl *= 1.0f - 0.38f * gh;
    }
    if (cfg.fog) {
        float f = clampf((gShadeD - 3.0f) / 11.0f, 0, 1) * 0.55f;
        r += (5 - r) * f; g += (13 - g) * f; bl += (7 - bl) * f;
    }
    if (gCamMode) {                                   /* yesil gece gorusu */
        float l = clampf((0.3f * r + 0.59f * g + 0.11f * bl) * 2.0f, 0, 255);
        r = l * 0.35f; g = l; bl = l * 0.45f;
    }
    return (Color){cb(r), cb(g), cb(bl), 255};
}


/* ---------- doku sistemi (textures/ klasoru; hepsi istege bagli) ---------- */
enum { T_WALL, T_WAINSCOT, T_FLOOR_WOOD, T_FLOOR_TILE, T_FLOOR_STONE, T_CEILING, T_DOOR, T_WOOD, T_FABRIC, T_RUG, T_WARDEN, T_STATUE, T_COUNT };
static const char *texFile[T_COUNT] = {"wall.png", "wainscot.png", "floor_wood.png", "floor_tile.png", "floor_stone.png", "ceiling.png",
                                       "door.png", "wood.png", "fabric.png", "rug.png", "warden.png", "statue.png"};
static const float texS[T_COUNT] = {1.2f, 1.1f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.6f, 1.0f, 1.0f, 1.0f};   /* bir tekrarin dunya boyutu */
static Texture2D tex[T_COUNT];
static bool hasTex[T_COUNT], useTex = true;
static bool texOn(int ti) { return useTex && hasTex[ti]; }
/* doku isik ve tema rengiyle carpilir: tema rengi hafif ton verir, parlaklik korunur */
static Color tintTex(Color c) {
    float m = fmaxf(1.0f, fmaxf((float)c.r, fmaxf((float)c.g, (float)c.b)));
    float f = 0.5f + 0.5f * fminf(1.0f, m / 150.0f);
    return (Color){cb((0.45f * 255 + 0.55f * c.r * 255 / m) * f), cb((0.45f * 255 + 0.55f * c.g * 255 / m) * f),
                   cb((0.45f * 255 + 0.55f * c.b * 255 / m) * f), 255};
}
/* mode 0: dunya koordinatina gore tekrar (duvarlar kesintisiz birlesir), mode 1: yuzeye tam otur (u0..u1, v0..v1) */
static void qv(Vector3 p, float w, float h, float l, float vx, float vy, float vz, int face, int mode, float S, float u0, float v0, float u1, float v1) {
    float nx = (vx - p.x + w / 2) / w, ny = (vy - p.y + h / 2) / h, nz = (vz - p.z + l / 2) / l, u, v;
    if (face < 2) { u = mode == 0 ? vx / S : lerpf(u0, u1, nx); v = mode == 0 ? vy / S : lerpf(v0, v1, ny); }
    else if (face < 4) { u = mode == 0 ? vx / S : lerpf(u0, u1, nx); v = mode == 0 ? vz / S : lerpf(v0, v1, nz); }
    else { u = mode == 0 ? vz / S : lerpf(u0, u1, nz); v = mode == 0 ? vy / S : lerpf(v0, v1, ny); }
    rlTexCoord2f(u, v);
    rlVertex3f(vx, vy, vz);
}
static void emit_cube(Texture2D t, Vector3 p, float w, float h, float l, Color c, int mode, float S, float u0, float v0, float u1, float v1) {
    float x = p.x, y = p.y, z = p.z, hw = w / 2, hh = h / 2, hl = l / 2;
    if (rlCheckRenderBatchLimit(24)) rlDrawRenderBatchActive();
    rlSetTexture(t.id);
    rlBegin(RL_QUADS);
    rlColor4ub(c.r, c.g, c.b, c.a);
#define Q(vx, vy, vz, f) qv(p, w, h, l, (vx), (vy), (vz), (f), mode, S, u0, v0, u1, v1)
    rlNormal3f(0, 0, 1);  Q(x - hw, y - hh, z + hl, 0); Q(x + hw, y - hh, z + hl, 0); Q(x + hw, y + hh, z + hl, 0); Q(x - hw, y + hh, z + hl, 0);
    rlNormal3f(0, 0, -1); Q(x - hw, y - hh, z - hl, 1); Q(x - hw, y + hh, z - hl, 1); Q(x + hw, y + hh, z - hl, 1); Q(x + hw, y - hh, z - hl, 1);
    rlNormal3f(0, 1, 0);  Q(x - hw, y + hh, z - hl, 2); Q(x - hw, y + hh, z + hl, 2); Q(x + hw, y + hh, z + hl, 2); Q(x + hw, y + hh, z - hl, 2);
    rlNormal3f(0, -1, 0); Q(x - hw, y - hh, z - hl, 3); Q(x + hw, y - hh, z - hl, 3); Q(x + hw, y - hh, z + hl, 3); Q(x - hw, y - hh, z + hl, 3);
    rlNormal3f(1, 0, 0);  Q(x + hw, y - hh, z - hl, 4); Q(x + hw, y + hh, z - hl, 4); Q(x + hw, y + hh, z + hl, 4); Q(x + hw, y - hh, z + hl, 4);
    rlNormal3f(-1, 0, 0); Q(x - hw, y - hh, z - hl, 5); Q(x - hw, y - hh, z + hl, 5); Q(x - hw, y + hh, z + hl, 5); Q(x - hw, y + hh, z - hl, 5);
#undef Q
    rlEnd();
    rlSetTexture(0);
}
/* dokulu kup; doku yoksa eski duz renkli kup. base: tema rengi, b: isik, emis: kendi isigi */
static void cubeT(int ti, Vector3 p, float w, float h, float l, Color base, float b, float emis) {
    if (texOn(ti)) emit_cube(tex[ti], p, w, h, l, shade(tintTex(base), b, emis), 0, texS[ti], 0, 0, 1, 1);
    else DrawCube(p, w, h, l, shade(base, b, emis));
}

/* ---------- harita sorgulari ---------- */
static bool oob(int tx, int ty) { return tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H; }
static bool is_stair(char c) { return c == 'U' || c == 'V' || c == 'D' || c == 'W'; }
static bool door_closed_at(int tx, int ty) { int di = dmap[ty][tx]; return di >= 0 && doors[di].open < DOOR_T; }
static bool solid_tile(int tx, int ty) {
    if (oob(tx, ty)) return true;
    char c = map[ty][tx];
    return c == '#' || c == 'L' || is_stair(c) || occ[ty][tx] || door_closed_at(tx, ty);
}
static bool opaque_tile(int tx, int ty) {
    if (oob(tx, ty)) return true;
    char c = map[ty][tx];
    return c == '#' || c == 'L' || c == 'U' || c == 'V' || occ[ty][tx] == 2 || door_closed_at(tx, ty);
}
/* canavar yolu: kapilar gecilir (kilitli olanlar haric) */
static bool solid_mon(int tx, int ty) {
    if (oob(tx, ty)) return true;
    char c = map[ty][tx];
    if (c == '#' || c == 'L' || is_stair(c) || occ[ty][tx]) return true;
    int di = dmap[ty][tx];
    if (di < 0) return false;
    if (doors[di].lock >= 0 && !gIgnoreLocks) return true;
    return gNoDoors && doors[di].open < DOOR_T;              /* kapi acamayan canavar (Heykel): kapali kapi duvardir */
}
static bool blocked(float x, float y, float r) {
    return solid_tile(tile_of(x - r), tile_of(y - r)) || solid_tile(tile_of(x + r), tile_of(y - r)) ||
           solid_tile(tile_of(x - r), tile_of(y + r)) || solid_tile(tile_of(x + r), tile_of(y + r));
}
static void move_circle(Vector2 *p, float dx, float dy, float r) {
    if (!blocked(p->x + dx, p->y, r)) p->x += dx;
    if (!blocked(p->x, p->y + dy, r)) p->y += dy;
}
/* a'dan b'ye gorus hatti acik mi? (b'nin kendi karosu engel sayilmaz) */
static bool los(Vector2 a, Vector2 b) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float d = sqrtf(dx * dx + dy * dy);
    int steps = (int)(d / 0.25f) + 1;
    int btx = tile_of(b.x), bty = tile_of(b.y);
    for (int i = 1; i < steps; i++) {
        float t = (float)i / steps;
        int tx = tile_of(a.x + dx * t), ty = tile_of(a.y + dy * t);
        if (opaque_tile(tx, ty) && !(tx == btx && ty == bty)) return false;
    }
    return true;
}
static Vector2 room_center(int r) { return tile_center(rooms[r].x + rooms[r].w / 2, rooms[r].y + rooms[r].h / 2); }
static Vector2 stair_front(void) { return tile_center(stairX, stairY + 2); }
static int cam_total(void) { return FL[0].ncm + FL[1].ncm; }
static const Cam *cam_get(int idx, int *lvl) {
    if (idx < FL[0].ncm) { *lvl = 0; return &FL[0].cm[idx]; }
    *lvl = 1; return &FL[1].cm[idx - FL[0].ncm];
}

/* dosyayi calisma klasorunde, yoksa exe'nin klasorunde ara */
static const char *find_asset(const char *name) {
    static char path[512];
    if (FileExists(name)) return name;
    snprintf(path, sizeof path, "%s%s", GetApplicationDirectory(), name);
    return FileExists(path) ? path : NULL;
}

/* ---------- ses (hepsi kodla uretilir) ---------- */
enum { S_STEP, S_HEART, S_DRONE, S_CREAK, S_SLAM, S_LOCKED, S_PAPER, S_KEY, S_BATT, S_GLASS, S_THROW, S_GROWL,
       S_RUMBLE, S_WHISPER, S_STING, S_CLICK, S_LOCKER, S_PIANO, S_STAIR, S_WIN, S_UI, S_SCREAM,
       S_PAD, S_PULSE, S_MUSICBOX, S_GENHUM, S_BREATH, S_GRIND, S_RAIN, S_THUNDER, S_ALARM, S_TYPE, S_COUNT };
static const char *sndName[S_COUNT] = {"step", "heart", "drone", "creak", "slam", "locked", "paper", "key", "batt", "glass", "throw", "growl",
    "rumble", "whisper", "sting", "click", "locker", "piano", "stair", "win", "ui", "scream",
    "pad", "pulse", "musicbox", "genhum", "breath", "grind", "rain", "thunder", "alarm", "type"};
static Sound snd[S_COUNT];
static bool audioOK;

static void make_sound(int kind, float dur, Sound *out) {
    const int sr = 22050;
    int n = (int)(dur * sr);
    short *d = (short *)malloc((size_t)n * sizeof(short));
    if (!d) return;
    float lp = 0, lp2 = 0, ph = 0, tink = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / sr, u = t / dur, v = 0, nz = frnd();
        float env = sinf(PI * u);
        lp += (nz - lp) * 0.10f;
        lp2 += (nz - lp2) * 0.45f;
        switch (kind) {
        case S_STEP: v = (lp * 5.0f + sinf(TAU * 62 * t) * 0.8f) * expf(-t * 26) * 0.9f; break;
        case S_HEART: {
            float b = t - 0.24f;
            v = sinf(TAU * 46 * t) * expf(-t * 16) * 0.95f;
            if (b > 0) v += sinf(TAU * 40 * b) * expf(-b * 18) * 0.7f;
        } break;
        case S_DRONE: {
            float am = 0.75f + 0.25f * sinf(TAU * t / 4.0f);
            v = (sinf(TAU * 55 * t) * 0.26f + sinf(TAU * 82.5f * t) * 0.14f + sinf(TAU * 110.25f * t + sinf(TAU * t / 8.0f) * 2.0f) * 0.07f) * am;
            v += lp * 0.9f * env * env * (0.5f + 0.5f * sinf(TAU * t / 4.0f));
        } break;
        case S_CREAK: {
            float f = 85 + 130 * u + 22 * sinf(TAU * 7 * t);
            ph += TAU * f / sr; if (ph > TAU) ph -= TAU;
            v = ((ph / PI - 1.0f) * 0.22f + lp2 * 0.18f) * env * (0.6f + 0.4f * sinf(TAU * 11 * t));
        } break;
        case S_SLAM: v = (sinf(TAU * 58 * t) * 0.9f + lp * 3.0f) * expf(-t * 13); break;
        case S_LOCKED: {
            static const float t0[3] = {0, 0.09f, 0.2f};
            for (int k = 0; k < 3; k++) {
                float x = t - t0[k];
                if (x >= 0) v += sinf(TAU * 900 * x) * expf(-x * 70) * 0.5f + lp2 * expf(-x * 90) * 0.5f;
            }
        } break;
        case S_PAPER: v = (nz - lp) * 0.7f * powf(env, 1.5f) * (0.5f + 0.5f * sinf(TAU * 28 * t)); break;
        case S_KEY: {
            static const float t0[3] = {0, 0.12f, 0.25f};
            for (int k = 0; k < 3; k++) {
                float x = t - t0[k];
                if (x >= 0) v += (sinf(TAU * 2093 * x) * expf(-x * 9) * 0.5f + sinf(TAU * 3136 * x) * expf(-x * 12) * 0.3f) * 0.6f;
            }
        } break;
        case S_BATT: {
            float b = t - 0.14f;
            v = sinf(TAU * 660 * t) * expf(-t * 9) * 0.5f;
            if (b > 0) v += sinf(TAU * 880 * b) * expf(-b * 9) * 0.5f;
        } break;
        case S_GLASS:
            if (t > 0.08f && rand() % 1400 == 0) tink = 1.0f;
            tink *= 0.985f;
            v = (nz - lp) * expf(-t * 9) * 0.8f + sinf(TAU * 3100 * t) * expf(-t * 30) * 0.3f + tink * nz * 0.5f;
            break;
        case S_THROW: v = lp * 3.5f * env * 0.7f; break;
        case S_GROWL: {
            float f = 62 + 8 * sinf(TAU * 7 * t);
            ph += TAU * f / sr; if (ph > TAU) ph -= TAU;
            v = ((ph / PI - 1.0f) * 0.35f + lp * 1.6f) * (0.6f + 0.4f * sinf(TAU * 13 * t)) * powf(env, 0.7f) * 1.5f;
            v = clampf(v, -0.8f, 0.8f);
        } break;
        case S_RUMBLE: v = (sinf(TAU * 38 * t) * 0.6f + sinf(TAU * 57 * t) * 0.3f) * (0.65f + 0.35f * sinf(TAU * t)); break;
        case S_WHISPER: v = (lp2 - lp) * 2.2f * env * (0.5f + 0.5f * sinf(TAU * 3.3f * t)) * (0.3f + 0.7f * fabsf(sinf(TAU * 1.7f * t))); break;
        case S_STING:
            v = (sinf(TAU * 220 * t) + sinf(TAU * 233 * t) + sinf(TAU * 311 * t)) * 0.25f * expf(-t * 2.2f) + (nz - lp) * expf(-t * 6) * 0.7f + sinf(TAU * 60 * t) * expf(-t * 5);
            break;
        case S_CLICK: v = sinf(TAU * 1400 * t) * expf(-t * 90) * 0.6f + lp2 * expf(-t * 120) * 0.4f; break;
        case S_LOCKER: {
            float f = 150 - 40 * u;
            ph += TAU * f / sr; if (ph > TAU) ph -= TAU;
            v = ((ph / PI - 1.0f) * 0.25f + lp2 * 0.12f) * env;
            if (t > dur - 0.12f) v += sinf(TAU * 70 * t) * 0.5f * expf(-(t - (dur - 0.12f)) * 25);
        } break;
        case S_PIANO:
            for (int k = 1; k <= 5; k++) v += sinf(TAU * 220 * k * t) / powf((float)k, 1.4f) * expf(-t * (1.1f + k * 0.5f));
            v += sinf(TAU * 220.7f * t) * 0.3f * expf(-t * 1.2f);
            v *= (1.0f - expf(-t * 200)) * 0.45f;
            break;
        case S_STAIR:
            for (int k = 0; k < 6; k++) {
                float x = t - k * 0.16f;
                if (x >= 0) v += (lp * 5.0f + sinf(TAU * 70 * x) * 0.8f) * expf(-x * 24) * 0.8f;
            }
            break;
        case S_WIN: v = (sinf(TAU * 261.6f * t) + sinf(TAU * 329.6f * t) + sinf(TAU * 392 * t)) * 0.3f * expf(-t * 0.8f); break;
        case S_UI: v = sinf(TAU * 600 * t) * expf(-t * 45) * 0.4f; break;
        case S_SCREAM: {                                   /* jumpscare cigligi: yukselen, bozuk ses */
            float f = 380 + 520 * u + 70 * sinf(TAU * 31 * t);
            ph += TAU * f / sr; if (ph > TAU) ph -= TAU;
            v = ((ph / PI - 1.0f) * 0.5f + nz * 0.45f + sinf(ph * 2.0f) * 0.2f) * (1.0f - expf(-t * 60)) * expf(-t * 1.6f) * 2.0f;
            v = clampf(v, -0.9f, 0.9f);
        } break;
        case S_PAD: {
            float am = 0.6f + 0.4f * sinf(TAU * t / 3.0f);
            v = (sinf(TAU * 220 * t) + sinf(TAU * 233.5f * t) * 0.9f + sinf(TAU * 311.5f * t) * 0.6f + sinf(TAU * 466.5f * t) * 0.3f) * 0.11f * am;
        } break;
        case S_PULSE: {
            float k = fmodf(t, 0.5f);
            v = sinf(TAU * 52 * k) * expf(-k * 14) * 0.8f + sinf(TAU * 880 * t + 8.0f * sinf(TAU * 0.5f * t)) * 0.025f * (0.5f + 0.5f * sinf(TAU * t / 4.0f));
        } break;
        case S_MUSICBOX: {
            static const float nf[13] = {659.25f, 587.33f, 523.25f, 587.33f, 659.25f, 659.25f, 659.25f, 587.33f, 587.33f, 587.33f, 659.25f, 783.99f, 783.99f};
            for (int k = 0; k < 13; k++) {
                float x = t - k * 0.36f;
                if (x >= 0) v += sinf(TAU * nf[k] * x) * expf(-x * 5.5f) * 0.45f + sinf(TAU * nf[k] * 2.76f * x) * expf(-x * 9.0f) * 0.12f;
            }
        } break;
        case S_GENHUM: v = (sinf(TAU * 50 * t) * 0.3f + sinf(TAU * 100 * t) * 0.2f + sinf(TAU * 150 * t) * 0.1f + lp * 0.8f) * (0.75f + 0.25f * sinf(TAU * 4 * t)); break;
        case S_BREATH: v = (nz * 0.5f + lp * 2.0f) * env * env * 0.35f; break;
        case S_GRIND: v = (lp * 3.0f * (0.6f + 0.4f * sinf(TAU * 18 * t)) + sinf(TAU * 45 * t) * 0.3f) * env * 0.7f; break;
        case S_RAIN: v = (nz - lp * 0.5f) * 0.2f * (0.85f + 0.15f * sinf(TAU * t / 4.0f)); break;
        case S_THUNDER: v = (lp * 6.0f + sinf(TAU * 38 * t) * 0.5f + sinf(TAU * 27 * t) * 0.4f) * expf(-t * 1.1f) * (1.0f - expf(-t * 30)) * 0.8f + nz * expf(-t * 20) * 0.6f; break;
        case S_ALARM: {
            float f = 600 + 300 * sinf(TAU * 1.5f * t);
            ph += TAU * f / sr; if (ph > TAU) ph -= TAU;
            v = clampf(sinf(ph) * 1.6f, -0.6f, 0.6f) * 0.7f * fminf(1.0f, fminf(t * 20, (dur - t) * 5));
        } break;
        case S_TYPE: {
            static const float t0[5] = {0, 0.07f, 0.15f, 0.22f, 0.30f};
            for (int k = 0; k < 5; k++) { float x = t - t0[k]; if (x >= 0) v += lp2 * expf(-x * 110) * 0.8f + sinf(TAU * 1800 * x) * expf(-x * 100) * 0.3f; }
            float y = t - 0.4f;
            if (y >= 0) v += sinf(TAU * 2400 * y) * expf(-y * 8) * 0.4f;
        } break;
        }
        d[i] = (short)(clampf(v, -1, 1) * 30000);
    }
    Wave w = {(unsigned int)n, (unsigned int)sr, 16, 1, d};
    *out = LoadSoundFromWave(w);
    free(d);
}
static bool echo_ok(int s) {
    return s == S_SLAM || s == S_GLASS || s == S_GROWL || s == S_SCREAM || s == S_STING || s == S_CREAK || s == S_LOCKED || s == S_THUNDER || s == S_MUSICBOX;
}
static Sound sndE[S_COUNT];                 /* yanki kopyalari */
static bool hasE[S_COUNT];
typedef struct { int s; float t, vol, pitch; bool on; } Echo;
static Echo echoQ[8];
static void init_audio(void) {
    InitAudioDevice();
    audioOK = IsAudioDeviceReady();
    if (!audioOK) return;
    SetMasterVolume(cfg.vol);
    static const float dur[S_COUNT] = {0.16f, 0.7f, 8.0f, 1.0f, 0.45f, 0.4f, 0.35f, 0.8f, 0.5f, 0.9f, 0.3f, 1.4f,
                                       3.0f, 1.6f, 1.6f, 0.06f, 0.6f, 3.2f, 1.0f, 3.0f, 0.08f, 1.0f,
                                       6.0f, 4.0f, 5.2f, 2.0f, 1.6f, 0.6f, 4.0f, 3.2f, 2.0f, 0.5f};
    static const char *ext[3] = {".wav", ".ogg", ".mp3"};
    for (int i = 0; i < S_COUNT; i++) {
        char nm[64];
        const char *p = NULL;
        for (int e = 0; e < 3 && !p; e++) { snprintf(nm, sizeof nm, "sounds/%s%s", sndName[i], ext[e]); p = find_asset(nm); }
        if (p) {                                               /* kendi ses dosyan varsa onu kullan */
            snd[i] = LoadSound(p);
            if (echo_ok(i)) { sndE[i] = LoadSound(p); hasE[i] = true; }
            continue;
        }
        make_sound(i, dur[i], &snd[i]);
        if (echo_ok(i)) { make_sound(i, dur[i], &sndE[i]); hasE[i] = true; }
    }
}
static float echo_delay(void) {                  /* oda buyudukce yanki gecikir */
    int tx = tile_of(P.pos.x), ty = tile_of(P.pos.y);
    if (oob(tx, ty)) return 0.2f;
    int r = FL[P.lvl].roomg[ty][tx];
    if (r < 0) return 0.30f;
    return FL[P.lvl].rm[r].w * FL[P.lvl].rm[r].h > 55 ? 0.24f : 0.13f;
}
static void add_echo(int s, float vol, float pitch) {
    if (!hasE[s]) return;
    for (int i = 0; i < 8; i++)
        if (!echoQ[i].on) { echoQ[i] = (Echo){s, echo_delay(), vol, pitch, true}; return; }
}
static void play(int s, float vol, float pitch) {
    if (!audioOK) return;
    SetSoundVolume(snd[s], clampf(vol, 0, 1));
    SetSoundPitch(snd[s], pitch);
    SetSoundPan(snd[s], 0.5f);
    PlaySound(snd[s]);
    if (echo_ok(s)) add_echo(s, vol * 0.35f, pitch * 0.92f);
}
static void play3d(int s, Vector2 src, int lvl, float vol, float range, float pitch) {
    if (!audioOK) return;
    float d = vdist(src, P.pos);
    float g = clampf(1.0f - d / range, 0, 1);
    if (lvl != P.lvl) g *= 0.25f;
    else if (d > 2.0f && !los(src, P.pos)) { g *= 0.6f; pitch *= 0.88f; }      /* duvar arkasi: boguk ses */
    g *= g;
    if (g < 0.005f) return;
    if (echo_ok(s) && g > 0.02f) add_echo(s, vol * g * 0.35f, pitch * 0.92f);
    float lat = d > 0.01f ? ((src.x - P.pos.x) * -sinf(P.yaw) + (src.y - P.pos.y) * cosf(P.yaw)) / d : 0;
    SetSoundVolume(snd[s], clampf(vol * g, 0, 1));
    SetSoundPitch(snd[s], pitch);
    SetSoundPan(snd[s], clampf(0.5f + PAN_SIGN * lat * 0.4f, 0, 1));
    PlaySound(snd[s]);
}
/* oyuncunun yaptigi ses: yakindaki canavarlar kontrole gelir */
static void make_noise(Vector2 p, int lvl, float radius) {
    for (int i = 0; i < NMON; i++) {
        Monster *m = &mons[i];
        if (!m->active || m->lvl != lvl || m->state == M_CHASE || m->toStairs) continue;
        float r = radius * (m->kind == K_WARDEN ? 1.35f : 1.0f);
        float d = vdist(m->pos, p);
        if (d > r) continue;
        if (d > r * 0.55f && !los(p, m->pos)) continue;       /* duvar arkasindan uzak sesler duyulmaz */
        m->state = M_SEARCH; m->lastSeen = p; m->timer = 0; m->checked = false; m->knows = false;
        m->pts = radius > 8.0f ? 2 : 1;
    }
}

/* ---------- BFS (canavar yolu + seviye kontrolu) ---------- */
static void bfs_field(int tx, int ty) {
    static int q[MAP_W * MAP_H][2];
    memset(df, -1, sizeof df);
    if (solid_mon(tx, ty)) {                 /* hedef dolap/duvar ise yanindaki acik karoyu kullan */
        static const int dx[4] = {0, 0, -1, 1}, dy[4] = {1, -1, 0, 0};
        bool found = false;
        for (int k = 0; k < 4; k++)
            if (!solid_mon(tx + dx[k], ty + dy[k])) { tx += dx[k]; ty += dy[k]; found = true; break; }
        if (!found) return;
    }
    int h = 0, t = 0;
    q[t][0] = tx; q[t][1] = ty; t++;
    df[ty][tx] = 0;
    static const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
    while (h < t) {
        int cx = q[h][0], cy = q[h][1]; h++;
        for (int k = 0; k < 4; k++) {
            int nx = cx + ddx[k], ny = cy + ddy[k];
            if (solid_mon(nx, ny) || df[ny][nx] >= 0) continue;
            df[ny][nx] = df[cy][cx] + 1;
            q[t][0] = nx; q[t][1] = ny; t++;
        }
    }
}

/* ---------- prosedurel harita uretimi ---------- */
static bool rect_overlap(Room a, Room b, int m) {
    return !(a.x + a.w + m <= b.x || b.x + b.w + m <= a.x || a.y + a.h + m <= b.y || b.y + b.h + m <= a.y);
}
static void add_room(Room r) {
    rooms[nrooms++] = r;
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++) { map[y][x] = '.'; roomOf[y][x] = (signed char)(nrooms - 1); }
}
/* odanin duvar halkasi (ic dikdortgenin 1 disi) dahil herhangi bir odaya ait mi? */
static bool in_ring(int x, int y) {
    for (int r = 0; r < nrooms; r++)
        if (x >= rooms[r].x - 1 && x <= rooms[r].x + rooms[r].w && y >= rooms[r].y - 1 && y <= rooms[r].y + rooms[r].h) return true;
    return false;
}
static bool corr_ok(int x, int y) { return x >= 1 && y >= 1 && x < MAP_W - 1 && y < MAP_H - 1 && !in_ring(x, y); }
static void carve_wide(int x, int y) {
    if (corr_ok(x, y)) map[y][x] = '.';
    if (corr_ok(x + 1, y)) map[y][x + 1] = '.';
    if (corr_ok(x, y + 1)) map[y + 1][x] = '.';
}
static bool l_path(int x1, int y1, int x2, int y2, bool hfirst, bool doCarve) {
    int x = x1, y = y1;
    if (!corr_ok(x, y)) return false;
    if (doCarve) carve_wide(x, y);
    for (int phase = 0; phase < 2; phase++) {
        bool horiz = (phase == 0) == hfirst;
        while (horiz ? x != x2 : y != y2) {
            if (horiz) x += (x2 > x) ? 1 : -1; else y += (y2 > y) ? 1 : -1;
            if (!corr_ok(x, y)) return false;
            if (doCarve) carve_wide(x, y);
        }
    }
    return true;
}
static bool bfs_carve(int x1, int y1, int x2, int y2) {
    static int par[MAP_H][MAP_W], q[MAP_W * MAP_H];
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) par[y][x] = -2;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    int h = 0, t = 0;
    q[t++] = y1 * MAP_W + x1; par[y1][x1] = -1;
    while (h < t) {
        int c = q[h++], cx = c % MAP_W, cy = c / MAP_W;
        if (cx == x2 && cy == y2) break;
        for (int k = 0; k < 4; k++) {
            int nx = cx + dx[k], ny = cy + dy[k];
            if (!corr_ok(nx, ny) || par[ny][nx] != -2) continue;
            par[ny][nx] = c; q[t++] = ny * MAP_W + nx;
        }
    }
    if (par[y2][x2] == -2) return false;
    for (int c = y2 * MAP_W + x2; c != -1; c = par[c / MAP_W][c % MAP_W]) carve_wide(c % MAP_W, c / MAP_W);
    return true;
}
static bool carve_route(int x1, int y1, int x2, int y2) {
    if (l_path(x1, y1, x2, y2, true, false)) return l_path(x1, y1, x2, y2, true, true);
    if (l_path(x1, y1, x2, y2, false, false)) return l_path(x1, y1, x2, y2, false, true);
    return bfs_carve(x1, y1, x2, y2);
}
typedef struct { int dx, dy, ox, oy, side; bool ok; } DoorSpot;   /* kapi karosu ve hemen disindaki koridor karosu */
static DoorSpot pick_door(int r, int toRoom) {
    const Room *R = &rooms[r];
    Vector2 a = room_center(r), b = room_center(toRoom);
    float ddx = b.x - a.x, ddy = b.y - a.y;
    int side;                                 /* 0 ust, 1 alt, 2 sol, 3 sag */
    if (fabsf(ddx) > fabsf(ddy)) side = ddx > 0 ? 3 : 2; else side = ddy > 0 ? 1 : 0;
    for (int t = 0; t < 30; t++) {
        int x, y, ox, oy;
        if (side < 2) {
            x = GetRandomValue(R->x + 1, R->x + R->w - 2); y = side == 0 ? R->y - 1 : R->y + R->h;
            ox = x; oy = side == 0 ? y - 1 : y + 1;
        } else {
            y = GetRandomValue(R->y + 1, R->y + R->h - 2); x = side == 2 ? R->x - 1 : R->x + R->w;
            oy = y; ox = side == 2 ? x - 1 : x + 1;
        }
        if (x == resvX && y == resvY) continue;       /* merdivenin arkasi */
        if (!corr_ok(ox, oy)) continue;
        return (DoorSpot){x, y, ox, oy, side, true};
    }
    return (DoorSpot){0, 0, 0, 0, 0, false};
}
static void add_door(DoorSpot s, int room) {
    if (dmap[s.dy][s.dx] >= 0 || ndoors >= MAX_DOORS) return;
    map[s.dy][s.dx] = '.';
    dmap[s.dy][s.dx] = (signed char)ndoors;
    doors[ndoors++] = (Door){s.dx, s.dy, s.side >= 2, 0.0f, false, -1, room, false};
}
static bool connect_rooms(int a, int b) {
    DoorSpot sa = pick_door(a, b), sb = pick_door(b, a);
    if (!sa.ok || !sb.ok) return false;
    if (!carve_route(sa.ox, sa.oy, sb.ox, sb.oy)) return false;
    add_door(sa, a); add_door(sb, b);
    return true;
}

/* ---------- mobilya yerlestirme ---------- */
static bool prop_tall(int t) {
    return t == P_BOOKSHELF || t == P_WARDROBE || t == P_FRIDGE || t == P_CLOCK || t == P_FIREPLACE;
}
static bool ring_info(int r, int x, int y, int *fx, int *fz) {
    const Room *R = &rooms[r];
    if (x < R->x || y < R->y || x >= R->x + R->w || y >= R->y + R->h) return false;
    bool top = (y == R->y), bot = (y == R->y + R->h - 1), left = (x == R->x), right = (x == R->x + R->w - 1);
    if (!(top || bot || left || right)) return false;
    if (top) { *fx = 0; *fz = 1; } else if (bot) { *fx = 0; *fz = -1; }
    else if (left) { *fx = 1; *fz = 0; } else { *fx = -1; *fz = 0; }
    return true;
}
static bool near_center(int r, int x, int y) {
    Vector2 c = room_center(r);
    return abs(x - tile_of(c.x)) <= 1 && abs(y - tile_of(c.y)) <= 1;
}
static bool prop_cell_ok(int r, int x, int y, int fx, int fz) {
    if (map[y][x] != '.' || occ[y][x] || near_center(r, x, y)) return false;
    if (map[y - fz][x - fx] != '#') return false;       /* arkasi duvar olmali (kapi/koridor agzi degil) */
    if (map[y - 1][x] == 'L') return false;             /* dolabin onunu kapatma */
    return true;
}
static void add_prop(int type, int tx, int ty, int fx, int fz, int len) {
    if (nprops >= MAX_PROPS) return;
    int rx = -fz, rz = fx;
    Prop *p = &props[nprops++];
    *p = (Prop){type, tx, ty, fx, fz, len, tx + 0.5f + rx * 0.5f * (len - 1), ty + 0.5f + rz * 0.5f * (len - 1), false, -1, false, 0.0f, false};
    if (type == P_LAMP) p->lit = GetRandomValue(0, 99) < 65;
    if (type == P_DESK || type == P_NIGHTSTAND || type == P_WARDROBE || type == P_CRATE) {
        int r = GetRandomValue(0, 99);                 /* acilinca icinden cikacak sey */
        p->loot = r < 30 ? 0 : (r < 52 ? 1 : (r < 70 ? 2 : (r < 90 ? 3 : 4)));
    }
    for (int i = 0; i < len; i++) occ[ty + rz * i][tx + rx * i] = prop_tall(type) ? 2 : 1;
}
static bool try_place(int r, int type, int len) {
    const Room *R = &rooms[r];
    for (int t = 0; t < 40; t++) {
        int x = GetRandomValue(R->x, R->x + R->w - 1), y = GetRandomValue(R->y, R->y + R->h - 1);
        int fx, fz;
        if (!ring_info(r, x, y, &fx, &fz)) continue;
        int rx = -fz, rz = fx;
        bool ok = true;
        for (int i = 0; i < len && ok; i++) {
            int cx = x + rx * i, cy = y + rz * i, f2x, f2z;
            if (!ring_info(r, cx, cy, &f2x, &f2z) || f2x != fx || f2z != fz || !prop_cell_ok(r, cx, cy, fx, fz)) ok = false;
        }
        if (!ok) continue;
        add_prop(type, x, y, fx, fz, len);
        if (type == P_DESK) {                           /* masanin onune sandalye */
            int sx = x + fx, sy = y + fz;
            if (map[sy][sx] == '.' && !occ[sy][sx] && map[sy - 1][sx] != 'L' && !near_center(r, sx, sy))
                add_prop(P_CHAIR, sx, sy, -fx, -fz, 1);
        }
        return true;
    }
    return false;
}
static bool place_special(int type, int firstRoom) {       /* jenerator, sigorta kutusu */
    for (int t = 0; t < 100; t++) {
        int r = GetRandomValue(firstRoom, nrooms - 1);
        if (roomTheme[r] == TH_CORRIDOR) continue;
        if (try_place(r, type, 1)) return true;
    }
    return false;
}
static void place_props(void) {
    for (int r = 0; r < nrooms; r++)
        for (const Want *w = wants[roomTheme[r]]; w && w->type >= 0; w++)
            for (int k = 0; k < w->count; k++) try_place(r, w->type, w->len);
}

/* ---------- isik kaynaklari ---------- */
static void add_light(float x, float z, float y, float rad, float inten, float warm, int kind, int room) {
    if (nlights >= MAX_LIGHTS) return;
    lights[nlights++] = (Light){x, z, y, rad, inten, warm, kind, room};
}
/* duvarin odaya bakan yonu (ox,oz) ve o odanin indeksi; -2: gorunmeyen blok */
static int wall_face(int tx, int ty, int *ox, int *oz) {
    static const int dx[4] = {1, -1, 0, 0}, dz[4] = {0, 0, 1, -1};
    int fallback = -2, fdx = 0, fdz = 0;
    for (int k = 0; k < 4; k++) {
        int nx = tx + dx[k], ny = ty + dz[k];
        if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H || map[ny][nx] == '#') continue;
        if (roomOf[ny][nx] >= 0) { *ox = dx[k]; *oz = dz[k]; return roomOf[ny][nx]; }   /* oda yuzu oncelikli */
        if (fallback == -2) { fallback = -1; fdx = dx[k]; fdz = dz[k]; }
    }
    *ox = fdx; *oz = fdz;
    return fallback;
}
static void build_lights(int fi) {
    for (int r = 0; r < nrooms; r++) {
        bool hall = roomTheme[r] == TH_CORRIDOR;
        cf->lampg[r] = (fi == 0 && r == 0) || GetRandomValue(0, 99) < (hall ? 70 : 45);
        if (cf->lampg[r]) { Vector2 c = room_center(r); add_light(c.x, c.y, 2.05f, 8.2f, 0.48f, -0.98f, LK_LAMP, r); }
    }
    for (int i = 0; i < nprops; i++) {
        const Prop *p = &props[i];
        if (p->type == P_LAMP && p->lit) add_light(p->cx, p->cz, 1.3f, 6.0f, 0.40f, -0.98f, LK_LAMP, roomOf[p->ty][p->tx]);
        if (p->type == P_FIREPLACE) add_light(p->cx + p->fx * 0.7f, p->cz + p->fz * 0.7f, 0.5f, 4.8f, 0.3f, 0.9f, LK_FIRE, -1);
    }
    for (int ty = 1; ty < MAP_H - 1; ty++)
        for (int tx = 1; tx < MAP_W - 1; tx++) {
            if (map[ty][tx] != '#' || nlights >= MAX_LIGHTS - 1) continue;
            int dx = 0, dz = 0, rm = wall_face(tx, ty, &dx, &dz);
            if (rm >= 0 && hsh(tx, ty) % 9 == 0) add_light(tx + 0.5f + dx * 0.9f, ty + 0.5f + dz * 0.9f, 1.5f, 4.0f, 0.12f, -0.98f, LK_MOON, rm);
        }
}

/* ---------- guvenlik kameralari ---------- */
static void place_cameras(int stairRoom, int want) {
    bool used[MAX_ROOMS] = {false};
    int order[MAX_ROOMS], n = 0;
    order[n++] = stairRoom; used[stairRoom] = true;
    for (int t = 0; t < 80 && n < nrooms; t++) {
        int r = GetRandomValue(0, nrooms - 1);
        if (!used[r]) { used[r] = true; order[n++] = r; }
    }
    for (int i = 0; i < n && cf->ncm < want && cf->ncm < MAX_CAMS; i++) {
        Room R = rooms[order[i]];
        Vector2 c = room_center(order[i]);
        int cx = GetRandomValue(0, 1) ? R.x : R.x + R.w - 1, cy = GetRandomValue(0, 1) ? R.y : R.y + R.h - 1;
        float px = cx + 0.5f + (cx == R.x ? -0.32f : 0.32f), pz = cy + 0.5f + (cy == R.y ? -0.32f : 0.32f);
        cf->cm[cf->ncm++] = (Cam){px, 2.15f, pz, c.x, c.y, order[i]};
    }
}

/* ---------- esya yerlestirme ---------- */
static bool free_cell(int r, int *ox, int *oy) {
    for (int t = 0; t < 80; t++) {
        int x = GetRandomValue(rooms[r].x + 1, rooms[r].x + rooms[r].w - 2);
        int y = GetRandomValue(rooms[r].y + 1, rooms[r].y + rooms[r].h - 2);
        if (map[y][x] != '.' || occ[y][x]) continue;
        if (x == stairX && y == stairY + 2) continue;
        bool near = false;
        for (int i = 0; i < nitems; i++)
            if (abs(tile_of(items[i].pos.x) - x) < 2 && abs(tile_of(items[i].pos.y) - y) < 2) near = true;
        if (near) continue;
        *ox = x; *oy = y;
        return true;
    }
    return false;
}
static bool add_item(int type, int room, int id) {
    int x, y;
    if (nitems < MAX_ITEMS && free_cell(room, &x, &y)) {
        items[nitems++] = (Item){tile_center(x, y), type, id, false, -1, (float)GetRandomValue(0, 628) / 100.0f};
        return true;
    }
    return false;
}
static bool add_item_rand(int type, int id, bool allowLocked, int firstRoom) {
    for (int t = 0; t < 80; t++) {
        int r = GetRandomValue(firstRoom, nrooms - 1);
        if (!allowLocked && cf->lockedg[r]) continue;
        if (add_item(type, r, id)) return true;
    }
    return false;
}

/* ---------- kat uretimi ---------- */
static void clear_floor(Floor *f) {
    memset(f->grid, '#', sizeof f->grid);
    memset(f->occg, 0, sizeof f->occg);
    memset(f->roomg, -1, sizeof f->roomg);
    memset(f->dmapg, -1, sizeof f->dmapg);
    memset(f->seeng, 0, sizeof f->seeng);
    memset(f->lampg, 0, sizeof f->lampg);
    memset(f->lockedg, 0, sizeof f->lockedg);
    memset(f->lkc, 0, sizeof f->lkc);
    memset(f->picChg, 0, sizeof f->picChg);
    f->nrm = f->npr = f->nit = f->ndr = f->nlt = f->ncm = 0;
}
static bool gen_floor(int fi) {
    cf = &FL[fi];
    clear_floor(cf);
    nkeys = fi == 0 ? 0 : nkeysF0;
    nrooms = 0;
    if (fi == 1) add_room(stairRect);
    for (int tries = 0; tries < 900 && nrooms < 9; tries++) {
        Room r = {0, 0, GetRandomValue(6, 10), GetRandomValue(5, 7)};
        r.x = GetRandomValue(3, MAP_W - r.w - 4);
        r.y = GetRandomValue(3, MAP_H - r.h - 4);
        bool ok = true;
        for (int i = 0; i < nrooms; i++) if (rect_overlap(r, rooms[i], 4)) { ok = false; break; }
        if (ok) add_room(r);
    }
    if (nrooms < 6) return false;

    int stairRoom = 0;
    if (fi == 0) {
        float best = -1;                                 /* cikis: baslangica en uzak oda */
        exitRoom = nrooms - 1;
        for (int i = 1; i < nrooms; i++) {
            float d = vdist(room_center(0), room_center(i));
            if (d > best) { best = d; exitRoom = i; }
        }
        do { stairRoom = GetRandomValue(1, nrooms - 1); } while (stairRoom == exitRoom);
        Room R = rooms[stairRoom];
        stairRect = R; stairX = GetRandomValue(R.x + 1, R.x + R.w - 2); stairY = R.y;
    }
    resvX = stairX; resvY = stairY - 1;

    int off = GetRandomValue(0, 4);
    for (int r = 0; r < nrooms; r++)
        roomTheme[r] = (r == stairRoom) ? TH_CORRIDOR : ((fi == 0 && r == 0) ? TH_LIVING : 1 + (r + off) % 5);

    /* baglantilar: en yakin onceki odaya (agac) + birkac donguler */
    for (int i = 1; i < nrooms; i++) {
        int bj = 0; float bd = 1e9f;
        for (int j = 0; j < i; j++) {
            float d = vdist(room_center(i), room_center(j)) + (float)GetRandomValue(0, 60) / 10.0f;
            if (d < bd) { bd = d; bj = j; }
        }
        if (!connect_rooms(i, bj)) return false;
    }
    for (int k = 0; k < 2; k++) {
        int a = GetRandomValue(0, nrooms - 1), b = GetRandomValue(0, nrooms - 1);
        if (a != b) connect_rooms(a, b);
    }

    if (fi == 0) { Vector2 ec = room_center(exitRoom); map[tile_of(ec.y)][tile_of(ec.x)] = 'E'; }
    map[stairY][stairX] = fi == 0 ? 'U' : 'D';
    map[stairY + 1][stairX] = fi == 0 ? 'V' : 'W';

    /* kilitli odalar: sadece tek kapili (cikmaz) odalar */
    int dcount[MAX_ROOMS] = {0};
    for (int i = 0; i < ndoors; i++) dcount[doors[i].room]++;
    int locks = 0;
    for (int t = 0; t < 40 && locks < 2 && nkeys < MAX_KEYS; t++) {
        int r = GetRandomValue(1, nrooms - 1);
        if (dcount[r] != 1 || r == stairRoom || (fi == 0 && r == exitRoom) || cf->lockedg[r]) continue;
        for (int i = 0; i < ndoors; i++)
            if (doors[i].room == r) { doors[i].lock = nkeys; break; }
        cf->lockedg[r] = true; keyLvl[nkeys] = fi; keyRoom[nkeys] = r; nkeys++; locks++;
    }
    if (fi == 0) nkeysF0 = nkeys;

    for (int r = 0; r < nrooms; r++) {       /* dolaplar: odanin ust duvari boyunca (kapi onunde degil) */
        if (GetRandomValue(0, 99) >= 70) continue;
        int x = GetRandomValue(rooms[r].x + 1, rooms[r].x + rooms[r].w - 2);
        int y = rooms[r].y;
        if (map[y][x] == '.' && map[y + 1][x] == '.' && map[y - 1][x] == '#') map[y][x] = 'L';
    }
    place_props();
    if (fi == 0) { if (!place_special(P_GENERATOR, 1)) return false; }
    else if (!place_special(P_FUSEBOX, 1)) return false;
    build_lights(fi);
    place_cameras(stairRoom, fi == 0 ? 5 : 4);
    return true;
}

/* mobilyalar yolu tikamis mi? baslangictan tum odalara, cikisa, merdivene ve dolap onlerine ulasilmali */
static bool floor_ok(int fi, bool checkItems) {
    cf = &FL[fi];
    gIgnoreLocks = true;
    Vector2 s = fi == 0 ? room_center(0) : stair_front();
    bfs_field(tile_of(s.x), tile_of(s.y));
    bool ok = df[tile_of(s.y)][tile_of(s.x)] >= 0 && df[stairY + 2][stairX] >= 0;
    for (int r = 0; r < nrooms && ok; r++) {
        Vector2 c = room_center(r);
        if (df[tile_of(c.y)][tile_of(c.x)] < 0) ok = false;
    }
    for (int y = 1; y < MAP_H - 2 && ok; y++)
        for (int x = 0; x < MAP_W; x++) {
            if (map[y][x] == 'E' && df[y][x] < 0) ok = false;
            if (map[y][x] == 'L' && df[y + 1][x] < 0) ok = false;
        }
    for (int i = 0; i < nprops && ok; i++) {                  /* ozel mobilyalarin onune gidilebilmeli */
        int t = props[i].type;
        if (t == P_GENERATOR || t == P_FUSEBOX)
            if (df[props[i].ty + props[i].fz][props[i].tx + props[i].fx] < 0) ok = false;
    }
    if (checkItems)
        for (int i = 0; i < nitems && ok; i++)
            if (items[i].prop < 0 && df[tile_of(items[i].pos.y)][tile_of(items[i].pos.x)] < 0) ok = false;
    gIgnoreLocks = false;
    return ok;
}
static bool place_items(void) {
    for (int f = 0; f < NFLOORS; f++) { cf = &FL[f]; nitems = 0; }
    for (int f = 0; f < NFLOORS; f++) {
        cf = &FL[f];
        int first = f == 0 ? 1 : 0, pages = 3;
        for (int r = 0; r < nrooms && pages > 0; r++)                  /* kilitli odalara oncelik */
            if (cf->lockedg[r] && add_item(IT_PAGE, r, 0)) pages--;
        while (pages > 0) { if (!add_item_rand(IT_PAGE, 0, false, first)) return false; pages--; }
        for (int k = 0; k < 2; k++) add_item_rand(IT_BATTERY, 0, true, 0);
        for (int k = 0; k < 2; k++) add_item_rand(IT_BOTTLE, 0, true, 0);
        add_item_rand(IT_MATCH, 0, true, 0);
        for (int i = 0; i < nprops; i++) {                              /* mobilyalarin icindeki esyalar */
            const Prop *p = &props[i];
            if (p->loot < 1 || p->loot > 3 || nitems >= MAX_ITEMS - 6) continue;
            items[nitems++] = (Item){(Vector2){p->cx, p->cz}, p->loot == 1 ? IT_BATTERY : (p->loot == 2 ? IT_BOTTLE : IT_MATCH), 0, false, i, 0.0f};
        }
        for (int k = 0; k < 4; k++) add_item_rand(IT_NOTE, f * 4 + k, true, 0);
    }
    cf = &FL[GetRandomValue(0, NFLOORS - 1)];                           /* sigorta ve yakit: kilitsiz odalarda */
    if (!add_item_rand(IT_FUSE, 0, false, 0)) return false;
    cf = &FL[GetRandomValue(0, NFLOORS - 1)];
    if (!add_item_rand(IT_FUEL, 0, false, 0)) return false;
    for (int k = 0; k < nkeys; k++) {                                   /* anahtarlar: kilitsiz odalarda */
        cf = &FL[GetRandomValue(0, NFLOORS - 1)];
        if (!add_item_rand(IT_KEY, k, false, 0)) return false;
        if (cf == &FL[keyLvl[k]] && cf->lockedg[keyRoom[k]]) {
            Item *it = &items[nitems - 1];                              /* kendi odasinda olamaz */
            if (roomOf[tile_of(it->pos.y)][tile_of(it->pos.x)] == keyRoom[k]) return false;
        }
    }
    return floor_ok(0, true) && floor_ok(1, true);
}
static void build_world(void) {
    for (;;) {
        int g;
        for (g = 0; g < 300; g++) if (gen_floor(0) && floor_ok(0, false)) break;
        if (g == 300) continue;
        nkeysF0 = nkeys;
        for (g = 0; g < 300; g++) if (gen_floor(1) && floor_ok(1, false)) break;
        if (g == 300) continue;
        if (place_items()) return;
    }
}

/* ---------- canavar yapay zekasi ---------- */
static void die(const char *why, int who) {
    gstate = ST_DEAD; killer = who;
    snprintf(deathMsg, sizeof deathMsg, "%s", why);
    EnableCursor();
    play(S_STING, 1.0f, 1.0f);
    jumpT = cfg.jump ? 1.5f : 0.0f;                        /* jumpscare sahnesi */
    if (who >= 0) {
        jumpSrc = mons[who].pos;
        mons[who].face = atan2f(P.pos.y - jumpSrc.y, P.pos.x - jumpSrc.x);
    } else jumpSrc = (Vector2){P.pos.x + cosf(P.yaw) * 2.2f, P.pos.y + sinf(P.yaw) * 2.2f};
    if (cfg.jump) { play(S_SCREAM, 1.0f, 1.0f); play(S_GROWL, 1.0f, 0.7f); }
}
static bool powerAnn, wasChasing;
static bool room_open(int r) {                              /* odanin kilitli kapisi var mi? */
    for (int i = 0; i < ndoors; i++) if (doors[i].room == r && doors[i].lock >= 0) return false;
    return true;
}
static void pick_patrol_target(Monster *m) {
    int r = GetRandomValue(0, nrooms - 1);
    for (int t = 0; t < 12 && !room_open(r); t++) r = GetRandomValue(0, nrooms - 1);
    m->target = room_center(r);
}

static void mon_step_to(Monster *m, Vector2 goal, float speed, float dt) {
    float d = vdist(m->pos, goal);
    if (d < 0.01f) return;
    float nx = m->pos.x + (goal.x - m->pos.x) / d * speed * dt, ny = m->pos.y + (goal.y - m->pos.y) / d * speed * dt;
    int tx = tile_of(nx), ty = tile_of(ny);
    m->face = atan2f(goal.y - m->pos.y, goal.x - m->pos.x);
    if (!oob(tx, ty) && (tx != tile_of(m->pos.x) || ty != tile_of(m->pos.y))) {
        int di = dmap[ty][tx];
        if (di >= 0 && doors[di].open < DOOR_T) {            /* kapi kapali: ac ve bekle */
            if (m->kind == K_STATUE) return;                 /* Heykel kapi acamaz */
            if (doors[di].lock < 0 && !doors[di].want) {
                doors[di].want = true;
                play3d(S_CREAK, tile_center(tx, ty), m->lvl, 0.9f, 16.0f, 1.0f);
            }
            return;
        }
    }
    m->pos.x = nx; m->pos.y = ny;
    m->stepT -= dt * (speed / 3.0f);
    if (m->stepT <= 0) {
        if (m->kind == K_STATUE) play3d(S_GRIND, m->pos, m->lvl, 0.9f, 16.0f, 1.0f);
        else play3d(S_STEP, m->pos, m->lvl, 0.9f, 14.0f, 0.7f);
        m->stepT = 0.5f;
    }
}
/* hedefe BFS ile yuru; ulasilamiyorsa false */
static bool mon_go(Monster *m, Vector2 target, float speed, float dt) {
    bfs_field(tile_of(target.x), tile_of(target.y));
    int mx = tile_of(m->pos.x), my = tile_of(m->pos.y);
    if (oob(mx, my) || df[my][mx] < 0) return false;
    Vector2 goal = target;
    if (df[my][mx] > 0) {
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        int bestd = df[my][mx];
        for (int k = 0; k < 4; k++) {
            int nx = mx + dx[k], ny = my + dy[k];
            if (solid_mon(nx, ny) || df[ny][nx] < 0) continue;
            if (df[ny][nx] < bestd) { bestd = df[ny][nx]; goal = tile_center(nx, ny); }
        }
    }
    mon_step_to(m, goal, speed, dt);
    return true;
}
/* arama noktasi: yakindaki dolaplar once, yoksa rastgele acik karo */
static Vector2 search_point(Vector2 c) {
    int lx[8], ly[8], n = 0, bx = -1, by = -1, bc = 0;
    for (int y = tile_of(c.y) - 9; y <= tile_of(c.y) + 9; y++)
        for (int x = tile_of(c.x) - 9; x <= tile_of(c.x) + 9; x++)
            if (!oob(x, y) && map[y][x] == 'L') {
                if (cf->lkc[y][x] > bc) { bc = cf->lkc[y][x]; bx = x; by = y; }   /* oyuncunun sik saklandığı dolap */
                if (n < 8 && abs(x - tile_of(c.x)) <= 6 && abs(y - tile_of(c.y)) <= 6) { lx[n] = x; ly[n] = y; n++; }
            }
    if (bx >= 0 && GetRandomValue(0, 99) < 85) return tile_center(bx, by + 1);
    if (n > 0 && GetRandomValue(0, 99) < 55) { int k = GetRandomValue(0, n - 1); return tile_center(lx[k], ly[k] + 1); }
    for (int t = 0; t < 30; t++) {
        int x = tile_of(c.x) + GetRandomValue(-5, 5), y = tile_of(c.y) + GetRandomValue(-5, 5);
        if (!solid_mon(x, y)) return tile_center(x, y);
    }
    return c;
}

static float tile_light(float x, float z);
/* Heykel: sen ona bakarken donar, bakmazken hizla yaklasir */
static void statue_think(Monster *m, float dt) {
    if (m->lvl != P.lvl || P.pages < 1) return;                       /* ilk sayfadan sonra uyanir */
    float d = vdist(m->pos, P.pos), dm = diff_mul();
    m->face = atan2f(P.pos.y - m->pos.y, P.pos.x - m->pos.x);
    float toM = atan2f(m->pos.y - P.pos.y, m->pos.x - P.pos.x);
    bool lit = d < 2.2f || tile_light(m->pos.x, m->pos.y) > 0.06f;
    bool watched = !showCctv && !P.hidden && angdiff(toM, P.yaw) < 0.85f && lit && d < 20.0f && los(P.pos, m->pos);
    m->seesPlayer = false;
    if (P.hidden) {
        if (d < 2.4f) {
            m->timer += dt;
            if (m->timer > 6.0f && !P.holding) { die("Heykel dolabın kapağını açtı.", (int)(m - mons)); return; }
        } else { m->timer = 0; gNoDoors = true; mon_go(m, P.pos, 2.4f * dm, dt); gNoDoors = false; }
        return;
    }
    m->timer = 0;
    if (!watched) { gNoDoors = true; mon_go(m, P.pos, (2.9f + 0.28f * P.pages) * dm, dt); gNoDoors = false; }
    if (d < 0.7f) die("Heykel seni yakaladı. Ona bakmayı bırakmamalıydın.", (int)(m - mons));
}
static void mon_think(Monster *m, float dt) {
    if (m->kind == K_STATUE) { statue_think(m, dt); return; }
    bool same = m->lvl == P.lvl, warden = true;
    float d = same ? vdist(m->pos, P.pos) : 999.0f, dm = diff_mul();
    float sight = warden ? 2.8f : ((P.torch && P.battery > 0) ? 13.0f : (P.matchT > 0 ? 8.0f : 5.3f));
    if (P.crouch && !warden) sight *= 0.75f;
    float chaseSpeed = (warden ? 3.3f : 3.4f + P.pages * 0.22f) * dm;
    float dirMul = dirPhase == 0 ? 0.85f : (dirPhase == 2 ? 1.1f : 1.0f);          /* yonetmen: sakin / gergin donem */
    float searchSpeed = (warden ? 2.4f : 2.9f + P.pages * 0.15f) * dm * dirMul;
    float patrolSpeed = (warden ? 1.7f : 2.1f + P.pages * 0.12f) * dm * dirMul;
    bool seen = same && !P.hidden && d < sight && los(m->pos, P.pos);
    m->seesPlayer = seen;
    if (seen) m->toStairs = false;

    if (m->toStairs) {                                       /* merdivenle kat degistir */
        Vector2 sf = stair_front();
        if (vdist(m->pos, sf) > 0.9f) mon_go(m, sf, 3.2f * dm, dt);
        else {
            m->timer += dt;
            if (m->timer > 1.2f) {
                m->lvl = 1 - m->lvl; m->pos = sf; m->toStairs = false; m->timer = 0;
                cf = &FL[m->lvl];
                if (m->lvl == P.lvl) {
                    m->state = M_SEARCH; m->lastSeen = P.pos; m->pts = 2; m->checked = false;
                    play3d(S_GROWL, m->pos, m->lvl, 0.9f, 20.0f, 0.9f);
                } else { m->state = M_PATROL; pick_patrol_target(m); }
                return;
            }
        }
        if (same && !P.hidden && d < 0.65f) die("Bekçi seni yakaladı.", (int)(m - mons));
        return;
    }

    if (seen) {
        if (m->state != M_CHASE) { play3d(S_GROWL, m->pos, m->lvl, 1.0f, 22.0f, warden ? 0.75f : 1.0f); scareT = 0.5f; }
        m->state = M_CHASE; m->lastSeen = P.pos; m->timer = 0; m->knows = false; m->checked = false;
    }

    switch (m->state) {
    case M_CHASE:
        if (seen) {
            if (d < 4.4f) mon_step_to(m, P.pos, chaseSpeed, dt);   /* yakinda: dogrudan saldir */
            else mon_go(m, P.pos, chaseSpeed, dt);
        } else {
            m->timer += dt;
            if (m->timer > 1.5f) { m->state = M_SEARCH; m->timer = 0; m->pts = 3; }
            else mon_go(m, m->lastSeen, chaseSpeed, dt);
        }
        break;
    case M_SEARCH:
        if (vdist(m->pos, m->lastSeen) > 0.8f) {
            if (!mon_go(m, m->lastSeen, searchSpeed, dt)) { m->timer += dt; if (m->timer > 2.0f) { m->state = M_PATROL; pick_patrol_target(m); } }
        } else {
            m->timer += dt;
            if (same && P.hidden && !m->checked && d < 2.2f && m->timer > 1.0f) {   /* dolabi kontrol ediyor */
                m->checked = true;
                int chance = cfg.diff == 0 ? 20 : (cfg.diff == 2 ? 50 : 35);
                chance += cf->lkc[P.lockTy][P.lockTx] * 12;                     /* ayni dolaba tekrar bakar */
                if (P.holding) chance = chance * 4 / 10;                        /* nefesini tutuyorsan daha az risk */
                if (chance > 85) chance = 85;
                if (m->knows || GetRandomValue(0, 99) < chance) { die("Dolabın kapağı açıldı...", (int)(m - mons)); return; }
            }
            if (m->timer > 1.8f) {
                if (m->pts > 0) { m->pts--; m->timer = 0; m->checked = false; m->lastSeen = search_point(m->lastSeen); }
                else { m->state = M_PATROL; pick_patrol_target(m); }
            }
        }
        break;
    case M_PATROL:
        if (vdist(m->pos, m->target) < 0.8f || !mon_go(m, m->target, patrolSpeed, dt)) pick_patrol_target(m);
        if (same && dirHunch) {                              /* yonetmen: gergin donem basladi, oyuncunun odasina yonel */
            int pr = roomOf[tile_of(P.pos.y)][tile_of(P.pos.x)];
            if (pr >= 0) m->target = room_center(pr);
        }
        if (!warden) {
            m->swapT -= dt;
            if (!same && m->swapT > 25.0f) m->swapT = 25.0f;
            if (m->swapT <= 0) { m->toStairs = true; m->timer = 0; m->swapT = 70.0f + GetRandomValue(0, 40); }
        }
        break;
    }
    if (same && !P.hidden && d < 0.65f) die("Bekçi seni yakaladı.", (int)(m - mons));
}
static void update_monsters(float dt) {
    Floor *save = cf;
    for (int i = 0; i < NMON; i++) {
        Monster *m = &mons[i];
        if (!m->active) continue;
        cf = &FL[m->lvl];
        Vector2 prev = m->pos;
        mon_think(m, dt);
        if (dt > 0.0001f) {                                  /* animasyon: gercek hiz ve katedilen yol */
            float moved = vdist(prev, m->pos);
            if (moved > 1.5f) moved = 0;                     /* merdivenle isinlanma sayilmaz */
            m->spd += (moved / dt - m->spd) * fminf(1.0f, dt * 8.0f);
            m->anim += moved;
        }
        if (gstate != ST_PLAY) break;
    }
    cf = save;
    dirHunch = false;
    gProx = 0; gMonD = 99;
    for (int i = 0; i < NMON; i++) {
        if (!mons[i].active || mons[i].lvl != P.lvl) continue;
        float d = vdist(mons[i].pos, P.pos);
        if (d < gMonD) gMonD = d;
        gProx = fmaxf(gProx, clampf(1.0f - d / 11.0f, 0, 1));
    }
}

/* ---------- oyun baslatma ---------- */
static void init_game(void) {
    build_world();
    cf = &FL[0];
    P = (Player){0};
    P.pos = room_center(0);
    P.torch = true; P.focus = 0.45f; P.battery = 100; P.stamina = 100; P.sanity = 100; P.eye = 1.55f; P.lvl = 0;
    memset(mons, 0, sizeof mons);
    memset(pj, 0, sizeof pj);
    for (int k = 0; k < NMON; k++) {
        Monster *m = &mons[k];
        m->kind = k; m->lvl = (k == K_WARDEN) ? 1 : 0; m->active = (k == K_WARDEN) || cfg.diff >= 1;   /* kolay: sadece Bekci */
        cf = &FL[m->lvl];
        int mr = -1;
        for (int t = 0; t < 200 && mr < 0; t++) {
            int r = GetRandomValue(k == K_WARDEN ? 0 : 1, nrooms - 1);
            if (k == K_STATUE && (r == exitRoom || vdist(room_center(r), room_center(0)) <= 18.0f)) continue;
            if (k == K_WARDEN && roomTheme[r] == TH_CORRIDOR) continue;
            if (!room_open(r)) continue;                          /* kilitli odada dogmasin */
            mr = r;
        }
        if (mr < 0) mr = nrooms - 1;
        m->pos = room_center(mr); m->state = M_PATROL; pick_patrol_target(m);
        m->hunchT = 45.0f; m->swapT = 70.0f + GetRandomValue(0, 40);
    }
    cf = &FL[0];
    genIdx = -1;
    for (int i = 0; i < FL[0].npr; i++) if (FL[0].pr[i].type == P_GENERATOR) genIdx = i;
    fuseIn = genOn = powerAnn = wasChasing = false;
    tension = 0; dirPhase = 0; dirT = 40.0f; ambT = 30.0f; dirHunch = false; powerFlashT = 0;
    gtime = 0; runTime = 0; hallT = 0; hallCd = 6; flickerT = 0; msgT = 0; trans = 0; scareT = 0; heartT = 0;
    showNotes = false; hasGame = true; jumpT = 0; fakeT = 0; scareEvtT = (cfg.diff == 2 ? 50.0f : 75.0f) + GetRandomValue(0, 40);
    cam3.fovy = 70.0f; cam3.projection = CAMERA_PERSPECTIVE; cam3.up = (Vector3){0, 1, 0};
    set_msg("Sayfaları topla: 0/6. Fener: F veya sol tık. Kapılar için E.");
}

/* ---------- oyuncu guncelleme ---------- */
static void update_doors(float dt) {
    for (int f = 0; f < NFLOORS; f++)
        for (int i = 0; i < FL[f].ndr; i++) {
            Door *d = &FL[f].dr[i];
            float tgt = d->want ? 1.0f : 0.0f;
            if (d->open < tgt) { d->open += (d->slow ? 0.7f : 2.2f) * dt; if (d->open >= tgt) { d->open = tgt; d->slow = false; } }
            else if (d->open > tgt) { d->open -= (d->slow ? 0.8f : 2.6f) * dt; if (d->open <= tgt) { d->open = tgt; d->slow = false; } }
        }
}
static void change_floor(int to) {
    P.lvl = to; cf = &FL[to];
    P.pos = stair_front(); P.yaw = PI / 2; P.pitch = 0;
    memset(pj, 0, sizeof pj);
}
static void update_proj(float dt) {
    for (int i = 0; i < 4; i++) {
        Proj *p = &pj[i];
        if (!p->on) continue;
        p->v.y -= 9.8f * dt;
        Vector3 np = {p->p.x + p->v.x * dt, p->p.y + p->v.y * dt, p->p.z + p->v.z * dt};
        bool hit = false;
        if (np.y <= 0.05f) { hit = true; np.y = 0.05f; }
        else if (solid_tile(tile_of(np.x), tile_of(np.z))) { hit = true; np = p->p; }
        if (hit) {
            Vector2 hp = {np.x, np.z};
            play3d(S_GLASS, hp, P.lvl, 1.0f, 26.0f, 1.0f);
            make_noise(hp, P.lvl, 16.0f);
            p->on = false;
        } else p->p = np;
    }
}
static void key_label(int k, char *out, size_t n) {
    snprintf(out, n, "%s anahtarı", thName[FL[keyLvl[k]].th[keyRoom[k]]]);
}
/* esyanin dunya konumu; mobilyanin icindekiler cekmece/raf ile birlikte hareket eder */
static Vector3 item_pos(const Item *it) {
    if (it->prop < 0) return (Vector3){it->pos.x, 0.0f, it->pos.y};
    const Prop *p = &props[it->prop];
    float so = 0, fo = 0, y = 0.4f, o = p->open, W = (float)p->len;
    if (hasPM[p->type] && useTex) { y = 0.95f; }
    else switch (p->type) {
        case P_NIGHTSTAND: fo = -0.25f + 0.30f * o; y = 0.26f; break;
        case P_DESK: so = -W / 2 + 0.25f; fo = -0.15f + 0.32f * o; y = 0.52f; break;
        case P_WARDROBE: fo = -0.15f; y = 1.04f; break;
        case P_CRATE: y = 0.05f; break;
        default: break;
    }
    float rx = -(float)p->fz, rz = (float)p->fx;
    return (Vector3){p->cx + rx * so + p->fx * fo, y, p->cz + rz * so + p->fz * fo};
}
static const char *item_name(int t) {
    static const char *n[8] = {"Sayfa", "Pil", "Anahtar", "Şişe", "Sigorta", "Yakıt bidonu", "Kibrit", "Not"};
    return n[t >= 0 && t < 8 ? t : 0];
}
static void find_interact(void) {
    ia_type = IA_NONE;
    float best = 1e9f;
    int px = tile_of(P.pos.x), py = tile_of(P.pos.y);
    for (int y = py - 2; y <= py + 2; y++)
        for (int x = px - 2; x <= px + 2; x++) {
            if (oob(x, y)) continue;
            char c = map[y][x];
            int di = dmap[y][x], type = IA_NONE;
            if (c == 'L') type = IA_LOCKER; else if (is_stair(c)) type = IA_STAIRS; else if (di >= 0) type = IA_DOOR;
            if (type == IA_NONE) continue;
            Vector2 ctr = tile_center(x, y);
            float d = vdist(P.pos, ctr);
            if (d > 1.85f) continue;
            float dot = ((ctr.x - P.pos.x) * cosf(P.yaw) + (ctr.y - P.pos.y) * sinf(P.yaw)) / (d + 0.001f);
            if (d > 0.9f && dot < 0.3f) continue;
            float score = d - dot * 0.6f;
            if (score < best) { best = score; ia_type = type; ia_x = type == IA_DOOR ? di : x; ia_y = y; }
        }
    for (int i = 0; i < nprops; i++) {                      /* jenerator, sigorta kutusu, acilabilir mobilya */
        const Prop *p = &props[i];
        bool special = p->type == P_GENERATOR || p->type == P_FUSEBOX;
        if (!special && p->loot < 0) continue;
        Vector2 ctr = {p->cx, p->cz};
        float d = vdist(P.pos, ctr);
        if (d > 1.9f) continue;
        float dot = ((ctr.x - P.pos.x) * cosf(P.yaw) + (ctr.y - P.pos.y) * sinf(P.yaw)) / (d + 0.001f);
        if (d > 1.0f && dot < 0.35f) continue;
        float score = d - dot * 0.6f - 0.15f;
        if (score < best) { best = score; ia_type = IA_PROP; ia_x = i; ia_y = 0; }
    }
    for (int i = 0; i < nitems; i++) {                      /* alinabilir esyalar (E ile) */
        const Item *it = &items[i];
        if (it->taken || (it->prop >= 0 && props[it->prop].open < 0.55f)) continue;
        Vector3 w = item_pos(it);
        Vector2 ctr = {w.x, w.z};
        float d = vdist(P.pos, ctr);
        if (d > 1.7f) continue;
        float dot = ((ctr.x - P.pos.x) * cosf(P.yaw) + (ctr.y - P.pos.y) * sinf(P.yaw)) / (d + 0.001f);
        if (d > 0.8f && dot < 0.45f) continue;
        float score = d - dot * 0.8f - 0.35f;               /* esya, mobilyadan once secilir */
        if (score < best) { best = score; ia_type = IA_ITEM; ia_x = i; ia_y = 0; }
    }
}
static void use_door(int di) {
    Door *d = &doors[di];
    Vector2 c = tile_center(d->tx, d->ty);
    if (d->lock >= 0) {
        if (P.keys & (1 << d->lock)) {
            d->lock = -1; d->want = true;
            set_msg("Kapının kilidini açtın."); play(S_KEY, 0.6f, 1.0f); play3d(S_CREAK, c, P.lvl, 0.8f, 16, 1.0f);
            make_noise(c, P.lvl, 5.0f);
        } else {
            char b[64], k[48];
            key_label(d->lock, k, sizeof k);
            snprintf(b, sizeof b, "Kilitli. Gerekli: %s", k);
            set_msg(b); play3d(S_LOCKED, c, P.lvl, 0.9f, 16, 1.0f); make_noise(c, P.lvl, 6.0f);
        }
        return;
    }
    if (d->want) {
        if (vdist(P.pos, c) < 0.9f) { set_msg("Kapının önünde duruyorsun."); return; }
        d->want = false; d->slow = P.crouch;
        play3d(S_SLAM, c, P.lvl, P.crouch ? 0.15f : 0.9f, 20, 1.0f); make_noise(c, P.lvl, P.crouch ? 1.2f : 8.0f);
    } else {
        d->want = true; d->slow = P.crouch;                       /* egilerek: yavas ve sessiz */
        play3d(S_CREAK, c, P.lvl, P.crouch ? 0.2f : 0.8f, P.crouch ? 8.0f : 16.0f, P.crouch ? 0.7f : 1.0f);
        make_noise(c, P.lvl, P.crouch ? 1.2f : 4.0f);
    }
}

static float breathT;
static void take_item(int i) {                              /* E ile esya al */
    Item *it = &items[i];
    if (it->type == IT_BOTTLE && P.bottles >= 5) { set_msg("Daha fazla şişe taşıyamazsın."); return; }
    it->taken = true;
    if (it->type == IT_PAGE) {
        P.pages++;
        P.sanity = clampf(P.sanity + 10, 0, 100);
        char b[96];
        snprintf(b, sizeof b, "Sayfa toplandı: %d/%d (TAB: oku). Bir ses duyuldu!", P.pages, PAGES_NEEDED);
        set_msg(b); play(S_PAPER, 0.7f, 1.0f);
        make_noise(P.pos, P.lvl, 30.0f);
    } else if (it->type == IT_KEY) {
        P.keys |= 1 << it->id;
        char b[96], k[48];
        key_label(it->id, k, sizeof k);
        snprintf(b, sizeof b, "Aldın: %s", k);
        set_msg(b); play(S_KEY, 0.7f, 1.0f);
    } else if (it->type == IT_BOTTLE) {
        P.bottles++; set_msg("Şişe aldın. G veya sağ tık: fırlat, dikkat dağıt."); play(S_PAPER, 0.5f, 1.4f);
    } else if (it->type == IT_FUSE) {
        P.hasFuse = true; set_msg("Sigorta aldın! Sigorta kutusunu bul."); play(S_KEY, 0.7f, 1.3f);
    } else if (it->type == IT_FUEL) {
        P.hasFuel = true; set_msg("Yakıt bidonu aldın! Jeneratörü bul (zemin kat)."); play(S_PAPER, 0.6f, 0.7f);
    } else if (it->type == IT_MATCH) {
        P.matches++; set_msg("Kibrit aldın! (Q: yak)"); play(S_PAPER, 0.5f, 1.3f);
    } else if (it->type == IT_NOTE) {
        P.notes |= 1 << it->id; set_msg("Bir not aldın. TAB ile oku."); play(S_PAPER, 0.6f, 1.1f);
    } else {
        P.battery = clampf(P.battery + 45, 0, 100);
        set_msg("Pil aldın! Fener doldu."); play(S_BATT, 0.6f, 1.0f);
    }
}
static void use_prop(int i) {
    Prop *p = &props[i];
    Vector2 pc = {p->cx, p->cz};
    if (p->type == P_GENERATOR) {
        if (genOn) set_msg("Jeneratör zaten çalışıyor.");
        else if (!P.hasFuel) set_msg("Jeneratörün yakıtı bitmiş. Bir yakıt bidonu bulmalısın.");
        else {
            P.hasFuel = false; genOn = true; scareT = 0.4f;
            set_msg("Jeneratör çalıştı! Gürültüsü her yerden duyuluyor!");
            play(S_GENHUM, 0.6f, 1.0f); make_noise(pc, P.lvl, 40.0f);
        }
    } else if (p->type == P_FUSEBOX) {
        if (fuseIn) set_msg("Sigorta takılı.");
        else if (!P.hasFuse) set_msg("Sigorta kutusunda sigorta yok. Bir sigorta bulmalısın.");
        else { P.hasFuse = false; fuseIn = true; set_msg("Sigortayı taktın."); play(S_KEY, 0.7f, 0.8f); }
    } else if (p->loot >= 0) {                               /* dolap / cekmece / sandik: ac veya kapat */
        p->want = !p->want;
        play3d(p->want ? S_CREAK : S_SLAM, pc, P.lvl, p->want ? 0.35f : 0.3f, 10.0f, p->want ? 1.2f : 1.4f);
        make_noise(pc, P.lvl, 2.5f);
        if (p->want && p->loot == 4 && !p->done) {           /* icinden bir sey firlar */
            p->done = true;
            set_msg("İçinden bir şey fırladı!");
            fakePos = (Vector2){P.pos.x + cosf(P.yaw) * 1.4f, P.pos.y + sinf(P.yaw) * 1.4f};
            fakeT = 0.3f; scareT = 0.5f; P.sanity = clampf(P.sanity - 6, 0, 100);
            play(S_STING, 0.9f, 1.3f);
        } else if (p->want) {
            bool has = false;
            for (int k = 0; k < nitems; k++) if (!items[k].taken && items[k].prop == i) has = true;
            set_msg(has ? "İçinde bir şey var." : "İçi boş.");
        }
    }
}
/* yonetmen: gerilim yukselir, kovalamaca sonrasi nefes alma donemi gelir */
static void update_director(float dt) {
    bool chasing = false;
    for (int i = 0; i < NMON; i++)
        if (mons[i].active && mons[i].lvl == P.lvl && mons[i].kind != K_STATUE && mons[i].state == M_CHASE) chasing = true;
    if (wasChasing && !chasing) { dirPhase = 0; dirT = 28.0f + GetRandomValue(0, 14); tension = fminf(tension, 0.25f); }
    wasChasing = chasing;
    float rate = cfg.diff == 2 ? 1.3f : (cfg.diff == 0 ? 0.8f : 1.0f);
    dirT -= dt;
    switch (dirPhase) {
    case 0:                                                  /* sakin */
        tension = fmaxf(0, tension - dt * 0.05f);
        if (dirT <= 0) { dirPhase = 1; dirT = 80.0f / rate; }
        break;
    case 1:                                                  /* gerilim artiyor */
        tension = fminf(1, tension + dt * 0.016f * rate);
        if (tension > 0.8f || dirT <= 0) { dirPhase = 2; dirT = 30.0f; dirHunch = true; }
        break;
    default:                                                 /* tepe noktasi */
        tension = fminf(1, tension + dt * 0.05f);
        if (chasing) dirT = 30.0f;
        if (dirT <= 0) { dirPhase = 0; dirT = 35.0f; }
        break;
    }
}
static bool wall_pic(int tx, int ty) {
    int dx, dz, rm = wall_face(tx, ty, &dx, &dz);
    unsigned h = hsh(tx, ty);
    return rm >= 0 && (h % 9 == 1 || h % 9 == 2);
}
/* sahte sesler ve sen bakmazken degisen seyler */
static void ambient_events(float dt) {
    ambT -= dt;
    if (ambT > 0 || dirPhase == 0 || P.hidden) return;
    ambT = (dirPhase == 2 ? 14.0f : 26.0f) + GetRandomValue(0, 18);
    switch (GetRandomValue(0, 4)) {
    case 0: case 1: {                                        /* uzak kapi carpmasi / muzik kutusu */
        Vector2 src = room_center(GetRandomValue(0, nrooms - 1));
        if (vdist(src, P.pos) < 9.0f) break;
        if (GetRandomValue(0, 1)) play3d(S_SLAM, src, P.lvl, 0.9f, 32.0f, 0.9f);
        else play3d(S_MUSICBOX, src, P.lvl, 0.6f, 26.0f, 1.0f);
    } break;
    case 2: {                                                /* arkandan adim sesi */
        Vector2 src = {P.pos.x - cosf(P.yaw) * 2.2f, P.pos.y - sinf(P.yaw) * 2.2f};
        play3d(S_STAIR, src, P.lvl, 0.5f, 8.0f, 0.9f);
    } break;
    case 3:                                                  /* kapi kendi kendine acilir/kapanir */
        for (int t = 0; t < 12; t++) {
            Door *d = &doors[GetRandomValue(0, ndoors - 1)];
            Vector2 c = tile_center(d->tx, d->ty);
            float dist = vdist(c, P.pos);
            if (d->lock >= 0 || dist < 4.0f || dist > 11.0f) continue;
            if (angdiff(atan2f(c.y - P.pos.y, c.x - P.pos.x), P.yaw) < 1.3f && los(P.pos, c)) continue;
            d->want = !d->want; d->slow = false;
            play3d(d->want ? S_CREAK : S_SLAM, c, P.lvl, 0.8f, 20.0f, 1.0f);
            break;
        }
        break;
    default: {                                               /* sen bakmazken tablo degisir */
        int cx[256], cy[256], n = 0;
        for (int y = 1; y < MAP_H - 1; y++)
            for (int x = 1; x < MAP_W - 1; x++) {
                if (map[y][x] != '#' || cf->picChg[y][x] || !wall_pic(x, y)) continue;
                Vector2 c = tile_center(x, y);
                float dist = vdist(c, P.pos);
                if (dist < 4.0f || dist > 14.0f) continue;
                if (angdiff(atan2f(c.y - P.pos.y, c.x - P.pos.x), P.yaw) < 1.4f) continue;
                if (n < 256) { cx[n] = x; cy[n] = y; n++; }
            }
        if (n > 0) { int k = GetRandomValue(0, n - 1); cf->picChg[cy[k]][cx[k]] = (unsigned char)GetRandomValue(1, 2); }
    } break;
    }
}

static void update_play(float dt) {
    gtime += dt; runTime += dt;
    update_doors(dt);
    if (trans > 0.0f) {                                    /* merdiven gecisi */
        float prev = trans;
        trans += dt / 0.9f;
        if (prev < 0.5f && trans >= 0.5f) change_floor(transTo);
        if (trans >= 1.0f) trans = 0.0f;
        update_monsters(dt);
        return;
    }

    /* kamera: fare + numpad */
    Vector2 md2 = GetMouseDelta();
    if (skipMouse) { md2 = (Vector2){0, 0}; skipMouse = false; }
    bool lock = showCctv;                                   /* kamera izlerken oyuncu hareket etmez */
    if (lock) md2 = (Vector2){0, 0};
    P.yaw += md2.x * 0.0025f * cfg.sens;
    P.pitch = clampf(P.pitch - md2.y * 0.0025f * cfg.sens, -1.2f, 1.2f);
    float ky = (IsKeyDown(KEY_KP_6) ? 1.0f : 0.0f) - (IsKeyDown(KEY_KP_4) ? 1.0f : 0.0f);
    float kp = (IsKeyDown(KEY_KP_8) ? 1.0f : 0.0f) - (IsKeyDown(KEY_KP_2) ? 1.0f : 0.0f);
    if (lock) ky = kp = 0;
    P.yaw += ky * 2.2f * cfg.sens * dt;
    P.pitch = clampf(P.pitch + kp * 1.6f * cfg.sens * dt, -1.2f, 1.2f);
    if (!lock && IsKeyPressed(KEY_KP_5)) P.pitch = 0;
    if (!lock) P.focus = clampf(P.focus + GetMouseWheelMove() * 0.12f, 0, 1);       /* fener odagi */
    if (!lock) {
        P.yaw += gp_axis(GAMEPAD_AXIS_RIGHT_X) * 2.6f * cfg.sens * dt;
        P.pitch = clampf(P.pitch - gp_axis(GAMEPAD_AXIS_RIGHT_Y) * 1.8f * cfg.sens * dt, -1.2f, 1.2f);
    }
    if (P.hidden) { P.yaw = clampf(P.yaw, PI / 2 - 0.6f, PI / 2 + 0.6f); P.pitch = clampf(P.pitch, -0.4f, 0.4f); }   /* dolap araligindan bak */

    /* hareket */
    P.sprinting = false; P.moving = false;
    P.crouch = !P.hidden && !lock && (kdown(A_CROUCH) || IsKeyDown(KEY_LEFT_CONTROL));
    if (!P.hidden) {
        float fx = cosf(P.yaw), fz = sinf(P.yaw), rx = -sinf(P.yaw), rz = cosf(P.yaw);
        float mx = 0, mz = 0;
        if (kdown(A_FWD) || IsKeyDown(KEY_UP)) { mx += fx; mz += fz; }
        if (kdown(A_BACK) || IsKeyDown(KEY_DOWN)) { mx -= fx; mz -= fz; }
        if (kdown(A_RIGHT) || IsKeyDown(KEY_RIGHT)) { mx += rx; mz += rz; }
        if (kdown(A_LEFT) || IsKeyDown(KEY_LEFT)) { mx -= rx; mz -= rz; }
        float lxs = gp_axis(GAMEPAD_AXIS_LEFT_X), lys = gp_axis(GAMEPAD_AXIS_LEFT_Y);
        mx += fx * -lys + rx * lxs; mz += fz * -lys + rz * lxs;
        if (lock) { mx = 0; mz = 0; }
        float len = sqrtf(mx * mx + mz * mz);
        if (len > 0) { mx /= len; mz /= len; P.moving = true; }
        if (P.exhausted && P.stamina > 25) P.exhausted = false;
        bool want = (kdown(A_SPRINT) || IsKeyDown(KEY_RIGHT_SHIFT)) && len > 0 && !P.crouch;
        bool sprint = want && !P.exhausted && P.stamina > 0;
        if (sprint) {
            P.stamina -= 32 * dt;
            if (P.stamina <= 0) { P.stamina = 0; P.exhausted = true; }
        } else P.stamina = fminf(100, P.stamina + 16 * dt);
        P.sprinting = sprint;
        float sp = P.crouch ? 1.9f : (sprint ? 5.5f : 3.6f);
        move_circle(&P.pos, mx * sp * dt, mz * sp * dt, 0.28f);
        if (P.moving) {
            P.bob += dt * (sprint ? 12.0f : (P.crouch ? 5.0f : 8.0f));
            P.stepT -= dt;
            if (P.stepT <= 0) {
                play(S_STEP, P.crouch ? 0.12f : (sprint ? 0.55f : 0.32f), 0.85f + GetRandomValue(0, 30) / 100.0f);
                P.stepT = sprint ? 0.3f : (P.crouch ? 0.75f : 0.5f);
            }
            P.noiseT -= dt;
            if (P.noiseT <= 0 && !P.crouch) { make_noise(P.pos, P.lvl, sprint ? 9.0f : 3.5f); P.noiseT = sprint ? 0.4f : 0.6f; }
        }
    }

    /* fener */
    if (!P.hidden && !lock && (kpress(A_TORCH) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
        if (P.battery > 0) { P.torch = !P.torch; play(S_CLICK, 0.5f, 1.0f); } else set_msg("Fenerin pili bitti!");
    }
    if (P.torch) {
        P.battery -= (cfg.diff == 2 ? 0.9f : (cfg.diff == 0 ? 0.55f : 0.7f)) * dt;
        if (P.battery <= 0) { P.battery = 0; P.torch = false; set_msg("Fener söndü. Karanlık her yerde..."); }
    }

    /* kibrit */
    if (!lock && !P.hidden && kpress(A_MATCH) && P.matches > 0 && P.matchT <= 0) {
        P.matches--; P.matchT = 25.0f; set_msg("Kibrit yaktın."); play(S_CLICK, 0.6f, 1.4f);
    }
    if (P.matchT > 0) P.matchT -= dt;

    /* sise at */
    if (!P.hidden && !lock && P.bottles > 0 && (kpress(A_THROW) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        for (int i = 0; i < 4; i++) {
            if (pj[i].on) continue;
            Vector3 f = {cosf(P.pitch) * cosf(P.yaw), sinf(P.pitch), cosf(P.pitch) * sinf(P.yaw)};
            pj[i] = (Proj){{P.pos.x, 1.5f, P.pos.y}, {f.x * 9.0f, f.y * 9.0f + 1.5f, f.z * 9.0f}, true};
            P.bottles--; play(S_THROW, 0.6f, 1.0f);
            break;
        }
    }
    update_proj(dt);

    /* etkilesim: kapi / dolap / merdiven / mobilya */
    if (P.hidden) {
        ia_type = IA_NONE;
        P.holding = kdown(A_HOLD) && P.breath > 0;                  /* nefes tut: dolap kontrolunde risk azalir */
        if (P.holding) P.breath = fmaxf(0, P.breath - 17.0f * dt); else P.breath = fminf(100, P.breath + 22.0f * dt);
        static float gaspT;
        gaspT -= dt; breathT -= dt;
        if (kdown(A_HOLD) && P.breath <= 0 && gaspT <= 0) { make_noise(P.pos, P.lvl, 6.0f); play(S_BREATH, 0.9f, 1.25f); set_msg("Nefesin tükendi!"); gaspT = 3.0f; }
        if (!P.holding && breathT <= 0) { play(S_BREATH, 0.3f, 1.0f); breathT = 2.8f; }
        if (kpress(A_USE)) {
            P.hidden = false; P.holding = false;
            P.pos = tile_center(P.lockTx, P.lockTy + 1);
            set_msg("Dolaptan çıktın."); play(S_LOCKER, 0.5f, 1.0f);
        }
    } else {
        P.holding = false; P.breath = fminf(100, P.breath + 30.0f * dt);
        find_interact();
        if (lock) ia_type = IA_NONE;
        if (ia_type != IA_NONE && kpress(A_USE)) {
            if (ia_type == IA_DOOR) use_door(ia_x);
            else if (ia_type == IA_ITEM) take_item(ia_x);
            else if (ia_type == IA_PROP) use_prop(ia_x);
            else if (ia_type == IA_STAIRS) { trans = 0.001f; transTo = 1 - P.lvl; play(S_STAIR, 0.7f, 1.0f); make_noise(P.pos, P.lvl, 6.0f); }
            else {
                P.hidden = true; P.lockTx = ia_x; P.lockTy = ia_y; P.breath = 100.0f;
                if (cf->lkc[ia_y][ia_x] < 250) cf->lkc[ia_y][ia_x]++;      /* canavar hangi dolaba saklandığını hatırlar */
                P.pos = tile_center(ia_x, ia_y);
                P.yaw = PI / 2; P.pitch = 0;                       /* dolabin onune (guney) bak */
                for (int i = 0; i < NMON; i++) {
                    Monster *m = &mons[i];
                    if (m->active && m->lvl == P.lvl && m->seesPlayer && vdist(m->pos, P.pos) < 9.0f) { m->knows = true; m->lastSeen = P.pos; }
                }
                set_msg("Dolabın içindesin. Nefesini tut (Boşluk), etrafa bak. E: çık"); play(S_LOCKER, 0.5f, 0.9f);
            }
        }
    }

    /* mobilya acilma / kapanma animasyonu */
    for (int i = 0; i < nprops; i++) {
        Prop *q = &props[i];
        if (q->loot < 0) continue;
        float tg = q->want ? 1.0f : 0.0f;
        if (q->open < tg) q->open = fminf(tg, q->open + 2.2f * dt);
        else if (q->open > tg) q->open = fmaxf(tg, q->open - 2.6f * dt);
    }

    /* elektrik geri geldi mi */
    if (POWER_ON && !powerAnn) {
        powerAnn = true; powerFlashT = 3.0f;
        set_msg("Elektrik geri geldi! Çıkış kapısı artık açılabilir."); play(S_ALARM, 0.7f, 1.0f);
    }
    if (powerFlashT > 0) powerFlashT -= dt;

    /* cikis kapisi */
    int ptx = tile_of(P.pos.x), pty = tile_of(P.pos.y);
    if (!oob(ptx, pty) && map[pty][ptx] == 'E') {
        if (P.pages >= PAGES_NEEDED && POWER_ON) {
            gstate = ST_WIN; EnableCursor(); play(S_WIN, 0.8f, 1.0f);
            ending = (P.notes == 0xFF) ? 1 : 0;                  /* tum notlar: gizli son */
            if (bestTime <= 0 || runTime < bestTime) { bestTime = runTime; save_cfg(); }
            return;
        }
        if (msgT < 1.0f) {
            char b[96];
            if (P.pages < PAGES_NEEDED) snprintf(b, sizeof b, "Kapı kilitli. %d sayfa eksik.", PAGES_NEEDED - P.pages);
            else snprintf(b, sizeof b, "Kapı elektrikli ve kapalı. Sigortayı tak, jeneratörü çalıştır.");
            set_msg(b);
        }
    }

    update_monsters(dt);
    if (gstate != ST_PLAY) return;
    update_director(dt);
    ambient_events(dt);

    /* akil sagligi */
    static bool lampCache;                                  /* kamera izlerken isik dizisi kameraya ait olur */
    if (!showCctv) lampCache = !oob(ptx, pty) && ls[pty][ptx] > 0.12f;
    bool lampLit = lampCache;
    bool lit = (P.torch && P.battery > 0) || lampLit || P.matchT > 0;
    float ds = lit ? 0.35f : -1.3f;
    if (lampLit) ds += 0.4f;
    if (P.hidden) ds = -0.4f;
    ds *= (ds < 0 ? diff_mul() : 1.0f);
    for (int i = 0; i < NMON; i++) {
        Monster *m = &mons[i];
        if (!m->active || m->lvl != P.lvl) continue;
        float md = vdist(m->pos, P.pos);
        if (md < 10.0f && (P.hidden ? true : los(P.pos, m->pos))) ds -= 5.0f * (1.0f - md / 10.0f) * diff_mul();
    }
    P.sanity = clampf(P.sanity + ds * dt, 0, 100);
    if (P.sanity <= 0) { die("Aklını kaybettin. Fısıltılar artık senin sesin.", -1); return; }

    /* halusinasyon & fener titremesi */
    hallCd -= dt;
    if (hallT > 0) hallT -= dt;
    if (P.sanity < 45 && hallCd <= 0) {
        float a = (float)GetRandomValue(0, 628) / 100.0f, r = (float)GetRandomValue(4, 7);
        hallPos = (Vector2){P.pos.x + cosf(a) * r, P.pos.y + sinf(a) * r};
        hallT = 0.35f; hallCd = (float)GetRandomValue(40, 90) / 10.0f;
        if (GetRandomValue(0, 2) == 0) { set_msg("Arkanda biri adını fısıldıyor..."); play(S_WHISPER, 0.6f, 1.0f); }
    }
    lightMul = 1.0f;
    if ((P.battery < 15 || P.sanity < 30) && GetRandomValue(0, 100) < 3) flickerT = 0.09f;
    if (flickerT > 0) { flickerT -= dt; lightMul = 0.25f; }
    /* ani korku olayi: kisa sureligine onunde bir figur belirir (zarar vermez) */
    if (fakeT > 0) fakeT -= dt;
    if (cfg.jump && cfg.diff >= 1 && !P.hidden && gtime > 40.0f && dirPhase != 0) {
        scareEvtT -= dt * (dirPhase == 2 ? 2.0f : 1.0f);
        if (scareEvtT <= 0) {
            bool ok = false;
            for (int t = 0; t < 5 && !ok && gMonD > 9.0f; t++) {      /* birkac uzaklik / aci dene */
                float a = P.yaw + GetRandomValue(-25, 25) / 100.0f, r = 2.4f - t * 0.3f;
                Vector2 fp = {P.pos.x + cosf(a) * r, P.pos.y + sinf(a) * r};
                if (!solid_tile(tile_of(fp.x), tile_of(fp.y)) && los(P.pos, fp)) { fakePos = fp; ok = true; }
            }
            if (ok) {
                fakeT = 0.28f; scareT = 0.5f;
                play(S_STING, 0.9f, 1.25f); play(S_SCREAM, 0.45f, 1.4f);
                scareEvtT = (cfg.diff == 2 ? 60.0f : 90.0f) + GetRandomValue(0, 60);
            } else scareEvtT = 4.0f;                       /* uygun yer yok, kisa sure sonra tekrar dene */
        }
    }
    if (scareT > 0) scareT -= dt;
    if (msgT > 0) msgT -= dt;
}

/* ---------- ses guncelleme (ortam, katmanli muzik, yanki, kalp atisi) ---------- */
static void layer(int snum, float v) {
    if (!audioOK) return;
    if (v > 0.02f) { if (!IsSoundPlaying(snd[snum])) PlaySound(snd[snum]); SetSoundVolume(snd[snum], v); }
    else if (IsSoundPlaying(snd[snum])) StopSound(snd[snum]);
}
static void update_audio(float dt, bool playing) {
    if (!audioOK) return;
    if (gstate == ST_CINE && introMode) { if (IsSoundPlaying(snd[S_DRONE])) StopSound(snd[S_DRONE]); return; }   /* kendi videonun sesi calarken ortam sesi sussun */
    if (!IsSoundPlaying(snd[S_DRONE])) PlaySound(snd[S_DRONE]);
    SetSoundVolume(snd[S_DRONE], playing ? 0.26f + 0.2f * (1.0f - P.sanity / 100.0f) : 0.25f);
    for (int i = 0; i < 8; i++) {                            /* yanki kuyrugu */
        if (!echoQ[i].on) continue;
        echoQ[i].t -= dt;
        if (echoQ[i].t <= 0) {
            echoQ[i].on = false;
            SetSoundVolume(sndE[echoQ[i].s], clampf(echoQ[i].vol, 0, 1));
            SetSoundPitch(sndE[echoQ[i].s], echoQ[i].pitch);
            PlaySound(sndE[echoQ[i].s]);
        }
    }
    if (!playing) { layer(S_RUMBLE, 0); layer(S_PAD, 0); layer(S_PULSE, 0); layer(S_GENHUM, 0); return; }
    layer(S_RUMBLE, gProx > 0.05f ? gProx * 0.9f : 0);
    float inten = fmaxf(fmaxf(gProx, tension * 0.7f), (1.0f - P.sanity / 100.0f) * 0.6f);
    layer(S_PAD, clampf(inten * 1.2f - 0.1f, 0, 0.45f));       /* katman 2: gergin pad */
    layer(S_PULSE, clampf(inten * 1.6f - 0.55f, 0, 0.55f));    /* katman 3: nabiz */
    if (genOn && genIdx >= 0) {                              /* calisan jenerator: konuma gore ses */
        float d = vdist((Vector2){FL[0].pr[genIdx].cx, FL[0].pr[genIdx].cz}, P.pos);
        float g = P.lvl == 0 ? clampf(1.0f - d / 20.0f, 0, 1) : 0.15f;
        layer(S_GENHUM, g * g * 0.5f);
    } else layer(S_GENHUM, 0);
    heartT -= dt;
    if (inten > 0.1f && heartT <= 0) { play(S_HEART, 0.3f + 0.7f * inten, 1.0f); heartT = 1.15f - 0.75f * inten; }
    pianoT -= dt;
    if (pianoT <= 0) {
        static const float pit[4] = {1.0f, 1.19f, 0.89f, 1.33f};
        play(S_PIANO, 0.22f, pit[GetRandomValue(0, 3)]);
        pianoT = 25.0f + GetRandomValue(0, 35);
    }
}


/* ---------- isik hesabi ---------- */
static bool in_view(float dx, float dz, float d) {      /* bakis yonunun arkasindaki karolari atla */
    if (d < 2.0f) return true;
    return (dx * cosf(P.yaw) + dz * sinf(P.yaw)) > -0.25f * d;
}
static float lamp_level(const Light *L, int idx) {
    if (L->kind == LK_MOON) return 1.0f;
    if (L->kind == LK_FIRE) return 0.85f + 0.15f * sinf(gtime * 9.0f + idx);
    float nearD = 99.0f;
    for (int i = 0; i < NMON; i++) {
        if (!mons[i].active || mons[i].lvl != P.lvl) continue;
        float d = vdist(mons[i].pos, (Vector2){L->x, L->z});
        if (d < nearD) nearD = d;
    }
    float f = 0.96f + 0.04f * sinf(gtime * 3.0f + idx);
    if (powerFlashT > 0) f *= 0.35f + 0.65f * fabsf(sinf(gtime * 22.0f + idx));      /* elektrik geldi */
    if (nearD < 7.0f) {                                   /* canavar yaklasinca lambalar titrer */
        float k = 1.0f - nearD / 7.0f;
        if (sinf(gtime * 37.0f + idx * 5.0f) + sinf(gtime * 23.0f + idx) > 1.4f - k * 2.0f) f *= 0.1f + 0.4f * (1.0f - k);
    }
    return f;
}
static float tile_light(float x, float z) {
    int tx = tile_of(x), ty = tile_of(z);
    if (oob(tx, ty)) return 0;
    return lg[ty][tx];
}
static void compute_light(void) {
    memset(lg, 0, sizeof lg); memset(lw, 0, sizeof lw); memset(ls, 0, sizeof ls);
    bool lit = P.torch && P.battery > 0 && !P.hidden;
    int px = tile_of(P.pos.x), pz = tile_of(P.pos.y);
    float torchFlick = 0.97f + 0.03f * sinf(gtime * 13.0f);
    float flCone = lerpf(1.08f, 0.50f, P.focus), flRange = lerpf(9.5f, 14.5f, P.focus);
    float flPow = lerpf(1.15f, 1.6f, P.focus) * (P.battery < 20.0f ? 0.5f + 0.5f * P.battery / 20.0f : 1.0f);
    for (int ty = pz - VIEW_R; ty <= pz + VIEW_R; ty++)
        for (int tx = px - VIEW_R; tx <= px + VIEW_R; tx++) {
            if (oob(tx, ty)) continue;
            Vector2 c = tile_center(tx, ty);
            float dx = c.x - P.pos.x, dz = c.y - P.pos.y;
            float d = sqrtf(dx * dx + dz * dz), b = 0;
            float wv = 0.1f;
            if (!in_view(dx, dz, d)) continue;
            if (!P.hidden && d < 4.5f) b = 0.42f * (1.0f - d / 4.5f);          /* yakin cevre aydinligi */
            if (lit && d < flRange) {
                float ad = angdiff(atan2f(dz, dx), P.yaw);
                if (ad < flCone) {
                    float u = clampf((ad - flCone * 0.35f) / (flCone * 0.65f), 0, 1);
                    float f = 1.0f - u * u * (3.0f - 2.0f * u);                  /* parlak cekirdek, yumusak kenar */
                    float fb = powf(1.0f - d / flRange, 0.85f) * f * flPow * lightMul * torchFlick;
                    if (fb > b) { b = fb; wv = -0.98f; } /* el feneri hastalikli yesil */
                }
            }
            if (!P.hidden && P.matchT > 0 && d < 6.0f) {               /* kibrit: kucuk, sicak, titrek isik */
                float mb = 0.55f * (1.0f - d / 6.0f) * (0.85f + 0.15f * sinf(gtime * 17.0f));
                if (P.matchT < 4.0f) mb *= P.matchT / 4.0f;
                if (mb > b) { b = mb; wv = 0.8f; }
            }
            if (b > 0.002f && !los(P.pos, c)) b = 0;
            lg[ty][tx] = b; lw[ty][tx] = b > 0 ? wv : 0;
        }

    for (int i = 0; i < nlights; i++) {                    /* lamba, somine, ay isigi */
        const Light *L = &lights[i];
        if (fabsf(L->x - P.pos.x) > VIEW_R + L->rad || fabsf(L->z - P.pos.y) > VIEW_R + L->rad) continue;
        float lv = lamp_level(L, i) * L->inten;
        if (lv < 0.01f) continue;
        Vector2 lp = {L->x, L->z};
        int r = (int)ceilf(L->rad);
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                int tx = tile_of(lp.x) + dx, ty = tile_of(lp.y) + dy;
                if (oob(tx, ty)) continue;
                Vector2 c = tile_center(tx, ty);
                float d = vdist(c, lp);
                if (d > L->rad) continue;
                float pdx = c.x - P.pos.x, pdz = c.y - P.pos.y;
                if (fabsf(pdx) > VIEW_R || fabsf(pdz) > VIEW_R || !in_view(pdx, pdz, sqrtf(pdx * pdx + pdz * pdz))) continue;
                if (!los(lp, c)) continue;
                float t = 1.0f - d / L->rad;
                /* daha genis ve yumusak gecis; keskin spot goruntusu olusmasin */
                float sm = t * t * (3.0f - 2.0f * t);
                float fb = lv * (0.22f * t + 0.78f * sm);
                float nb = lg[ty][tx] + fb;
                lw[ty][tx] = (lw[ty][tx] * lg[ty][tx] + L->warm * fb) / nb;
                lg[ty][tx] = nb; ls[ty][tx] += fb;
            }
    }
    for (int ty = pz - VIEW_R; ty <= pz + VIEW_R; ty++)     /* mini harita icin kesfedilenler */
        for (int tx = px - VIEW_R; tx <= px + VIEW_R; tx++) {
            if (oob(tx, ty)) continue;
            float dx = tx + 0.5f - P.pos.x, dz = ty + 0.5f - P.pos.y;
            if (lg[ty][tx] > 0.04f || dx * dx + dz * dz < 6.0f) cf->seeng[ty][tx] = true;
        }
}

static void update_camera(void) {
    float shake = (P.sanity < 40 ? (40 - P.sanity) / 40.0f * 0.03f : 0) + gProx * 0.025f;
    float r1 = (float)GetRandomValue(-100, 100) / 100.0f, r2 = (float)GetRandomValue(-100, 100) / 100.0f;
    float target = P.hidden ? 1.45f : (P.crouch ? 1.05f : 1.55f);
    P.eye += (target - P.eye) * fminf(1.0f, GetFrameTime() * 10.0f);
    float eye = P.eye + (P.moving ? sinf(P.bob) * 0.045f : 0);
    Vector3 pos = {P.pos.x + r1 * shake, eye + r2 * shake, P.pos.y + r1 * shake};
    Vector3 fwd = {cosf(P.pitch) * cosf(P.yaw), sinf(P.pitch), cosf(P.pitch) * sinf(P.yaw)};
    cam3.fovy = cfg.fov;
    cam3.position = pos;
    cam3.target = (Vector3){pos.x + fwd.x, pos.y + fwd.y, pos.z + fwd.z};
    float roll = P.sanity < 40 ? sinf(gtime * 1.3f) * 0.05f * (1.0f - P.sanity / 40.0f) : 0;
    cam3.up = (Vector3){roll, 1, 0};
    if (gstate == ST_DEAD && jumpT > 0) {                  /* jumpscare: yuz kameraya firlar */
        float e = 1.5f - jumpT, ang = atan2f(P.pos.y - jumpSrc.y, P.pos.x - jumpSrc.x);
        float k = clampf(e / JUMP_LUNGE, 0, 1), sh = 0.05f + 0.03f * sinf(e * 60.0f);
        int kk = killer >= 0 ? mons[killer].kind : K_STATUE;     /* akil kaybi: onune Heykel gelir */
        float headY = kk == K_STATUE ? 2.1f : 1.9f;
        float lean = 0.15f * k;                              /* canavar one egilirken basi de one gelir */
        float hf = headY * sinf(lean), hy = headY * cosf(lean);
        float dist = hf + lerpf(2.2f, 0.8f, k);              /* kamera her zaman kafanin onunde kalir */
        cam3.position = (Vector3){jumpSrc.x + cosf(ang) * dist + r1 * sh, 1.75f + r2 * sh, jumpSrc.y + sinf(ang) * dist + r1 * sh};
        cam3.target = (Vector3){jumpSrc.x + cosf(ang) * hf, hy, jumpSrc.y + sinf(ang) * hf};
        cam3.up = (Vector3){0, 1, 0};
    } else if (gstate == ST_DEAD && killer >= 0 && mons[killer].lvl == P.lvl) cam3.target = (Vector3){mons[killer].pos.x, 1.7f, mons[killer].pos.y};
}

/* ---------- canavarlar ---------- */
/* ---------- canavar animasyonu: modele her karede prosedurel poz uygulanir ---------- */
typedef struct { float bob, roll, lean, yaw, sx, sy, shear; } Pose;
typedef struct { float a[16]; } M4;                          /* satir esasli 4x4 (sutun vektorlu) */
static const Monster *gAnimM;                                /* simdi cizilen canavar (NULL: sabit/halusinasyon) */
static M4 m4_id(void) { return (M4){{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}; }
static M4 m4_mul(M4 x, M4 y) {
    M4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float v = 0;
            for (int k = 0; k < 4; k++) v += x.a[i * 4 + k] * y.a[k * 4 + j];
            r.a[i * 4 + j] = v;
        }
    return r;
}
static M4 m4_from(Matrix m) { return (M4){{m.m0, m.m4, m.m8, m.m12, m.m1, m.m5, m.m9, m.m13, m.m2, m.m6, m.m10, m.m14, m.m3, m.m7, m.m11, m.m15}}; }
static Matrix m4_to(M4 x) {
    Matrix m = {x.a[0], x.a[1], x.a[2], x.a[3], x.a[4], x.a[5], x.a[6], x.a[7], x.a[8], x.a[9], x.a[10], x.a[11], x.a[12], x.a[13], x.a[14], x.a[15]};
    return m;
}
static Pose mon_pose(int kind, const Monster *m) {
    Pose q = {0, 0, 0, 0, 1, 1, 0};
    float t = gtime + (m ? (float)((m - mons) * 7) : 0.0f);   /* her canavar farkli fazda */
    float spd = m ? m->spd : 0, ang = m ? m->anim : 0;
    bool chase = m && m->state == M_CHASE;
    float mv = clampf(spd / 1.6f, 0, 1);
    if (kind == K_WARDEN) {                                   /* Bekci: agir, topal adimlarla yuruyen */
        float w = ang * 3.5f;
        q.bob = 0.05f * fabsf(sinf(w)) * mv;
        q.roll = (0.075f * sinf(w) + 0.02f) * mv;
        q.lean = (0.05f + (chase ? 0.10f : 0.0f)) * mv + 0.01f * sinf(t * 1.7f);
        q.yaw = 0.05f * sinf(w) * mv;
        q.sy = 1.0f + 0.01f * sinf(t * 2.0f);                 /* nefes */
    } else {                                                  /* Heykel: mekanik, sarsak adimlar */
        float w = ang * 2.6f, sw = sinf(w);
        float ss = (sw >= 0 ? 1.0f : -1.0f) * powf(fabsf(sw), 0.55f);
        q.bob = 0.03f * fabsf(sw) * mv;
        q.roll = 0.09f * ss * mv;
        q.lean = 0.05f * mv;
        q.yaw = 0.04f * ss * mv;
        float jit = sinf(t * 5.3f) > 0.72f ? 1.0f : 0.0f;      /* donukken bile ara sira titrer */
        q.roll += 0.012f * jit * sinf(t * 70.0f);
        q.yaw += 0.015f * jit * sinf(t * 63.0f);
    }
    if (gstate == ST_DEAD && jumpT > 0 && m && (int)(m - mons) == killer) {   /* jumpscare: one atilir */
        float e = clampf((1.5f - jumpT) / JUMP_LUNGE, 0, 1);
        q.lean += 0.15f * e; q.sy += 0.04f * e;
    }
    return q;
}
static M4 pose_m4(Pose q, Matrix base) {
    float cr = cosf(q.roll), sr = sinf(q.roll), cl = cosf(q.lean), sl = sinf(q.lean), cw = cosf(q.yaw), sw = sinf(q.yaw);
    M4 T = m4_id(); T.a[7] = q.bob;
    M4 Rz = {{cr, -sr, 0, 0, sr, cr, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    M4 Rx = {{1, 0, 0, 0, 0, cl, -sl, 0, 0, sl, cl, 0, 0, 0, 0, 1}};
    M4 Ry = {{cw, 0, sw, 0, 0, 1, 0, 0, -sw, 0, cw, 0, 0, 0, 0, 1}};
    M4 Sh = m4_id(); Sh.a[1] = q.shear;
    M4 S = {{q.sx, 0, 0, 0, 0, q.sy, 0, 0, 0, 0, q.sx, 0, 0, 0, 0, 1}};
    return m4_mul(T, m4_mul(Rz, m4_mul(Rx, m4_mul(Ry, m4_mul(Sh, m4_mul(S, m4_from(base)))))));
}
static Vector3 m4_pt(M4 A, Vector3 p) {
    return (Vector3){A.a[0] * p.x + A.a[1] * p.y + A.a[2] * p.z + A.a[3], A.a[4] * p.x + A.a[5] * p.y + A.a[6] * p.z + A.a[7],
                     A.a[8] * p.x + A.a[9] * p.y + A.a[10] * p.z + A.a[11]};
}
static void draw_sprite(int ti, Vector2 p, float hgt, float b) {
    Texture2D t = tex[ti];
    Pose q = mon_pose(ti == T_WARDEN ? K_WARDEN : K_STATUE, gAnimM);
    float wd = hgt * (float)t.width / (float)t.height * q.sx, hh = hgt * q.sy;
    Rectangle src = {0, 0, (float)t.width, (float)t.height};
    DrawBillboardRec(cam3, t, src, (Vector3){p.x, hh / 2 + q.bob, p.y}, (Vector2){wd, hh}, shade((Color){255, 255, 255, 255}, fmaxf(b, 0.1f), 0));
}
static void draw_mon_model(int k, Vector2 p, float face, float b) {
    gShadeD = 0; gWarm = 0;
    Pose q = mon_pose(k, gAnimM);
    Model mm = monModel[k];
    mm.transform = m4_to(pose_m4(q, monModel[k].transform));
    DrawModelEx(mm, (Vector3){p.x, 0, p.y}, (Vector3){0, 1, 0}, (PI / 2 - face) * RAD2DEG, (Vector3){1, 1, 1}, shade((Color){255, 255, 255, 255}, fmaxf(b, 0.1f), 0));
    if (k == K_STATUE) {                                     /* statue.glb: gozler karanlikta da parlar (pozla birlikte hareket eder) */
        M4 A = pose_m4(q, monModel[k].transform);
        float th = PI / 2 - face, ct = cosf(th), st = sinf(th);
        for (int e = -1; e <= 1; e += 2) {
            Vector3 w = m4_pt(A, (Vector3){0.07f * e, 2.14f, 0.235f});
            DrawSphere((Vector3){p.x + w.x * ct + w.z * st, w.y, p.y - w.x * st + w.z * ct}, 0.04f, (Color){255, 30, 30, 255});
        }
    }
}
static void draw_statue(Vector2 p, float face, float b) {
    if (hasMM[K_STATUE] && useTex) { draw_mon_model(K_STATUE, p, face, b); return; }
    gShadeD = 0; gWarm = 0;
    if (texOn(T_STATUE)) { draw_sprite(T_STATUE, p, 2.0f, b); return; }
    float bb = fmaxf(b, 0.08f);
    Color stone = shade((Color){150, 150, 148, 255}, bb, 0), dk = shade((Color){92, 92, 94, 255}, bb, 0);
    float sx = -sinf(face), sz = cosf(face), fx = cosf(face), fz = sinf(face);
    DrawCylinder((Vector3){p.x, 0.0f, p.y}, 0.2f, 0.3f, 1.2f, 10, stone);
    DrawCylinder((Vector3){p.x, 1.15f, p.y}, 0.24f, 0.18f, 0.55f, 10, stone);
    DrawSphere((Vector3){p.x + fx * 0.06f, 1.9f, p.y + fz * 0.06f}, 0.18f, stone);
    for (int s = -1; s <= 1; s += 2) {
        DrawCube((Vector3){p.x + fx * 0.2f + sx * 0.1f * s, 1.85f, p.y + fz * 0.2f + sz * 0.1f * s}, 0.08f, 0.3f, 0.08f, dk);      /* eller yuzu kapatir */
        DrawCube((Vector3){p.x - fx * 0.18f + sx * 0.32f * s, 1.45f, p.y - fz * 0.18f + sz * 0.32f * s}, 0.05f, 1.1f, 0.4f, dk);   /* kanatlar */
    }
}
static void draw_warden(Vector2 p, float face, float b) {
    if (hasMM[K_WARDEN] && useTex) { draw_mon_model(K_WARDEN, p, face, b); return; }
    gShadeD = 0; gWarm = 0;
    if (texOn(T_WARDEN)) { draw_sprite(T_WARDEN, p, 2.2f, b); return; }
    Color skin = shade((Color){150, 148, 138, 255}, b, 0), sack = shade((Color){120, 100, 70, 255}, b, 0), dk = shade((Color){20, 18, 20, 255}, b, 0);
    float sx = -sinf(face), sz = cosf(face), fx = cosf(face), fz = sinf(face);
    DrawCylinder((Vector3){p.x, 0.0f, p.y}, 0.13f, 0.17f, 1.3f, 8, skin);                /* uzun govde */
    DrawCylinder((Vector3){p.x + fx * 0.06f, 1.2f, p.y + fz * 0.06f}, 0.2f, 0.14f, 0.7f, 8, skin);
    DrawSphere((Vector3){p.x + fx * 0.14f, 1.95f, p.y + fz * 0.14f}, 0.2f, sack);        /* cuval kafa */
    for (int s = -1; s <= 1; s += 2)
        DrawSphere((Vector3){p.x + fx * 0.3f + sx * 0.08f * s, 1.98f, p.y + fz * 0.3f + sz * 0.08f * s}, 0.035f, dk);
    for (int s = -1; s <= 1; s += 2)
        DrawCube((Vector3){p.x + sx * 0.25f * s + fx * 0.1f, 1.0f, p.y + sz * 0.25f * s + fz * 0.1f}, 0.06f, 1.2f, 0.06f, skin);
    float lx = p.x + sx * 0.3f + fx * 0.25f, lz = p.y + sz * 0.3f + fz * 0.25f;          /* fener: uzaktan gorunur */
    float sw = sinf(gtime * 2.5f) * 0.04f;
    DrawCube((Vector3){lx + sw, 0.75f, lz}, 0.1f, 0.16f, 0.1f, (Color){255, 190, 80, 255});
    DrawCube((Vector3){lx + sw, 0.86f, lz}, 0.13f, 0.03f, 0.13f, (Color){30, 26, 20, 255});
}

/* ---------- mobilya ---------- */
static int prop_tex(Color c) {                    /* mobilya renginden doku sec */
    static const Color wd[5] = {{98, 66, 42, 255}, {58, 38, 26, 255}, {118, 88, 54, 255}, {86, 62, 38, 255}, {106, 78, 48, 255}};
    static const Color fb[3] = {{96, 52, 48, 255}, {80, 42, 40, 255}, {110, 62, 56, 255}};
    for (int i = 0; i < 5; i++) if (c.r == wd[i].r && c.g == wd[i].g && c.b == wd[i].b) return T_WOOD;
    for (int i = 0; i < 3; i++) if (c.r == fb[i].r && c.g == fb[i].g && c.b == fb[i].b) return T_FABRIC;
    return -1;
}
static void pbox(const Prop *p, float so, float fo, float y, float sw, float sh, float sd, Color c, float b, float emis) {
    float rx = -(float)p->fz, rz = (float)p->fx;
    float x = p->cx + rx * so + p->fx * fo, z = p->cz + rz * so + p->fz * fo;
    float ex = p->fx != 0 ? sd : sw, ez = p->fx != 0 ? sw : sd;
    int ti = prop_tex(c);
    if (ti >= 0) cubeT(ti, (Vector3){x, y + sh / 2, z}, ex, sh, ez, c, b, emis);
    else DrawCube((Vector3){x, y + sh / 2, z}, ex, sh, ez, shade(c, b, emis));
}
#define PB(so, fo, y, sw, sh, sd, ...) pbox(p, (so), (fo), (y), (sw), (sh), (sd), (__VA_ARGS__), b, 0.0f)
#define PE(so, fo, y, sw, sh, sd, em, ...) pbox(p, (so), (fo), (y), (sw), (sh), (sd), (__VA_ARGS__), b, (em))

/* menteseli kapak: prop'un yerel eksenlerinde (so: yana, fo: one) mentese noktasi, ang: acilma acisi */
static void pleaf(const Prop *p, float so, float fo, float y, float w, float h, float th, float ang, Color c, Color hc, float b) {
    float rx = -(float)p->fz, rz = (float)p->fx;
    float hx = p->cx + rx * so + p->fx * fo, hz = p->cz + rz * so + p->fz * fo;
    float phi = atan2f(-rz, rx) * RAD2DEG;                   /* yerel +X = prop'un +so ekseni, -Z = on */
    float dir = so < 0 ? 1.0f : -1.0f;                       /* menteseden orta noktaya dogru uzanir */
    rlPushMatrix();
    rlTranslatef(hx, y, hz);
    rlRotatef(phi + (dir > 0 ? ang : -ang), 0, 1, 0);
    DrawCube((Vector3){dir * w / 2, h / 2, 0}, w, h, th, shade(c, b, 0));
    DrawCube((Vector3){dir * (w - 0.07f), h * 0.5f, -th / 2 - 0.01f}, 0.03f, 0.12f, 0.02f, shade(hc, b, 0.05f));
    rlPopMatrix();
}
/* toplanabilir esyalar: yerde/rafta durur, gercekci sekillerle cizilir */
static void draw_item(const Item *it, float b, float dist) {
    Vector3 w = item_pos(it);
    gShadeD = dist; gWarm = 0;
#define SC(r, g, bl) shade((Color){r, g, bl, 255}, b, 0.05f)
    rlPushMatrix();
    rlTranslatef(w.x, w.y, w.z);
    rlRotatef(it->rot * RAD2DEG, 0, 1, 0);
    switch (it->type) {
    case IT_PAGE:
        DrawCube((Vector3){0.012f, 0.003f, 0.01f}, 0.21f, 0.006f, 0.29f, SC(214, 205, 176));
        DrawCube((Vector3){0, 0.009f, 0}, 0.21f, 0.006f, 0.29f, SC(232, 224, 196));
        for (int k = 0; k < 6; k++) DrawCube((Vector3){0, 0.0125f, -0.11f + k * 0.043f}, 0.15f, 0.002f, 0.008f, SC(60, 50, 40));
        DrawCube((Vector3){0.078f, 0.0125f, 0.118f}, 0.05f, 0.002f, 0.05f, SC(205, 195, 165));
        break;
    case IT_NOTE:
        DrawCube((Vector3){0, 0.006f, 0}, 0.16f, 0.012f, 0.11f, SC(205, 192, 160));
        DrawCube((Vector3){0, 0.0125f, 0}, 0.17f, 0.003f, 0.012f, SC(140, 40, 40));
        DrawCube((Vector3){0, 0.0125f, 0}, 0.012f, 0.003f, 0.12f, SC(140, 40, 40));
        break;
    case IT_BATTERY:
        DrawCylinder((Vector3){0, 0, 0}, 0.034f, 0.034f, 0.14f, 10, SC(60, 170, 70));
        DrawCylinder((Vector3){0, 0.05f, 0}, 0.0345f, 0.0345f, 0.04f, 10, SC(30, 36, 30));
        DrawCylinder((Vector3){0, 0.14f, 0}, 0.014f, 0.014f, 0.02f, 8, SC(200, 200, 205));
        break;
    case IT_KEY:
        DrawCube((Vector3){0, 0.006f, -0.085f}, 0.05f, 0.008f, 0.01f, SC(220, 180, 60));
        DrawCube((Vector3){0, 0.006f, -0.035f}, 0.05f, 0.008f, 0.01f, SC(220, 180, 60));
        DrawCube((Vector3){-0.02f, 0.006f, -0.06f}, 0.01f, 0.008f, 0.05f, SC(220, 180, 60));
        DrawCube((Vector3){0.02f, 0.006f, -0.06f}, 0.01f, 0.008f, 0.05f, SC(220, 180, 60));
        DrawCube((Vector3){0, 0.006f, 0.04f}, 0.012f, 0.008f, 0.14f, SC(220, 180, 60));
        DrawCube((Vector3){0.014f, 0.006f, 0.09f}, 0.02f, 0.008f, 0.012f, SC(220, 180, 60));
        DrawCube((Vector3){0.012f, 0.006f, 0.066f}, 0.016f, 0.008f, 0.012f, SC(220, 180, 60));
        break;
    case IT_BOTTLE:
        DrawCylinder((Vector3){0, 0, 0}, 0.045f, 0.045f, 0.18f, 10, SC(50, 120, 70));
        DrawCylinder((Vector3){0, 0.004f, 0}, 0.0455f, 0.0455f, 0.09f, 10, SC(34, 70, 40));
        DrawCylinder((Vector3){0, 0.18f, 0}, 0.045f, 0.018f, 0.05f, 10, SC(50, 120, 70));
        DrawCylinder((Vector3){0, 0.23f, 0}, 0.018f, 0.018f, 0.08f, 8, SC(50, 120, 70));
        DrawCylinder((Vector3){0, 0.31f, 0}, 0.016f, 0.016f, 0.02f, 8, SC(120, 84, 50));
        break;
    case IT_MATCH:
        DrawCube((Vector3){0, 0.014f, 0}, 0.085f, 0.028f, 0.055f, SC(190, 60, 40));
        DrawCube((Vector3){0, 0.0285f, 0}, 0.06f, 0.002f, 0.04f, SC(235, 225, 200));
        DrawCube((Vector3){0.07f, 0.003f, 0.02f}, 0.05f, 0.004f, 0.004f, SC(200, 170, 110));
        DrawCube((Vector3){0.098f, 0.003f, 0.02f}, 0.01f, 0.006f, 0.006f, SC(170, 40, 30));
        break;
    case IT_FUSE:
        DrawCylinder((Vector3){0, 0, 0}, 0.016f, 0.016f, 0.065f, 8, SC(225, 220, 205));
        DrawCylinder((Vector3){0, 0, 0}, 0.0175f, 0.0175f, 0.01f, 8, SC(210, 170, 70));
        DrawCylinder((Vector3){0, 0.055f, 0}, 0.0175f, 0.0175f, 0.01f, 8, SC(210, 170, 70));
        break;
    case IT_FUEL:
        DrawCube((Vector3){0, 0.15f, 0}, 0.2f, 0.3f, 0.11f, SC(170, 40, 30));
        DrawCube((Vector3){0, 0.15f, 0.058f}, 0.16f, 0.02f, 0.004f, SC(110, 24, 20));
        DrawCube((Vector3){0, 0.15f, -0.058f}, 0.16f, 0.02f, 0.004f, SC(110, 24, 20));
        DrawCube((Vector3){0.02f, 0.312f, 0}, 0.12f, 0.025f, 0.02f, SC(140, 32, 26));
        DrawCylinder((Vector3){-0.07f, 0.3f, 0}, 0.018f, 0.018f, 0.05f, 8, SC(210, 170, 40));
        break;
    }
    rlPopMatrix();
#undef SC
    if (it->type == IT_PAGE || it->type == IT_KEY || it->type == IT_FUSE || it->type == IT_FUEL || it->type == IT_NOTE) {
        if (dist < 8.0f && b > 0.03f) {                                 /* hafif parilti: karanlikta fark edilsin */
            float g = 0.5f + 0.5f * sinf(gtime * 2.3f + it->rot * 5.0f);
            DrawCube((Vector3){w.x, w.y + 0.11f + 0.02f * g, w.z}, 0.012f, 0.012f, 0.012f, (Color){255, 255, 235, (unsigned char)(40 + 120 * g)});
        }
    }
}
static void draw_prop_model(const Prop *p, float b) {
    float ang = atan2f((float)p->fx, (float)p->fz) * RAD2DEG;
    Color c = shade((Color){255, 255, 255, 255}, b, (p->type == P_LAMP && p->lit) ? 0.75f : 0.0f);
    DrawModelEx(propModel[p->type], (Vector3){p->cx, 0, p->cz}, (Vector3){0, 1, 0}, ang, (Vector3){(float)p->len, 1, 1}, c);
}
static void draw_prop(const Prop *p, float b, float dist) {
    if (hasPM[p->type] && useTex) { draw_prop_model(p, b); return; }
    const Color wood = {98, 66, 42, 255}, dark = {58, 38, 26, 255}, fabric = {96, 52, 48, 255};
    const Color fabricD = {80, 42, 40, 255}, cream = {200, 194, 176, 255}, black = {24, 22, 26, 255};
    float W = (float)p->len;
    switch (p->type) {
    case P_BOOKSHELF: {
        static const Color bc[6] = {{110, 40, 38, 255}, {40, 70, 60, 255}, {60, 52, 90, 255},
                                    {120, 96, 50, 255}, {80, 60, 44, 255}, {50, 50, 56, 255}};
        PB(0, -0.48f, 0, W, 2.0f, 0.04f, dark);
        PB(-W / 2 + 0.03f, -0.3f, 0, 0.06f, 2.0f, 0.4f, wood);
        PB(W / 2 - 0.03f, -0.3f, 0, 0.06f, 2.0f, 0.4f, wood);
        for (int s = 0; s < 5; s++) PB(0, -0.3f, s * 0.48f, W, 0.04f, 0.4f, wood);
        if (dist < 8.0f) {
            int n = p->len * 4;
            float step = (W - 0.28f) / n;
            for (int s = 0; s < 4; s++)
                for (int k = 0; k < n; k++) {
                    unsigned h = hsh(p->tx * 31 + k * 7 + s, p->ty * 17 + s * 5 + k);
                    if (h % 7 == 0) continue;
                    PB(-W / 2 + 0.14f + k * step, -0.3f, s * 0.48f + 0.04f, step * 0.85f, 0.26f + (h % 5) * 0.04f, 0.28f, bc[h % 6]);
                }
        }
        break; }
    case P_BED:
        PB(0, 0, 0.05f, W, 0.25f, 1.0f, dark);
        PB(0, 0, 0.30f, W - 0.1f, 0.16f, 0.9f, cream);
        PB(-0.72f, 0, 0.46f, 0.45f, 0.1f, 0.65f, (Color){220, 216, 205, 255});
        PB(0.2f, 0, 0.46f, 1.25f, 0.05f, 0.92f, (Color){70, 48, 52, 255});
        PB(-W / 2 + 0.03f, 0, 0.05f, 0.06f, 0.95f, 1.0f, dark);
        PB(W / 2 - 0.03f, 0, 0.05f, 0.06f, 0.5f, 1.0f, dark);
        break;
    case P_WARDROBE: {                                        /* dolap: kapaklar menteseden acilir, icinde raf */
        Color brass = {180, 150, 60, 255};
        PB(0, -0.48f, 0, W, 2.1f, 0.04f, wood);
        PB(-(W / 2 - 0.025f), -0.2f, 0, 0.05f, 2.1f, 0.6f, wood);
        PB(W / 2 - 0.025f, -0.2f, 0, 0.05f, 2.1f, 0.6f, wood);
        PB(0, -0.2f, 2.04f, W, 0.06f, 0.6f, wood);
        PB(0, -0.2f, 0, W, 0.1f, 0.6f, dark);
        PB(0, -0.2f, 1.0f, W - 0.1f, 0.04f, 0.56f, dark);
        float ang = 105.0f * p->open;
        pleaf(p, -W / 2, 0.115f, 0.05f, W / 2, 1.97f, 0.03f, ang, wood, brass, b);
        pleaf(p, W / 2, 0.115f, 0.05f, W / 2, 1.97f, 0.03f, ang, wood, brass, b);
        break; }
    case P_NIGHTSTAND: {                                      /* komodin: cekmece one kayar */
        float d = 0.30f * p->open;
        PB(-0.23f, -0.25f, 0, 0.04f, 0.55f, 0.5f, wood);
        PB(0.23f, -0.25f, 0, 0.04f, 0.55f, 0.5f, wood);
        PB(0, -0.47f, 0, 0.5f, 0.55f, 0.04f, wood);
        PB(0, -0.25f, 0, 0.5f, 0.04f, 0.5f, dark);
        PB(0, -0.25f, 0.51f, 0.5f, 0.04f, 0.5f, wood);
        PB(0, -0.25f, 0.21f, 0.42f, 0.02f, 0.46f, dark);
        PB(0, -0.02f, 0.03f, 0.44f, 0.17f, 0.03f, wood);
        PB(0, -0.02f + d, 0.24f, 0.44f, 0.24f, 0.03f, wood);
        PB(0, -0.25f + d, 0.24f, 0.38f, 0.02f, 0.40f, dark);
        PB(-0.19f, -0.25f + d, 0.24f, 0.02f, 0.09f, 0.40f, dark);
        PB(0.19f, -0.25f + d, 0.24f, 0.02f, 0.09f, 0.40f, dark);
        PB(0, -0.45f + d, 0.24f, 0.38f, 0.09f, 0.02f, dark);
        PB(0, 0.0f + d, 0.34f, 0.1f, 0.02f, 0.02f, (Color){180, 150, 60, 255});
        PE(0, -0.25f, 0.55f, 0.12f, 0.22f, 0.12f, 0.3f, (Color){230, 205, 140, 255});
        break; }
    case P_COUCH:
        PB(0, -0.05f, 0, W, 0.4f, 0.9f, fabric);
        PB(0, -0.4f, 0.4f, W, 0.55f, 0.2f, fabricD);
        PB(-(W / 2 - 0.1f), -0.05f, 0.4f, 0.2f, 0.3f, 0.9f, fabricD);
        PB(W / 2 - 0.1f, -0.05f, 0.4f, 0.2f, 0.3f, 0.9f, fabricD);
        PB(-W / 4 - 0.02f, 0.0f, 0.4f, W / 2 - 0.25f, 0.12f, 0.6f, (Color){110, 62, 56, 255});
        PB(W / 4 + 0.02f, 0.0f, 0.4f, W / 2 - 0.25f, 0.12f, 0.6f, (Color){110, 62, 56, 255});
        break;
    case P_ARMCHAIR:
        PB(0, -0.1f, 0, 0.9f, 0.4f, 0.8f, fabric);
        PB(0, -0.4f, 0.4f, 0.9f, 0.5f, 0.15f, fabricD);
        PB(-0.38f, -0.1f, 0.4f, 0.14f, 0.28f, 0.7f, fabricD);
        PB(0.38f, -0.1f, 0.4f, 0.14f, 0.28f, 0.7f, fabricD);
        PB(0, 0.0f, 0.4f, 0.6f, 0.1f, 0.5f, (Color){110, 62, 56, 255});
        break;
    case P_TABLE:
        PB(0, -0.1f, 0.72f, W, 0.06f, 0.8f, wood);
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sz = -1; sz <= 1; sz += 2) PB(sx * (W / 2 - 0.07f), -0.1f + sz * 0.3f, 0, 0.07f, 0.72f, 0.07f, dark);
        break;
    case P_DESK: {                                            /* masa: sol gozde cekmece one kayar */
        float so0 = -W / 2 + 0.25f, d = 0.32f * p->open;
        PB(0, -0.15f, 0.72f, W, 0.05f, 0.7f, dark);
        PB(so0 - 0.23f, -0.15f, 0, 0.04f, 0.72f, 0.65f, wood);
        PB(so0 + 0.23f, -0.15f, 0, 0.04f, 0.72f, 0.65f, wood);
        PB(so0, -0.45f, 0, 0.5f, 0.72f, 0.04f, wood);
        PB(so0, -0.15f, 0, 0.5f, 0.04f, 0.65f, dark);
        PB(so0, 0.16f, 0.04f, 0.46f, 0.43f, 0.03f, wood);
        PB(so0, 0.16f + d, 0.48f, 0.46f, 0.22f, 0.03f, wood);
        PB(so0, -0.15f + d, 0.50f, 0.42f, 0.02f, 0.58f, dark);
        PB(so0 - 0.20f, -0.15f + d, 0.50f, 0.02f, 0.09f, 0.58f, dark);
        PB(so0 + 0.20f, -0.15f + d, 0.50f, 0.02f, 0.09f, 0.58f, dark);
        PB(so0, -0.43f + d, 0.50f, 0.42f, 0.09f, 0.02f, dark);
        PB(so0, 0.18f + d, 0.58f, 0.12f, 0.02f, 0.02f, (Color){180, 150, 60, 255});
        PB(W / 2 - 0.05f, -0.15f, 0, 0.06f, 0.72f, 0.65f, wood);
        PB(0.2f, -0.15f, 0.77f, 0.3f, 0.01f, 0.22f, (Color){210, 205, 185, 255});
        PE(-0.6f, -0.3f, 0.77f, 0.12f, 0.25f, 0.12f, 0.25f, (Color){230, 205, 140, 255});
        break; }
    case P_CHAIR:
        PB(0, 0, 0.42f, 0.42f, 0.05f, 0.42f, wood);
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sz = -1; sz <= 1; sz += 2) PB(sx * 0.17f, sz * 0.17f, 0, 0.04f, 0.42f, 0.04f, dark);
        PB(0, -0.19f, 0.47f, 0.42f, 0.5f, 0.05f, wood);
        break;
    case P_FRIDGE:
        PB(0, -0.15f, 0, 0.9f, 1.9f, 0.7f, (Color){188, 192, 186, 255});
        PB(0, 0.201f, 1.25f, 0.9f, 0.02f, 0.02f, (Color){80, 84, 80, 255});
        PB(0.3f, 0.22f, 1.0f, 0.03f, 0.3f, 0.03f, (Color){120, 120, 120, 255});
        PB(0.3f, 0.22f, 1.5f, 0.03f, 0.3f, 0.03f, (Color){120, 120, 120, 255});
        break;
    case P_COUNTER:
        PB(0, -0.2f, 0, W, 0.85f, 0.6f, (Color){78, 92, 90, 255});
        PB(0, -0.19f, 0.85f, W, 0.05f, 0.62f, (Color){46, 46, 50, 255});
        PB(0, 0.101f, 0.1f, 0.02f, 0.7f, 0.01f, black);
        break;
    case P_CRATE: {                                           /* sandik: kapak yana kayar, icinde bosluk */
        float o = p->open;
        Color cr = {118, 88, 54, 255}, cl = {86, 62, 38, 255};
        PB(0, 0, 0, 0.8f, 0.04f, 0.8f, cr);
        PB(-0.38f, 0, 0, 0.04f, 0.8f, 0.8f, cr);
        PB(0.38f, 0, 0, 0.04f, 0.8f, 0.8f, cr);
        PB(0, -0.38f, 0, 0.72f, 0.8f, 0.04f, cr);
        PB(0, 0.38f, 0, 0.72f, 0.8f, 0.04f, cr);
        PB(0.55f * o, 0, 0.8f + 0.06f * o, 0.84f, 0.04f, 0.84f, cl);
        break; }
    case P_BARREL:
        DrawCylinder((Vector3){p->cx, 0, p->cz}, 0.31f, 0.33f, 0.9f, 12, shade(wood, b, 0));
        DrawCylinder((Vector3){p->cx, 0.18f, p->cz}, 0.335f, 0.335f, 0.05f, 12, shade(black, b, 0));
        DrawCylinder((Vector3){p->cx, 0.65f, p->cz}, 0.335f, 0.335f, 0.05f, 12, shade(black, b, 0));
        break;
    case P_CLOCK: {
        PB(0, -0.325f, 0, 0.56f, 2.1f, 0.39f, dark);
        PB(0, -0.325f, 1.98f, 0.66f, 0.12f, 0.46f, wood);
        PB(0, -0.325f, 0.06f, 0.62f, 0.12f, 0.44f, wood);
        PE(0, -0.15f, 1.58f, 0.34f, 0.34f, 0.025f, 0.12f, cream);
        PB(0, -0.15f, 1.58f, 0.025f, 0.25f, 0.018f, (Color){65, 56, 45, 255});
        PB(0, -0.15f, 1.58f, 0.20f, 0.018f, 0.018f, (Color){65, 56, 45, 255});
        PB(0, -0.15f, 1.58f, 0.018f, 0.11f, 0.022f, (Color){150, 35, 28, 255});
        PB(0, -0.15f, 0.48f, 0.32f, 0.72f, 0.025f, (Color){35, 40, 38, 255});
        PB(-0.11f, -0.13f, 0.48f, 0.018f, 0.55f, 0.035f, (Color){145, 112, 65, 255});
        PB(0.11f, -0.13f, 0.48f, 0.018f, 0.55f, 0.035f, (Color){145, 112, 65, 255});
        break; }
    case P_PLANT: {
        Color leaf = {38, 66, 39, 255}, leaf2 = {58, 78, 43, 255};
        DrawCylinder((Vector3){p->cx, 0.16f, p->cz}, 0.21f, 0.15f, 0.34f, 12, shade((Color){92, 54, 39, 255}, b, 0));
        DrawCylinder((Vector3){p->cx, 0.34f, p->cz}, 0.155f, 0.155f, 0.035f, 12, shade((Color){28, 24, 20, 255}, b, 0));
        for (int k = 0; k < 7; k++) {
            float a = k * 0.897f, hgt = 0.52f + (k % 3) * 0.16f;
            float ox = cosf(a) * (0.12f + 0.02f * (k % 2)), oz = sinf(a) * (0.12f + 0.02f * (k % 2));
            DrawCylinder((Vector3){p->cx + ox * 0.35f, 0.43f + hgt * 0.36f, p->cz + oz * 0.35f}, 0.012f, 0.009f, hgt * 0.72f, 5, shade((Color){58, 48, 30, 255}, b, 0));
            DrawSphere((Vector3){p->cx + ox, 0.48f + hgt * 0.72f, p->cz + oz}, 0.105f + 0.015f * (k % 2), shade(k % 2 ? leaf : leaf2, b, 0));
        }
        DrawSphere((Vector3){p->cx, 1.18f, p->cz}, 0.12f, shade(leaf, b, 0));
        break; }

    case P_PIANO: {                                             /* eski, cilasi dokulmus kuyruklu piyano */
        Color lacquer = {38, 29, 28, 255}, edge = {82, 55, 43, 255};
        Color ivory = {218, 211, 190, 255}, keyBlack = {12, 11, 13, 255};
        /* agir kasa, ust tabla ve yanaklar */
        PB(0, -0.15f, 0, W, 0.92f, 0.68f, lacquer);
        PB(0, -0.15f, 0.92f, W + 0.10f, 0.08f, 0.76f, edge);
        PB(0, -0.15f, 0.99f, W + 0.06f, 0.035f, 0.72f, lacquer);
        PB(-W / 2 + 0.07f, 0.08f, 0.74f, 0.13f, 0.42f, 0.28f, edge);
        PB(W / 2 - 0.07f, 0.08f, 0.74f, 0.13f, 0.42f, 0.28f, edge);
        /* klavye zemini ve tek tek kirli fildisi tuslar */
        PB(0, 0.29f, 0.70f, W - 0.20f, 0.055f, 0.25f, keyBlack);
        int keys = (int)(W * 7.0f); if (keys < 12) keys = 12; if (keys > 24) keys = 24;
        float kw = (W - 0.25f) / keys;
        for (int k = 0; k < keys; k++) {
            float so = -W * 0.5f + 0.125f + (k + 0.5f) * kw;
            PB(so, 0.30f, 0.715f, kw * 0.88f, 0.035f, 0.205f, ivory);
            PB(so, 0.405f, 0.75f, 0.008f, 0.008f, 0.012f, (Color){105, 91, 73, 255});
            /* siyah tuslar: her oktavda iki ve uc tus grubu */
            int pos = k % 7;
            if (pos == 0 || pos == 1 || pos == 3 || pos == 4 || pos == 5) {
                PB(so + kw * 0.48f, 0.235f, 0.748f, kw * 0.48f, 0.055f, 0.115f, keyBlack);
            }
        }
        /* yukari kalkmis nota sehpası, ince altin detaylar ve ayaklar */
        PB(0, -0.12f, 1.05f, W * 0.62f, 0.28f, 0.035f, lacquer);
        PB(0, -0.095f, 1.20f, W * 0.58f, 0.025f, 0.025f, edge);
        for (int sx = -1; sx <= 1; sx += 2) {
            PB(sx * (W * 0.36f), -0.13f, 0, 0.075f, 0.72f, 0.075f, lacquer);
            PB(sx * (W * 0.36f), -0.13f, 0.035f, 0.13f, 0.035f, 0.13f, edge);
        }
        PB(0, 0.53f, 0.06f, 0.32f, 0.025f, 0.035f, (Color){145, 116, 70, 255});
        PB(-0.12f, 0.52f, 0.06f, 0.025f, 0.16f, 0.025f, (Color){145, 116, 70, 255});
        PB(0.12f, 0.52f, 0.06f, 0.025f, 0.16f, 0.025f, (Color){145, 116, 70, 255});
        break; }
    case P_FIREPLACE:
        PB(0, -0.25f, 0, W, 1.7f, 0.5f, (Color){80, 76, 72, 255});
        PB(0, 0.0f, 0.05f, 0.95f, 0.75f, 0.02f, (Color){12, 8, 6, 255});
        PE(0, 0.0f, 0.08f, 0.5f, 0.28f, 0.02f, 0.5f + 0.15f * sinf(gtime * 11.0f), (Color){255, 120, 30, 255});
        PB(0, -0.15f, 1.55f, W + 0.2f, 0.09f, 0.62f, dark);
        break;
    case P_GENERATOR:
        PB(0, -0.2f, 0, 0.96f, 0.86f, 0.66f, (Color){58, 72, 52, 255});
        PB(0, -0.2f, 0.86f, 1.0f, 0.06f, 0.7f, (Color){34, 38, 34, 255});
        PB(-0.25f, 0.14f, 0.44f, 0.34f, 0.34f, 0.035f, (Color){24, 28, 25, 255});
        for (int k = 0; k < 5; k++) PB(-0.25f, 0.14f, 0.30f + k * 0.065f, 0.25f, 0.018f, 0.045f, (Color){85, 95, 82, 255});
        PB(0.27f, -0.2f, 0.48f, 0.16f, 0.48f, 0.16f, (Color){38, 42, 38, 255});
        PB(0.27f, -0.2f, 0.73f, 0.19f, 0.06f, 0.19f, (Color){80, 84, 72, 255});
        PE(-0.3f, 0.18f, 0.66f, 0.08f, 0.08f, 0.04f, 0.9f, genOn ? (Color){90, 255, 110, 255} : (Color){255, 48, 38, 255});
        for (int k = -1; k <= 1; k++) PB(k * 0.25f, -0.2f, 0.04f, 0.055f, 0.08f, 0.055f, (Color){25, 28, 25, 255});
        break;
    case P_FUSEBOX:
        PB(0, -0.46f, 1.1f, 0.56f, 0.76f, 0.10f, (Color){65, 74, 68, 255});
        PB(0, -0.395f, 1.1f, 0.43f, 0.62f, 0.025f, (Color){28, 34, 30, 255});
        PB(-0.13f, -0.37f, 1.12f, 0.12f, 0.42f, 0.025f, (Color){72, 78, 68, 255});
        PB(0.13f, -0.37f, 1.12f, 0.12f, 0.42f, 0.025f, (Color){72, 78, 68, 255});
        for (int k = 0; k < 3; k++) PB(-0.13f, -0.345f, 0.98f + k * 0.13f, 0.06f, 0.025f, 0.03f, (Color){150, 145, 115, 255});
        PE(0, -0.36f, 1.38f, 0.075f, 0.075f, 0.025f, 0.95f, fuseIn ? (Color){100, 255, 120, 255} : (Color){255, 48, 38, 255});
        PB(0, -0.36f, 0.78f, 0.32f, 0.08f, 0.025f, (Color){18, 20, 18, 255});
        break;
    case P_LAMP:
        PB(0, 0, 0, 0.3f, 0.04f, 0.3f, dark);
        PB(0, 0, 0.04f, 0.04f, 1.2f, 0.04f, black);
        PE(0, 0, 1.2f, 0.34f, 0.30f, 0.34f, p->lit ? 0.82f : 0.0f, (Color){150, 220, 115, 255});
        break;
    }
}

/* ---------- duvar, kapi, zemin ---------- */
static void draw_wall(int tx, int ty, float b, float dist) {
    int dx = 0, dz = 0;
    int rm = wall_face(tx, ty, &dx, &dz);
    if (rm == -2) return;                                   /* icte kalan, gorunmeyen blok */
    unsigned h = hsh(tx, ty);
    bool win = rm >= 0 && h % 9 == 0, pic = rm >= 0 && (h % 9 == 1 || h % 9 == 2);
    if (b <= 0.0f && dist > 3.0f && !win && !gCamMode) return;
    int th = rm >= 0 ? roomTheme[rm] : TH_CORRIDOR;
    const Theme *T = &themes[th];
    int var = (int)((h >> 8) % 7);
    Color wc = {(unsigned char)(T->wall.r + var), (unsigned char)(T->wall.g + var), (unsigned char)(T->wall.b + var), 255};
    float x = tx + 0.5f, z = ty + 0.5f;
    cubeT(T_WAINSCOT, (Vector3){x, 0.55f, z}, 1, 1.1f, 1, T->panel, b, 0);
    cubeT(T_WALL, (Vector3){x, 1.1f + (WALL_H - 1.1f) / 2, z}, 1, WALL_H - 1.1f, 1, wc, b, 0);
    DrawCube((Vector3){x, 1.1f, z}, 1.02f, 0.07f, 1.02f, shade(T->trim, b, 0));
    DrawCube((Vector3){x, 0.09f, z}, 1.025f, 0.18f, 1.025f, shade(T->trim, b, 0));
    DrawCube((Vector3){x, WALL_H - 0.07f, z}, 1.025f, 0.14f, 1.025f, shade(T->trim, b, 0));
    float fx = x + dx * 0.5f, fz = z + dz * 0.5f;           /* odaya bakan yuz */
    if (rm >= 0 && dist < 8.0f && b > 0.05f && (th == TH_LIVING || th == TH_BEDROOM || th == TH_STUDY) && !win && !pic) {
        Color sc = shade((Color){(unsigned char)(wc.r * 0.8f), (unsigned char)(wc.g * 0.8f), (unsigned char)(wc.b * 0.8f), 255}, b, 0);
        for (int k = -1; k <= 1; k++)                       /* duvar kagidi seritleri */
            DrawCube((Vector3){fx + (dz != 0 ? k * 0.3f : 0), 1.75f, fz + (dx != 0 ? k * 0.3f : 0)},
                     dx != 0 ? 0.012f : 0.06f, 1.2f, dx != 0 ? 0.06f : 0.012f, sc);
    }
    if (win) {
        Color glass = shade((Color){110, 140, 190, 255}, b, 0.28f);
        DrawCube((Vector3){fx, 1.55f, fz}, dx != 0 ? 0.03f : 0.8f, 1.0f, dx != 0 ? 0.8f : 0.03f, glass);
        DrawCube((Vector3){fx, 1.55f, fz}, dx != 0 ? 0.045f : 0.04f, 1.0f, dx != 0 ? 0.04f : 0.045f, shade(T->trim, b, 0));
        DrawCube((Vector3){fx, 1.55f, fz}, dx != 0 ? 0.045f : 0.8f, 0.04f, dx != 0 ? 0.8f : 0.045f, shade(T->trim, b, 0));
    } else if (pic && dist < 9.0f) {
        DrawCube((Vector3){fx, 1.6f, fz}, dx != 0 ? 0.04f : 0.56f, 0.76f, dx != 0 ? 0.56f : 0.04f, shade((Color){88, 66, 28, 255}, b, 0));
        unsigned char r = (unsigned char)(30 + (h >> 4) % 30), g = (unsigned char)(34 + (h >> 7) % 26), bl = (unsigned char)(30 + (h >> 10) % 26);
        unsigned char pc = cf->picChg[ty][tx];
        Color canvas = pc == 1 ? (Color){6, 6, 8, 255} : (pc == 2 ? (Color){92, 14, 14, 255} : (Color){r, g, bl, 255});
        DrawCube((Vector3){fx + dx * 0.006f, 1.6f, fz + dz * 0.006f}, dx != 0 ? 0.05f : 0.44f, 0.64f, dx != 0 ? 0.44f : 0.05f, shade(canvas, b, 0));
        if (pc == 1)                                          /* karanlik tabloda iki goz */
            for (int k = -1; k <= 1; k += 2)
                DrawCube((Vector3){fx + dx * 0.012f + (dz != 0 ? 0.08f * k : 0), 1.68f, fz + dz * 0.012f + (dx != 0 ? 0.08f * k : 0)},
                         dx != 0 ? 0.02f : 0.05f, 0.05f, dx != 0 ? 0.05f : 0.02f, (Color){230, 230, 215, 255});
        else if (pc == 2)                                     /* kan damlasi */
            DrawCube((Vector3){fx + dx * 0.012f, 1.48f, fz + dz * 0.012f}, dx != 0 ? 0.02f : 0.04f, 0.3f, dx != 0 ? 0.04f : 0.02f, shade((Color){150, 10, 10, 255}, b, 0.2f));
    }
}
static void draw_door(const Door *d, float b) {
    float x = d->tx + 0.5f, z = d->ty + 0.5f;
    Color fr = shade((Color){42, 28, 20, 255}, b, 0);
    Color wd = d->lock >= 0 ? shade((Color){62, 38, 34, 255}, b, 0) : shade((Color){98, 66, 40, 255}, b, 0);
    Color pn = shade((Color){d->lock >= 0 ? 48 : 78, 32, 24, 255}, b, 0), brass = shade((Color){190, 160, 70, 255}, b, 0.1f);
    DrawCube((Vector3){x, 2.25f, z}, 1.0f, 0.3f, 1.0f, shade((Color){60, 52, 50, 255}, b, 0));
    for (int s = -1; s <= 1; s += 2) {
        if (!d->alongX) DrawCube((Vector3){x + 0.45f * s, 1.05f, z}, 0.1f, 2.1f, 0.3f, fr);
        else DrawCube((Vector3){x, 1.05f, z + 0.45f * s}, 0.3f, 2.1f, 0.1f, fr);
    }
    float ang = (d->alongX ? -90.0f : 0.0f) + d->open * 95.0f;
    float hx = d->alongX ? x : x - 0.4f, hz = d->alongX ? z - 0.4f : z;
    rlPushMatrix();
    rlTranslatef(hx, 0, hz);
    rlRotatef(ang, 0, 1, 0);
    bool dtex = texOn(T_DOOR);
    if (dtex) emit_cube(tex[T_DOOR], (Vector3){0.4f, 1.02f, 0}, 0.8f, 2.04f, 0.06f,
                        shade(tintTex(d->lock >= 0 ? (Color){62, 38, 34, 255} : (Color){98, 66, 40, 255}), b, 0), 1, 1, 0, 0, 1, 1);
    else DrawCube((Vector3){0.4f, 1.02f, 0}, 0.8f, 2.04f, 0.06f, wd);
    for (int s = -1; s <= 1; s += 2) {
        if (!dtex) {
            DrawCube((Vector3){0.4f, 1.5f, 0.033f * s}, 0.56f, 0.7f, 0.012f, pn);
            DrawCube((Vector3){0.4f, 0.55f, 0.033f * s}, 0.56f, 0.7f, 0.012f, pn);
        }
        DrawCube((Vector3){0.7f, 1.0f, 0.055f * s}, 0.05f, 0.05f, 0.05f, brass);
        if (d->lock >= 0) DrawCube((Vector3){0.64f, 1.14f, 0.04f * s}, 0.07f, 0.1f, 0.02f, shade((Color){200, 40, 40, 255}, b, 0.45f));
    }
    rlPopMatrix();
}
static void draw_stairs(int tx, int ty, char c, float b) {
    float x = tx + 0.5f, z = ty + 0.5f;
    bool up = (c == 'U' || c == 'V'), north = (c == 'U' || c == 'D');
    const Color tread = {96, 66, 42, 255}, riser = {70, 46, 30, 255}, side = {44, 30, 22, 255};
    for (int s = 0; s < 8; s++) {
        int g = (north ? 8 : 0) + s;
        float zc = z + 0.5f - (s + 0.5f) * 0.125f;
        if (up) {
            float hgt = 0.15f * (g + 1);
            cubeT(T_WOOD, (Vector3){x, hgt / 2, zc}, 0.84f, hgt, 0.125f, (g & 1) ? tread : riser, b, 0);
            for (int k = -1; k <= 1; k += 2) cubeT(T_WOOD, (Vector3){x + 0.46f * k, (hgt + 0.35f) / 2, zc}, 0.08f, hgt + 0.35f, 0.125f, side, b, 0);
        } else {
            float yt = -0.15f * (g + 1);
            cubeT(T_WOOD, (Vector3){x, yt - 0.075f, zc}, 0.84f, 0.15f, 0.125f, (g & 1) ? tread : riser, b, 0);
            for (int k = -1; k <= 1; k += 2) cubeT(T_WOOD, (Vector3){x + 0.46f * k, -0.75f, zc}, 0.08f, 3.3f, 0.125f, side, b, 0);
        }
    }
    if (!up) for (int k = -1; k <= 1; k += 2) cubeT(T_WOOD, (Vector3){x + 0.46f * k, 0.9f, z}, 0.08f, 0.05f, 1.0f, tread, b, 0);
}
static bool rug_at(int tx, int ty, int *ring) {
    int rm = roomOf[ty][tx];
    if (rm < 0) return false;
    int th = roomTheme[rm];
    if (th != TH_LIVING && th != TH_BEDROOM && th != TH_LIBRARY && th != TH_STUDY) return false;
    Vector2 c = room_center(rm);
    int dx = abs(tx - tile_of(c.x)), dy = abs(ty - tile_of(c.y));
    if (dx > 1 || dy > 1) return false;
    *ring = (dx == 1 || dy == 1);
    return true;
}
static void draw_floor_tile(int tx, int ty, float b, float dist) {
    int rm = roomOf[ty][tx];
    const Theme *T = &themes[rm >= 0 ? roomTheme[rm] : TH_CORRIDOR];
    unsigned h = hsh(tx, ty);
    float x = tx + 0.5f, z = ty + 0.5f;
    char c = map[ty][tx];
    Color fc = ((tx + ty) & 1) ? T->floorA : T->floorB;
    if (T->floorType == 0) {
        int v = (int)(h & 7) * 2 - 7;
        fc = (Color){cb(T->floorA.r + v), cb(T->floorA.g + v), cb(T->floorA.b + v), 255};
    }
    bool hole = (c == 'D' || c == 'W');
    if (!hole) cubeT(T_FLOOR_WOOD + T->floorType, (Vector3){x, -0.05f, z}, 1, 0.1f, 1, fc, b, 0);
    if (T->floorType == 0 && dist < 8.0f && b > 0.05f && !hole && !texOn(T_FLOOR_WOOD)) {
        Color sc = {cb(fc.r * 0.45f), cb(fc.g * 0.45f), cb(fc.b * 0.45f), 255};
        bool alongZ = rm < 0 || (rm & 1);
        for (int k = -1; k <= 1; k += 2)
            DrawCube((Vector3){alongZ ? x + k * 0.17f : x, 0.003f, alongZ ? z : z + k * 0.17f}, alongZ ? 0.015f : 1.0f, 0.006f,
                     alongZ ? 1.0f : 0.015f, shade(sc, b, 0));
    }
    if (dist < 9.0f && b > 0.04f && !hole && c == '.') {    /* lekeler */
        if ((h >> 12) % 11 == 0)
            DrawCube((Vector3){x + ((int)((h >> 3) % 5) - 2) * 0.1f, 0.005f, z + ((int)((h >> 5) % 5) - 2) * 0.1f}, 0.35f, 0.004f, 0.3f, shade((Color){30, 22, 18, 255}, b, 0));
        else if ((h >> 14) % 61 == 0)
            DrawCube((Vector3){x, 0.006f, z}, 0.25f, 0.004f, 0.18f, shade((Color){90, 14, 14, 255}, b, 0));
    }
    int ring = 0;
    if (rug_at(tx, ty, &ring)) {
        Color rc = ring ? (Color){92, 26, 32, 255} : (Color){122, 36, 42, 255};
        if (texOn(T_RUG)) {                                 /* 3x3 hali: tek gorsel, karelere bolunur */
            Vector2 rcn = room_center(rm);
            int ddx = tx - tile_of(rcn.x), ddy = ty - tile_of(rcn.y);
            emit_cube(tex[T_RUG], (Vector3){x, 0.009f, z}, 1, 0.018f, 1, shade(tintTex(rc), b, 0), 1, 1,
                      (ddx + 1) / 3.0f, (ddy + 1) / 3.0f, (ddx + 2) / 3.0f, (ddy + 2) / 3.0f);
        } else DrawCube((Vector3){x, 0.009f, z}, 1, 0.018f, 1, shade(rc, b, 0));
    }
    cubeT(T_CEILING, (Vector3){x, WALL_H + 0.05f, z}, 1, 0.1f, 1, T->ceil, b * 0.8f, 0);
    if (rm >= 0 && tx % 3 == 0 && dist < 9.0f)              /* tavan kirisi */
        DrawCube((Vector3){x, WALL_H - 0.1f, z}, 0.22f, 0.2f, 1.0f, shade(T->trim, b * 0.8f, 0));
    if (c == 'L') {
        cubeT(T_WOOD, (Vector3){x, 0.95f, z}, 0.84f, 1.9f, 0.84f, (Color){110, 70, 42, 255}, b, 0);
        DrawCube((Vector3){x, 1.4f, z + 0.43f}, 0.4f, 0.07f, 0.02f, shade((Color){12, 8, 6, 255}, b, 0));
        DrawCube((Vector3){x, 1.25f, z + 0.43f}, 0.4f, 0.07f, 0.02f, shade((Color){12, 8, 6, 255}, b, 0));
    } else if (c == 'E') {
        Color k = (P.pages >= PAGES_NEEDED && POWER_ON) ? (Color){50, 210, 110, 255} : (Color){200, 50, 50, 255};
        DrawCube((Vector3){x, 1.1f, z}, 0.9f, 2.2f, 0.12f, shade(k, b, 0.35f));
    } else if (is_stair(c)) draw_stairs(tx, ty, c, b);
}

/* ---------- sahne ---------- */
static void draw_dust(void) {
    if (!cfg.fx) return;
    for (int i = 0; i < 40; i++) {
        unsigned h = hsh(i, 77);
        float ang = P.yaw + ((h & 255) / 255.0f - 0.5f) * 1.8f, r = 1.0f + ((h >> 8) & 255) / 255.0f * 7.0f;
        float x = P.pos.x + cosf(ang) * r + sinf(gtime * 0.3f + i) * 0.3f, z = P.pos.y + sinf(ang) * r + cosf(gtime * 0.27f + i) * 0.3f;
        float y = 0.3f + ((h >> 16) & 255) / 255.0f * 1.9f + sinf(gtime * 0.5f + i * 1.7f) * 0.1f;
        float l = tile_light(x, z);
        if (l < 0.1f) continue;
        DrawCube((Vector3){x, y, z}, 0.012f, 0.012f, 0.012f, (Color){210, 200, 170, (unsigned char)clampf(l * 170, 0, 110)});
    }
}
static void draw_player_marker(void) {                /* kamera goruntusunde oyuncunun yeri */
    gShadeD = 0; gWarm = 0;
    float x = camPlayerPos.x, z = camPlayerPos.y, pulse = 0.5f + 0.5f * sinf(gtime * 6.0f);
    Color c = {120, 220, 255, 255};
    DrawCylinder((Vector3){x, 0.0f, z}, 0.2f, 0.26f, 1.25f, 10, c);
    DrawSphere((Vector3){x, 1.45f, z}, 0.19f, c);
    DrawCube((Vector3){x + cosf(camPlayerYaw) * 0.45f, 1.1f, z + sinf(camPlayerYaw) * 0.45f}, 0.14f, 0.14f, 0.14f, (Color){255, 240, 120, 255});
    DrawCylinder((Vector3){x, 1.7f, z}, 0.015f, 0.015f, 0.5f, 6, (Color){255, 255, 255, 255});
    DrawSphere((Vector3){x, 2.2f, z}, 0.07f + 0.03f * pulse, (Color){255, 255, 255, 255});
}
static void draw_scene(void) {
    BeginMode3D(cam3);
    int px = tile_of(P.pos.x), pz = tile_of(P.pos.y);
    for (int ty = pz - VIEW_R; ty <= pz + VIEW_R; ty++)
        for (int tx = px - VIEW_R; tx <= px + VIEW_R; tx++) {
            if (oob(tx, ty)) continue;
            float dx = tx + 0.5f - P.pos.x, dz = ty + 0.5f - P.pos.y;
            float dist = sqrtf(dx * dx + dz * dz), b = lg[ty][tx];
            if (gCamMode && (dist > 11.5f || !in_view(dx, dz, dist))) continue;
            gShadeD = dist; gWarm = lw[ty][tx];
            if (map[ty][tx] == '#') draw_wall(tx, ty, b, dist);
            else if (b > 0.0f || dist <= 3.0f || gCamMode) {
                draw_floor_tile(tx, ty, b, dist);
                if (dmap[ty][tx] >= 0) draw_door(&doors[dmap[ty][tx]], b);
            }
        }

    for (int i = 0; i < nprops; i++) {
        const Prop *p = &props[i];
        float b = lg[p->ty][p->tx];
        int tx2 = p->tx - p->fz * (p->len - 1), ty2 = p->ty + p->fx * (p->len - 1);
        if (lg[ty2][tx2] > b) b = lg[ty2][tx2];
        float dist = vdist(P.pos, (Vector2){p->cx, p->cz});
        if (dist > VIEW_R + 1) continue;
        if (b <= 0.0f && dist > 3.0f && p->type != P_FIREPLACE && !gCamMode) continue;
        gShadeD = dist; gWarm = lw[p->ty][p->tx];
        draw_prop(p, b, dist);
    }

    for (int i = 0; i < nlights; i++) {                      /* tavan lambalari */
        const Light *L = &lights[i];
        if (L->kind != LK_LAMP || L->y < 1.8f) continue;
        float dist = vdist(P.pos, (Vector2){L->x, L->z});
        if (dist > VIEW_R + 2) continue;
        float lv = lamp_level(L, i);
        gShadeD = dist; gWarm = 0.5f;
        DrawCube((Vector3){L->x, 2.25f, L->z}, 0.02f, 0.3f, 0.02f, shade((Color){20, 18, 18, 255}, 0.1f, 0));
        DrawCylinder((Vector3){L->x, 1.88f, L->z}, 0.07f, 0.24f, 0.22f, 10, shade((Color){235, 205, 140, 255}, 0.1f, 0.15f + 0.8f * lv));
    }

    for (int i = 0; i < nitems; i++) {
        const Item *it = &items[i];
        if (it->taken || (it->prop >= 0 && props[it->prop].open < 0.4f)) continue;   /* kapali mobilyanin icindekiler gorunmez */
        Vector3 w = item_pos(it);
        float dist = vdist(P.pos, (Vector2){w.x, w.z});
        if (dist > VIEW_R + 2) continue;
        draw_item(it, tile_light(w.x, w.z), dist);
    }
    for (int i = 0; i < 4; i++)
        if (pj[i].on) DrawCube(pj[i].p, 0.07f, 0.22f, 0.07f, (Color){60, 170, 90, 255});

    for (int i = 0; i < NMON; i++) {
        const Monster *m = &mons[i];
        if (!m->active || m->lvl != P.lvl) continue;
        float mb = (gstate == ST_DEAD && jumpT > 0 && i == killer) ? 0.9f : tile_light(m->pos.x, m->pos.y);
        gAnimM = m;
        if (m->kind == K_WARDEN) draw_warden(m->pos, m->face, mb);
        else draw_statue(m->pos, m->face, mb);
    }
    gAnimM = NULL;
    if (hallT > 0 && !gCamMode) draw_statue(hallPos, atan2f(P.pos.y - hallPos.y, P.pos.x - hallPos.x), 0.05f);
    if (fakeT > 0 && !gCamMode) draw_statue(fakePos, atan2f(P.pos.y - fakePos.y, P.pos.x - fakePos.x), 0.5f);
    if (gstate == ST_DEAD && jumpT > 0 && killer < 0) draw_statue(jumpSrc, atan2f(P.pos.y - jumpSrc.y, P.pos.x - jumpSrc.x), 0.9f);
    if (!gCamMode) draw_dust();
    if (gCamMode && camPlayerLvl == P.lvl) draw_player_marker();
    EndMode3D();
}

/* ---------- son islem shader'i (tane, renk sapmasi, hafif blur, vinyet) ---------- */
static const char *postFS =
"#version 330\n"
"in vec2 fragTexCoord; in vec4 fragColor;\n"
"uniform sampler2D texture0; uniform vec4 colDiffuse;\n"
"uniform float uTime; uniform float uStress; uniform vec2 uRes;\n"
"out vec4 finalColor;\n"
"float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898,78.233))) * 43758.5453); }\n"
"void main(){\n"
"  vec2 uv = fragTexCoord;\n"
"  uv.x += sin(uv.y*40.0 + uTime*8.0) * 0.0015 * uStress;\n"
"  vec2 c = uv - 0.5;\n"
"  float ca = 0.0012 + 0.006*uStress;\n"
"  vec3 col;\n"
"  col.r = texture(texture0, uv + c*ca).r;\n"
"  col.g = texture(texture0, uv).g;\n"
"  col.b = texture(texture0, uv - c*ca).b;\n"
"  vec2 px = 1.0/uRes;\n"
"  vec3 b = (texture(texture0, uv+vec2(px.x,0.0)).rgb + texture(texture0, uv-vec2(px.x,0.0)).rgb + texture(texture0, uv+vec2(0.0,px.y)).rgb + texture(texture0, uv-vec2(0.0,px.y)).rgb)*0.25;\n"
"  col = mix(col, b, 0.10 + 0.25*uStress);\n"
"  col += max(b - 0.6, 0.0) * 0.35;\n"
"  col += (hash(uv*uRes + fract(uTime)*100.0) - 0.5) * (0.05 + 0.05*uStress);\n"
"  float v = smoothstep(0.95, 0.30, length(c)*1.2);\n"
"  col *= mix(0.6, 1.0, v);\n"
"  float l = dot(col, vec3(0.299,0.587,0.114));\n"
"  col = mix(col, vec3(l), 0.3*uStress);\n"
"  finalColor = vec4(col, 1.0) * colDiffuse * fragColor;\n"
"}\n";
static bool postActive;
static void init_post(void) {
    postSh = LoadShaderFromMemory(NULL, postFS);
    postOK = postSh.id != 0 && postSh.id != rlGetShaderIdDefault();
    if (postOK) { locTime = GetShaderLocation(postSh, "uTime"); locStress = GetShaderLocation(postSh, "uStress"); locRes = GetShaderLocation(postSh, "uRes"); }
}
static void begin_post(void) {
    postActive = cfg.post && postOK;
    if (!postActive) return;
    int w = SCREEN_W, h = SCREEN_H;
    if (postRT.id == 0 || postRT.texture.width != w || postRT.texture.height != h) {
        if (postRT.id) UnloadRenderTexture(postRT);
        postRT = LoadRenderTexture(w, h);
        if (postRT.id == 0) { postActive = false; return; }
    }
    BeginTextureMode(postRT);
    ClearBackground(BLACK);
}
static void end_post(float stress) {
    if (!postActive) return;
    EndTextureMode();
    float t = (float)GetTime(), res[2] = {(float)SCREEN_W, (float)SCREEN_H};
    SetShaderValue(postSh, locTime, &t, SHADER_UNIFORM_FLOAT);
    SetShaderValue(postSh, locStress, &stress, SHADER_UNIFORM_FLOAT);
    SetShaderValue(postSh, locRes, res, SHADER_UNIFORM_VEC2);
    BeginShaderMode(postSh);
    DrawTextureRec(postRT.texture, (Rectangle){0, 0, (float)postRT.texture.width, -(float)postRT.texture.height}, (Vector2){0, 0}, WHITE);
    EndShaderMode();
}
static float post_stress(void) {
    if (gstate == ST_DEAD && jumpT > 0) return 1.0f;
    return clampf(fmaxf(gProx * 0.9f, (1.0f - P.sanity / 100.0f) * 0.9f), 0, 1);
}

/* ---------- guvenlik kamerasi goruntusu ---------- */
static void render_cctv(int idx) {
    int lvl;
    const Cam *c = cam_get(idx, &lvl);
    Player sp = P;
    Floor *sc = cf;
    Camera3D scam = cam3;
    camPlayerPos = P.pos; camPlayerYaw = P.yaw; camPlayerLvl = P.lvl;
    P.lvl = lvl; cf = &FL[lvl];
    float base = atan2f(c->tz - c->z, c->tx - c->x);
    P.pos = (Vector2){c->x, c->z};
    P.yaw = base + 0.38f * sinf(gtime * 0.35f + idx);        /* kamera yavasca saga sola doner */
    P.hidden = true; P.torch = false;                        /* fener ve yakin cevre isigi yok */
    gCamMode = true;
    compute_light();
    float pitch = -0.4f;
    cam3.position = (Vector3){c->x, c->y, c->z};
    cam3.target = (Vector3){c->x + cosf(P.yaw) * cosf(pitch), c->y + sinf(pitch), c->z + sinf(P.yaw) * cosf(pitch)};
    cam3.up = (Vector3){0, 1, 0}; cam3.fovy = 80.0f;
    cctvCam = cam3;
    begin_post();
    draw_scene();
    end_post(0.25f);
    gCamMode = false;
    P = sp; cf = sc; cam3 = scam;
}

/* ---------- arayuz yardimcilari ---------- */
static Texture2D menuBg;
static bool hasBg, bgFull;                 /* bgFull: gorselde yazilar hazir (menu_full.png) */

static void load_textures(void) {
    for (int i = 0; i < T_COUNT; i++) {
        char nm[64];
        snprintf(nm, sizeof nm, "textures/%s", texFile[i]);
        const char *p = find_asset(nm);
        if (!p) continue;
        tex[i] = LoadTexture(p);
        if (tex[i].id == 0) continue;
        hasTex[i] = true;
        if (i < T_WARDEN) { GenTextureMipmaps(&tex[i]); SetTextureFilter(tex[i], TEXTURE_FILTER_TRILINEAR); SetTextureWrap(tex[i], TEXTURE_WRAP_REPEAT); }
        else { SetTextureFilter(tex[i], TEXTURE_FILTER_BILINEAR); SetTextureWrap(tex[i], TEXTURE_WRAP_CLAMP); }
    }
}
static Rectangle bg_rect(void) {
    float w = (float)SCREEN_W, h = (float)SCREEN_H;
    float sc = fmaxf(w / menuBg.width, h / menuBg.height);
    return (Rectangle){(w - menuBg.width * sc) / 2, (h - menuBg.height * sc) / 2, menuBg.width * sc, menuBg.height * sc};
}
static int menuSel, pauseSel, endSel;

static float uiS(void) { return clampf(SCREEN_H / 720.0f, 0.75f, 2.5f); }

static int menu_list(const char **lb, const bool *en, int n, float x, float y, float w, float ih, float size, bool center, int *sel) {
    Vector2 mp = GetMousePosition(), md = GetMouseDelta();
    bool moved = md.x != 0 || md.y != 0;
    int chosen = -1;
    if (IsKeyPressed(KEY_DOWN)) { do { *sel = (*sel + 1) % n; } while (!en[*sel]); }
    if (IsKeyPressed(KEY_UP)) { do { *sel = (*sel + n - 1) % n; } while (!en[*sel]); }
    if (!en[*sel]) { do { *sel = (*sel + 1) % n; } while (!en[*sel]); }
    for (int i = 0; i < n; i++) {
        Rectangle r = {x, y + i * ih, w, ih};
        bool hov = CheckCollisionPointRec(mp, r);
        if (hov && moved && en[i]) *sel = i;
        bool on = (*sel == i) && en[i];
        if (on) DrawRectangleGradientH((int)r.x, (int)r.y + 2, (int)r.width, (int)r.height - 4, (Color){150, 150, 160, 80}, (Color){150, 150, 160, 0});
        Color c = !en[i] ? (Color){85, 85, 90, 255} : (on ? (Color){245, 240, 230, 255} : (Color){185, 182, 180, 255});
        float ty = r.y + (ih - size) / 2;
        if (center) dcenter(lb[i], x + w / 2, ty, size, c); else dtext(lb[i], x + size * 1.6f, ty, size, c);
        if (en[i] && ((hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || (on && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))))) chosen = i;
    }
    if (chosen >= 0) play(S_UI, 0.5f, 1.0f);
    return chosen;
}

/* ---------- menu arka plani (kodla cizilir) ---------- */
static float hash01(int i) { return (float)(hsh(i, i * 7 + 3) & 1023) / 1023.0f; }
static void draw_branch(float x, float y, float len, float ang, int depth, float thick, Color c, int seed) {
    if (depth <= 0 || len < 3) return;
    float x2 = x + cosf(ang) * len, y2 = y + sinf(ang) * len;
    DrawLineEx((Vector2){x, y}, (Vector2){x2, y2}, thick, c);
    float a1 = ang - 0.3f - hash01(seed * 3 + 1) * 0.4f, a2 = ang + 0.3f + hash01(seed * 3 + 2) * 0.4f;
    draw_branch(x2, y2, len * 0.74f, a1, depth - 1, thick * 0.7f, c, seed * 2 + 1);
    draw_branch(x2, y2, len * 0.74f, a2, depth - 1, thick * 0.7f, c, seed * 2 + 2);
    if (depth > 3 && hash01(seed) > 0.5f) draw_branch(x2, y2, len * 0.6f, ang + (hash01(seed + 9) - 0.5f) * 0.7f, depth - 2, thick * 0.6f, c, seed * 5 + 3);
}
static bool cineDraw;
static void draw_menu_bg(void) {
    float w = (float)SCREEN_W, h = (float)SCREEN_H, s = h / 720.0f, t = (float)GetTime();
    if (hasBg) {
        Rectangle src = {0, 0, (float)menuBg.width, (float)menuBg.height};
        DrawTexturePro(menuBg, src, bg_rect(), (Vector2){0, 0}, 0, WHITE);
    } else {
        float hx = w * 0.62f, hy = h * 0.66f;
#define IX(v) ((int)(hx + (v) * s))
#define IY(v) ((int)(hy + (v) * s))
#define VX(v) (hx + (v) * s)
#define VY(v) (hy + (v) * s)
        DrawRectangleGradientV(0, 0, (int)w, (int)(h * 0.66f), (Color){7, 9, 15, 255}, (Color){30, 36, 48, 255});
        for (int i = 0; i < 10; i++) DrawCircle((int)(hx - 30 * s), (int)(h * 0.2f), (190 - i * 17) * s, (Color){120, 140, 180, 6});
        for (int i = 0; i < 7; i++) {
            float cx = fmodf(i * w * 0.2f + t * (5.0f + (i % 3) * 3.0f), w + 400) - 200;
            DrawEllipse((int)cx, (int)(h * 0.1f + (i % 4) * h * 0.07f), 230 * s, 36 * s, (Color){16, 20, 28, 140});
        }
        DrawRectangleGradientV(0, (int)(h * 0.64f), (int)w, (int)(h * 0.36f), (Color){7, 9, 11, 255}, (Color){2, 3, 4, 255});
        Color tc = {3, 4, 6, 255};
        draw_branch(w * 0.08f, h * 0.78f, 130 * s, -PI / 2, 7, 11 * s, tc, 1);
        draw_branch(w * 0.95f, h * 0.7f, 170 * s, -PI / 2 - 0.1f, 7, 13 * s, tc, 40);
        draw_branch(w * 0.43f, h * 0.65f, 70 * s, -PI / 2, 6, 5 * s, (Color){6, 8, 11, 255}, 80);
        draw_branch(w * 0.84f, h * 0.65f, 80 * s, -PI / 2 + 0.1f, 6, 6 * s, (Color){6, 8, 11, 255}, 120);
        /* ev */
        Color hc = {13, 15, 20, 255}, hc2 = {9, 11, 15, 255};
        DrawRectangle(IX(-180), IY(-140), (int)(360 * s), (int)(140 * s), hc);
        DrawTriangle((Vector2){VX(0), VY(-235)}, (Vector2){VX(-205), VY(-140)}, (Vector2){VX(205), VY(-140)}, hc2);
        DrawRectangle(IX(-45), IY(-270), (int)(90 * s), (int)(140 * s), hc);
        DrawTriangle((Vector2){VX(0), VY(-350)}, (Vector2){VX(-62), VY(-270)}, (Vector2){VX(62), VY(-270)}, hc2);
        DrawTriangle((Vector2){VX(-110), VY(-215)}, (Vector2){VX(-170), VY(-140)}, (Vector2){VX(-50), VY(-140)}, hc);
        DrawRectangle(IX(110), IY(-250), (int)(22 * s), (int)(70 * s), hc2);
        DrawRectangle(IX(-210), IY(-55), (int)(420 * s), (int)(8 * s), hc2);
        for (int i = 0; i < 8; i++) DrawRectangle(IX(-200 + i * 57), IY(-55), (int)(5 * s), (int)(55 * s), hc2);
        static const float wp[6][2] = {{-125, -110}, {-65, -110}, {55, -110}, {115, -110}, {-11, -235}, {-95, -40}};
        for (int i = 0; i < 6; i++) {
            bool litw = (i == 1 || i == 2 || i == 4);
            float fl = 0.78f + 0.22f * sinf(t * 3.0f + i * 2.0f);
            Color c = litw ? (Color){255, 200, 100, (unsigned char)(255 * fl)} : (Color){26, 32, 46, 255};
            if (litw) DrawRectangle(IX(wp[i][0] - 8), IY(wp[i][1] - 8), (int)(38 * s), (int)(50 * s), (Color){255, 190, 90, (unsigned char)(26 * fl)});
            DrawRectangle(IX(wp[i][0]), IY(wp[i][1]), (int)(22 * s), (int)(34 * s), c);
            DrawRectangle(IX(wp[i][0] + 10), IY(wp[i][1]), (int)(2 * s), (int)(34 * s), hc2);
        }
        DrawRectangle(IX(-34), IY(0), (int)(68 * s), (int)(5 * s), hc2);
        DrawRectangle(IX(-26), IY(5), (int)(52 * s), (int)(5 * s), hc2);
        /* yol, kapi ve cit */
        DrawTriangle((Vector2){VX(0), VY(8)}, (Vector2){VX(-w * 0.35f / s), h}, (Vector2){VX(w * 0.33f / s), h}, (Color){24, 28, 34, 255});
        for (int i = 0; i < 7; i++)
            DrawEllipse(IX((hash01(i + 3) - 0.5f) * 120 * (1 + i * 0.5f)), IY(55 + i * 42), (30 + i * 12) * s, (2 + i) * s, (Color){200, 210, 220, (unsigned char)(10 + i * 3)});
        Color fc = {6, 7, 10, 255}, sc2 = {20, 22, 27, 255};
        for (int side = -1; side <= 1; side += 2) {
            float px = hx + side * 255 * s;
            for (float x = px + side * 30 * s; x > -10 && x < w + 10; x += side * 15 * s) {
                DrawRectangle((int)x, IY(-55), (int)(3 * s), (int)(110 * s), fc);
                DrawTriangle((Vector2){x + 1.5f * s, hy - 70 * s}, (Vector2){x - 2.5f * s, hy - 54 * s}, (Vector2){x + 5.5f * s, hy - 54 * s}, fc);
            }
            DrawRectangle((int)(px - 21 * s), IY(-100), (int)(42 * s), (int)(150 * s), sc2);
            DrawRectangle((int)(px - 27 * s), IY(-112), (int)(54 * s), (int)(14 * s), (Color){28, 30, 36, 255});
            for (int k = 0; k < 7; k++) {
                float gx = px - side * (30 + k * 11) * s;
                DrawRectangle((int)gx, (int)(hy - (60 - k * 4) * s), (int)(3 * s), (int)((106 - k * 7) * s), fc);
            }
        }
    }
    /* yagmur, sis, vinyet */
    for (int i = 0; i < 110; i++) {
        float x = fmodf(hash01(i) * (w + 200) + t * 90, w + 200) - 100, y = fmodf(hash01(i + 500) * h + t * (700 + (i % 5) * 60), h);
        DrawLineEx((Vector2){x, y}, (Vector2){x - 4 * s, y + 16 * s}, 1, (Color){190, 200, 215, (unsigned char)(18 + i % 4 * 6)});
    }
    for (int k = 0; k < 3; k++) {
        float y = h * (0.6f + k * 0.08f);
        DrawRectangleGradientV(0, (int)y, (int)w, (int)(80 * s), (Color){120, 130, 145, 0}, (Color){120, 130, 145, 22});
        float cx = fmodf(t * (9.0f + k * 4) + k * 420, w + 500) - 250;
        DrawEllipse((int)cx, (int)(y + 50 * s), 300 * s, 22 * s, (Color){120, 130, 145, 14});
    }
    if (!bgFull && !cineDraw) DrawRectangleGradientH(0, 0, (int)(w * 0.5f), (int)h, (Color){0, 0, 0, 175}, (Color){0, 0, 0, 0});
    Color v0 = {0, 0, 0, 190}, v1 = {0, 0, 0, 0};
    int bx = (int)(w * 0.18f), by = (int)(h * 0.2f);
    DrawRectangleGradientH(0, 0, bx, (int)h, v0, v1);
    DrawRectangleGradientH((int)w - bx, 0, bx, (int)h, v1, v0);
    DrawRectangleGradientV(0, 0, (int)w, by, v0, v1);
    DrawRectangleGradientV(0, (int)h - by, (int)w, by, v1, v0);
#undef IX
#undef IY
#undef VX
#undef VY
}

static void draw_x_icon(float cx, float cy, float r, Color c) {
    DrawLineEx((Vector2){cx - r, cy - r}, (Vector2){cx + r, cy + r}, 2, c);
    DrawLineEx((Vector2){cx - r, cy + r}, (Vector2){cx + r, cy - r}, 2, c);
}
static void draw_gear_icon(float cx, float cy, float r, Color c) {
    DrawCircleLines((int)cx, (int)cy, r * 0.62f, c);
    DrawCircleLines((int)cx, (int)cy, r * 0.28f, c);
    for (int i = 0; i < 8; i++) {
        float a = i * PI / 4;
        DrawLineEx((Vector2){cx + cosf(a) * r * 0.62f, cy + sinf(a) * r * 0.62f}, (Vector2){cx + cosf(a) * r, cy + sinf(a) * r}, 3, c);
    }
}
static float cinePrev = -1;
static Texture2D introTex;
static int introLoaded = -1, introTry = -1;
/* MP4 sesini hazirlar. Goruntu artik diske binlerce PNG yazmak yerine
   FFmpeg pipe uzerinden RGB kareleri olarak akis halinde okunur. */
static void intro_convert_mp4(void) {
    const char *input = find_asset("intro/intro.mp4");
    if (!input) return;
#ifdef _WIN32
    if (system("ffmpeg -version > nul 2>&1") != 0) {
#else
    if (system("ffmpeg -version > /dev/null 2>&1") != 0) {
#endif
        TraceLog(LOG_WARNING, "intro/intro.mp4 bulundu ancak FFmpeg bulunamadi.");
        return;
    }
    char audio[900], cmd[2200];
    snprintf(audio, sizeof audio, "%sintro/audio.mp3", GetApplicationDirectory());
    snprintf(cmd, sizeof cmd,
        "ffmpeg -hide_banner -loglevel error -y -i \"%s\" -vn -codec:a libmp3lame -q:a 3 \"%s\"",
        input, audio);
    if (system(cmd) != 0) remove(audio);
}

/* MP4 varsa akis modu; yoksa onceki kareli intro dosyalari desteklenir. */
static void intro_scan(void) {
    introCount = 0;
    introStream = find_asset("intro/intro.mp4") != NULL;
    introFps = 60.0f;
    if (introStream) { introCount = 1; introJpg = false; return; }
    static const char *ext[2] = {".jpg", ".png"};
    for (int k = 0; k < 2 && !introCount; k++) {
        char p[160]; snprintf(p, sizeof p, "intro/frame_0001%s", ext[k]);
        if (!find_asset(p)) continue;
        introJpg = (k == 0);
        int n = 0;
        while (n < 20000) {
            snprintf(p, sizeof p, "intro/frame_%04d%s", n + 1, ext[k]);
            if (!find_asset(p)) break;
            n++;
        }
        introCount = n;
    }
    const char *fp = find_asset("intro/fps.txt");
    if (fp) { FILE *f = fopen(fp, "r"); if (f) { float v; if (fscanf(f, "%f", &v) == 1 && v >= 1 && v <= 120) introFps = v; fclose(f); } }
}
static void intro_begin(void) {
    static const char *ext[3] = {".ogg", ".mp3", ".wav"};
    introLoaded = introTry = -1; introAudio = false; introStreamEnded = false; introFrameTimer = 0;
    if (introStream) {
        const char *input = find_asset("intro/intro.mp4");
        char cmd[2400];
        if (input) {
            snprintf(cmd, sizeof cmd,
                "ffmpeg -hide_banner -loglevel error -i \"%s\" -vf \"fps=60,scale=%d:%d:force_original_aspect_ratio=decrease,pad=%d:%d:(ow-iw)/2:(oh-ih)/2\" -f rawvideo -pix_fmt rgb24 -",
                input, INTRO_W, INTRO_H, INTRO_W, INTRO_H);
#ifdef _WIN32
            introPipe = _popen(cmd, "rb");
#else
            introPipe = popen(cmd, "r");
#endif
        }
        if (introPipe) {
            introPixels = (unsigned char *)malloc(INTRO_FRAME_BYTES);
            if (!introPixels) { 
#ifdef _WIN32
                _pclose(introPipe);
#else
                pclose(introPipe);
#endif
                introPipe = NULL;
            }
        }
        if (!introPipe) { introStreamEnded = true; TraceLog(LOG_WARNING, "MP4 video akisi baslatilamadi."); }
    }
    for (int i = 0; i < 3 && audioOK && !introAudio; i++) {
        char p[64]; snprintf(p, sizeof p, "intro/audio%s", ext[i]);
        const char *f = find_asset(p); if (!f) continue;
        introMusic = LoadMusicStream(f);
        if (introMusic.frameCount > 0) { introAudio = true; SetMusicVolume(introMusic, 1.0f); PlayMusicStream(introMusic); }
    }
}
static void intro_end(void) {
    if (introAudio) { StopMusicStream(introMusic); UnloadMusicStream(introMusic); introAudio = false; }
    if (introPipe) {
#ifdef _WIN32
        _pclose(introPipe);
#else
        pclose(introPipe);
#endif
        introPipe = NULL;
    }
    free(introPixels); introPixels = NULL;
    if (introLoaded >= 0) { UnloadTexture(introTex); introLoaded = -1; }
}
static void start_play(bool fresh) {
    if (fresh) init_game();
    else if (!hasGame) return;
    gstate = ST_PLAY; DisableCursor(); skipMouse = true;
}
static void new_game(void) {                                 /* her yeni oyun sinematikle baslar */
    if (cfg.cine) { introMode = introCount > 0; gstate = ST_CINE; cineT = 0; cinePrev = -1; if (introMode) intro_begin(); }
    else start_play(true);
}
static void draw_main_menu(void) {
    float w = (float)SCREEN_W, h = (float)SCREEN_H, s = uiS();
    draw_menu_bg();
    if (bgFull) {                            /* gorselin ustundeki yazilar tiklanabilir alan olur */
        Rectangle d = bg_rect();
        static const char *none[5] = {"", "", "", "", ""};
        bool en[5] = {hasGame, true, true, true, true};
        float ih = d.height * 0.0635f, x = d.x + d.width * 0.12f, y0 = d.y + d.height * 0.4268f - ih / 2;
        if (!hasGame) DrawRectangle((int)x, (int)y0, (int)(d.width * 0.2f), (int)ih, (Color){0, 0, 0, 150});
        int ch = menu_list(none, en, 5, x, y0, d.width * 0.2f, ih, 20, false, &menuSel);
        Vector2 xc = {d.x + d.width * 0.146f, d.y + d.height * 0.85f}, gc = {d.x + d.width * 0.185f, d.y + d.height * 0.85f};
        float rr = d.height * 0.03f;
        bool hx = CheckCollisionPointCircle(GetMousePosition(), xc, rr), hg = CheckCollisionPointCircle(GetMousePosition(), gc, rr);
        if (hx) DrawCircleLines((int)xc.x, (int)xc.y, rr, (Color){255, 255, 255, 120});
        if (hg) DrawCircleLines((int)gc.x, (int)gc.y, rr, (Color){255, 255, 255, 120});
        if (ch == 0) start_play(false);
        else if (ch == 1) new_game();
        else if (ch == 2) gstate = ST_SETTINGS;
        else if (ch == 3) gstate = ST_CREDITS;
        else if (ch == 4) wantQuit = true;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { if (hx) wantQuit = true; else if (hg) gstate = ST_SETTINGS; }
        if (IsKeyPressed(KEY_ESCAPE)) wantQuit = true;
        return;
    }
    float tx = w * 0.075f, ty = h * 0.15f, ts = 112 * s;
    const char *title = "MALİKANE";
    float sp = ts * 0.04f, tw = MeasureTextEx(uiFont, U(title), ts, sp).x;
    dtext_sp(title, tx + 4 * s, ty + 5 * s, ts, sp, (Color){0, 0, 0, 200});
    dtext_sp(title, tx, ty, ts, sp, (Color){196, 198, 204, 255});
    dtext_sp(title, tx - 1.5f * s, ty - 1.5f * s, ts, sp, (Color){255, 255, 255, 50});
    float ly = ty + ts * 1.18f;
    DrawLineEx((Vector2){tx, ly}, (Vector2){tx + tw, ly}, 1, (Color){150, 150, 158, 120});
    DrawPoly((Vector2){tx + tw / 2, ly}, 4, 6 * s, 45.0f, (Color){170, 170, 178, 200});

    const char *lb[5] = {"DEVAM ET", "YENİ OYUN", "AYARLAR", "KREDİLER", "ÇIKIŞ"};
    bool en[5] = {hasGame, true, true, true, true};
    float ix = w * 0.1f, iy = h * 0.40f, ih = 63 * s;
    int ch = menu_list(lb, en, 5, ix, iy, w * 0.27f, ih, 25 * s, false, &menuSel);
    if (ch == 0) start_play(false);
    else if (ch == 1) new_game();
    else if (ch == 2) gstate = ST_SETTINGS;
    else if (ch == 3) gstate = ST_CREDITS;
    else if (ch == 4) wantQuit = true;

    float by = h * 0.85f, bx0 = w * 0.146f, bx1 = w * 0.186f;
    DrawLineEx((Vector2){w * 0.12f, h * 0.805f}, (Vector2){w * 0.285f, h * 0.805f}, 1, (Color){120, 120, 128, 90});
    DrawLineEx((Vector2){(bx0 + bx1) / 2, by - 14 * s}, (Vector2){(bx0 + bx1) / 2, by + 14 * s}, 1, (Color){120, 120, 128, 120});
    Vector2 mp = GetMousePosition();
    bool hx = CheckCollisionPointCircle(mp, (Vector2){bx0, by}, 20 * s), hg = CheckCollisionPointCircle(mp, (Vector2){bx1, by}, 20 * s);
    draw_x_icon(bx0, by, 11 * s, hx ? WHITE : (Color){170, 170, 175, 255});
    draw_gear_icon(bx1, by, 12 * s, hg ? WHITE : (Color){170, 170, 175, 255});
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { if (hx) wantQuit = true; else if (hg) gstate = ST_SETTINGS; }
    if (IsKeyPressed(KEY_ESCAPE)) wantQuit = true;
    dtext("Başlamak için bir seçeneğe tıkla", w * 0.1f + 25 * s, h * 0.93f, 15 * s, (Color){120, 120, 128, 255});
    if (!hasBg) dtext("Arka plan görseli bulunamadı (menu_full.png veya menu_bg.png)", w * 0.62f, h * 0.96f, 14 * s, (Color){90, 90, 98, 255});
}

/* ---------- ayarlar / krediler ---------- */
static bool slider(const char *label, float x, float y, float w, float *v, float lo, float hi, const char *valtxt, float s) {
    dtext(label, x, y, 22 * s, (Color){210, 206, 200, 255});
    float tx = x + w * 0.5f, tw = w * 0.38f, ty = y + 12 * s;
    DrawRectangle((int)tx, (int)ty, (int)tw, (int)(4 * s), (Color){70, 70, 78, 255});
    float f = (*v - lo) / (hi - lo);
    DrawRectangle((int)tx, (int)ty, (int)(tw * f), (int)(4 * s), (Color){190, 70, 60, 255});
    DrawCircle((int)(tx + tw * f), (int)(ty + 2 * s), 9 * s, (Color){225, 220, 210, 255});
    dtext(valtxt, tx + tw + 16 * s, y, 20 * s, (Color){170, 170, 176, 255});
    Vector2 mp = GetMousePosition();
    Rectangle hit = {tx - 10 * s, y - 4 * s, tw + 20 * s, 34 * s};
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, hit)) {
        float nv = lo + clampf((mp.x - tx) / tw, 0, 1) * (hi - lo);
        if (fabsf(nv - *v) > 0.0001f) { *v = nv; return true; }
    }
    return false;
}
static bool row_button(const char *label, const char *value, float x, float y, float w, float s) {
    Rectangle r = {x - 12 * s, y - 6 * s, w + 24 * s, 38 * s};
    bool hov = CheckCollisionPointRec(GetMousePosition(), r);
    if (hov) DrawRectangleGradientH((int)r.x, (int)r.y, (int)r.width, (int)r.height, (Color){150, 150, 160, 50}, (Color){150, 150, 160, 0});
    dtext(label, x, y, 22 * s, (Color){210, 206, 200, 255});
    dtext(value, x + w * 0.5f, y, 22 * s, hov ? WHITE : (Color){190, 186, 180, 255});
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
static void draw_settings(void) {
    float w = (float)SCREEN_W, h = (float)SCREEN_H, s = uiS();
    draw_menu_bg();
    DrawRectangle(0, 0, (int)w, (int)h, (Color){0, 0, 0, 160});
    dcenter("AYARLAR", w / 2, h * 0.07f, 54 * s, (Color){196, 198, 204, 255});
    float cw = 500 * s, lx = w / 2 - cw - 30 * s, rx = w / 2 + 30 * s, y0 = h * 0.2f, y = y0;
    char b[32];
    snprintf(b, sizeof b, "%.0f%%", cfg.sens * 100);
    slider("Fare hassasiyeti", lx, y, cw, &cfg.sens, 0.3f, 2.5f, b, s); y += 50 * s;
    snprintf(b, sizeof b, "%.0f%%", cfg.bright * 100);
    slider("Parlaklık", lx, y, cw, &cfg.bright, 0.0f, 1.0f, b, s); y += 50 * s;
    snprintf(b, sizeof b, "%.0f%%", cfg.vol * 100);
    if (slider("Ses seviyesi", lx, y, cw, &cfg.vol, 0.0f, 1.0f, b, s) && audioOK) SetMasterVolume(cfg.vol);
    y += 50 * s;
    snprintf(b, sizeof b, "%.0f°", cfg.fov);
    slider("Görüş açısı (FOV)", lx, y, cw, &cfg.fov, 60.0f, 100.0f, b, s); y += 62 * s;
    if (row_button("Tuş atama ve oyun kolu", "Değiştir", lx, y, cw, s)) { gstate = ST_KEYS; play(S_UI, 0.5f, 1); }
    float yl = y + 44 * s;
    y = y0;
    static const char *dn[3] = {"Kolay", "Normal", "Zor"};
    if (row_button("Zorluk (yeni oyun)", dn[cfg.diff], rx, y, cw, s)) { cfg.diff = (cfg.diff + 1) % 3; play(S_UI, 0.5f, 1); } y += 44 * s;
    if (row_button("Sis", cfg.fog ? "Açık" : "Kapalı", rx, y, cw, s)) { cfg.fog = !cfg.fog; play(S_UI, 0.5f, 1); } y += 44 * s;
    if (row_button("Efektler (toz, parazit)", cfg.fx ? "Açık" : "Kapalı", rx, y, cw, s)) { cfg.fx = !cfg.fx; play(S_UI, 0.5f, 1); } y += 44 * s;
    if (row_button("Jumpscare", cfg.jump ? "Açık" : "Kapalı", rx, y, cw, s)) { cfg.jump = !cfg.jump; play(S_UI, 0.5f, 1); } y += 44 * s;
    if (row_button("Gelişmiş görüntü (shader)", postOK ? (cfg.post ? "Açık" : "Kapalı") : "Desteklenmiyor", rx, y, cw, s)) { cfg.post = !cfg.post; play(S_UI, 0.5f, 1); } y += 44 * s;
    if (row_button("Başlangıç sinematiği", cfg.cine ? (introCount > 0 ? "Açık (kendi videon)" : "Açık") : "Kapalı", rx, y, cw, s)) { cfg.cine = !cfg.cine; play(S_UI, 0.5f, 1); } y += 44 * s;
    if (row_button("Tam ekran (F11)", IsWindowFullscreen() ? "Açık" : "Kapalı", rx, y, cw, s)) { ToggleFullscreen(); play(S_UI, 0.5f, 1); } y += 44 * s;
    y = fmaxf(y, yl) + 10 * s;
    if (bestTime > 0) {
        snprintf(b, sizeof b, "En iyi süre: %d:%02d", (int)bestTime / 60, (int)bestTime % 60);
        dcenter(b, w / 2, y, 20 * s, (Color){160, 160, 168, 255});
        y += 36 * s;
    }
    const char *lb[1] = {"GERİ"};
    bool en[1] = {true};
    int sel = 0;
    if (menu_list(lb, en, 1, w / 2 - 100 * s, y, 200 * s, 50 * s, 26 * s, true, &sel) == 0 || IsKeyPressed(KEY_ESCAPE)) { save_cfg(); gstate = ST_MENU; }
}

/* ---------- tus atama ekrani ---------- */
static int rebind = -1;
static void draw_keys(void) {
    float w = (float)SCREEN_W, h = (float)SCREEN_H, s = uiS();
    static const int defKeys[A_COUNT] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_LEFT_SHIFT, KEY_C, KEY_E, KEY_F, KEY_G, KEY_Q, KEY_TAB, KEY_M, KEY_V, KEY_SPACE};
    draw_menu_bg();
    DrawRectangle(0, 0, (int)w, (int)h, (Color){0, 0, 0, 170});
    dcenter("TUŞ ATAMA", w / 2, h * 0.07f, 54 * s, (Color){196, 198, 204, 255});
    bool wasRebind = rebind >= 0;
    Vector2 mp = GetMousePosition();
    for (int i = 0; i < A_COUNT; i++) {
        float x = w / 2 - 540 * s + (i / 7) * 560 * s, y = h * 0.2f + (i % 7) * 50 * s;
        Rectangle r = {x, y, 500 * s, 42 * s};
        bool hov = CheckCollisionPointRec(mp, r);
        if (hov || rebind == i) DrawRectangleGradientH((int)r.x, (int)r.y, (int)r.width, (int)r.height, (Color){150, 150, 160, 60}, (Color){150, 150, 160, 0});
        dtext(actName[i], x + 10 * s, y + 8 * s, 22 * s, (Color){210, 206, 200, 255});
        dtext(rebind == i ? "Bir tuşa bas..." : key_name(keymap[i]), x + 280 * s, y + 8 * s, 22 * s, rebind == i ? (Color){240, 200, 60, 255} : (Color){190, 186, 180, 255});
        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && rebind < 0) { rebind = i; while (GetKeyPressed()) {} }
    }
    if (rebind >= 0) {
        int k = GetKeyPressed();
        if (k) { if (k != KEY_ESCAPE) keymap[rebind] = k; rebind = -1; }
    }
    float y = h * 0.2f + 7 * 50 * s + 10 * s;
    dcenter("Ok tuşları ve fare tuşları (sol: fener, sağ: şişe) ek olarak çalışmaya devam eder.", w / 2, y, 17 * s, (Color){150, 150, 158, 255}); y += 26 * s;
    dcenter("Oyun kolu: sol çubuk yürü, sağ çubuk bak, A etkileşim, X fener, Y şişe, B eğil, RT koş, RB kibrit, LB not defteri, LT nefes, Geri: kamera, Start: duraklat", w / 2, y, 15 * s, (Color){150, 150, 158, 255}); y += 40 * s;
    const char *lb[2] = {"VARSAYILANA DÖN", "GERİ"};
    bool en[2] = {true, true};
    static int sel;
    int ch = menu_list(lb, en, 2, w / 2 - 170 * s, y, 340 * s, 52 * s, 24 * s, true, &sel);
    if (ch == 0) for (int i = 0; i < A_COUNT; i++) keymap[i] = defKeys[i];
    if (ch == 1 || (IsKeyPressed(KEY_ESCAPE) && !wasRebind)) { save_cfg(); gstate = ST_SETTINGS; rebind = -1; }
}

/* ---------- ilk acilis: parlaklik ayari ---------- */
static void draw_calib(void) {
    float w = (float)SCREEN_W, h = (float)SCREEN_H, s = uiS();
    dcenter("PARLAKLIK AYARI", w / 2, h * 0.08f, 48 * s, (Color){150, 150, 158, 255});
    dcenter("Aşağıdaki simge zar zor görünene kadar kaydırıcıyı ayarla.", w / 2, h * 0.08f + 66 * s, 22 * s, (Color){120, 120, 128, 255});
    dcenter("Çok karanlık hissedersen biraz daha yukarı çek. Daha sonra Ayarlar'dan değiştirebilirsin.", w / 2, h * 0.08f + 96 * s, 18 * s, (Color){100, 100, 108, 255});
    int v = (int)(255 * clampf(amb() * 4.0f, 0, 1));
    Color ec = {(unsigned char)v, (unsigned char)v, (unsigned char)(v + 3 > 255 ? 255 : v + 3), 255};
    float cx = w / 2, cy = h * 0.46f;
    for (int i = 0; i < 3; i++) DrawCircleLines((int)cx, (int)cy, 120 * s + i, ec);
    DrawPoly((Vector2){cx, cy}, 6, 84 * s, 0, ec);
    DrawPoly((Vector2){cx, cy}, 6, 72 * s, 0, BLACK);
    DrawEllipse((int)cx, (int)cy, 46 * s, 20 * s, ec);
    DrawCircle((int)cx, (int)cy, 11 * s, BLACK);
    char b[32];
    snprintf(b, sizeof b, "%.0f%%", cfg.bright * 100);
    slider("Parlaklık", w / 2 - 300 * s, h * 0.75f, 600 * s, &cfg.bright, 0.0f, 1.0f, b, s);
    const char *lb[1] = {"TAMAM"};
    bool en[1] = {true};
    int sel = 0;
    if (menu_list(lb, en, 1, w / 2 - 100 * s, h * 0.83f, 200 * s, 50 * s, 26 * s, true, &sel) == 0) { save_cfg(); firstRun = false; gstate = ST_MENU; }
}

/* ---------- baslangic sinematigi (tamamen kodla) ---------- */
static int utf8_len(const char *s) { int c = 0; for (; *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) c++; return c; }
static int utf8_prefix(const char *s, int n) {
    int i = 0, c = 0;
    while (s[i] && c < n) { i++; while (((unsigned char)s[i] & 0xC0) == 0x80) i++; c++; }
    return i;
}
/* kelime kaydirmali yazi; visible: gosterilecek karakter sayisi (-1: hepsi). Cizilen satir sayisini dondurur */
static int draw_wrapped(const char *txt, float x, float y, float maxW, float size, float lineH, Color col, int visible) {
    char line[256];
    const char *p = txt;
    int shown = 0, lines = 0;
    while (*p) {
        int n = 0;
        line[0] = 0;
        const char *q = p;
        while (*q) {
            const char *e = q;
            while (*e && *e != ' ') e++;
            int wl = (int)(e - q), tl = n ? n + 1 + wl : wl;
            if (tl >= 254) break;
            char trial[256];
            memcpy(trial, line, (size_t)n);
            if (n) trial[n] = ' ';
            memcpy(trial + (n ? n + 1 : 0), q, (size_t)wl);
            trial[tl] = 0;
            if (n && twidth(trial, size) > maxW) break;
            memcpy(line, trial, (size_t)tl + 1); n = tl;
            q = e;
            while (*q == ' ') q++;
        }
        if (n == 0) break;
        int cp = utf8_len(line);
        int vis = visible < 0 ? cp : (int)fminf((float)cp, (float)(visible - shown));
        if (vis > 0) {
            char tmp[256];
            int bl = utf8_prefix(line, vis);
            memcpy(tmp, line, (size_t)bl); tmp[bl] = 0;
            dtext(tmp, x, y, size, col);
        }
        shown += cp; y += lineH; lines++; p = q;
        if (visible >= 0 && shown >= visible) break;
    }
    return lines;
}
/* kendi videon: kareleri sirayla yukler ve ortalayarak cizer */
static bool intro_read_exact(FILE *f, unsigned char *dst, size_t bytes) {
    size_t got = 0;
    while (got < bytes) { size_t n = fread(dst + got, 1, bytes - got, f); if (!n) return false; got += n; }
    return true;
}
static void draw_intro_video(float dt) {
    float W = (float)SCREEN_W, H = (float)SCREEN_H, s = uiS();
    cineT += dt;
    bool skip = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
    if (skip) { intro_end(); cineT = 0; start_play(true); return; }
    if (introAudio) UpdateMusicStream(introMusic);

    if (introStream) {
        introFrameTimer += dt;
        while (introFrameTimer >= (1.0f / 60.0f) && !introStreamEnded) {
            introFrameTimer -= (1.0f / 60.0f);
            if (!introPipe || !introPixels || !intro_read_exact(introPipe, introPixels, INTRO_FRAME_BYTES)) {
                introStreamEnded = true; break;
            }
            if (introLoaded < 0) {
                Image im = { introPixels, INTRO_W, INTRO_H, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8 };
                introTex = LoadTextureFromImage(im);
                if (introTex.id > 0) introLoaded = 0;
            } else UpdateTexture(introTex, introPixels);
        }
        if (introStreamEnded) { intro_end(); cineT = 0; start_play(true); return; }
    } else {
        int fi = (int)(cineT * introFps);
        if (fi >= introCount) { intro_end(); cineT = 0; start_play(true); return; }
        if (fi != introTry) {
            introTry = fi; char p[160];
            snprintf(p, sizeof p, "intro/frame_%04d%s", fi + 1, introJpg ? ".jpg" : ".png");
            const char *f = find_asset(p);
            if (f) { Texture2D t = LoadTexture(f); if (t.id > 0) { if (introLoaded >= 0) UnloadTexture(introTex); introTex = t; introLoaded = fi; } }
        }
    }
    if (introLoaded >= 0) {
        float sc = fminf(W / introTex.width, H / introTex.height);
        Rectangle src = {0, 0, (float)introTex.width, (float)introTex.height};
        Rectangle dst = {(W - introTex.width * sc) / 2, (H - introTex.height * sc) / 2, introTex.width * sc, introTex.height * sc};
        DrawTexturePro(introTex, src, dst, (Vector2){0, 0}, 0, WHITE);
    }
    if (cineT < 0.5f) DrawRectangle(0, 0, (int)W, (int)H, Fade(BLACK, 1.0f - cineT / 0.5f));
    dtext("Atlamak için: Boşluk / tık", W - twidth("Atlamak için: Boşluk / tık", 16 * s) - 24 * s, H * 0.95f, 16 * s, Fade(RAYWHITE, 0.4f));
}
static void draw_cinematic(float dt) {
    if (introMode) { draw_intro_video(dt); return; }
    static const char *cl[5] = {
        "1987. Üç gün önce kız kardeşin Elif, dedenin terk edilmiş malikanesine girdi ve bir daha çıkmadı.",
        "Geride tek bir ipucu bıraktı: dedenin deney defterinden yırtılmış altı sayfa.",
        "Sayfalar evin içine dağılmış. Çıkış kapısı ancak altısı bir araya gelince ve elektrik geri gelince açılıyor.",
        "Ama malikane boş değil: kör ama her sesi duyan bir Bekçi ve sen bakmadığında yaklaşan bir Heykel var.",
        "Sayfaları bul. Elektriği geri getir. Elif'i bul... ve sağ çık."};
    static const float st[5] = {4.5f, 10.2f, 15.6f, 21.2f, 27.2f}, du[5] = {5.4f, 4.6f, 5.2f, 5.8f, 4.6f};
    static const float thunder[4] = {6.5f, 12.2f, 19.8f, 27.5f};
    const float total = 34.0f;
    float W = (float)SCREEN_W, H = (float)SCREEN_H, s = uiS(), sb = H / 720.0f;
    cineT += dt;
    float t = cineT;
    bool skip = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
    if (skip || t >= total) { layer(S_RAIN, 0); cineT = 0; start_play(true); return; }
    layer(S_RAIN, 0.5f * fminf(1.0f, t / 2.0f));
    /* sesler */
    if (cinePrev < 0.2f && t >= 0.2f) play(S_PIANO, 0.35f, 0.89f);
    for (int i = 0; i < 4; i++) if (cinePrev < thunder[i] && t >= thunder[i]) play(S_THUNDER, 0.8f, 1.0f);
    for (int i = 0; i < 5; i++) if (cinePrev < st[i] && t >= st[i]) play(S_TYPE, 0.3f, 1.0f);
    if (cinePrev < 31.0f && t >= 31.0f) play(S_CREAK, 0.7f, 0.9f);
    if (cinePrev < 32.6f && t >= 32.6f) play(S_STING, 0.5f, 0.8f);
    cinePrev = t;
    /* kamera: evin yanik penceresine dogru yavas yaklasma */
    float k = clampf(t / 31.0f, 0, 1);
    k = k * k * (3.0f - 2.0f * k);
    Vector2 tgt = {W * 0.62f + (-65 + 11) * sb, H * 0.66f + (-110 + 17) * sb};
    Camera2D c2 = {(Vector2){W / 2, H / 2}, (Vector2){lerpf(W / 2, tgt.x, k), lerpf(H / 2, tgt.y, k)}, 0.0f, lerpf(1.0f, 3.4f, k)};
    bool sh = hasBg;
    hasBg = false;                                          /* sinematik her zaman kodla cizilen sahneyi kullanir */
    cineDraw = true;
    BeginMode2D(c2);
    draw_menu_bg();
    EndMode2D();
    cineDraw = false;
    hasBg = sh;
    /* simsek */
    for (int i = 0; i < 4; i++) {
        float x = t - thunder[i];
        if (x > -0.05f && x < 1.2f) DrawRectangle(0, 0, (int)W, (int)H, (Color){235, 240, 255, (unsigned char)(clampf(0.85f * expf(-fmaxf(x, 0) * 5.0f) * (0.7f + 0.3f * sinf(x * 60.0f)), 0, 1) * 255)});
    }
    Color v0 = {0, 0, 0, 215}, v1 = {0, 0, 0, 0};
    int bx = (int)(W * 0.25f), by = (int)(H * 0.25f);
    DrawRectangleGradientH(0, 0, bx, (int)H, v0, v1);
    DrawRectangleGradientH((int)W - bx, 0, bx, (int)H, v1, v0);
    DrawRectangleGradientV(0, 0, (int)W, by, v0, v1);
    DrawRectangleGradientV(0, (int)H - by, (int)W, by, v1, v0);
    /* baslik */
    if (t > 0.8f && t < 4.6f) {
        float a = clampf(fminf((t - 0.8f) / 1.0f, (4.6f - t) / 1.0f), 0, 1);
        dcenter("KARANLIK MALİKANE", W / 2, H * 0.4f, 64 * s, Fade((Color){200, 202, 208, 255}, a));
        dcenter("1987", W / 2, H * 0.4f + 80 * s, 28 * s, Fade((Color){170, 120, 110, 255}, a));
    }
    /* altyazi */
    DrawRectangleGradientV(0, (int)(H * 0.70f), (int)W, (int)(H * 0.2f), (Color){0, 0, 0, 0}, (Color){0, 0, 0, 190});
    for (int i = 0; i < 5; i++) {
        if (t < st[i] || t > st[i] + du[i] + 0.8f) continue;
        float a = clampf((st[i] + du[i] + 0.8f - t) / 0.8f, 0, 1);
        draw_wrapped(cl[i], W * 0.15f, H * 0.76f, W * 0.7f, 30 * s, 38 * s, Fade((Color){232, 226, 214, 255}, a), (int)((t - st[i]) * 32.0f));
    }
    /* sinema bantlari, giris/cikis karartmasi */
    DrawRectangle(0, 0, (int)W, (int)(H * 0.09f), BLACK);
    DrawRectangle(0, (int)(H * 0.91f), (int)W, (int)H, BLACK);
    float fa = t < 2.0f ? 1.0f - t / 2.0f : (t > total - 2.5f ? (t - (total - 2.5f)) / 2.5f : 0.0f);
    if (fa > 0) DrawRectangle(0, 0, (int)W, (int)H, Fade(BLACK, clampf(fa, 0, 1)));
    dtext("Atlamak için: Boşluk / tık", W - twidth("Atlamak için: Boşluk / tık", 16 * s) - 24 * s, H * 0.93f, 16 * s, Fade(RAYWHITE, 0.45f));
}

static void draw_credits(void) {
    float w = (float)SCREEN_W, h = (float)SCREEN_H, s = uiS();
    draw_menu_bg();
    DrawRectangle(0, 0, (int)w, (int)h, (Color){0, 0, 0, 160});
    dcenter("KREDİLER", w / 2, h * 0.2f, 54 * s, (Color){196, 198, 204, 255});
    dcenter("MALİKANE 3D", w / 2, h * 0.36f, 30 * s, (Color){220, 200, 160, 255});
    dcenter("raylib ile C dilinde yazıldı", w / 2, h * 0.43f, 22 * s, (Color){180, 180, 186, 255});
    dcenter("Ses efektleri ve müzik oyun içinde kodla üretilir", w / 2, h * 0.49f, 22 * s, (Color){180, 180, 186, 255});
    dcenter("Teşekkürler ve iyi şanslar... Bekçi dinliyor, Heykel seni bekliyor.", w / 2, h * 0.58f, 22 * s, (Color){180, 90, 80, 255});
    const char *lb[1] = {"GERİ"};
    bool en[1] = {true};
    int sel = 0;
    if (menu_list(lb, en, 1, w / 2 - 100 * s, h * 0.72f, 200 * s, 50 * s, 26 * s, true, &sel) == 0 || IsKeyPressed(KEY_ESCAPE)) gstate = ST_MENU;
}

/* ---------- oyun ici arayuz ---------- */
static void bar(int x, int y, int w, int h, float frac, Color c, const char *label) {
    DrawRectangle(x, y, w, h, (Color){20, 20, 24, 200});
    DrawRectangle(x, y, (int)(w * clampf(frac, 0, 1)), h, c);
    DrawRectangleLines(x, y, w, h, (Color){120, 120, 130, 255});
    dtext(label, (float)(x + w + 8), (float)(y - 2), 16, RAYWHITE);
}
static void draw_plan(int f, float x0, float y0, float cell, int selCam) {
    Floor *save = cf;
    cf = &FL[f];
    float s = uiS(), mw = MAP_W * cell, mh = MAP_H * cell;
    DrawRectangle((int)x0 - 4, (int)y0 - 4, (int)mw + 8, (int)mh + 8, (Color){0, 0, 0, 150});
    DrawRectangleLines((int)x0 - 4, (int)y0 - 4, (int)mw + 8, (int)mh + 8, (Color){100, 100, 110, 200});
    int cs = (int)ceilf(cell);
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            if (!cf->seeng[y][x]) continue;
            char c = map[y][x];
            Color col = {64, 62, 72, 255};
            int di = dmap[y][x];
            if (c == '#') col = (Color){34, 34, 42, 255};
            else if (di >= 0) col = doors[di].lock >= 0 ? (Color){200, 50, 50, 255} : (Color){150, 100, 50, 255};
            else if (is_stair(c)) col = (Color){230, 200, 70, 255};
            else if (c == 'E') col = (P.pages >= PAGES_NEEDED && POWER_ON) ? (Color){60, 220, 110, 255} : (Color){210, 60, 60, 255};
            else if (c == 'L') col = (Color){120, 80, 46, 255};
            DrawRectangle((int)(x0 + x * cell), (int)(y0 + y * cell), cs, cs, col);
        }
    int base = f == 0 ? 0 : FL[0].ncm;
    for (int i = 0; i < cf->ncm; i++) {                        /* kameralar */
        const Cam *c = &cf->cm[i];
        bool sel = base + i == selCam;
        float cx = x0 + c->x * cell, cy = y0 + c->z * cell;
        if (sel) DrawLineEx((Vector2){cx, cy}, (Vector2){x0 + c->tx * cell, y0 + c->tz * cell}, 1.5f, (Color){255, 230, 80, 160});
        DrawRectangle((int)(cx - 2 * s), (int)(cy - 2 * s), (int)(4 * s), (int)(4 * s), sel ? (Color){255, 230, 80, 255} : (Color){80, 200, 255, 255});
    }
    if (P.lvl == f) {                                          /* sen */
        float px = x0 + P.pos.x * cell, py = y0 + P.pos.y * cell;
        DrawCircle((int)px, (int)py, 3 * s, WHITE);
        DrawLineEx((Vector2){px, py}, (Vector2){px + cosf(P.yaw) * 9 * s, py + sinf(P.yaw) * 9 * s}, 2, (Color){255, 220, 120, 255});
    }
    cf = save;
}
static void draw_minimap(void) {
    float s = uiS(), cell = 3.0f * s;
    float x0 = SCREEN_W - MAP_W * cell - 16 * s, y0 = 16 * s;
    draw_plan(P.lvl, x0, y0, cell, -1);
    dtext(P.lvl == 0 ? "ZEMİN KAT" : "ÜST KAT", x0, y0 + MAP_H * cell + 6 * s, 16 * s, (Color){200, 200, 208, 255});
}

/* ---------- guvenlik kameralari ekrani ---------- */
static void cctv_open(void) { showCctv = true; EnableCursor(); }
static void cctv_close(void) { showCctv = false; DisableCursor(); skipMouse = true; }
static Rectangle cam_box(int i, int total, float s) {
    float bw = 44 * s, gap = 8 * s, tw = total * (bw + gap) - gap;
    return (Rectangle){(SCREEN_W - tw) / 2 + i * (bw + gap), SCREEN_H - 78 * s, bw, bw};
}
static void draw_cctv(void) {
    int W = SCREEN_W, H = SCREEN_H, total = cam_total();
    float s = uiS();
    if (total <= 0) { cctv_close(); return; }
    camSel = ((camSel % total) + total) % total;
    int lvl;
    const Cam *c = cam_get(camSel, &lvl);
    render_cctv(camSel);

    /* parazit: canavar kameraya yaklasinca goruntu bozulur */
    float intf = 0;
    for (int i = 0; i < NMON; i++) {
        if (!mons[i].active || mons[i].lvl != lvl) continue;
        float d = vdist(mons[i].pos, (Vector2){c->x, c->z});
        if (d < 7.0f) intf = fmaxf(intf, 1.0f - d / 7.0f);
    }
    DrawRectangle(0, 0, W, H, (Color){0, 50, 15, 45});
    for (int y = 0; y < H; y += 3) DrawRectangle(0, y, W, 1, (Color){0, 0, 0, 45});
    int nn = 60 + (int)(intf * 400);
    for (int i = 0; i < nn; i++)
        DrawRectangle(GetRandomValue(0, W), GetRandomValue(0, H), GetRandomValue(2, 40 + (int)(intf * 200)), 1 + GetRandomValue(0, 1), Fade(RAYWHITE, 0.06f + intf * 0.12f));
    if (intf > 0.2f && GetRandomValue(0, 100) < 15 + (int)(intf * 50))
        DrawRectangle(0, GetRandomValue(0, H), W, GetRandomValue(4, 30), (Color){120, 255, 150, (unsigned char)(30 + intf * 60)});
    Color v0 = {0, 0, 0, 200}, v1 = {0, 0, 0, 0};
    int bx = W / 6, by = H / 6;
    DrawRectangleGradientH(0, 0, bx, H, v0, v1);
    DrawRectangleGradientH(W - bx, 0, bx, H, v1, v0);
    DrawRectangleGradientV(0, 0, W, by, v0, v1);
    DrawRectangleGradientV(0, H - by, W, by, v1, v0);
    Color g = {120, 255, 150, 255};
    float m = 22 * s, L = 34 * s;                              /* kose isaretleri */
    DrawLineEx((Vector2){m, m}, (Vector2){m + L, m}, 2, g); DrawLineEx((Vector2){m, m}, (Vector2){m, m + L}, 2, g);
    DrawLineEx((Vector2){W - m, m}, (Vector2){W - m - L, m}, 2, g); DrawLineEx((Vector2){W - m, m}, (Vector2){W - m, m + L}, 2, g);
    DrawLineEx((Vector2){m, H - m}, (Vector2){m + L, H - m}, 2, g); DrawLineEx((Vector2){m, H - m}, (Vector2){m, H - m - L}, 2, g);
    DrawLineEx((Vector2){W - m, H - m}, (Vector2){W - m - L, H - m}, 2, g); DrawLineEx((Vector2){W - m, H - m}, (Vector2){W - m, H - m - L}, 2, g);

    /* bilgi yazilari */
    char b[128];
    snprintf(b, sizeof b, "KAM %d/%d  -  %s (%s)", camSel + 1, total, thName[FL[lvl].th[c->room]], lvl == 0 ? "Zemin Kat" : "Üst Kat");
    dtext(b, 40 * s, 36 * s, 24 * s, g);
    if (((int)(gtime * 2.0f)) & 1) DrawCircle((int)(52 * s), (int)(84 * s), 7 * s, (Color){230, 30, 30, 255});
    dtext("REC", 66 * s, 72 * s, 22 * s, (Color){230, 60, 60, 255});
    int t = (int)runTime;
    snprintf(b, sizeof b, "%02d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60);
    dtext(b, 40 * s, 104 * s, 20 * s, g);
    int pr = FL[P.lvl].roomg[tile_of(P.pos.y)][tile_of(P.pos.x)];
    snprintf(b, sizeof b, "Konumun: %s (%s)", pr >= 0 ? thName[FL[P.lvl].th[pr]] : "Koridor", P.lvl == 0 ? "Zemin Kat" : "Üst Kat");
    dtext(b, 40 * s, 132 * s, 20 * s, (Color){120, 220, 255, 255});
    if (intf > 0.5f) dtext("SİNYAL ZAYIF", W / 2 - twidth("SİNYAL ZAYIF", 26 * s) / 2, 40 * s, 26 * s, (Color){255, 80, 60, (unsigned char)(120 + 120 * ((int)(gtime * 6) & 1))});

    /* oyuncunun ekrandaki isareti */
    if (camPlayerLvl == lvl) {
        Vector3 pp = {camPlayerPos.x, 2.4f, camPlayerPos.y};
        Vector3 f = {cctvCam.target.x - cctvCam.position.x, cctvCam.target.y - cctvCam.position.y, cctvCam.target.z - cctvCam.position.z};
        Vector3 dv = {pp.x - cctvCam.position.x, pp.y - cctvCam.position.y, pp.z - cctvCam.position.z};
        if (f.x * dv.x + f.y * dv.y + f.z * dv.z > 0) {
            Vector2 sp = GetWorldToScreen(pp, cctvCam);
            if (sp.x > 0 && sp.x < W && sp.y > 0 && sp.y < H) {
                float tw = twidth("SEN", 22 * s);
                DrawRectangle((int)(sp.x - tw / 2 - 8 * s), (int)(sp.y - 26 * s), (int)(tw + 16 * s), (int)(28 * s), (Color){0, 0, 0, 170});
                DrawRectangleLines((int)(sp.x - tw / 2 - 8 * s), (int)(sp.y - 26 * s), (int)(tw + 16 * s), (int)(28 * s), (Color){120, 220, 255, 255});
                dtext("SEN", sp.x - tw / 2, sp.y - 24 * s, 22 * s, (Color){120, 220, 255, 255});
            }
        }
    }

    /* plan: kamera konumlari ve sen */
    float cell = 3.0f * s, px0 = W - MAP_W * cell - 24 * s, py0 = 36 * s;
    draw_plan(lvl, px0, py0, cell, camSel);
    dtext(lvl == 0 ? "ZEMİN KAT PLANI" : "ÜST KAT PLANI", px0, py0 + MAP_H * cell + 8 * s, 16 * s, g);

    /* kamera secimi: kutular, oklar, fare */
    Vector2 mp = GetMousePosition();
    int hit = -1, old = camSel;
    for (int i = 0; i < total; i++) {
        Rectangle r = cam_box(i, total, s);
        bool hov = CheckCollisionPointRec(mp, r), sel = i == camSel;
        DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, sel ? (Color){40, 120, 60, 220} : (Color){0, 0, 0, 170});
        DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, hov || sel ? g : (Color){70, 130, 90, 255});
        snprintf(b, sizeof b, "%d", i + 1);
        dcenter(b, r.x + r.width / 2, r.y + r.height / 2 - 11 * s, 22 * s, sel ? WHITE : g);
        if (hov) hit = i;
    }
    Rectangle first = cam_box(0, total, s), last = cam_box(total - 1, total, s);
    Rectangle lb = {first.x - 56 * s, first.y, 44 * s, 44 * s}, rb = {last.x + last.width + 12 * s, last.y, 44 * s, 44 * s};
    bool hl = CheckCollisionPointRec(mp, lb), hr = CheckCollisionPointRec(mp, rb);
    DrawRectangleLines((int)lb.x, (int)lb.y, (int)lb.width, (int)lb.height, hl ? g : (Color){70, 130, 90, 255});
    DrawRectangleLines((int)rb.x, (int)rb.y, (int)rb.width, (int)rb.height, hr ? g : (Color){70, 130, 90, 255});
    dcenter("<", lb.x + lb.width / 2, lb.y + 8 * s, 26 * s, g);
    dcenter(">", rb.x + rb.width / 2, rb.y + 8 * s, 26 * s, g);
    dcenter("Tıkla / tekerlek: kamera değiştir     1-9: seç     V veya ESC: kapat     (Kamera izlerken hareket edemezsin!)", W / 2.0f, H - 26 * s, 15 * s, Fade(g, 0.8f));
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hit >= 0) camSel = hit; else if (hl) camSel--; else camSel++;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) camSel--;
    float wh = GetMouseWheelMove();
    if (wh < 0) camSel++; else if (wh > 0) camSel--;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) camSel++;
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) camSel--;
    for (int i = 0; i < total && i < 9; i++) if (IsKeyPressed(KEY_ONE + i)) camSel = i;
    if (camSel != old) play(S_CLICK, 0.6f, 0.7f);
}

static void draw_hud(void) {
    float s = uiS();
    int W = SCREEN_W, H = SCREEN_H;
    if (gProx > 0.05f) {                                       /* kalp atisi / tehlike vinyeti */
        float pulse = 0.5f + 0.5f * sinf(gtime * (6.0f + gProx * 10.0f));
        DrawRectangle(0, 0, W, H, Fade(RED, gProx * (0.06f + 0.16f * pulse)));
    }
    if (scareT > 0) DrawRectangle(0, 0, W, H, Fade(RED, clampf(scareT, 0, 0.5f) * 0.35f));
    if (cfg.fx) {
        if (P.sanity < 40) {                                   /* dusuk akil: parazit */
            int n = (int)((40 - P.sanity) / 3);
            for (int i = 0; i < n; i++)
                DrawRectangle(GetRandomValue(0, W), GetRandomValue(0, H), GetRandomValue(20, 200), 1, Fade(RAYWHITE, 0.18f));
        }
        for (int i = 0; i < 40; i++) DrawRectangle(GetRandomValue(0, W), GetRandomValue(0, H), 2, 2, Fade(RAYWHITE, 0.05f));
        Color v0 = {0, 0, 0, 170}, v1 = {0, 0, 0, 0};
        int bx = W / 5, by = H / 5;
        DrawRectangleGradientH(0, 0, bx, H, v0, v1);
        DrawRectangleGradientH(W - bx, 0, bx, H, v1, v0);
        DrawRectangleGradientV(0, 0, W, by, v0, v1);
        DrawRectangleGradientV(0, H - by, W, by, v1, v0);
    }
    if (P.hidden) {                                            /* dolap aralik gorunumu */
        DrawRectangle(0, 0, W, H / 2 - 22, BLACK);
        DrawRectangle(0, H / 2 + 22, W, H / 2 - 22, BLACK);
        char hb[96];
        snprintf(hb, sizeof hb, "DOLABIN İÇİNDESİN  (%s: çık,  %s: nefes tut)", key_name(keymap[A_USE]), key_name(keymap[A_HOLD]));
        dcenter(hb, W / 2.0f, 60, 22, (Color){200, 80, 80, 255});
        bar(W / 2 - 90, H / 2 + 50, 180, 12, P.breath / 100.0f, (Color){120, 200, 220, 255}, "NEFES");
    } else {
        DrawCircle(W / 2, H / 2, 2, Fade(RAYWHITE, 0.5f));
        const char *tip = NULL;
        char tb[96];
        static char tk[112];
#define TIPK(txt) (snprintf(tk, sizeof tk, "%s: %s", key_name(keymap[A_USE]), (txt)), tk)
        if (ia_type == IA_LOCKER) tip = TIPK("dolaba saklan");
        else if (ia_type == IA_STAIRS) tip = TIPK(P.lvl == 0 ? "üst kata çık" : "aşağı in");
        else if (ia_type == IA_ITEM) { snprintf(tb, sizeof tb, "%s: al (%s)", key_name(keymap[A_USE]), item_name(items[ia_x].type)); tip = tb; }
        else if (ia_type == IA_PROP) {
            const Prop *pp = &props[ia_x];
            if (pp->type == P_GENERATOR) tip = genOn ? "Jeneratör çalışıyor" : (P.hasFuel ? TIPK("jeneratörü çalıştır") : "Jeneratör: yakıt gerekli");
            else if (pp->type == P_FUSEBOX) tip = fuseIn ? "Sigorta takılı" : (P.hasFuse ? TIPK("sigortayı tak") : "Sigorta kutusu: sigorta gerekli");
            else tip = TIPK(pp->want ? "kapat" : "aç");
        } else if (ia_type == IA_DOOR) {
            const Door *d = &doors[ia_x];
            if (d->lock >= 0 && !(P.keys & (1 << d->lock))) {
                char k[48];
                key_label(d->lock, k, sizeof k);
                snprintf(tb, sizeof tb, "Kilitli kapı  (%s)", k); tip = tb;
            } else if (d->lock >= 0) tip = TIPK("kilidi aç");
            else tip = TIPK(d->want ? "kapıyı kapat" : "kapıyı aç");
        }
#undef TIPK
        if (tip) dcenter(tip, W / 2.0f, H / 2.0f + 60, 20, (Color){220, 190, 120, 255});
    }
    char buf[96];
    snprintf(buf, sizeof buf, "Sayfa: %d / %d", P.pages, PAGES_NEEDED);
    dtext(buf, 20, 16, 26, (Color){235, 230, 200, 255});
    float yy = 50;
    snprintf(buf, sizeof buf, "Sigorta: %s", fuseIn ? "takıldı" : (P.hasFuse ? "elinde" : "bulunacak"));
    dtext(buf, 20, yy, 16, fuseIn ? (Color){120, 230, 140, 255} : (Color){200, 190, 150, 255}); yy += 20;
    snprintf(buf, sizeof buf, "Jeneratör: %s", genOn ? "çalışıyor" : (P.hasFuel ? "yakıt elinde" : "yakıt lazım"));
    dtext(buf, 20, yy, 16, genOn ? (Color){120, 230, 140, 255} : (Color){200, 190, 150, 255}); yy += 22;
    for (int k = 0; k < nkeys; k++) {
        if (!(P.keys & (1 << k))) continue;
        char kl[48];
        key_label(k, kl, sizeof kl);
        DrawRectangle(22, (int)yy + 5, 14, 5, (Color){230, 190, 60, 255});
        DrawCircle(40, (int)yy + 7, 5, (Color){230, 190, 60, 255});
        dtext(kl, 54, yy, 16, (Color){230, 200, 100, 255});
        yy += 20;
    }
    if (P.bottles > 0) { snprintf(buf, sizeof buf, "Şişe: %d  (%s)", P.bottles, key_name(keymap[A_THROW])); dtext(buf, 20, yy, 16, (Color){120, 200, 140, 255}); yy += 20; }
    if (P.matches > 0 || P.matchT > 0) {
        snprintf(buf, sizeof buf, "Kibrit: %d  (%s)%s", P.matches, key_name(keymap[A_MATCH]), P.matchT > 0 ? "  - yanıyor" : "");
        dtext(buf, 20, yy, 16, (Color){240, 170, 90, 255}); yy += 20;
    }
    if (P.crouch) dtext("EĞİLDİN - sessiz", 20, yy, 16, (Color){160, 160, 200, 255});
    if (powerFlashT > 0) DrawRectangle(0, 0, W, H, Fade((Color){255, 240, 180, 255}, 0.12f * fabsf(sinf(gtime * 22.0f))));
    bar(20, H - 84, 180, 14, P.sanity / 100.0f, (Color){130, 90, 200, 255}, "AKIL");
    bar(20, H - 62, 180, 14, P.battery / 100.0f, (Color){240, 200, 60, 255}, "FENER");
    bar(20, H - 40, 180, 14, P.stamina / 100.0f, (Color){70, 170, 230, 255}, "KOŞU");
    if (msgT > 0) dcenter(msg, W / 2.0f, H - 70.0f, 20, Fade(RAYWHITE, clampf(msgT, 0, 1)));
    if (showMap) draw_minimap();
    {
        char hint[160];
        snprintf(hint, sizeof hint, "%s: not defteri   %s: harita   %s: kameralar   Tekerlek: fener odağı", key_name(keymap[A_NOTES]), key_name(keymap[A_MAP]), key_name(keymap[A_CCTV]));
        dtext(hint, 20, (float)H - 108, 14, Fade(RAYWHITE, 0.35f));
    }
    if (trans > 0) DrawRectangle(0, 0, W, H, Fade(BLACK, 1.0f - fabsf(2.0f * trans - 1.0f)));
    (void)s;
}
static void draw_jump_overlay(void) {
    int W = SCREEN_W, H = SCREEN_H;
    float e = 1.5f - jumpT;
    DrawRectangle(0, 0, W, H, Fade(RED, 0.22f + 0.12f * sinf(gtime * 45.0f)));
    for (int i = 0; i < 160; i++)
        DrawRectangle(GetRandomValue(0, W), GetRandomValue(0, H), GetRandomValue(30, 300), GetRandomValue(1, 3), Fade(RAYWHITE, 0.12f));
    Color v0 = {0, 0, 0, 230}, v1 = {0, 0, 0, 0};
    int bx = W / 4, by = H / 4;
    DrawRectangleGradientH(0, 0, bx, H, v0, v1);
    DrawRectangleGradientH(W - bx, 0, bx, H, v1, v0);
    DrawRectangleGradientV(0, 0, W, by, v0, v1);
    DrawRectangleGradientV(0, H - by, W, by, v1, v0);
    if (e < 0.12f) DrawRectangle(0, 0, W, H, Fade(WHITE, 0.9f * (1.0f - e / 0.12f)));
    if (e > 1.05f) DrawRectangle(0, 0, W, H, Fade(BLACK, clampf((e - 1.05f) / 0.4f, 0, 1)));
}
static void draw_notes(void) {
    float s = uiS(), W = (float)SCREEN_W, H = (float)SCREEN_H;
    DrawRectangle(0, 0, (int)W, (int)H, Fade(BLACK, 0.82f));
    float px = W * 0.1f, pw = W * 0.8f, y = H * 0.05f;
    DrawRectangle((int)px, (int)y, (int)pw, (int)(H * 0.9f), (Color){38, 32, 26, 238});
    DrawRectangleLines((int)px, (int)y, (int)pw, (int)(H * 0.9f), (Color){120, 100, 70, 255});
    dcenter("NOT DEFTERİ", W / 2, y + 10 * s, 36 * s, (Color){230, 210, 160, 255});
    y += 60 * s;
    char b[240];
    float tw = pw - 48 * s, lh = 20 * s;
    snprintf(b, sizeof b, "Amaç: 6 sayfayı topla (%d/%d) ve elektriği geri getir (sigorta: %s, jeneratör: %s). Sonra zemin kattaki yeşil kapıdan çık.",
             P.pages, PAGES_NEEDED, fuseIn ? "takıldı" : (P.hasFuse ? "elinde" : "yok"), genOn ? "çalışıyor" : (P.hasFuel ? "yakıt elinde" : "yakıtsız"));
    y += draw_wrapped(b, px + 24 * s, y, tw, 17 * s, lh, (Color){220, 215, 200, 255}, -1) * lh + 4 * s;
    snprintf(b, sizeof b, "Şişe: %d   Kibrit: %d   Anahtar: %d/%d   Not: %d/8   Kat: %s",
             P.bottles, P.matches, __builtin_popcount((unsigned)P.keys), nkeys, __builtin_popcount((unsigned)P.notes), P.lvl == 0 ? "Zemin" : "Üst");
    dtext(b, px + 24 * s, y, 16 * s, (Color){190, 185, 170, 255}); y += 26 * s;
    y += draw_wrapped("İpucu: Eğilerek (C) kapıyı yavaş aç, sessiz ol. Dolapta nefes tut. Heykel'e bakarken hareket etmez ve kapı açamaz, kapıları kapat. Dolap, çekmece ve sandıkları aç; içindekileri E ile al.",
                      px + 24 * s, y, tw, 15 * s, 19 * s, (Color){150, 160, 140, 255}, -1) * 19 * s + 10 * s;
    float colW = pw / 2 - 40 * s, yL = y, yR = y;
    dtext("SAYFALAR", px + 24 * s, yL, 18 * s, (Color){230, 210, 160, 255}); yL += 28 * s;
    dtext("NOTLAR", px + pw / 2 + 10 * s, yR, 18 * s, (Color){230, 210, 160, 255}); yR += 28 * s;
    for (int i = 0; i < P.pages; i++) {
        snprintf(b, sizeof b, "%d. %s", i + 1, pageLore[i]);
        yL += draw_wrapped(b, px + 24 * s, yL, colW, 14 * s, 19 * s, (Color){205, 190, 150, 255}, -1) * 19 * s + 6 * s;
    }
    if (P.pages == 0) dtext("Henüz hiç sayfa bulamadın.", px + 24 * s, yL, 16 * s, (Color){140, 135, 125, 255});
    int shown = 0;
    for (int i = 0; i < 8; i++) {
        if (!(P.notes & (1 << i))) continue;
        snprintf(b, sizeof b, "%d. %s", i + 1, noteText[i]);
        yR += draw_wrapped(b, px + pw / 2 + 10 * s, yR, colW, 14 * s, 19 * s, (Color){175, 195, 200, 255}, -1) * 19 * s + 6 * s;
        shown++;
    }
    if (!shown) dtext("Henüz hiç not bulamadın.", px + pw / 2 + 10 * s, yR, 16 * s, (Color){140, 135, 125, 255});
    snprintf(b, sizeof b, "%s: kapat", key_name(keymap[A_NOTES]));
    dcenter(b, W / 2, H * 0.95f - 30 * s, 18 * s, (Color){150, 145, 135, 255});
}
static void draw_end_screen(bool win) {
    float s = uiS(), W = (float)SCREEN_W, H = (float)SCREEN_H;
    DrawRectangle(0, 0, (int)W, (int)H, Fade(BLACK, 0.78f));
    bool secret = win && ending == 1;
    dcenter(win ? (secret ? "GERÇEK SON" : "KAÇTIN!") : "ÖLDÜN", W / 2, H / 2 - 230 * s, 76 * s,
            win ? (secret ? (Color){240, 210, 120, 255} : (Color){80, 220, 120, 255}) : (Color){200, 30, 30, 255});
    float y = H / 2 - 130 * s;
    if (win) {
        char b[96];
        if (secret) {
            dcenter("Altı sayfayı ve Elif'in bütün notlarını okumuştun.", W / 2, y, 23 * s, RAYWHITE); y += 34 * s;
            dcenter("Kapıdan çıkarken Heykel kapının önünde durdu. Bu kez sana dokunmadı.", W / 2, y, 23 * s, RAYWHITE); y += 34 * s;
            dcenter("Dışarıda sabah ışığı vardı ve Elif seni bekliyordu.", W / 2, y, 23 * s, RAYWHITE); y += 34 * s;
            dcenter("Malikane arkanda sessizliğe gömüldü.", W / 2, y, 25 * s, (Color){240, 210, 120, 255}); y += 44 * s;
        } else {
            dcenter("Malikanenin kapısı arkanda kapandı... ama içeriden hâlâ bir fısıltı geliyor.", W / 2, y, 23 * s, RAYWHITE); y += 38 * s;
            dcenter("Notların hepsini bulsaydın belki gerçeği öğrenirdin.", W / 2, y, 18 * s, (Color){160, 160, 170, 255}); y += 36 * s;
        }
        snprintf(b, sizeof b, "Süre: %d:%02d     En iyi: %d:%02d", (int)runTime / 60, (int)runTime % 60, (int)bestTime / 60, (int)bestTime % 60);
        dcenter(b, W / 2, y, 22 * s, (Color){200, 200, 160, 255}); y += 40 * s;
    } else { dcenter(deathMsg, W / 2, y, 26 * s, RAYWHITE); y += 56 * s; }
    const char *lbw[2] = {"YENİDEN OYNA", "MENÜ"};
    const char *lbd[2] = {"YENİDEN DENE", "MENÜ"};
    bool enw[2] = {true, true};
    int ch = menu_list(win ? lbw : lbd, enw, 2, W / 2 - 160 * s, y, 320 * s, 54 * s, 26 * s, true, &endSel);
    if (win) {
        if (ch == 0 || IsKeyPressed(KEY_R)) new_game();
        else if (ch == 1) { gstate = ST_MENU; hasGame = false; }
    } else {
        if (ch == 0 || IsKeyPressed(KEY_R)) new_game();
        else if (ch == 1) { gstate = ST_MENU; hasGame = false; }
    }
}
static void draw_pause(void) {
    float s = uiS(), W = (float)SCREEN_W, H = (float)SCREEN_H;
    DrawRectangle(0, 0, (int)W, (int)H, Fade(BLACK, 0.7f));
    dcenter("DURAKLATILDI", W / 2, H / 2 - 190 * s, 60 * s, (Color){220, 220, 225, 255});
    const char *lb[3] = {"DEVAM ET", "MENÜYE DÖN", "ÇIKIŞ"};
    bool en[3] = {true, true, true};
    int ch = menu_list(lb, en, 3, W / 2 - 160 * s, H / 2 - 90 * s, 320 * s, 62 * s, 28 * s, true, &pauseSel);
    if (ch == 0) { gstate = ST_PLAY; DisableCursor(); skipMouse = true; }
    else if (ch == 1) { gstate = ST_MENU; menuSel = 0; }
    else if (ch == 2) wantQuit = true;
}

static void load_models(void) {
    static const char *pn[NPROPTYPES] = {"bookshelf", "bed", "wardrobe", "nightstand", "couch", "armchair", "table", "desk", "chair", "fridge",
        "counter", "crate", "barrel", "clock", "plant", "piano", "fireplace", "lamp", "generator", "fusebox"};
    static const char *mn[2] = {"warden", "statue"};
    for (int i = 0; i < NPROPTYPES; i++) {
        char nm[64];
        snprintf(nm, sizeof nm, "models/%s.glb", pn[i]);
        const char *p = find_asset(nm);
        if (!p) continue;
        Model m = LoadModel(p);
        if (m.meshCount > 0) { propModel[i] = m; hasPM[i] = true; }
    }
    for (int i = 0; i < 2; i++) {
        char nm[64];
        snprintf(nm, sizeof nm, "models/%s.glb", mn[i]);
        const char *p = find_asset(nm);
        if (!p) continue;
        Model m = LoadModel(p);
        if (m.meshCount > 0) { monModel[i] = m; hasMM[i] = true; }
    }
}

/* ---------- ana dongu ---------- */
int main(void) {
    InitWindow(WIN_W, WIN_H, "Malikane 3D");
    SetExitKey(KEY_NULL);                    /* ESC artik oyunu kapatmaz, duraklatir */
    ToggleFullscreen();                      /* tam ekran baslat (F11 ile degistirilebilir) */
    SetTargetFPS(60);
    unsigned seed = (unsigned)time(NULL);
    SetRandomSeed(seed); srand(seed);
    load_cfg();
    load_ui_font();
    load_textures();
    load_models();
    init_post();
    init_audio();
    intro_convert_mp4();                    /* MP4 sesini hazirla; goruntu akis halinde oynatilir */
    intro_scan();
    if (firstRun) gstate = ST_CALIB;
    {
        static const char *names[4] = {"menu_full.png", "menu.png", "menu_bg.png", NULL};
        for (int i = 0; names[i] && !hasBg; i++) {
            const char *p = find_asset(names[i]);
            if (!p) continue;
            menuBg = LoadTexture(p);
            if (menuBg.id > 0) { hasBg = true; bgFull = (i < 2); SetTextureFilter(menuBg, TEXTURE_FILTER_BILINEAR); }
        }
    }

    while (!WindowShouldClose() && !wantQuit) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_F9)) useTex = !useTex;
        if (gstate == ST_DEAD) { gtime += dt; if (jumpT > 0) jumpT -= dt; }

        if (gstate == ST_PLAY) {
            if (IsKeyPressed(KEY_ESCAPE) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))) {
                if (showCctv) cctv_close();
                else { gstate = ST_PAUSE; EnableCursor(); showNotes = false; pauseSel = 0; }
            } else {
                if (kpress(A_NOTES) && !showCctv) showNotes = !showNotes;
                if (kpress(A_CCTV) && !showNotes && !P.hidden && trans <= 0.0f) { if (showCctv) cctv_close(); else cctv_open(); }
                if (kpress(A_MAP)) showMap = !showMap;
                if (!showNotes) update_play(dt);
            }
            if (gstate == ST_DEAD || gstate == ST_WIN) { hasGame = false; endSel = 0; showNotes = false; showCctv = false; }
        } else if (gstate == ST_PAUSE && IsKeyPressed(KEY_ESCAPE)) { gstate = ST_PLAY; DisableCursor(); skipMouse = true; }
        update_audio(dt, gstate == ST_PLAY && !showNotes);

        BeginDrawing();
        ClearBackground(BLACK);
        if (gstate == ST_MENU) draw_main_menu();
        else if (gstate == ST_SETTINGS) draw_settings();
        else if (gstate == ST_CREDITS) draw_credits();
        else if (gstate == ST_KEYS) draw_keys();
        else if (gstate == ST_CALIB) draw_calib();
        else if (gstate == ST_CINE) draw_cinematic(dt);
        else if (gstate == ST_PLAY && showCctv) draw_cctv();
        else {
            compute_light();
            if (gstate != ST_PAUSE) update_camera();
            begin_post();
            draw_scene();
            end_post(post_stress());
            if (gstate == ST_DEAD && jumpT > 0) draw_jump_overlay();
            else {
                draw_hud();
                if (showNotes && gstate == ST_PLAY) draw_notes();
                if (gstate == ST_PAUSE) draw_pause();
                else if (gstate == ST_DEAD) draw_end_screen(false);
                else if (gstate == ST_WIN) draw_end_screen(true);
            }
        }
        EndDrawing();
    }
    save_cfg();
    if (audioOK) CloseAudioDevice();
    CloseWindow();
    return 0;
}
