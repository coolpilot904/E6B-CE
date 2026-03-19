/*
 * E6B Flight Computer for TI-84 CE
 * Version 1.0
 *
 * By Andrew Sottile
 * GitHub: github.com/coolpilot904
 * Digital Logbook: log61.com
 *
 * Built with CE Toolchain (ez80-clang)
 */

#include <graphx.h>
#include <keypadc.h>
#include <fileioc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ti/getcsc.h>

/* ── Palette ──────────────────────────────────────────────────── */
#define CI_BG      0
#define CI_HDR     1
#define CI_SELBG   2
#define CI_EDITBG  3
#define CI_LABEL   4
#define CI_VALUE   5
#define CI_AMBER   6
#define CI_GREEN   7
#define CI_RED     8
#define CI_DIV     9
#define CI_TRANS   255

#define SCR_W  320
#define SCR_H  240
#define MARGIN  8
#define STEP   20
#define ROW_Y(r) (26 + (r)*STEP)
#define VAL_X  165

#define DEG2RAD(d) ((d)*M_PI/180.0)
#define RAD2DEG(r) ((r)*180.0/M_PI)
#define FABS(x)    ((x)<0?-(x):(x))
#define CLAMP(x,a,b) ((x)<(a)?(a):((x)>(b)?(b):(x)))

/* Key masks */
#define BK_0   (1<<0)
#define BK_1   (1<<1)
#define BK_4   (1<<2)
#define BK_7   (1<<3)
#define BK_DP  (1<<0)
#define BK_2   (1<<1)
#define BK_5   (1<<2)
#define BK_8   (1<<3)
#define BK_CHS (1<<0)
#define BK_3   (1<<1)
#define BK_6   (1<<2)
#define BK_9   (1<<3)
#define BK_ENT  (1<<0)
#define BK_CLR  (1<<6)
#define BK_DEL  (1<<7)
#define BK_2ND  (1<<5)
#define BK_MOD  (1<<6)
#define BK_DOWN (1<<0)
#define BK_DWN  (1<<0)
#define BK_LFT  (1<<1)
#define BK_LEFT (1<<1)
#define BK_RGT  (1<<2)
#define BK_RIGHT (1<<2)
#define BK_UP   (1<<3)
#define KN(g,m) ((cur[g]&(m))&&!(prev[g]&(m)))

/* ── Theme ────────────────────────────────────────────────────── */
static int g_theme=0; /* 0=Standard 1=Night 2=Daylight */

static void apply_theme(void)
{
    uint16_t p[256]; int i;
    for(i=0;i<256;i++) p[i]=0;
#define RGB(r,g,b) gfx_RGBTo1555((r),(g),(b))
    switch(g_theme){
    case 1: /* Night – green on black */
        p[CI_BG]    = RGB(0,   0,   0);
        p[CI_HDR]   = RGB(0,  45,   0);
        p[CI_SELBG] = RGB(0,  30,   0);
        p[CI_EDITBG]= RGB(50, 35,   0);
        p[CI_LABEL] = RGB(0, 150,   0);
        p[CI_VALUE] = RGB(0, 210,   0);
        p[CI_AMBER] = RGB(80,255,  80);
        p[CI_GREEN] = RGB(0, 255,   0);
        p[CI_RED]   = RGB(200,  0,   0);
        p[CI_DIV]   = RGB(0,  65,   0);
        break;
    case 2: /* Daylight – white background, medium-blue header */
        p[CI_BG]    = RGB(228,228,228);
        p[CI_HDR]   = RGB(130,170,210);   /* medium steel blue – dark text readable */
        p[CI_SELBG] = RGB(155,155,155);
        p[CI_EDITBG]= RGB(255,215,  60);
        p[CI_LABEL] = RGB(60,  60,  60);
        p[CI_VALUE] = RGB(10,  10,  10);  /* near-black on white bg and blue header */
        p[CI_AMBER] = RGB(0,    0, 180);  /* dark blue for selection highlight */
        p[CI_GREEN] = RGB(0,  110,   0);
        p[CI_RED]   = RGB(180,  0,   0);
        p[CI_DIV]   = RGB(130,130,130);
        break;
    default: /* Standard – dark blue */
        p[CI_BG]    = RGB(8,  12, 28);
        p[CI_HDR]   = RGB(15, 50,110);
        p[CI_SELBG] = RGB(20, 55, 90);
        p[CI_EDITBG]= RGB(60, 45,  0);
        p[CI_LABEL] = RGB(110,130,160);
        p[CI_VALUE] = RGB(210,225,245);
        p[CI_AMBER] = RGB(255,185,  0);
        p[CI_GREEN] = RGB(45, 210, 90);
        p[CI_RED]   = RGB(230, 55, 55);
        p[CI_DIV]   = RGB(35,  60,100);
        break;
    }
#undef RGB
    gfx_SetPalette(p,sizeof(p),0);
    gfx_SetTextTransparentColor(CI_TRANS);
}

/* ── Draw helpers ─────────────────────────────────────────────── */
static void draw_header(const char *t)
{
    gfx_SetColor(CI_BG);
    gfx_FillRectangle_NoClip(0,0,SCR_W,SCR_H);
    gfx_SetColor(CI_HDR);
    gfx_FillRectangle_NoClip(0,0,SCR_W,22);
    gfx_SetTextScale(1,1);
    gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_HDR);
    gfx_PrintStringXY(t,(SCR_W-(int)strlen(t)*8)/2,6);
}

static void draw_footer(int editing)
{
    gfx_SetColor(CI_BG);
    gfx_FillRectangle_NoClip(0,SCR_H-14,SCR_W,14);
    gfx_SetColor(CI_DIV);
    gfx_HorizLine_NoClip(0,SCR_H-15,SCR_W);
    gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
    gfx_PrintStringXY(editing?"CLR=Cancel":"ALPHA=Help",
                      MARGIN,SCR_H-12);
}

static void draw_divider(int r)
{
    gfx_SetColor(CI_DIV);
    gfx_HorizLine_NoClip(MARGIN,ROW_Y(r)-5,SCR_W-MARGIN*2);
}

static void draw_field(int row,const char *label,const char *val,int sel,int edit)
{
    int y=ROW_Y(row);
    if(sel){ gfx_SetColor(CI_SELBG); gfx_FillRectangle_NoClip(0,y-3,SCR_W,STEP); }
    gfx_SetTextBGColor(sel?CI_SELBG:CI_BG);
    gfx_SetTextFGColor(sel?CI_AMBER:CI_BG);
    gfx_PrintStringXY(">",MARGIN,y+3);
    gfx_SetTextFGColor(sel?CI_AMBER:CI_LABEL);
    gfx_PrintStringXY(label,MARGIN+10,y+3);
    if(sel&&edit){
        gfx_SetColor(CI_EDITBG);
        gfx_FillRectangle_NoClip(VAL_X-3,y-3,SCR_W-VAL_X-MARGIN+3,STEP);
        gfx_SetTextFGColor(CI_AMBER); gfx_SetTextBGColor(CI_EDITBG);
    } else {
        gfx_SetTextFGColor(sel?CI_AMBER:CI_VALUE);
        gfx_SetTextBGColor(sel?CI_SELBG:CI_BG);
    }
    gfx_PrintStringXY(val,VAL_X,y+3);
}

static void draw_result(int row,const char *label,const char *val)
{
    int y=ROW_Y(row);
    gfx_SetTextBGColor(CI_BG);
    gfx_SetTextFGColor(CI_LABEL);
    gfx_PrintStringXY(label,MARGIN+10,y+3);
    gfx_SetTextFGColor(CI_GREEN);
    gfx_PrintStringXY(val,VAL_X,y+3);
}

static void draw_warn(const char *msg)
{
    gfx_SetColor(CI_RED);
    gfx_FillRectangle_NoClip(MARGIN,SCR_H-32,SCR_W-MARGIN*2,14);
    gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_RED);
    gfx_PrintStringXY(msg,MARGIN+4,SCR_H-30);
}

/* ── Input state ─────────────────────────────────────────────── */
typedef struct { int on,neg,len,dot; char buf[14]; } IS;

static void is_start(IS *s)
{ s->on=1;s->neg=0;s->len=0;s->dot=0;s->buf[0]='\0'; }

static int is_key(IS *s,const uint8_t cur[8],const uint8_t prev[8])
{
    if(!s->on) return 0;
    if(KN(6,BK_ENT)){s->on=0;return 1;}
    if(KN(6,BK_CLR)){s->on=0;return -1;}
    if(KN(1,BK_DEL)&&s->len>0){
        if(s->buf[s->len-1]=='.')s->dot=0;
        s->buf[--s->len]='\0';
    }
    if(KN(5,BK_CHS)) s->neg=!s->neg;
    if(KN(4,BK_DP)&&!s->dot&&s->len<12){
        if(!s->len){s->buf[s->len++]='0';s->buf[s->len]='\0';}
        s->buf[s->len++]='.';s->buf[s->len]='\0';s->dot=1;
    }
#define D(g,m,c) if(KN(g,m)&&s->len<12){s->buf[s->len++]=(c);s->buf[s->len]='\0';}
    D(3,BK_0,'0')D(3,BK_1,'1')D(3,BK_4,'4')D(3,BK_7,'7')
    D(4,BK_2,'2')D(4,BK_5,'5')D(4,BK_8,'8')
    D(5,BK_3,'3')D(5,BK_6,'6')D(5,BK_9,'9')
#undef D
    return 0;
}

static double is_val(const IS *s){ double v=atof(s->buf); return s->neg?-v:v; }

static void is_fmt(char *o,int sz,const IS *s,int active,double stored,const char *unit)
{
    if(active&&s->on){
        snprintf(o,sz,"%s%s_",s->neg?"-":"",s->buf);
    } else {
        if(stored==0.0){ snprintf(o,sz,"0 %s",unit); return; }
        double r=round(stored*100.0)/100.0;
        int cents=(int)round(FABS(fmod(r,1.0))*100.0);
        if(cents==0)          snprintf(o,sz,"%.0f %s",r,unit);
        else if(cents%10==0)  snprintf(o,sz,"%.1f %s",r,unit);
        else                  snprintf(o,sz,"%.2f %s",r,unit);
    }
}

static void fmt_hhmm(char *o,int sz,double hrs)
{
    hrs=fmod(hrs,24.0); if(hrs<0)hrs+=24.0;
    int h=(int)hrs, m=(int)((hrs-(int)hrs)*60.0+0.5);
    if(m==60){h=(h+1)%24;m=0;}
    snprintf(o,sz,"%02d:%02d",h,m);
}

/* ── Help overlays ────────────────────────────────────────────── */
static void show_std_help(void); /* forward declaration */

static const char *g_scr_help_title = NULL;
static const char *g_scr_help_body  = NULL;

static void show_screen_help(void)
{
    if(!g_scr_help_body){ show_std_help(); return; }
    gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,0,SCR_W,SCR_H);
    gfx_SetColor(CI_HDR); gfx_FillRectangle_NoClip(0,0,SCR_W,22);
    gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_HDR);
    gfx_SetTextScale(1,1);
    if(g_scr_help_title)
        gfx_PrintStringXY(g_scr_help_title,
            (SCR_W-(int)strlen(g_scr_help_title)*8)/2,6);
    gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_BG);
    const char *p=g_scr_help_body;
    int y=28;
    char ln[42];
    while(*p && y<SCR_H-14){
        int i=0;
        while(*p && *p!='\n' && i<41) ln[i++]=*p++;
        ln[i]='\0'; if(*p=='\n')p++;
        gfx_PrintStringXY(ln,MARGIN,y);
        y+=12;
    }
    gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-15,SCR_W);
    gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
    gfx_PrintStringXY("Any key to continue",MARGIN,SCR_H-12);
    gfx_BlitBuffer();
    while(kb_AnyKey()) kb_Scan();   /* flush triggering key */
    while(!kb_AnyKey()) kb_Scan();  /* wait for dismiss key */
    while(kb_AnyKey()) kb_Scan();   /* wait for release */
}

static void show_std_help(void)
{
    gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,0,SCR_W,SCR_H);
    gfx_SetColor(CI_HDR); gfx_FillRectangle_NoClip(0,0,SCR_W,22);
    gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_HDR);
    gfx_PrintStringXY("  KEY REFERENCE  ",60,6);
    gfx_SetTextBGColor(CI_BG);
    int y=30;
#define HL(lbl,val) gfx_SetTextFGColor(CI_LABEL);gfx_PrintStringXY(lbl,MARGIN,y); \
    gfx_SetTextFGColor(CI_VALUE);gfx_PrintStringXY(val,120,y);y+=12;
    gfx_SetTextFGColor(CI_AMBER); gfx_PrintStringXY("Navigating:",MARGIN,y); y+=12;
    HL("UP / DOWN",  "Select field")
    HL("ENTER",      "Edit selected field")
    HL("CLEAR",      "Go back")
    HL("MODE",       "This help screen")
    y+=4;
    gfx_SetTextFGColor(CI_AMBER); gfx_PrintStringXY("While editing:",MARGIN,y); y+=12;
    HL("0-9",        "Type digits")
    HL("(.)",        "Decimal point")
    HL("(-)",        "Toggle negative")
    HL("DEL",        "Backspace")
    HL("ENTER",      "Confirm value")
    HL("CLEAR",      "Cancel edit")
#undef HL
    gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-18,SCR_W);
    gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
    gfx_PrintStringXY("Press any key to close",MARGIN,SCR_H-13);
    gfx_BlitBuffer();
    while(kb_AnyKey()) kb_Scan();
    while(!kb_AnyKey()) kb_Scan();
}

static void show_wb_help(void)
{
    gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,0,SCR_W,SCR_H);
    gfx_SetColor(CI_HDR); gfx_FillRectangle_NoClip(0,0,SCR_W,22);
    gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_HDR);
    gfx_PrintStringXY("  W&B KEY REFERENCE  ",48,6);
    gfx_SetTextBGColor(CI_BG);
    int y=30;
#define HL(lbl,val) gfx_SetTextFGColor(CI_LABEL);gfx_PrintStringXY(lbl,MARGIN,y); \
    gfx_SetTextFGColor(CI_VALUE);gfx_PrintStringXY(val,120,y);y+=12;
    gfx_SetTextFGColor(CI_AMBER); gfx_PrintStringXY("Navigation:",MARGIN,y); y+=12;
    HL("UP / DOWN",   "Select station / row")
    HL("LEFT / RIGHT","Weight vs Arm column")
    HL("ENTER",       "Edit value / Add station")
    HL("CLEAR",       "Go back")
    HL("MODE",        "This help screen")
    y+=4;
    gfx_SetTextFGColor(CI_AMBER); gfx_PrintStringXY("Station controls:",MARGIN,y); y+=12;
    HL("2ND",         "Rename station")
    HL("DEL",         "Delete station")
    HL("ALPHA",       "Cycle: lbs > AvGas > Jet-A")
    y+=4;
    gfx_SetTextFGColor(CI_AMBER); gfx_PrintStringXY("Fuel types (ALPHA key):",MARGIN,y); y+=12;
    HL("lbs",         "Weight in pounds (direct)")
    HL("AvG",         "Gallons of AvGas x6.0 lbs/gal")
    HL("JtA",         "Gallons of Jet-A x6.84 lbs/gal")
#undef HL
    gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-18,SCR_W);
    gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
    gfx_PrintStringXY("Press any key to close",MARGIN,SCR_H-13);
    gfx_BlitBuffer();
    while(kb_AnyKey()) kb_Scan();
    while(!kb_AnyKey()) kb_Scan();
}

/* ── Field loop macros ────────────────────────────────────────── */
#define FL_START \
    static int sel=0; IS inp={0}; int _rpt=0; \
    uint8_t prev[8]={0},cur[8]; \
    while(1){ \
        draw_header(TITLE); \
        char s[48];

#define FL_DIV(nf)  draw_divider(nf);

#define FL_END(nf) \
        draw_footer(inp.on); \
        gfx_BlitBuffer(); \
        kb_Scan(); \
        {int _i;for(_i=0;_i<8;_i++)cur[_i]=kb_Data[_i];} \
        if(inp.on){ int r=is_key(&inp,cur,prev); if(r==1)v[sel]=is_val(&inp); } \
        else{ \
            if(cur[7]&(BK_UP|BK_DOWN)) _rpt++; else _rpt=0; \
            if((KN(7,BK_UP)||((cur[7]&BK_UP)&&_rpt>18&&(_rpt%4==0)))&&sel>0)sel--; \
            if((KN(7,BK_DOWN)||((cur[7]&BK_DOWN)&&_rpt>18&&(_rpt%4==0)))&&sel<(nf)-1)sel++; \
            if(KN(6,BK_ENT))is_start(&inp); \
            if(KN(6,BK_CLR)){memcpy(prev,cur,8);return;} \
            if(KN(2,kb_Alpha)) show_screen_help(); \
        } \
        memcpy(prev,cur,8); \
    }

/* ── Sub-menu ─────────────────────────────────────────────────── */
static int sub_menu(const char *title,const char **items,int n,int init_sel)
{
    while(kb_AnyKey()) kb_Scan();
    int sel=init_sel, _rpt=0; uint8_t prev[8]={0},cur[8];
    while(1){
        gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,0,SCR_W,SCR_H);
        gfx_SetColor(CI_HDR); gfx_FillRectangle_NoClip(0,0,SCR_W,22);
        gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_HDR);
        gfx_SetTextScale(1,1);
        gfx_PrintStringXY(title,(SCR_W-(int)strlen(title)*8)/2,6);
        int i;
        for(i=0;i<n;i++){
            int y=30+i*23;
            if(i==sel){
                gfx_SetColor(CI_SELBG);
                gfx_FillRectangle_NoClip(MARGIN,y-2,SCR_W-MARGIN*2,20);
                gfx_SetColor(CI_AMBER);
                gfx_Rectangle_NoClip(MARGIN,y-2,SCR_W-MARGIN*2,20);
                gfx_SetTextFGColor(CI_AMBER); gfx_SetTextBGColor(CI_SELBG);
            } else {
                gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_BG);
            }
            gfx_PrintStringXY(items[i],MARGIN+14,y+5);
        }
        gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-15,SCR_W);
        gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
        gfx_PrintStringXY("ALPHA=Help  CLR=Back",MARGIN,SCR_H-12);
        gfx_BlitBuffer();
        kb_Scan();
        for(i=0;i<8;i++) cur[i]=kb_Data[i];
        if(cur[7]&(BK_UP|BK_DOWN)) _rpt++; else _rpt=0;
        if((KN(7,BK_UP)||((cur[7]&BK_UP)&&_rpt>18&&(_rpt%4==0)))&&sel>0) sel--;
        if((KN(7,BK_DOWN)||((cur[7]&BK_DOWN)&&_rpt>18&&(_rpt%4==0)))&&sel<n-1) sel++;
        if(KN(6,BK_ENT)){memcpy(prev,cur,8);return sel;}
        if(KN(6,BK_CLR)){memcpy(prev,cur,8);return -1;}
        if(KN(2,kb_Alpha)) show_std_help();
        memcpy(prev,cur,8);
    }
}

/* ══════════════════════════════════════════════════════════════
   ALTITUDE FUNCTIONS
   ══════════════════════════════════════════════════════════════ */

/* Combined: field elev + altimeter + OAT → PA and DA in one step */
static void calc_da_quick(void)
{
#define TITLE "  DENSITY ALTITUDE  "
    g_scr_help_title="DENSITY ALTITUDE";
    g_scr_help_body=
        "Combines pressure alt + OAT to\n"
        "find density altitude.\n"
        "\n"
        "Field Elev: airport elevation.\n"
        "Altimeter: current baro setting.\n"
        "OAT: outside air temp (Celsius).\n"
        "\n"
        "Pressure Alt: alt at 29.92 baro.\n"
        "Density Alt: performance alt.\n"
        "High DA = less power, more roll.\n"
        "\n"
        "ENTER=edit  UP/DN=move sel\n"
        "DEL=backspace  CLR=back";
    double v[3]={0,0,0}; /* Field Elev ft, Altimeter inHg, OAT C */
    const char *lab[3]={"Field Elev / Ind Alt","Altimeter","OAT"};
    const char *un[3]={"ft","in","C"};
    FL_START
        int i; for(i=0;i<3;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(3)
        {
            double pa=v[0]+(29.92-v[1])*1000.0;
            double isa=15.0-1.98*(pa/1000.0);
            double da=pa+118.8*(v[2]-isa);
            snprintf(s,sizeof(s),"%.0f ft",pa);
            draw_result(4,"Pressure Alt",s);
            snprintf(s,sizeof(s),"%.0f ft",da);
            draw_result(5,"Density Alt",s);
            snprintf(s,sizeof(s),"%.1f C (ISA%+.0f)",isa,v[2]-isa);
            draw_result(6,"ISA Temp / Dev",s);
            if(da>8000) draw_warn("HIGH DA - CHECK PERF CHARTS");
        }
    FL_END(3)
#undef TITLE
}

static void calc_palt(void)
{
#define TITLE "  PRESSURE ALTITUDE  "
    g_scr_help_title="PRESSURE ALTITUDE";
    g_scr_help_body=
        "Converts indicated altitude to\n"
        "pressure altitude.\n"
        "\n"
        "PAlt = IAlt + (29.92-Baro)*1000\n"
        "\n"
        "Used for TAS, density alt, and\n"
        "high-altitude flight levels.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[2]={0,0};
    const char *lab[2]={"Indicated Alt","Altimeter"};
    const char *un[2]={"ft","in"};
    FL_START
        int i; for(i=0;i<2;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(2)
        {
            double pa = v[0] + (29.92-v[1])*1000.0;
            snprintf(s,sizeof(s),"%.0f ft",pa); draw_result(3,"Pressure Alt",s);
        }
    FL_END(2)
#undef TITLE
}

static void calc_dalt(void)
{
#define TITLE "  DENSITY ALTITUDE  "
    g_scr_help_title="DA FROM PRESS ALT";
    g_scr_help_body=
        "Density alt from pressure alt.\n"
        "\n"
        "Use when PAlt is already known\n"
        "(eg from a METAR or already\n"
        "computed on another screen).\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[2]={0,0};
    const char *lab[2]={"Pressure Alt","OAT"};
    const char *un[2]={"ft","C"};
    FL_START
        int i; for(i=0;i<2;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(2)
        {
            double isa=15.0-1.98*(v[0]/1000.0);
            double da=v[0]+118.8*(v[1]-isa);
            snprintf(s,sizeof(s),"%.0f ft",da);     draw_result(3,"Density Alt",s);
            snprintf(s,sizeof(s),"%.1f C (ISA%+.0f)",isa,v[1]-isa);
            draw_result(4,"ISA Temp / Dev",s);
            if(da>8000) draw_warn("HIGH DA - CHECK PERF CHARTS");
        }
    FL_END(2)
#undef TITLE
}

static void calc_cloudbase(void)
{
#define TITLE "  CLOUD BASE  "
    g_scr_help_title="CLOUD BASE";
    g_scr_help_body=
        "Estimates cloud base AGL from\n"
        "temperature/dew point spread.\n"
        "\n"
        "Spread = OAT - Dew Point (F)\n"
        "Cloud Base ~ Spread * 400 ft\n"
        "\n"
        "Result is AGL above the field,\n"
        "not above sea level (MSL).\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[2]={0,0}; /* OAT F, Dewpoint F */
    const char *lab[2]={"OAT","Dew Point"};
    const char *un[2]={"F","F"};
    FL_START
        int i; for(i=0;i<2;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(2)
        {
            double spread=v[0]-v[1];
            double agl=(spread/4.4)*1000.0;
            snprintf(s,sizeof(s),"%.0f ft AGL",agl); draw_result(3,"Cloud Base",s);
            /* Spread in C */
            double sc=(v[0]-32)*5.0/9.0 - (v[1]-32)*5.0/9.0;
            snprintf(s,sizeof(s),"%.1f F  (%.1f C spread)",spread,sc);
            draw_result(4,"Temp Spread",s);
            if(agl<1000) draw_warn("LOW CEILING - Check VFR minimums");
        }
    FL_END(2)
#undef TITLE
}

static void calc_stdatmos(void)
{
#define TITLE "  STD ATMOSPHERE  "
    g_scr_help_title="STANDARD ATMOSPHERE";
    g_scr_help_body=
        "ISA standard values at altitude.\n"
        "\n"
        "Sea level: 29.92 inHg, 59F\n"
        "10,000 ft: 23.09 inHg, 23F\n"
        "20,000 ft: 13.75 inHg, -12F\n"
        "\n"
        "Use to find ISA temp deviation\n"
        "or standard pressure for filing.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[1]={0};
    const char *lab[1]={"Altitude"};
    const char *un[1]={"ft"};
    FL_START
        is_fmt(s,sizeof(s),&inp,0==sel,v[0],un[0]);
        draw_field(0,lab[0],s,0==sel,inp.on);
    FL_DIV(1)
        {
            double alt=v[0];
            double tc=15.0-1.98*(alt/1000.0);
            double tf=tc*9.0/5.0+32.0;
            double pm=alt*0.3048;
            double inhg=29.92*pow((tc+273.15)/288.15,5.2561);
            double mb=inhg*33.8639;
            snprintf(s,sizeof(s),"%.2f in Hg",inhg); draw_result(2,"Pressure",s);
            snprintf(s,sizeof(s),"%.0f mb",mb);       draw_result(3,"Pressure",s);
            snprintf(s,sizeof(s),"%.1f F  (%.1f C)",tf,tc); draw_result(4,"ISA Temp",s);
            (void)pm;
        }
    FL_END(1)
#undef TITLE
}

static void menu_altitude(void)
{
    const char *items[]={"Density Altitude","Pressure Altitude","DA from Press Alt","Cloud Base","Std Atmosphere"};
    static int _s=0;
    while(1){
        int r=sub_menu("  ALTITUDE  ",items,5,_s);
        if(r<0) return;
        _s=r;
        switch(r){
            case 0: calc_da_quick(); break;
            case 1: calc_palt();     break;
            case 2: calc_dalt();     break;
            case 3: calc_cloudbase(); break;
            case 4: calc_stdatmos(); break;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   AIRSPEED FUNCTIONS
   ══════════════════════════════════════════════════════════════ */

static void calc_tas(void)
{
#define TITLE "  TRUE AIRSPEED  "
    g_scr_help_title="TRUE AIRSPEED";
    g_scr_help_body=
        "Converts CAS to True Airspeed.\n"
        "\n"
        "CAS: from your airspeed indicator\n"
        "     (or IAS if not corrected).\n"
        "Pressure Alt: from altimeter.\n"
        "OAT: outside air temp (Celsius).\n"
        "\n"
        "TAS: actual speed thru the air.\n"
        "Mach: fraction of speed of sound.\n"
        "TAT: total air temp probe reads.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[3]={0,0,0};
    const char *lab[3]={"CAS","Pressure Alt","OAT"};
    const char *un[3]={"kts","ft","C"};
    FL_START
        int i; for(i=0;i<3;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(3)
        {
            double pm=v[1]*0.3048;
            double delta=pow(1.0-6.8756e-6*pm,5.2561);
            double theta=(v[2]+273.15)/288.15;
            double sigma=theta>0?delta/theta:1.0;
            double tas=sigma>0?v[0]/sqrt(sigma):v[0];
            /* Mach */
            double a0=661.47, p0=29.92;
            double qc=p0*(pow(1.0+0.2*pow(v[0]/a0,2.0),3.5)-1.0);
            double ps=29.92*pow(1.0-6.8756e-6*pm,5.2558797);
            double mach=sqrt(5.0*(pow(qc/ps+1.0,2.0/7.0)-1.0));
            double isa=15.0-1.98*(v[1]/1000.0);
            double tat=(v[2]+273.15)*(1.0+0.2*mach*mach)-273.15;
            snprintf(s,sizeof(s),"%.0f kts",tas);              draw_result(4,"TAS",s);
            snprintf(s,sizeof(s),"%.3f",mach);                 draw_result(5,"Mach",s);
            snprintf(s,sizeof(s),"%.1f C",tat);                draw_result(6,"TAT (probe)",s);
            snprintf(s,sizeof(s),"%.1f C (ISA%+.0f)",isa,v[2]-isa);
            draw_result(7,"ISA Temp/Dev",s);
        }
    FL_END(3)
#undef TITLE
}

static void calc_reqcas(void)
{
#define TITLE "  REQUIRED CAS  "
    g_scr_help_title="REQUIRED CAS";
    g_scr_help_body=
        "Find CAS to fly for a target TAS.\n"
        "\n"
        "TAS: your desired true airspeed.\n"
        "Pressure Alt and OAT as inputs.\n"
        "\n"
        "Use when cruise speed is given\n"
        "in TAS but you fly by IAS/CAS.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[3]={0,0,0};
    const char *lab[3]={"TAS","Pressure Alt","OAT"};
    const char *un[3]={"kts","ft","C"};
    FL_START
        int i; for(i=0;i<3;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(3)
        {
            double pm=v[1]*0.3048;
            double delta=pow(1.0-6.8756e-6*pm,5.2561);
            double theta=(v[2]+273.15)/288.15;
            double sigma=theta>0?delta/theta:1.0;
            double cas=v[0]*sqrt(sigma);
            double a=(v[2]>-273.0)?38.967854*sqrt(v[2]+273.15):0.0;
            double mach=(a>0)?v[0]/a:0.0;
            snprintf(s,sizeof(s),"%.0f kts",cas);  draw_result(4,"Required CAS",s);
            snprintf(s,sizeof(s),"%.3f",mach);     draw_result(5,"Mach",s);
        }
    FL_END(3)
#undef TITLE
}

/* TAS from TAT – actual TAS using probe reading (iterative OAT solve) */
static void calc_tas_tat(void)
{
#define TITLE "  TAS FROM TAT  "
    g_scr_help_title="TAS FROM TAT";
    g_scr_help_body=
        "Actual TAS using probe reading.\n"
        "\n"
        "TAT (Total Air Temp) is warmer\n"
        "than OAT due to ram heating at\n"
        "speed. This screen iterates to\n"
        "find the real OAT, then TAS.\n"
        "\n"
        "More accurate than OAT method\n"
        "above ~150 kts.\n"
        "\n"
        "TAT / Probe Temp: thermometer\n"
        "reading from outside air probe.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[3]={0,0,0}; /* CAS kts, PAlt ft, TAT C */
    const char *lab[3]={"CAS","Pressure Alt","TAT / Probe Temp"};
    const char *un[3]={"kts","ft","C"};
    FL_START
        int i; for(i=0;i<3;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(3)
        {
            /* Iteratively solve for OAT from TAT and Mach */
            double pm=v[1]*0.3048;
            double delta=pow(1.0-6.8756e-6*pm,5.2561);
            double oat=v[2]; /* initial guess: OAT ≈ TAT */
            int it;
            for(it=0;it<10;it++){
                double theta=(oat+273.15)/288.15;
                double sigma=theta>0?delta/theta:1.0;
                double tas_g=sigma>0?v[0]/sqrt(sigma):v[0];
                double a=38.967854*sqrt(oat+273.15);
                double m=a>0?tas_g/a:0;
                double oat_new=(v[2]+273.15)/(1.0+0.2*m*m)-273.15;
                if(FABS(oat_new-oat)<0.01) { oat=oat_new; break; }
                oat=oat_new;
            }
            double theta=(oat+273.15)/288.15;
            double sigma=theta>0?delta/theta:1.0;
            double tas=sigma>0?v[0]/sqrt(sigma):v[0];
            double a=38.967854*sqrt(oat+273.15);
            double mach=a>0?tas/a:0;
            double isa=15.0-1.98*(v[1]/1000.0);
            snprintf(s,sizeof(s),"%.0f kts",tas);              draw_result(4,"TAS",s);
            snprintf(s,sizeof(s),"%.1f C",oat);                draw_result(5,"OAT (computed)",s);
            snprintf(s,sizeof(s),"%.3f",mach);                 draw_result(6,"Mach",s);
            snprintf(s,sizeof(s),"%.1f C (ISA%+.0f)",isa,oat-isa);
            draw_result(7,"ISA Temp/Dev",s);
        }
    FL_END(3)
#undef TITLE
}

static void menu_airspeed(void)
{
    const char *items[]={"True Airspeed (CAS->TAS)","Required CAS (TAS->CAS)","TAS from TAT (actual)"};
    static int _s=0;
    while(1){
        int r=sub_menu("  AIRSPEED  ",items,3,_s);
        if(r<0) return;
        _s=r;
        switch(r){
            case 0: calc_tas();     break;
            case 1: calc_reqcas();  break;
            case 2: calc_tas_tat(); break;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   WIND FUNCTIONS
   ══════════════════════════════════════════════════════════════ */

static void calc_wind(void)
{
#define TITLE "  WIND CORRECTION  "
    g_scr_help_title="WIND CORRECTION";
    g_scr_help_body=
        "Plan a leg with a crosswind.\n"
        "\n"
        "Wind From: direction wind is FROM\n"
        "           (degrees true).\n"
        "Wind Speed: wind speed in kts.\n"
        "True Course: desired track.\n"
        "TAS: true airspeed.\n"
        "\n"
        "Results: True HDG to fly, GS,\n"
        "and wind correction angle (WCA).\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[4]={0,0,0,0};
    const char *lab[4]={"Wind From","Wind Speed","True Course","TAS"};
    const char *un[4]={"deg","kts","deg","kts"};
    FL_START
        int i; for(i=0;i<4;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(4)
        {
            double ang=DEG2RAD(v[0]-v[2]);
            double xw=v[1]*sin(ang), hw=v[1]*cos(ang);
            double r2=v[3]>0?CLAMP(xw/v[3],-1.0,1.0):0;
            double wr=asin(r2), wca=RAD2DEG(wr);
            double gs=v[3]*cos(wr)-hw;
            double hdg=v[2]+wca;
            if(hdg<0)hdg+=360; if(hdg>=360)hdg-=360;
            snprintf(s,sizeof(s),"%.0f deg",hdg); draw_result(5,"True HDG",s);
            snprintf(s,sizeof(s),"%.0f kts",gs);  draw_result(6,"Ground Speed",s);
            snprintf(s,sizeof(s),"%.1f deg",wca); draw_result(7,"WCA",s);
        }
    FL_END(4)
#undef TITLE
}

static void calc_windcomp(void)
{
#define TITLE "  WIND COMPONENT  "
    g_scr_help_title="WIND COMPONENT";
    g_scr_help_body=
        "Headwind and crosswind for a\n"
        "specific runway.\n"
        "\n"
        "Wind Dir: reported wind direction\n"
        "           (magnetic degrees).\n"
        "Wind Speed: in knots.\n"
        "Runway: number only (eg: 27)\n"
        "         NOT the heading (270).\n"
        "\n"
        "Positive H Wnd = headwind.\n"
        "Negative H Wnd = tailwind.\n"
        "Positive X Wnd = from right.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[3]={0,0,0}; /* WSpd, WDir, Runway# */
    const char *lab[3]={"Wind Speed","Wind Dir","Runway Number"};
    const char *un[3]={"kts","deg",""};
    FL_START
        int i; for(i=0;i<3;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(3)
        {
            double rwy_hdg=v[2]*10.0;
            double angle=DEG2RAD(v[1]-rwy_hdg);
            double xwind=v[0]*sin(angle);
            double hwind=v[0]*cos(angle);
            const char *xside=xwind<0?"Left":"Right";
            const char *hside=hwind>=0?"Head":"Tail";
            snprintf(s,sizeof(s),"%.1f kts %s",FABS(xwind),xside);
            draw_result(4,"Crosswind",s);
            snprintf(s,sizeof(s),"%.1f kts %s",FABS(hwind),hside);
            draw_result(5,"Head/Tailwind",s);
        }
    FL_END(3)
#undef TITLE
}

static void calc_windfind(void)
{
#define TITLE "  IN-FLIGHT WIND  "
    g_scr_help_title="IN-FLIGHT WIND FIND";
    g_scr_help_body=
        "Find actual wind aloft while\n"
        "flying.\n"
        "\n"
        "Read from instruments:\n"
        "Ground Speed: from GPS or DME.\n"
        "TAS: from TAS screen or EFIS.\n"
        "True Course: GPS track.\n"
        "True HDG: heading indicator\n"
        "           (corrected for Var).\n"
        "\n"
        "Results: actual wind speed,\n"
        "direction, and correction angle.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[4]={0,0,0,0}; /* GS, TAS, TCrs, THdg */
    const char *lab[4]={"Ground Speed","TAS","True Course","True HDG"};
    const char *un[4]={"kts","kts","deg","deg"};
    FL_START
        int i; for(i=0;i<4;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(4)
        {
            double wx=v[1]*sin(DEG2RAD(v[3]))-v[0]*sin(DEG2RAD(v[2]));
            double wy=v[1]*cos(DEG2RAD(v[3]))-v[0]*cos(DEG2RAD(v[2]));
            double wspd=sqrt(wx*wx+wy*wy);
            double wdir=RAD2DEG(atan2(wx,wy));
            if(wdir<0)wdir+=360; if(wdir>=360)wdir-=360;
            double wca=v[3]-v[2];
            snprintf(s,sizeof(s),"%.0f kts",wspd); draw_result(5,"Wind Speed",s);
            snprintf(s,sizeof(s),"%.0f deg",wdir); draw_result(6,"Wind From",s);
            snprintf(s,sizeof(s),"%.1f deg",wca);  draw_result(7,"WCA",s);
        }
    FL_END(4)
#undef TITLE
}

static void menu_wind(void)
{
    const char *items[]={"Wind Correction (plan)","Wind Component (runway)","In-flight Wind Find"};
    static int _s=0;
    while(1){
        int r=sub_menu("  WIND  ",items,3,_s);
        if(r<0) return;
        _s=r;
        switch(r){
            case 0: calc_wind();     break;
            case 1: calc_windcomp(); break;
            case 2: calc_windfind(); break;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   NAVIGATION FUNCTIONS
   ══════════════════════════════════════════════════════════════ */

static void calc_tsd(void)
{
    double v[3]={0,0,0};
    const char *lab[3]={"Speed","Distance","Time"};
    const char *un[3]={"kts","nm","min"};
    int solve=2,sel=0; IS inp={0};
    uint8_t prev[8]={0},cur[8];
    g_scr_help_title="TIME / SPEED / DIST";
    g_scr_help_body=
        "Solve for any one of three:\n"
        "  Speed (kts), Distance (nm),\n"
        "  or Time (min).\n"
        "\n"
        "2ND cycles which to solve for\n"
        "(shown as 'solve' in the list).\n"
        "Enter the other two values.\n"
        "\n"
        "Speed result = Ground Speed\n"
        "when Distance and Time known.\n"
        "\n"
        "2ND=cycle solve  ENTER=edit\n"
        "UP/DN=move  CLR=back";
    while(1){
        draw_header("  TIME / SPEED / DIST  ");
        char s[48]; int i;
        for(i=0;i<3;i++){
            if(i==solve&&!inp.on) snprintf(s,sizeof(s),"(solve)");
            else is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on&&i==sel);
        }
        draw_divider(3);
        if(solve==2&&v[0]>0){
            double t=(v[1]/v[0])*60.0;
            snprintf(s,sizeof(s),"%.0f min (%dh%02dm)",(double)t,(int)(t/60),(int)fmod(t,60));
            draw_result(4,"Time",s);
        } else if(solve==1){
            snprintf(s,sizeof(s),"%.1f nm",v[0]*(v[2]/60.0)); draw_result(4,"Distance",s);
        } else if(solve==0&&v[2]>0){
            snprintf(s,sizeof(s),"%.0f kts",v[1]/(v[2]/60.0)); draw_result(4,"Speed",s);
        }
        gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,SCR_H-14,SCR_W,14);
        gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-15,SCR_W);
        gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
        gfx_PrintStringXY(inp.on?"[ENTER]=Done  [DEL]=Bksp"
                                 :"[UP/DN]=Sel  [ENTER]=Edit  [2ND]=Cycle Solve  [CLR]=Back",
                          MARGIN,SCR_H-12);
        gfx_BlitBuffer();
        kb_Scan(); for(i=0;i<8;i++) cur[i]=kb_Data[i];
        if(inp.on){int r=is_key(&inp,cur,prev);if(r==1)v[sel]=is_val(&inp);}
        else{
            if(KN(7,BK_UP)&&sel>0)sel--;
            if(KN(7,BK_DOWN)&&sel<2)sel++;
            if(KN(6,BK_ENT)){if(sel==solve)solve=(solve+1)%3;else is_start(&inp);}
            if(KN(1,BK_2ND))solve=(solve+1)%3;
            if(KN(6,BK_CLR)){memcpy(prev,cur,8);return;}
            if(KN(2,kb_Alpha)) show_screen_help();
        }
        memcpy(prev,cur,8);
    }
}

static void calc_glide(void)
{
    /* Solve for any one: Ratio, Distance(nm), Descent(ft) */
    double v[3]={0,0,0}; /* ratio, dist nm, desc ft */
    int solve=1; int sel=0; IS inp={0};
    uint8_t prev[8]={0},cur[8];
    g_scr_help_title="GLIDE RATIO";
    g_scr_help_body=
        "Solve for glide ratio, distance,\n"
        "or descent altitude.\n"
        "\n"
        "Glide Ratio: from POH (eg 10).\n"
        "Distance: horizontal in nm.\n"
        "Descent: altitude lost in ft.\n"
        "\n"
        "Press ENTER on the highlighted\n"
        "row to cycle which to solve.\n"
        "\n"
        "Use for engine-out glide range\n"
        "planning.\n"
        "\n"
        "ENTER=edit/cycle  UP/DN=move\n"
        "CLR=back";
    while(1){
        draw_header("  GLIDE RATIO  ");
        char s[48]; int i;
        const char *lab[3]={"Glide Ratio (X:1)","Distance","Descent"};
        const char *un[3]={":1","nm","ft"};
        for(i=0;i<3;i++){
            if(i==solve&&!inp.on) snprintf(s,sizeof(s),"(solve)");
            else is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on&&i==sel);
        }
        draw_divider(3);
        if(solve==1&&v[0]>0)
        { double d=(v[0]*v[2])/6076.12; snprintf(s,sizeof(s),"%.2f nm",d); draw_result(4,"Distance",s); }
        else if(solve==2&&v[1]>0&&v[0]>0)
        { double d=v[1]*6076.12/v[0]; snprintf(s,sizeof(s),"%.0f ft",d); draw_result(4,"Descent",s); }
        else if(solve==0&&v[2]>0)
        { double r=v[1]*6076.12/v[2]; snprintf(s,sizeof(s),"%.1f:1",r); draw_result(4,"Glide Ratio",s); }

        gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,SCR_H-14,SCR_W,14);
        gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-15,SCR_W);
        gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
        gfx_PrintStringXY(inp.on?"[ENTER]=Done  [DEL]=Bksp"
                                 :"[ENTER]=Edit  [2ND]=Cycle Solve  [CLR]=Back",
                          MARGIN,SCR_H-12);
        gfx_BlitBuffer();
        kb_Scan(); for(i=0;i<8;i++) cur[i]=kb_Data[i];
        if(inp.on){int r=is_key(&inp,cur,prev);if(r==1)v[sel]=is_val(&inp);}
        else{
            if(KN(7,BK_UP)&&sel>0)sel--;
            if(KN(7,BK_DOWN)&&sel<2)sel++;
            if(KN(6,BK_ENT)){if(sel==solve)solve=(solve+1)%3;else is_start(&inp);}
            if(KN(1,BK_2ND))solve=(solve+1)%3;
            if(KN(6,BK_CLR)){memcpy(prev,cur,8);return;}
            if(KN(2,kb_Alpha)) show_screen_help();
        }
        memcpy(prev,cur,8);
    }
}

static void calc_climbdesc(void)
{
#define TITLE "  CLIMB / DESCENT  "
    g_scr_help_title="CLIMB / DESCENT";
    g_scr_help_body=
        "Climb/descent performance.\n"
        "\n"
        "Distance: horizontal (nm).\n"
        "Vert Change: altitude gain/loss.\n"
        "Ground Speed: for rate calc.\n"
        "\n"
        "Gradient: feet per nautical mile\n"
        "  (eg 200 ft/nm for 3-deg GS).\n"
        "Ratio: horizontal to vertical.\n"
        "Rate (FPM): at given GS.\n"
        "\n"
        "3-deg glide slope = 318 ft/nm\n"
        "VASI/PAPI = ~300 ft/nm.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[3]={0,0,0}; /* GS kts, dist nm, vert ft */
    const char *lab[3]={"Ground Speed","Distance","Vert Change"};
    const char *un[3]={"kts","nm","ft"};
    FL_START
        int i; for(i=0;i<3;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(3)
        {
            double aoc=v[1]>0?v[2]/v[1]:0;          /* ft per nm */
            double roc=v[0]>0?(aoc*v[0])/60.0:0;    /* ft per min */
            double grade=v[1]>0?(v[2]/(v[1]*6076.12))*100.0:0;
            snprintf(s,sizeof(s),"%.0f ft/nm",aoc);  draw_result(4,"Angle (ft/NM)",s);
            snprintf(s,sizeof(s),"%.0f fpm",roc);    draw_result(5,"Rate (FPM)",s);
            snprintf(s,sizeof(s),"%.1f %%",grade);   draw_result(6,"Grade",s);
        }
    FL_END(3)
#undef TITLE
}

static void calc_tofrom(void)
{
#define TITLE "  RECIPROCAL HDG  "
    g_scr_help_title="RECIPROCAL HDG";
    g_scr_help_body=
        "Enter any course to find its\n"
        "opposite (reciprocal).\n"
        "\n"
        "If you are ON the 150 radial,\n"
        "the course TO the station is\n"
        "330 degrees.\n"
        "\n"
        "Also use for: runway opposite,\n"
        "back-course, or reversing any\n"
        "bearing.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[1]={0};
    const char *lab[1]={"Course FROM station"};
    const char *un[1]={"deg"};
    FL_START
        is_fmt(s,sizeof(s),&inp,0==sel,v[0],un[0]);
        draw_field(0,lab[0],s,0==sel,inp.on);
    FL_DIV(1)
        {
            double to=fmod(v[0]+180.0,360.0);
            snprintf(s,sizeof(s),"%.0f deg",to); draw_result(2,"Course TO station",s);
        }
    FL_END(1)
#undef TITLE
}

static void calc_compass(void)
{
#define TITLE "  COMPASS HEADING  "
    g_scr_help_title="COMPASS HEADING";
    g_scr_help_body=
        "True HDG to Magnetic to Compass.\n"
        "\n"
        "Variation: west = positive (+),\n"
        "           east = negative (-).\n"
        "  'East is least, west is best'\n"
        "\n"
        "Deviation: from your aircraft\n"
        "  compass correction card.\n"
        "\n"
        "MHdg = True HDG + Variation\n"
        "CHdg = MHdg + Deviation\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[3]={0,0,0}; /* True Hdg, Var (W+), Dev */
    const char *lab[3]={"True HDG","Variation (W=+,E=-)","Deviation"};
    const char *un[3]={"deg","deg","deg"};
    FL_START
        int i; for(i=0;i<3;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(3)
        {
            double mhdg=fmod(v[0]+v[1]+360.0,360.0);
            double chdg=fmod(mhdg+v[2]+360.0,360.0);
            snprintf(s,sizeof(s),"%.0f deg",mhdg); draw_result(4,"Magnetic HDG",s);
            snprintf(s,sizeof(s),"%.0f deg",chdg); draw_result(5,"Compass HDG",s);
        }
    FL_END(3)
#undef TITLE
}

static void calc_gs(void)
{
#define TITLE "  GROUND SPEED  "
    g_scr_help_title="GROUND SPEED";
    g_scr_help_body=
        "Simple ground speed calculator.\n"
        "\n"
        "Enter distance flown (nm) and\n"
        "time taken (minutes).\n"
        "\n"
        "Result: ground speed in knots\n"
        "and seconds per nautical mile.\n"
        "\n"
        "Tip: fly between two known fixes,\n"
        "time it, and enter here for an\n"
        "accurate in-flight GS check.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[2]={0,0}; /* dist nm, time min */
    const char *lab[2]={"Distance","Time"};
    const char *un[2]={"nm","min"};
    FL_START
        int i; for(i=0;i<2;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(2)
        if(v[1]>0){
            double gs=v[0]/(v[1]/60.0);
            double secpnm=v[0]>0?v[1]*60.0/v[0]:0;
            snprintf(s,sizeof(s),"%.0f kts",gs);       draw_result(3,"Ground Speed",s);
            if(secpnm>0){ snprintf(s,sizeof(s),"%.0f sec/nm",secpnm); draw_result(4,"Sec per NM",s); }
        }
    FL_END(2)
#undef TITLE
}

static void calc_hold(void)
{
    while(kb_AnyKey()) kb_Scan();
    static double hdg=0, ic=0;
    static int right=1;
    int sel=0, _rpt=0;
    IS inp={0}; uint8_t prev[8]={0},cur[8];
    g_scr_help_title="HOLDING PATTERN";
    g_scr_help_body=
        "FAA standard holding entry type.\n"
        "\n"
        "Aircraft HDG: current heading.\n"
        "Inbound Course: course TO fix\n"
        "  (NOT the holding radial).\n"
        "Turns: Right=standard, Left=non.\n"
        "\n"
        "Direct: cross fix, join pattern.\n"
        "Teardrop: cross, fly ~30deg to\n"
        "  non-hold side ~1min, turn in.\n"
        "Parallel: cross, fly outbound\n"
        "  parallel ~1min, turn to inbd.\n"
        "\n"
        "ENTER=toggle turns  UP/DN=move\n"
        "L/R arrow=set turn dir  CLR=back";
    while(1){
        draw_header("  HOLDING PATTERN  ");
        char s[48];
        /* field 0: aircraft heading */
        is_fmt(s,sizeof(s),&inp,sel==0,hdg,"deg");
        draw_field(0,"Aircraft HDG",s,sel==0,inp.on&&sel==0);
        /* field 1: hold inbound course */
        is_fmt(s,sizeof(s),&inp,sel==1,ic,"deg");
        draw_field(1,"Inbound Course",s,sel==1,inp.on&&sel==1);
        /* field 2: turn direction toggle */
        {
            int y=ROW_Y(2);
            if(sel==2){ gfx_SetColor(CI_SELBG); gfx_FillRectangle_NoClip(0,y-3,SCR_W,STEP); }
            gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(sel==2?CI_SELBG:CI_BG);
            gfx_PrintStringXY("Turns",MARGIN,y);
            gfx_SetTextFGColor(sel==2?CI_AMBER:CI_VALUE); gfx_SetTextBGColor(sel==2?CI_SELBG:CI_BG);
            gfx_PrintStringXY(right?"Right (std)":"Left",VAL_X,y);
        }
        draw_divider(3);
        {
            /* sector: θ = clockwise angle from inbound to aircraft hdg (right turns) */
            double rel=right ? fmod(hdg-ic+360.0,360.0) : fmod(ic-hdg+360.0,360.0);
            const char *entry;
            if(rel<=70.0||rel>250.0)   entry="Direct Entry";
            else if(rel<=180.0)        entry="Teardrop Entry";
            else                       entry="Parallel Entry";
            double outbound=fmod(ic+180.0,360.0);
            snprintf(s,sizeof(s),"%s",entry);         draw_result(4,"Entry Type",s);
            snprintf(s,sizeof(s),"%.0f deg",ic);      draw_result(5,"Fly Inbound",s);
            snprintf(s,sizeof(s),"%.0f deg",outbound);draw_result(6,"Fly Outbound",s);
        }
        draw_footer(inp.on);
        gfx_BlitBuffer();
        kb_Scan(); {int i;for(i=0;i<8;i++)cur[i]=kb_Data[i];}
        if(inp.on){
            int r=is_key(&inp,cur,prev);
            if(r==1){
                if(sel==0) hdg=fmod(is_val(&inp)+3600.0,360.0);
                if(sel==1) ic =fmod(is_val(&inp)+3600.0,360.0);
            }
        } else {
            if(cur[7]&(BK_UP|BK_DOWN)) _rpt++; else _rpt=0;
            if((KN(7,BK_UP)||((cur[7]&BK_UP)&&_rpt>18&&(_rpt%4==0)))&&sel>0) sel--;
            if((KN(7,BK_DOWN)||((cur[7]&BK_DOWN)&&_rpt>18&&(_rpt%4==0)))&&sel<2) sel++;
            if(KN(6,BK_ENT)){ if(sel==2) right=!right; else is_start(&inp); }
            if(sel==2&&KN(7,BK_LEFT))  right=0;
            if(sel==2&&KN(7,BK_RIGHT)) right=1;
            if(KN(6,BK_CLR)){ memcpy(prev,cur,8); return; }
            if(KN(2,kb_Alpha)) show_screen_help();
        }
        memcpy(prev,cur,8);
    }
}

static void menu_navigation(void)
{
    const char *items[]={"Time/Speed/Distance","Glide Ratio","Climb/Descent","Reciprocal HDG","Compass HDG","Ground Speed","Holding Pattern"};
    static int _s=0;
    while(1){
        int r=sub_menu("  NAVIGATION  ",items,7,_s);
        if(r<0) return;
        _s=r;
        switch(r){
            case 0: calc_tsd();        break;
            case 1: calc_glide();      break;
            case 2: calc_climbdesc();  break;
            case 3: calc_tofrom();     break;
            case 4: calc_compass();    break;
            case 5: calc_gs();         break;
            case 6: calc_hold();       break;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   FUEL & ETA
   ══════════════════════════════════════════════════════════════ */

static void calc_fuelburn(void) /* Rate + Time -> Volume */
{
#define TITLE "  FUEL BURN  "
    g_scr_help_title="FUEL BURN";
    g_scr_help_body=
        "Fuel consumed for a given\n"
        "burn rate and duration.\n"
        "\n"
        "Burn Rate: engine consumption\n"
        "           in gallons per hour.\n"
        "Duration: flight time (minutes).\n"
        "\n"
        "Result: fuel used in gallons.\n"
        "\n"
        "Use to estimate fuel needed\n"
        "for a specific leg duration.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[2]={0,0}; /* rate gph, dur min */
    const char *lab[2]={"Burn Rate","Duration"};
    const char *un[2]={"gph","min"};
    FL_START
        int i; for(i=0;i<2;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(2)
        {
            double vol=v[0]*(v[1]/60.0);
            double liters=vol*3.78541;
            snprintf(s,sizeof(s),"%.2f gal",vol);    draw_result(3,"Fuel Used",s);
            snprintf(s,sizeof(s),"%.1f L",liters);   draw_result(4,"Fuel Used",s);
            snprintf(s,sizeof(s),"%.1f lbs",vol*6.0);draw_result(5,"Weight (AvGas)",s);
        }
    FL_END(2)
#undef TITLE
}

static void calc_fuelrate(void) /* Volume + Time -> Rate */
{
#define TITLE "  FUEL RATE  "
    g_scr_help_title="FUEL RATE";
    g_scr_help_body=
        "Find fuel burn rate from actual\n"
        "fuel used and time.\n"
        "\n"
        "Fuel Used: gallons consumed.\n"
        "Duration: time flown (minutes).\n"
        "\n"
        "Result: burn rate in gph.\n"
        "\n"
        "Useful for updating your fuel\n"
        "planning with measured values\n"
        "after a cruise segment.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[2]={0,0}; /* vol gal, dur min */
    const char *lab[2]={"Fuel Used","Duration"};
    const char *un[2]={"gal","min"};
    FL_START
        int i; for(i=0;i<2;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(2)
        {
            double rate=v[1]>0?v[0]/(v[1]/60.0):0;
            snprintf(s,sizeof(s),"%.2f gph",rate);  draw_result(3,"Fuel Rate",s);
            snprintf(s,sizeof(s),"%.2f lph",rate*3.78541); draw_result(4,"Fuel Rate",s);
        }
    FL_END(2)
#undef TITLE
}

static void calc_endurance(void)
{
#define TITLE "  FUEL PLANNING  "
    g_scr_help_title="FUEL PLANNING";
    g_scr_help_body=
        "Plan fuel for a flight.\n"
        "\n"
        "Fuel on Board: total usable fuel.\n"
        "Fuel Burn: engine consumption.\n"
        "Ground Speed: planned cruise GS.\n"
        "Reserve: required fuel (min).\n"
        "\n"
        "Results: total endurance, range\n"
        "in nm, and fuel after reserve.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[4]={0,0,0,0};
    const char *lab[4]={"Fuel on Board","Fuel Burn","Ground Speed","Reserve"};
    const char *un[4]={"gal","gph","kts","min"};
    FL_START
        int i; for(i=0;i<4;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(4)
        {
            double endur=v[1]>0?v[0]/v[1]:0;
            double resg=v[1]*(v[3]/60.0);
            double usbl=v[0]-resg;
            double usblh=v[1]>0?usbl/v[1]:0;
            int eh=(int)endur,em=(int)(fmod(endur,1.0)*60);
            snprintf(s,sizeof(s),"%.0f nm (%dh%02dm)",endur*v[2],eh,em);
            draw_result(5,"Total Range",s);
            int uh=(int)usblh,um=(int)(fmod(usblh,1.0)*60);
            snprintf(s,sizeof(s),"%.0f nm (%dh%02dm)",usblh*v[2],uh,um);
            draw_result(6,"Range w/Reserve",s);
            if(usbl<0) draw_warn("NOT ENOUGH FUEL for reserve!");
        }
    FL_END(4)
#undef TITLE
}

static void calc_eta(void)
{
#define TITLE "  ETA CALCULATOR  "
    g_scr_help_title="ETA CALCULATOR";
    g_scr_help_body=
        "Compute estimated time of\n"
        "arrival.\n"
        "\n"
        "Dep Time - Hours: departure\n"
        "  hour in 24-hr UTC (Zulu).\n"
        "Dep Time - Mins: departure\n"
        "  minute.\n"
        "Flight Time: total time in min.\n"
        "\n"
        "Result: ETA in UTC.\n"
        "\n"
        "Always use Zulu time for flight\n"
        "planning to avoid confusion.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    /* v[0]=dep hours(0-23), v[1]=dep minutes(0-59), v[2]=flight duration(min) */
    double v[3]={0,0,0};
    const char *lab[3]={"Dep Time - Hours","Dep Time - Minutes","Flight Time"};
    const char *un[3]={"hr","min","min"};
    FL_START
        int i; for(i=0;i<3;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(3)
        {
            double dep_hrs=v[0]+v[1]/60.0;
            double eta_hrs=dep_hrs+v[2]/60.0;
            char dep_s[12], eta_s[12];
            fmt_hhmm(dep_s,sizeof(dep_s),dep_hrs);
            fmt_hhmm(eta_s,sizeof(eta_s),eta_hrs);
            draw_result(4,"Departure",dep_s);
            draw_result(5,"ETA",eta_s);
            int fh=(int)(v[2]/60), fm=(int)fmod(v[2],60.0);
            snprintf(s,sizeof(s),"%dh %02dm",fh,fm);
            draw_result(6,"Flight Time",s);
        }
    FL_END(3)
#undef TITLE
}

static void menu_fuel(void)
{
    const char *items[]={"Fuel Planning / Endurance","Fuel Burn (Rate+Time->Vol)","Fuel Rate (Vol+Time->Rate)","ETA Calculator"};
    static int _s=0;
    while(1){
        int r=sub_menu("  FUEL & ETA  ",items,4,_s);
        if(r<0) return;
        _s=r;
        switch(r){
            case 0: calc_endurance(); break;
            case 1: calc_fuelburn();  break;
            case 2: calc_fuelrate();  break;
            case 3: calc_eta();       break;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   WEIGHT & BALANCE
   ══════════════════════════════════════════════════════════════ */

/* ── W&B persistent storage ───────────────────────────────────── */
#define WB_MAX       20
#define WB_VISIBLE    7   /* rows shown at once */
#define WB_NAME_LEN   8   /* 7 chars + null */
#define WB_APPVAR     "E6BWB"
#define THEME_APPVAR  "E6BTH"

/* fuel[i]: 0=lbs, 1=AvGas gal (6.0 lbs/gal), 2=Jet-A gal (6.84 lbs/gal) */
typedef struct {
    uint8_t n;
    char    names[WB_MAX][WB_NAME_LEN];
    double  wt[WB_MAX];   /* gallons when fuel[i]>0, else lbs */
    double  arm[WB_MAX];
    uint8_t fuel[WB_MAX];
} WBSave;

static WBSave wbd; /* static – not on stack */

static double wb_lbs(int i)
{
    if(wbd.fuel[i]==1) return wbd.wt[i]*6.0;
    if(wbd.fuel[i]==2) return wbd.wt[i]*6.84;
    return wbd.wt[i];
}

static void wb_save(void)
{
    ti_var_t f=ti_Open(WB_APPVAR,"w");
    if(f){ ti_Write(&wbd,sizeof(WBSave),1,f); ti_Close(f); }
}

static void wb_defaults(void)
{
    int i;
    wbd.n=7;
    for(i=0;i<WB_MAX;i++){wbd.wt[i]=0;wbd.arm[i]=0;wbd.fuel[i]=0;wbd.names[i][0]='\0';}
    /* name, default wt/arm, fuel flag */
    strncpy(wbd.names[0],"Emp Wt ",WB_NAME_LEN); wbd.wt[0]=1495; wbd.arm[0]=101.5;
    strncpy(wbd.names[1],"Fuel   ",WB_NAME_LEN); wbd.wt[1]=0;    wbd.arm[1]=95.0; wbd.fuel[1]=1;
    strncpy(wbd.names[2],"Pilot  ",WB_NAME_LEN); wbd.wt[2]=0;    wbd.arm[2]=85.5;
    strncpy(wbd.names[3],"Co-Plt ",WB_NAME_LEN); wbd.wt[3]=0;    wbd.arm[3]=85.5;
    strncpy(wbd.names[4],"Pax 1  ",WB_NAME_LEN); wbd.wt[4]=0;    wbd.arm[4]=121.0;
    strncpy(wbd.names[5],"Pax 2  ",WB_NAME_LEN); wbd.wt[5]=0;    wbd.arm[5]=121.0;
    strncpy(wbd.names[6],"Bags   ",WB_NAME_LEN); wbd.wt[6]=0;    wbd.arm[6]=150.0;
}

static void wb_load(void)
{
    ti_var_t f=ti_Open(WB_APPVAR,"r");
    if(f && ti_GetSize(f)==(uint16_t)sizeof(WBSave)){
        ti_Read(&wbd,sizeof(WBSave),1,f); ti_Close(f);
    } else {
        if(f) ti_Close(f);
        wb_defaults();
    }
}

static void theme_save(void)
{
    ti_var_t f=ti_Open(THEME_APPVAR,"w");
    if(f){ uint8_t t=(uint8_t)g_theme; ti_Write(&t,1,1,f); ti_Close(f); }
}

static void theme_load(void)
{
    ti_var_t f=ti_Open(THEME_APPVAR,"r");
    if(f){ uint8_t t=0; ti_Read(&t,1,1,f); ti_Close(f);
           g_theme=(t<3)?(int)t:0; }
}

/* ── Rename dialog: ALPHA+key for letters, numbers direct ──────── */
static void wb_rename_dialog(char *name)
{
    char orig[WB_NAME_LEN];
    int i, pos=0, alpha_mode=0;
    memcpy(orig,name,WB_NAME_LEN);
    for(i=(int)strlen(name);i<WB_NAME_LEN-1;i++) name[i]=' ';
    name[WB_NAME_LEN-1]='\0';

    uint8_t prev[8]={0},cur[8];
    while(1){
        gfx_SetColor(CI_HDR);
        gfx_FillRectangle_NoClip(24,80,272,82);
        gfx_SetColor(CI_AMBER);
        gfx_Rectangle_NoClip(24,80,272,82);

        gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_HDR);
        gfx_PrintStringXY("  RENAME STATION  ",68,86);

        /* Alpha indicator */
        if(alpha_mode){
            gfx_SetTextFGColor(CI_GREEN); gfx_SetTextBGColor(CI_HDR);
            gfx_PrintStringXY("ALPHA",214,86);
        }

        /* Name + cursor */
        int nx=80, ny=104;
        gfx_SetTextFGColor(CI_AMBER); gfx_SetTextBGColor(CI_HDR);
        gfx_PrintStringXY(name,nx,ny);
        gfx_SetColor(CI_AMBER);
        gfx_HorizLine_NoClip(nx+pos*8,ny+10,8);

        gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_HDR);
        if(alpha_mode)
            gfx_PrintStringXY("Press letter key to type",36,122);
        else
            gfx_PrintStringXY("ALPHA=Type  UP/DN=Cycle  L/R=Move",36,122);
        gfx_PrintStringXY("ENT=Save  DEL=Bksp  CLR=Cancel",36,134);

        gfx_BlitBuffer();
        kb_Scan();
        for(i=0;i<8;i++) cur[i]=kb_Data[i];

        if(KN(6,BK_ENT)){memcpy(prev,cur,8);return;}
        if(KN(6,BK_CLR)){memcpy(name,orig,WB_NAME_LEN);memcpy(prev,cur,8);return;}

        /* ALPHA key toggles alpha mode */
        if(KN(2,kb_Alpha)){ alpha_mode=!alpha_mode; }
        /* DEL = backspace: remove char before cursor, shift left */
        else if(KN(1,BK_DEL)&&pos>0){
            pos--;
            {int j;for(j=pos;j<WB_NAME_LEN-2;j++) name[j]=name[j+1];}
            name[WB_NAME_LEN-2]=' ';
        }
        else if(alpha_mode){
            /* Direct letter input from ALPHA secondary key labels */
            char ch=0;
            if     (KN(2,kb_Math))    ch='A';
            else if(KN(3,kb_Apps))    ch='B';
            else if(KN(4,kb_Prgm))   ch='C';
            else if(KN(2,kb_Recip))  ch='D';
            else if(KN(3,kb_Sin))    ch='E';
            else if(KN(4,kb_Cos))    ch='F';
            else if(KN(5,kb_Tan))    ch='G';
            else if(KN(6,kb_Power))  ch='H';
            else if(KN(2,kb_Square)) ch='I';
            else if(KN(3,kb_Comma))  ch='J';
            else if(KN(4,kb_LParen)) ch='K';
            else if(KN(5,kb_RParen)) ch='L';
            else if(KN(6,kb_Div))    ch='M';
            else if(KN(2,kb_Log))    ch='N';
            else if(KN(3,kb_7))      ch='O';
            else if(KN(4,kb_8))      ch='P';
            else if(KN(5,kb_9))      ch='Q';
            else if(KN(6,kb_Mul))    ch='R';
            else if(KN(2,kb_Ln))     ch='S';
            else if(KN(3,kb_4))      ch='T';
            else if(KN(4,kb_5))      ch='U';
            else if(KN(5,kb_6))      ch='V';
            else if(KN(6,kb_Sub))    ch='W';
            else if(KN(2,kb_Sto))    ch='X';
            else if(KN(3,kb_1))      ch='Y';
            else if(KN(4,kb_2))      ch='Z';
            else if(KN(3,kb_0))      ch=' ';
            if(ch&&pos<WB_NAME_LEN-1){
                name[pos]=ch;
                if(pos<WB_NAME_LEN-2) pos++;
                alpha_mode=0;
            }
            /* arrows still work in alpha mode */
            if(KN(7,BK_LFT)){if(pos>0)pos--;alpha_mode=0;}
            if(KN(7,BK_RGT)){if(pos<WB_NAME_LEN-2)pos++;alpha_mode=0;}
        } else {
            /* Normal mode: number keys type digits directly */
            char dg=0;
            if     (KN(3,kb_0)) dg='0';
            else if(KN(3,kb_1)) dg='1';
            else if(KN(3,kb_4)) dg='4';
            else if(KN(3,kb_7)) dg='7';
            else if(KN(4,kb_2)) dg='2';
            else if(KN(4,kb_5)) dg='5';
            else if(KN(4,kb_8)) dg='8';
            else if(KN(5,kb_3)) dg='3';
            else if(KN(5,kb_6)) dg='6';
            else if(KN(5,kb_9)) dg='9';
            if(dg&&pos<WB_NAME_LEN-1){
                name[pos]=dg;
                if(pos<WB_NAME_LEN-2) pos++;
            } else {
                /* UP/DOWN cycles through character set */
                const char cs[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.";
                int nc=(int)(sizeof(cs)-1), ci=0;
                {int j;for(j=0;j<nc;j++)if(cs[j]==name[pos]){ci=j;break;}}
                if(KN(7,BK_UP))   name[pos]=cs[(ci+1)%nc];
                if(KN(7,BK_DOWN)) name[pos]=cs[(ci-1+nc)%nc];
            }
            if(KN(7,BK_LFT)&&pos>0)             pos--;
            if(KN(7,BK_RGT)&&pos<WB_NAME_LEN-2) pos++;
        }
        memcpy(prev,cur,8);
    }
}

/* Layout constants */
#define WB_COLHDR_Y  30
#define WB_DATA_Y    46
#define WB_ROW_H     17
#define WB_X_NAME     8
#define WB_X_WT      95
#define WB_X_ARM    178
#define WB_X_MOM    258

/* Layout constants for W&B screen */
#define WB_COLHDR_Y  30   /* column header row                   */
#define WB_DATA_Y    46   /* first data row (buffer below title) */
#define WB_ROW_H     17   /* pixels per station row              */
#define WB_X_NAME    8
#define WB_X_WT      95
#define WB_X_ARM    178
#define WB_X_MOM    258

static void calc_wb(void)
{
    wb_load();
    while(kb_AnyKey()) kb_Scan();
    int row=0, col=0, scroll=0, _rpt=0;
    IS inp={0};
    uint8_t prev[8]={0},cur[8];
    g_scr_help_title="WEIGHT AND BALANCE";
    g_scr_help_body=
        "Enter weight and arm for each\n"
        "station to compute CG.\n"
        "\n"
        "ALPHA: toggle fuel type for\n"
        "  selected row (lbs / AvG gal\n"
        "  / JtA gal). Fuel rows convert\n"
        "  gallons to pounds automatically.\n"
        "\n"
        "Results: gross weight, total\n"
        "moment, and CG location.\n"
        "\n"
        "Compare CG to your POH envelope\n"
        "limits (forward/aft limits).\n"
        "\n"
        "UP/DN=move  ENTER=edit\n"
        "ALPHA=fuel type  CLR=back";

    while(1){
        draw_header("  WEIGHT & BALANCE  ");

        /* Column headers */
        gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
        gfx_SetTextScale(1,1);
        gfx_PrintStringXY("Station",WB_X_NAME,WB_COLHDR_Y);
        gfx_PrintStringXY("Weight", WB_X_WT,  WB_COLHDR_Y);
        gfx_PrintStringXY("Arm",    WB_X_ARM, WB_COLHDR_Y);
        gfx_PrintStringXY("Moment", WB_X_MOM, WB_COLHDR_Y);
        gfx_SetColor(CI_DIV);
        gfx_HorizLine_NoClip(MARGIN,WB_COLHDR_Y+10,SCR_W-MARGIN*2);

        char s[32];
        int i;
        double totw=0, totm=0;
        /* accumulate all stations for totals */
        for(i=0;i<wbd.n;i++){ double lb=wb_lbs(i); totw+=lb; totm+=lb*wbd.arm[i]; }

        /* Draw visible station rows */
        int visible_end=scroll+WB_VISIBLE;
        int list_len=wbd.n+1; /* +1 for Add Station row */
        for(i=scroll; i<list_len && i<scroll+WB_VISIBLE; i++){
            int y=WB_DATA_Y+(i-scroll)*WB_ROW_H;
            int sr=(i==row);
            if(sr){ gfx_SetColor(CI_SELBG); gfx_FillRectangle_NoClip(0,y-2,SCR_W,WB_ROW_H); }

            if(i==wbd.n){
                /* Add Station row */
                gfx_SetTextFGColor(sr?CI_AMBER:CI_DIV);
                gfx_SetTextBGColor(sr?CI_SELBG:CI_BG);
                gfx_PrintStringXY("[+] Add Station",WB_X_NAME,y);
            } else {
                double lbs=wb_lbs(i);
                double mom=lbs*wbd.arm[i];
                int wsel=(sr&&col==0), asel=(sr&&col==1);

                /* Name */
                gfx_SetTextBGColor(sr?CI_SELBG:CI_BG);
                gfx_SetTextFGColor(sr?CI_AMBER:CI_LABEL);
                gfx_PrintStringXY(wbd.names[i],WB_X_NAME,y);

                /* Fuel type indicator after name */
                if(wbd.fuel[i]>0){
                    gfx_SetTextFGColor(CI_GREEN);
                    gfx_SetTextBGColor(sr?CI_SELBG:CI_BG);
                    gfx_PrintStringXY(wbd.fuel[i]==1?"AvG":"JtA",WB_X_NAME+56,y);
                }

                /* Weight / gallons */
                if(wsel&&inp.on){
                    gfx_SetColor(CI_EDITBG);
                    gfx_FillRectangle_NoClip(WB_X_WT-2,y-2,76,WB_ROW_H);
                    gfx_SetTextFGColor(CI_AMBER); gfx_SetTextBGColor(CI_EDITBG);
                    snprintf(s,sizeof(s),"%s%s_",inp.neg?"-":"",inp.buf);
                } else {
                    gfx_SetTextFGColor(wsel?CI_AMBER:CI_VALUE);
                    gfx_SetTextBGColor(sr?CI_SELBG:CI_BG);
                    if(wbd.fuel[i]>0)
                        snprintf(s,sizeof(s),"%.1f gal",wbd.wt[i]);
                    else
                        snprintf(s,sizeof(s),"%.0f",wbd.wt[i]);
                }
                gfx_PrintStringXY(s,WB_X_WT,y);

                /* Arm */
                if(asel&&inp.on){
                    gfx_SetColor(CI_EDITBG);
                    gfx_FillRectangle_NoClip(WB_X_ARM-2,y-2,70,WB_ROW_H);
                    gfx_SetTextFGColor(CI_AMBER); gfx_SetTextBGColor(CI_EDITBG);
                    snprintf(s,sizeof(s),"%s%s_",inp.neg?"-":"",inp.buf);
                } else {
                    gfx_SetTextFGColor(asel?CI_AMBER:CI_VALUE);
                    gfx_SetTextBGColor(sr?CI_SELBG:CI_BG);
                    snprintf(s,sizeof(s),"%.1f",wbd.arm[i]);
                }
                gfx_PrintStringXY(s,WB_X_ARM,y);

                /* Moment */
                gfx_SetTextFGColor(CI_GREEN);
                gfx_SetTextBGColor(sr?CI_SELBG:CI_BG);
                snprintf(s,sizeof(s),"%.0f",mom);
                gfx_PrintStringXY(s,WB_X_MOM,y);
            }
        }

        /* Scroll indicator */
        if(scroll>0){
            gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
            gfx_PrintStringXY("^",SCR_W-14,WB_DATA_Y);
        }
        if(visible_end<list_len){
            gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
            gfx_PrintStringXY("v",SCR_W-14,WB_DATA_Y+(WB_VISIBLE-1)*WB_ROW_H);
        }

        /* Totals */
        int ty=WB_DATA_Y+WB_VISIBLE*WB_ROW_H+2;
        gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(MARGIN,ty-2,SCR_W-MARGIN*2);
        double cg=totw>0?totm/totw:0;

        gfx_SetTextBGColor(CI_BG);
        gfx_SetTextFGColor(CI_AMBER);
        gfx_PrintStringXY("TOTAL",WB_X_NAME,ty+2);
        gfx_SetTextFGColor(CI_GREEN);
        snprintf(s,sizeof(s),"%.0f lbs",totw);
        gfx_PrintStringXY(s,WB_X_WT,ty+2);
        snprintf(s,sizeof(s),"%.0f lb-in",totm);
        gfx_PrintStringXY(s,WB_X_ARM-2,ty+2);

        gfx_SetTextFGColor(CI_LABEL);
        gfx_PrintStringXY("CG",WB_X_NAME,ty+15);
        gfx_SetTextFGColor(CI_AMBER);
        snprintf(s,sizeof(s),"%.2f in",cg);
        gfx_PrintStringXY(s,WB_X_WT,ty+15);

        /* Footer */
        gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,SCR_H-14,SCR_W,14);
        gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-15,SCR_W);
        gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
        gfx_PrintStringXY(inp.on?"CLR=Cancel":"ALPHA=Fuel  MODE=Help  CLR=Back",MARGIN,SCR_H-12);
        gfx_BlitBuffer();

        kb_Scan(); for(i=0;i<8;i++) cur[i]=kb_Data[i];

        if(inp.on){
            int r=is_key(&inp,cur,prev);
            if(r==1){
                if(col==0) wbd.wt[row]=is_val(&inp);
                else       wbd.arm[row]=is_val(&inp);
                wb_save();
            }
        } else {
            int max_row=wbd.n; /* wbd.n = index of "Add Station" row */
            if(cur[7]&(BK_UP|BK_DOWN)) _rpt++; else _rpt=0;
            if((KN(7,BK_UP)||((cur[7]&BK_UP)&&_rpt>18&&(_rpt%4==0)))&&row>0){
                row--;
                if(row<scroll) scroll--;
            }
            if((KN(7,BK_DOWN)||((cur[7]&BK_DOWN)&&_rpt>18&&(_rpt%4==0)))&&row<max_row){
                row++;
                if(row>=scroll+WB_VISIBLE) scroll++;
            }
            if(KN(7,BK_LFT)&&row<wbd.n) col=0;
            if(KN(7,BK_RGT)&&row<wbd.n) col=1;

            if(KN(6,BK_ENT)){
                if(row==wbd.n){
                    /* Add new station */
                    if(wbd.n<WB_MAX){
                        snprintf(wbd.names[wbd.n],WB_NAME_LEN,"Extra%d",wbd.n+1);
                        wbd.wt[wbd.n]=0; wbd.arm[wbd.n]=0; wbd.fuel[wbd.n]=0;
                        wbd.n++;
                        wb_save();
                    }
                } else {
                    is_start(&inp);
                }
            }

            /* [2ND] = rename */
            if(KN(1,BK_2ND)&&row<wbd.n){
                wb_rename_dialog(wbd.names[row]);
                wb_save();
            }

            /* [DEL] key = delete station */
            if(KN(1,BK_DEL)&&row<wbd.n&&wbd.n>1){
                int j;
                for(j=row;j<wbd.n-1;j++){
                    memcpy(wbd.names[j],wbd.names[j+1],WB_NAME_LEN);
                    wbd.wt[j]=wbd.wt[j+1];
                    wbd.arm[j]=wbd.arm[j+1];
                    wbd.fuel[j]=wbd.fuel[j+1];
                }
                wbd.n--;
                if(row>=wbd.n&&row>0){ row--; if(row<scroll&&scroll>0)scroll--; }
                wb_save();
            }

            /* [ALPHA] (group 2 bit 7) = cycle fuel type on weight column */
            if(KN(2,(1<<7))&&row<wbd.n&&col==0){
                wbd.fuel[row]=(wbd.fuel[row]+1)%3;
                wb_save();
            }

            /* [MODE] = help overlay */
            if(KN(1,BK_MOD)) show_wb_help();

            if(KN(6,BK_CLR)){memcpy(prev,cur,8);return;}
        }
        memcpy(prev,cur,8);
    }
}

static void calc_wshift(void)
{
#define TITLE "  WEIGHT SHIFT  "
    g_scr_help_title="WEIGHT SHIFT";
    g_scr_help_body=
        "New CG after moving a weight.\n"
        "\n"
        "Item Wt: weight of item moved.\n"
        "Total Wt: aircraft gross weight.\n"
        "Current CG: starting CG (in).\n"
        "Shift Dist: distance moved.\n"
        "  Positive = moved forward.\n"
        "\n"
        "CG Change = (Item x Dist)/Total\n"
        "New CG = Current CG + Change\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    /* v[0]=Item Wt, v[1]=Total Wt, v[2]=Current CG, v[3]=Dist Moved */
    double v[4]={0,0,0,0};
    const char *lab[4]={"Item Wt","Total Wt","Current CG","Shift Distance"};
    const char *un[4]={"lbs","lbs","in","in"};
    FL_START
        int i; for(i=0;i<4;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(4)
        {
            /* CG change = (Item Wt × Shift Dist) / Total Wt */
            double dcg = (v[1]>0) ? (v[0]*v[3])/v[1] : 0;
            double newcg = v[2] + dcg;
            snprintf(s,sizeof(s),"%+.2f in",dcg); draw_result(5,"CG Change",s);
            snprintf(s,sizeof(s),"%.2f in",newcg); draw_result(6,"New CG",s);
        }
    FL_END(4)
#undef TITLE
}

static void calc_pmac(void)
{
#define TITLE "  PERCENT MAC  "
    g_scr_help_title="PERCENT MAC";
    g_scr_help_body=
        "CG as a percent of the Mean\n"
        "Aerodynamic Chord.\n"
        "\n"
        "MAC Length: chord length (in).\n"
        "CG Location: CG from datum (in).\n"
        "LMAC: leading edge of MAC (in).\n"
        "\n"
        "%MAC = (CG - LMAC)/MAC * 100\n"
        "\n"
        "Required by jets and turboprops\n"
        "instead of an absolute CG in.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[3]={0,0,0}; /* MAC len, CG, LMAC */
    const char *lab[3]={"MAC Length","CG Location","LMAC"};
    const char *un[3]={"in","in","in"};
    FL_START
        int i; for(i=0;i<3;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(3)
        {
            double pmac=v[0]>0?((v[1]-v[2])/v[0])*100.0:0;
            snprintf(s,sizeof(s),"%.1f %%",pmac); draw_result(4,"%MAC",s);
        }
    FL_END(3)
#undef TITLE
}

/* CX-3 style: given ΔCG desired + arm distance → weight needed to move */
static void calc_wshift_needed(void)
{
#define TITLE "  WT NEEDED TO SHIFT CG  "
    g_scr_help_title="WT NEEDED TO SHIFT CG";
    g_scr_help_body=
        "Find weight to move to hit a\n"
        "desired CG shift.\n"
        "\n"
        "Total Wt: aircraft gross weight.\n"
        "Desired CG Change: shift needed\n"
        "  in inches.\n"
        "Shift Distance: how far you\n"
        "  will move the item (inches).\n"
        "\n"
        "Wt = (Total x CG Change) / Arm\n"
        "\n"
        "Example: shift CG 0.5in by\n"
        "moving bags 40in: enter those\n"
        "values to find the bag weight.\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";
    double v[3]={0,0,0}; /* Total Wt, Desired ΔCG, Shift Arm */
    const char *lab[3]={"Total Wt","Desired CG Change","Shift Distance"};
    const char *un[3]={"lbs","in","in"};
    FL_START
        int i; for(i=0;i<3;i++){
            is_fmt(s,sizeof(s),&inp,i==sel,v[i],un[i]);
            draw_field(i,lab[i],s,i==sel,inp.on);
        }
    FL_DIV(3)
        {
            /* Item Wt = (Total Wt × ΔCG) / Shift Arm */
            double wt=(v[2]>0)?(v[0]*v[1])/v[2]:0;
            snprintf(s,sizeof(s),"%.1f lbs",wt); draw_result(4,"Wt to Move",s);
        }
    FL_END(3)
#undef TITLE
}

static void menu_wb(void)
{
    const char *items[]={"Weight & Balance","Wt Shift (new CG)","Wt Needed (shift CG)","%MAC"};
    static int _s=0;
    while(1){
        int r=sub_menu("  WT & BALANCE  ",items,4,_s);
        if(r<0) return;
        _s=r;
        switch(r){
            case 0: calc_wb();            break;
            case 1: calc_wshift();        break;
            case 2: calc_wshift_needed(); break;
            case 3: calc_pmac();          break;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   UNIT CONVERSIONS  (12 categories)
   ══════════════════════════════════════════════════════════════ */

static void calc_conv(void)
{
    while(kb_AnyKey()) kb_Scan();
    double val=1.0; int cat=0;
    const char *cats[12]={
        "DISTANCE","SPEED","TEMPERATURE","ALTITUDE",
        "PRESSURE","VOLUME","WEIGHT","RATE OF CLIMB",
        "ANGLE OF CLIMB","FUEL RATE","DURATION","ANGLE"
    };
    IS inp={0};
    uint8_t prev[8]={0},cur[8];
    while(1){
        draw_header("  UNIT CONVERSIONS  ");
        char s[48];
        /* Category */
        gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
        gfx_PrintStringXY("Category",MARGIN+10,ROW_Y(0));
        gfx_SetTextFGColor(CI_AMBER);
        gfx_PrintStringXY(cats[cat],VAL_X,ROW_Y(0));
        /* Value */
        if(inp.on) snprintf(s,sizeof(s),"%s%s_",inp.neg?"-":"",inp.buf);
        else       snprintf(s,sizeof(s),"%.6g",val);
        draw_field(1,"Value",s,1,inp.on);
        draw_divider(2);

        int r=3;
        switch(cat){
        case 0: /* Distance */
            snprintf(s,sizeof(s),"%.3f sm",  val*1.15078);   draw_result(r++,"nm > sm",s);
            snprintf(s,sizeof(s),"%.3f km",  val*1.852);     draw_result(r++,"nm > km",s);
            snprintf(s,sizeof(s),"%.0f ft",  val*6076.12);   draw_result(r++,"nm > ft",s);
            snprintf(s,sizeof(s),"%.0f m",   val*1852.0);    draw_result(r++,"nm > m",s);
            snprintf(s,sizeof(s),"%.3f nm",  val/1.15078);   draw_result(r++,"sm > nm",s);
            break;
        case 1: /* Speed */
            snprintf(s,sizeof(s),"%.2f mph", val*1.15078);   draw_result(r++,"kts > mph",s);
            snprintf(s,sizeof(s),"%.2f kph", val*1.852);     draw_result(r++,"kts > kph",s);
            snprintf(s,sizeof(s),"%.2f kts", val/1.15078);   draw_result(r++,"mph > kts",s);
            snprintf(s,sizeof(s),"%.2f kts", val/1.852);     draw_result(r++,"kph > kts",s);
            break;
        case 2: /* Temperature */
            snprintf(s,sizeof(s),"%.1f F",   val*9.0/5+32);  draw_result(r++,"C > F",s);
            snprintf(s,sizeof(s),"%.1f C",   (val-32)*5.0/9);draw_result(r++,"F > C",s);
            snprintf(s,sizeof(s),"%.2f K",   val+273.15);    draw_result(r++,"C > K",s);
            break;
        case 3: /* Altitude */
            snprintf(s,sizeof(s),"%.1f m",   val*0.3048);    draw_result(r++,"ft > m",s);
            snprintf(s,sizeof(s),"%.1f ft",  val/0.3048);    draw_result(r++,"m > ft",s);
            snprintf(s,sizeof(s),"%.4f nm",  val/6076.12);   draw_result(r++,"ft > nm",s);
            break;
        case 4: /* Pressure */
            snprintf(s,sizeof(s),"%.2f mb",  val*33.8639);   draw_result(r++,"inHg > mb",s);
            snprintf(s,sizeof(s),"%.4f inHg",val/33.8639);   draw_result(r++,"mb > inHg",s);
            snprintf(s,sizeof(s),"%.2f hPa", val*33.8639);   draw_result(r++,"inHg > hPa",s);
            break;
        case 5: /* Volume */
            snprintf(s,sizeof(s),"%.3f L",   val*3.78541);   draw_result(r++,"gal > L",s);
            snprintf(s,sizeof(s),"%.3f gal", val/3.78541);   draw_result(r++,"L > gal",s);
            break;
        case 6: /* Weight */
            snprintf(s,sizeof(s),"%.3f kg",  val*0.453592);  draw_result(r++,"lbs > kg",s);
            snprintf(s,sizeof(s),"%.3f lbs", val/0.453592);  draw_result(r++,"kg > lbs",s);
            snprintf(s,sizeof(s),"%.2f lbs", val*6.0);       draw_result(r++,"gal AvGas > lbs",s);
            snprintf(s,sizeof(s),"%.2f lbs", val*6.84);      draw_result(r++,"gal JetA > lbs",s);
            break;
        case 7: /* Rate of Climb */
            snprintf(s,sizeof(s),"%.2f m/s", val*0.00508);   draw_result(r++,"fpm > m/s",s);
            snprintf(s,sizeof(s),"%.0f fpm", val/0.00508);   draw_result(r++,"m/s > fpm",s);
            break;
        case 8: /* Angle of Climb */
            snprintf(s,sizeof(s),"%.2f ft/sm",val/1.15078);  draw_result(r++,"ft/nm > ft/sm",s);
            snprintf(s,sizeof(s),"%.2f ft/nm",val*1.15078);  draw_result(r++,"ft/sm > ft/nm",s);
            break;
        case 9: /* Fuel Rate */
            snprintf(s,sizeof(s),"%.3f lph",  val*3.78541);  draw_result(r++,"gph > lph",s);
            snprintf(s,sizeof(s),"%.3f gph",  val/3.78541);  draw_result(r++,"lph > gph",s);
            snprintf(s,sizeof(s),"%.2f lbs/h",val*6.0);      draw_result(r++,"gph > lbs/hr",s);
            break;
        case 10: /* Duration */
            { double h=val/60.0; snprintf(s,sizeof(s),"%.4f hrs",h); draw_result(r++,"min > hrs",s); }
            { double m=val*60.0; snprintf(s,sizeof(s),"%.1f min",m); draw_result(r++,"hrs > min",s); }
            { double sec=val*3600; snprintf(s,sizeof(s),"%.0f sec",sec); draw_result(r++,"hrs > sec",s); }
            break;
        case 11: /* Angle */
            {
                double deg=floor(val);
                double min_f=(val-deg)*60.0;
                double mn=floor(min_f);
                double sec=(min_f-mn)*60.0;
                snprintf(s,sizeof(s),"%.0f* %.0f' %.1f\"",deg,mn,sec);
                draw_result(r++,"dec > DMS",s);
                /* DMS to decimal: assume val is already decimal */
            }
            break;
        }

        gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,SCR_H-14,SCR_W,14);
        gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-15,SCR_W);
        gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
        gfx_PrintStringXY(inp.on?"CLR=Cancel"
                                 :"2ND=Next Cat  ENTER=Edit  ALPHA=Help",
                          MARGIN,SCR_H-12);
        gfx_BlitBuffer();

        kb_Scan(); int i; for(i=0;i<8;i++) cur[i]=kb_Data[i];
        if(inp.on){int rc=is_key(&inp,cur,prev);if(rc==1)val=is_val(&inp);}
        else{
            if(KN(6,BK_ENT))is_start(&inp);
            if(KN(1,BK_2ND))cat=(cat+1)%12;
            if(KN(6,BK_CLR)){memcpy(prev,cur,8);return;}
            if(KN(2,kb_Alpha)) show_std_help();
        }
        memcpy(prev,cur,8);
    }
}

/* ══════════════════════════════════════════════════════════════
   QUICK CONVERSIONS  (SM/NM, inHg/mb, C/F)
   ══════════════════════════════════════════════════════════════ */

static void calc_quickconv(void)
{
    while(kb_AnyKey()) kb_Scan();
    double v[6]={0,0,0,0,0,0};
    const char *lbl[6]={"SM","NM","inHg","mb","deg C","deg F"};
    int sel=0, _rpt=0;
    IS inp={0};
    uint8_t prev[8]={0},cur[8];
    int i;
    g_scr_help_title="CONVERSIONS";
    g_scr_help_body=
        "Quick unit conversions.\n"
        "\n"
        "Enter any value in the left\n"
        "column and see the result to\n"
        "the right instantly.\n"
        "\n"
        "SM   - Statute Miles to NM\n"
        "NM   - Nautical Miles to SM\n"
        "inHg - pressure to millibars\n"
        "mb   - millibars to inHg\n"
        "C    - Celsius to Fahrenheit\n"
        "F    - Fahrenheit to Celsius\n"
        "\n"
        "ENTER=edit  UP/DN=move  CLR=back";

    while(1){
        draw_header("  CONVERSIONS  ");
        char s[32], rs[28];

        for(i=0;i<6;i++){
            int y=ROW_Y(i);
            int issel=(i==sel);
            if(issel){ gfx_SetColor(CI_SELBG); gfx_FillRectangle_NoClip(0,y-3,SCR_W,STEP); }
            gfx_SetTextBGColor(issel?CI_SELBG:CI_BG);
            /* arrow */
            gfx_SetTextFGColor(issel?CI_AMBER:CI_BG);
            gfx_PrintStringXY(">",MARGIN,y);
            /* label */
            gfx_SetTextFGColor(issel?CI_AMBER:CI_LABEL);
            gfx_PrintStringXY(lbl[i],MARGIN+10,y);
            /* editable value */
            if(issel&&inp.on){
                gfx_SetColor(CI_EDITBG);
                gfx_FillRectangle_NoClip(76,y-3,76,STEP);
                gfx_SetTextFGColor(CI_AMBER); gfx_SetTextBGColor(CI_EDITBG);
                snprintf(s,sizeof(s),"%s%s_",inp.neg?"-":"",inp.buf);
            } else {
                gfx_SetTextFGColor(issel?CI_AMBER:CI_VALUE);
                gfx_SetTextBGColor(issel?CI_SELBG:CI_BG);
                { double r=round(v[i]*100.0)/100.0; int c=(int)round(FABS(fmod(r,1.0))*100.0);
                  if(r==0.0||c==0) snprintf(s,sizeof(s),"%.0f",r);
                  else if(c%10==0) snprintf(s,sizeof(s),"%.1f",r);
                  else             snprintf(s,sizeof(s),"%.2f",r); }
            }
            gfx_PrintStringXY(s,78,y);
            /* conversion result */
            switch(i){
                case 0: snprintf(rs,sizeof(rs),"= %.2f nm", v[0]/1.15078); break;
                case 1: snprintf(rs,sizeof(rs),"= %.2f sm", v[1]*1.15078); break;
                case 2: snprintf(rs,sizeof(rs),"= %.1f mb", v[2]*33.8639); break;
                case 3: snprintf(rs,sizeof(rs),"= %.2f inHg",v[3]/33.8639); break;
                case 4: snprintf(rs,sizeof(rs),"= %.1f F",  v[4]*9.0/5.0+32.0); break;
                case 5: snprintf(rs,sizeof(rs),"= %.1f C",  (v[5]-32.0)*5.0/9.0); break;
                default: rs[0]='\0'; break;
            }
            gfx_SetTextFGColor(issel?CI_GREEN:CI_LABEL);
            gfx_SetTextBGColor(issel?CI_SELBG:CI_BG);
            gfx_PrintStringXY(rs,158,y);
            /* divider between groups */
            if(i==1||i==3){
                gfx_SetColor(CI_DIV);
                gfx_HorizLine_NoClip(MARGIN,y+STEP-5,SCR_W-MARGIN*2);
            }
        }

        draw_footer(inp.on);
        gfx_BlitBuffer();

        kb_Scan(); for(i=0;i<8;i++) cur[i]=kb_Data[i];
        if(inp.on){
            int r=is_key(&inp,cur,prev);
            if(r==1) v[sel]=is_val(&inp);
        } else {
            if(cur[7]&(BK_UP|BK_DOWN)) _rpt++; else _rpt=0;
            if((KN(7,BK_UP)||((cur[7]&BK_UP)&&_rpt>18&&(_rpt%4==0)))&&sel>0) sel--;
            if((KN(7,BK_DOWN)||((cur[7]&BK_DOWN)&&_rpt>18&&(_rpt%4==0)))&&sel<5) sel++;
            if(KN(6,BK_ENT)) is_start(&inp);
            if(KN(6,BK_CLR)){memcpy(prev,cur,8);return;}
            if(KN(2,kb_Alpha)) show_screen_help();
        }
        memcpy(prev,cur,8);
    }
}

/* ── About screen ────────────────────────────────────────────── */
static void show_about(void)
{
    while(kb_AnyKey()) kb_Scan();
    gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,0,SCR_W,SCR_H);
    gfx_SetColor(CI_HDR); gfx_FillRectangle_NoClip(0,0,SCR_W,22);
    gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_HDR);
    gfx_SetTextScale(1,1);
    gfx_PrintStringXY("  ABOUT  ",(SCR_W-9*8)/2,7);

    gfx_SetTextScale(1,2);
    gfx_SetTextFGColor(CI_AMBER); gfx_SetTextBGColor(CI_BG);
    gfx_PrintStringXY("E6B Flight Computer",(SCR_W-19*8*2)/2+8,32);
    gfx_SetTextScale(1,1);

    gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
    gfx_PrintStringXY("Version 1.0  |  TI-84 CE",MARGIN,68);

    gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(MARGIN,82,SCR_W-MARGIN*2);

    gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_BG);
    gfx_PrintStringXY("By Andrew Sottile",MARGIN,90);

    gfx_SetTextFGColor(CI_GREEN);
    gfx_PrintStringXY("github.com/coolpilot904",MARGIN,108);

    gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(MARGIN,124,SCR_W-MARGIN*2);

    gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
    gfx_PrintStringXY("Track your flights at:",MARGIN,132);
    gfx_SetTextFGColor(CI_AMBER);
    gfx_PrintStringXY("log61.com",MARGIN,148);
    gfx_SetTextFGColor(CI_LABEL);
    gfx_PrintStringXY("Digital pilot logbook",MARGIN,162);

    gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(MARGIN,178,SCR_W-MARGIN*2);

    gfx_SetTextFGColor(CI_LABEL);
    gfx_PrintStringXY("Built with CE Toolchain",MARGIN,186);
    gfx_PrintStringXY("Based on ASA CX-3 feature set",MARGIN,200);

    gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-15,SCR_W);
    gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
    gfx_PrintStringXY("Any key to continue",MARGIN,SCR_H-12);
    gfx_BlitBuffer();
    while(!kb_AnyKey()) kb_Scan();
    while(kb_AnyKey()) kb_Scan();
}

/* ── Theme selector ───────────────────────────────────────────── */
static void calc_theme(void)
{
    while(kb_AnyKey()) kb_Scan();
    static int sel=0;
    uint8_t prev[8]={0},cur[8];
    int _rpt=0;
    const char *names[3]={"Standard","Night","Daylight"};
    while(1){
        gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,0,SCR_W,SCR_H);
        gfx_SetColor(CI_HDR); gfx_FillRectangle_NoClip(0,0,SCR_W,22);
        gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_HDR);
        gfx_SetTextScale(1,1);
        gfx_PrintStringXY("  THEME  ",(SCR_W-9*8)/2,6);
        int i;
        for(i=0;i<3;i++){
            int y=40+i*30;
            if(i==sel){
                gfx_SetColor(CI_SELBG);
                gfx_FillRectangle_NoClip(MARGIN,y-2,SCR_W-MARGIN*2,22);
                gfx_SetColor(CI_AMBER);
                gfx_Rectangle_NoClip(MARGIN,y-2,SCR_W-MARGIN*2,22);
                gfx_SetTextFGColor(CI_AMBER); gfx_SetTextBGColor(CI_SELBG);
            } else {
                gfx_SetTextFGColor(i==g_theme?CI_GREEN:CI_VALUE);
                gfx_SetTextBGColor(CI_BG);
            }
            gfx_PrintStringXY(names[i],MARGIN+14,y+3);
            if(i==g_theme){
                gfx_SetTextFGColor(i==sel?CI_GREEN:CI_GREEN);
                gfx_SetTextBGColor(i==sel?CI_SELBG:CI_BG);
                gfx_PrintStringXY("*",SCR_W-MARGIN-8,y+3);
            }
        }
        gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-15,SCR_W);
        gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
        gfx_PrintStringXY("ENTER=Apply  CLR=Back",MARGIN,SCR_H-12);
        gfx_BlitBuffer();
        kb_Scan();
        for(i=0;i<8;i++) cur[i]=kb_Data[i];
        if(cur[7]&(BK_UP|BK_DOWN)) _rpt++; else _rpt=0;
        if((KN(7,BK_UP)||((cur[7]&BK_UP)&&_rpt>18&&(_rpt%4==0)))&&sel>0) sel--;
        if((KN(7,BK_DOWN)||((cur[7]&BK_DOWN)&&_rpt>18&&(_rpt%4==0)))&&sel<2) sel++;
        if(KN(6,BK_ENT)){ g_theme=sel; apply_theme(); theme_save(); }
        if(KN(6,BK_CLR)){ memcpy(prev,cur,8); return; }
        memcpy(prev,cur,8);
    }
}

/* ══════════════════════════════════════════════════════════════
   MAIN MENU
   ══════════════════════════════════════════════════════════════ */

static const char *main_items[9]={
    "1  ALTITUDE",
    "2  AIRSPEED",
    "3  WIND",
    "4  NAVIGATION",
    "5  FUEL & ETA",
    "6  WT & BALANCE",
    "7  CONVERSIONS",
    "8  THEME",
    "9  ABOUT"
};

int main(void)
{
    gfx_Begin();
    theme_load();
    apply_theme();
    gfx_SetDrawBuffer();

    int sel=0, _rpt=0;
    uint8_t prev[8]={0},cur[8];

    while(1){
        gfx_SetColor(CI_BG); gfx_FillRectangle_NoClip(0,0,SCR_W,SCR_H);
        gfx_SetColor(CI_HDR); gfx_FillRectangle_NoClip(0,0,SCR_W,32);
        gfx_SetTextScale(1,2);
        gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_HDR);
        gfx_PrintStringXY("E6B FLIGHT COMPUTER",18,6);
        gfx_SetTextScale(1,1);
        int i;
        for(i=0;i<9;i++){
            int y=35+i*21;
            if(i==sel){
                gfx_SetColor(CI_SELBG);
                gfx_FillRectangle_NoClip(MARGIN,y-2,SCR_W-MARGIN*2,20);
                gfx_SetColor(CI_AMBER);
                gfx_Rectangle_NoClip(MARGIN,y-2,SCR_W-MARGIN*2,20);
                gfx_SetTextFGColor(CI_AMBER); gfx_SetTextBGColor(CI_SELBG);
            } else {
                gfx_SetTextFGColor(CI_VALUE); gfx_SetTextBGColor(CI_BG);
            }
            gfx_PrintStringXY(main_items[i],MARGIN+14,y+4);
        }

        gfx_SetColor(CI_DIV); gfx_HorizLine_NoClip(0,SCR_H-15,SCR_W);
        gfx_SetTextFGColor(CI_LABEL); gfx_SetTextBGColor(CI_BG);
        gfx_PrintStringXY("ALPHA=Help  |  CLR=Exit",MARGIN,SCR_H-12);
        gfx_BlitBuffer();

        kb_Scan(); for(i=0;i<8;i++) cur[i]=kb_Data[i];
        if(cur[7]&(BK_UP|BK_DOWN)) _rpt++; else _rpt=0;
        if((KN(7,BK_UP)||((cur[7]&BK_UP)&&_rpt>18&&(_rpt%4==0)))&&sel>0) sel--;
        if((KN(7,BK_DOWN)||((cur[7]&BK_DOWN)&&_rpt>18&&(_rpt%4==0)))&&sel<8) sel++;
        if(KN(6,BK_CLR)){gfx_End();return 0;}
        if(KN(2,kb_Alpha)) show_std_help();
        if(KN(6,BK_ENT)){
            memcpy(prev,cur,8);
            switch(sel){
                case 0: menu_altitude();   break;
                case 1: menu_airspeed();   break;
                case 2: menu_wind();       break;
                case 3: menu_navigation(); break;
                case 4: menu_fuel();       break;
                case 5: menu_wb();         break;
                case 6: calc_quickconv();  break;
                case 7: calc_theme();      break;
                case 8: show_about();      break;
            }
            while(kb_AnyKey()) kb_Scan();
            kb_Scan();
            {int _j; for(_j=0;_j<8;_j++) cur[_j]=kb_Data[_j];}
        }
        memcpy(prev,cur,8);
    }
}
