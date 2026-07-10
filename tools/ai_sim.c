/* ai_sim.c — desktop harness that replicates Conway's Conquerors' board rules
 * and CPU AI exactly (a faithful port of src/main.c, minus all Game Boy I/O),
 * so the three difficulties can be played head-to-head and their relative
 * strength measured. This is how the difficulty ladder was tuned and verified.
 *
 *   cc -O2 -o ai_sim ai_sim.c && ./ai_sim
 *
 * Runs a round-robin (Easy/Normal/Hard, each pairing played both ways to cancel
 * first-move bias) and prints win counts. Expected: a clean ladder,
 * Hard > Normal > Easy, with no 100% blowouts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8; typedef unsigned short u16;
typedef signed char i8;   typedef signed short i16;

#define BW 20
#define BH 14
#define BN (BW*BH)
#define ZONE_BLUE_MAX 5
#define ZONE_RED_MIN  14
#define DEAD 0
#define BLUE 1
#define RED  2
#define MAX_ROUNDS 12
#define CELLS_PER_TURN 4
#define WIN 5

static u8 board[BN], nextb[BN];
static u16 rng = 0x1234;
static u8 rnd(void){ rng^=rng<<7; rng^=rng>>9; rng^=rng<<8; return (u8)rng; }

static u8 zone_of(u8 x){ if(x<=ZONE_BLUE_MAX)return BLUE; if(x>=ZONE_RED_MIN)return RED; return 0; }
static void clear_board(void){ for(int i=0;i<BN;i++) board[i]=DEAD; }
static void count_cells(u8*b,u8*r){ u8 cb=0,cr=0; for(int i=0;i<BN;i++){ if(board[i]==BLUE)cb++; else if(board[i]==RED)cr++; } *b=cb;*r=cr; }

static void step(void){
  for(u8 y=0;y<BH;y++)for(u8 x=0;x<BW;x++){
    u8 n1=0,n2=0;
    for(i8 dy=-1;dy<=1;dy++)for(i8 dx=-1;dx<=1;dx++){
      if(!dx&&!dy)continue; i8 nx=(i8)x+dx, ny=(i8)y+dy;
      if(nx<0||nx>=BW||ny<0||ny>=BH)continue;
      u8 v=board[ny*BW+nx]; if(v==BLUE)n1++; else if(v==RED)n2++;
    }
    u8 tot=n1+n2, cur=board[y*BW+x], res;
    if(cur) res=(tot==2||tot==3)?cur:DEAD; else res=(tot==3)?((n1>n2)?BLUE:RED):DEAD;
    nextb[y*BW+x]=res;
  }
  memcpy(board,nextb,BN);
}

static u8 scratchA[WIN*WIN], scratchB[WIN*WIN];
static i16 place_impact_n(u8 player,u8 px,u8 py,u8 gens){
  u8 enemy=(player==BLUE)?RED:BLUE; u8 half=WIN/2;
  i16 own_before=0, enemy_before=0;
  for(i8 dy=0;dy<WIN;dy++)for(i8 dx=0;dx<WIN;dx++){
    i8 bx=(i8)px-half+dx, by=(i8)py-half+dy; u8 v=0;
    if(bx>=0&&bx<BW&&by>=0&&by<BH) v=board[by*BW+bx];
    if(dx==half&&dy==half) v=player;
    scratchA[dy*WIN+dx]=v;
  }
  for(int i=0;i<WIN*WIN;i++){ if(scratchA[i]==player)own_before++; else if(scratchA[i]==enemy)enemy_before++; }
  for(u8 g=0;g<gens;g++){
    for(i8 dy=0;dy<WIN;dy++)for(i8 dx=0;dx<WIN;dx++){
      u8 n1=0,n2=0;
      for(i8 a=-1;a<=1;a++)for(i8 b=-1;b<=1;b++){
        i8 nx=dx+b, ny=dy+a; if(!a&&!b)continue;
        if(nx<0||nx>=WIN||ny<0||ny>=WIN)continue;
        u8 v=scratchA[ny*WIN+nx]; if(v==BLUE)n1++; else if(v==RED)n2++;
      }
      u8 tot=n1+n2, cur=scratchA[dy*WIN+dx], res;
      if(cur)res=(tot==2||tot==3)?cur:DEAD; else res=(tot==3)?((n1>n2)?BLUE:RED):DEAD;
      scratchB[dy*WIN+dx]=res;
    }
    memcpy(scratchA,scratchB,WIN*WIN);
  }
  i16 own_after=0, enemy_after=0;
  for(int i=0;i<WIN*WIN;i++){ if(scratchA[i]==player)own_after++; else if(scratchA[i]==enemy)enemy_after++; }
  return (own_after-enemy_after)+(own_after-own_before)+(enemy_before-enemy_after);
}

static u8 cpu_can_neutral; static u8 cpu_can_enemy_col[BW];
static void cpu_prep(u8 player){
  u8 cb,cr; u8 enemy_zone=(player==BLUE)?RED:BLUE; count_cells(&cb,&cr);
  cpu_can_neutral=(player==BLUE)?(cb>0):(cr>0);
  for(u8 x=0;x<BW;x++) cpu_can_enemy_col[x]=0;
  u8 has=0; for(u8 y=0;y<BH;y++)for(u8 x=0;x<BW;x++) if(board[y*BW+x]==player&&zone_of(x)==enemy_zone)has=1;
  for(u8 x=0;x<BW;x++) if(zone_of(x)==enemy_zone) cpu_can_enemy_col[x]=has;
}
static u8 cpu_can_place(u8 player,u8 x,u8 y){
  if(board[y*BW+x]!=DEAD)return 0; u8 z=zone_of(x);
  if(z==player)return 1; if(z==0)return cpu_can_neutral; return cpu_can_enemy_col[x];
}
static u8 has_live_neighbor(u8 x,u8 y){
  for(i8 dy=-1;dy<=1;dy++)for(i8 dx=-1;dx<=1;dx++){ if(!dx&&!dy)continue;
    i8 nx=(i8)x+dx,ny=(i8)y+dy; if(nx<0||nx>=BW||ny<0||ny>=BH)continue;
    if(board[ny*BW+nx]!=DEAD)return 1; } return 0;
}

/* One free skip per player per game, same as the ROM (index 0=blue,1=red). */
static u8 skip_used[2];

/* This cpu_turn is the same logic shipping in src/main.c. */
static void cpu_turn(u8 player, int difficulty){
  u8 cb,cr; count_cells(&cb,&cr); u8 own=(player==BLUE)?cb:cr;
  if(own<2){
    u8 hx=(player==BLUE)?3:(BW-4); u8 hy=BH/2;
    static const i8 sdx[8]={0,1,0,1,2,2,1,0}, sdy[8]={0,0,1,1,0,1,2,2};
    u8 ph=0;
    for(u8 k=0;k<8&&ph<CELLS_PER_TURN;k++){ i8 sx=(i8)hx+sdx[k],sy=(i8)hy+sdy[k];
      if(sx<0||sx>=BW||sy<0||sy>=BH)continue; if(cpu_can_place(player,sx,sy)){board[sy*BW+sx]=player;ph++;} }
    return;
  }
  /* Skip (once per game): if Normal/Hard still hold their skip and every legal
   * move would be a net loss, pass instead. Mirrors src/main.c. */
  {
    u8 si=(player==BLUE)?0:1;
    if(!skip_used[si] && difficulty>0){
      i16 be=-30000; cpu_prep(player);
      for(u8 y=0;y<BH;y++)for(u8 x=0;x<BW;x++){
        if(!cpu_can_place(player,x,y))continue;
        if(!has_live_neighbor(x,y))continue;
        i16 s=place_impact_n(player,x,y,1);
        if(zone_of(x)==0)s+=2;
        if(player==RED&&zone_of(x)==BLUE)s+=1;
        if(player==BLUE&&zone_of(x)==RED)s+=1;
        if(s>be)be=s;
      }
      if(be<0){ skip_used[si]=1; return; }
    }
  }
  u8 cells=CELLS_PER_TURN;
  if(difficulty==0 && (rnd()%2==0)) cells=3;
  for(u8 n=0;n<cells;n++){
    i16 best=-30000; u8 bx=255,by=255; cpu_prep(player);
    for(u8 y=0;y<BH;y++)for(u8 x=0;x<BW;x++){
      if(!cpu_can_place(player,x,y))continue;
      if(!has_live_neighbor(x,y))continue;
      i16 s=place_impact_n(player,x,y,1);
      if(zone_of(x)==0)s+=2;
      if(player==RED&&zone_of(x)==BLUE)s+=1;
      if(player==BLUE&&zone_of(x)==RED)s+=1;
      if(difficulty==0){ s+=(i16)(rnd()%19)-9; }
      else if(difficulty==1){ s+=(i16)(rnd()%3)-1; }
      else { i16 s2=place_impact_n(player,x,y,2); s=s2+(s>>2); }
      if(s>best){best=s;bx=x;by=y;}
    }
    if(bx==255){ u8 hx=(player==BLUE)?2:(BW-3);
      for(u8 y=0;y<BH&&bx==255;y++)for(u8 x=0;x<BW;x++) if(cpu_can_place(player,x,y)&&(x==hx||has_live_neighbor(x,y))){bx=x;by=y;break;}
      if(bx==255) for(u8 y=0;y<BH&&bx==255;y++)for(u8 x=0;x<BW;x++) if(cpu_can_place(player,x,y)){bx=x;by=y;break;}
    }
    if(bx==255)break; board[by*BW+bx]=player;
  }
}

static u8 play_game(int dB,int dR){
  clear_board();
  skip_used[0]=0; skip_used[1]=0;
  for(int round=1; round<=MAX_ROUNDS; round++){
    cpu_turn(BLUE,dB); step();
    u8 cb,cr; count_cells(&cb,&cr);
    if(round>=2&&(cb==0||cr==0)) return cb>cr?BLUE:cr>cb?RED:0;
    cpu_turn(RED,dR);  step();
    count_cells(&cb,&cr);
    if(round>=2&&(cb==0||cr==0)) return cb>cr?BLUE:cr>cb?RED:0;
  }
  u8 cb,cr; count_cells(&cb,&cr); return cb>cr?BLUE:cr>cb?RED:0;
}

int main(void){
  int N=400; const char*names[3]={"Easy","Normal","Hard"};
  printf("=== %d games per matchup, sides swapped ===\n", N*2);
  for(int a=0;a<3;a++)for(int b=a+1;b<3;b++){
    int winA=0,winB=0,draw=0;
    for(int g=0;g<N;g++){
      rng=0x1000+g*7+a*101+b*53; u8 w=play_game(a,b);
      if(w==BLUE)winA++; else if(w==RED)winB++; else draw++;
      rng=0x2000+g*7+a*101+b*53; u8 w2=play_game(b,a);
      if(w2==RED)winA++; else if(w2==BLUE)winB++; else draw++;
    }
    printf("%-7s vs %-7s :  %-7s %3d  | %-7s %3d  | draws %3d  -> %s favored\n",
      names[a],names[b], names[a],winA, names[b],winB, draw,
      winA>winB?names[a]:winB>winA?names[b]:"even");
  }
  return 0;
}
