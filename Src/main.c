/*
 * Beta30
 *
 * Created: 2025/09/12
 * Author : ochik
 */ 
/*
	5ms main loop
*/	

/*
 *	IO Multiplexing TCA/TCB
 *	PKGPIN	name	TCA			TCB		CCL			IF		This PRJ.
 *		1	VDD												VDD
 *		2	PA6					WO0		LUT0 OUT	TxD		[DEBUG_OUT]
 *		3	PA7		WO3(alt)			LUT1 OUT	RxD		~TCA.WO0
 *		4	PA1		WO1								SDA		
 *		5	PA2		WO2								SCL		
 *		6	PA0												UPDI
 *		7	PA3		WO0/WO3									TCA.WO0
 *		8	GND												GND
 *
 *	CCLでWO0を反転してPA7(CCL)とPA3(WO0)で差動出力を構成
*/

/*
  Beta30
	struct _teeth　のエンベロープをポインタから実データに変更
	メインループのエンベロープ処理時に実データを代入することにした
*/

//PA6(PKG.2PIN) Debug out
//#define DEBUG_OUT

#define F_CPU 20000000UL  // ATtiny202の内部クロック（20MHz）
#define F_CLK_PER 20000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include "OrgelWave.h"
//#include "score_test.h"
//#include "score_Etude_op10no3.h"
#include "score_gnoss1_116bpm.h"

#define MAX_NOTE 8		// 最大8まで
#define TCA_PER (256)	// PWM周期 PER = 255, fPER = 78.125KHz
#define TCB_PER (TCA_PER * 3 - 1)	// TCB割り込み周期 fSMP = fPER/3

// NoteOnビットマスク
const uint8_t NoteOn_bm[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

// 発音構造体
typedef struct _teeth {
	uint16_t Pitch;			// ポインタ変動/サンプル時間
	uint16_t waveIdx;		// Wave table ポインタ(小数点以下8ビット)
	uint8_t EnvDat;			// Envelope データ
} TEETH_t;

// ISRで使われる変数
volatile uint8_t primeCntr = 0;			// タイミング生成用カウンタ
volatile uint8_t ms5Flag = 0;			// 5ミリ秒(4.992ms)カウントフラグ
volatile TEETH_t teeth[MAX_NOTE];		// 各発音情報

// プロトタイプ
void setCLK_PER_20M(void);
void setPWM_PINS(void);
void setTCA_SSPWM(void);
void setCCL_LUT1(void);
void setTCB_PeriodicInterrupt(void);
void initTeeth(void);
uint8_t calEnvelope(const uint16_t pitch, uint16_t* envelope);


// TCB T-sample ISR
ISR(TCB0_INT_vect) {
  #ifdef DEBUG_OUT
    PORTA.OUTSET = PIN6_bm;
  #endif
// 少数点以下切り捨てて積算
	int16_t per = 0;
	for(uint8_t i = 0; i<MAX_NOTE; i++) {
		// WaveTableを読み出しエンベロープ処理
		per += ((int16_t)Wave[teeth[i].waveIdx >> 8] * (int16_t)(teeth[i].EnvDat)) >> 8;
		// WaveTableインデックス更新
		teeth[i].waveIdx += teeth[i].Pitch;
		if(teeth[i].waveIdx>262143) {
			teeth[i].waveIdx -= 32768;
		}
	}
	// リミット処理後にPWM PERBUFへ書き込み
	if(per>127) {
		per = 127;
		} else if(per<-127) {
		per = -127;
	}
	TCA0.SINGLE.CMP0BUF = per + 128;

	// 5ミリ秒カウンタ更新(実際は4.99ms)
	primeCntr++;
	if(primeCntr>=130) {
		ms5Flag = 1;
		primeCntr = 0;
	}

	// 割り込みフラグをクリア
	TCB0.INTFLAGS = TCB_CAPT_bm;

  #ifdef DEBUG_OUT
    PORTA.OUTCLR = PIN6_bm;
  #endif

}


int main(void)
{
	// Main Clock
	setCLK_PER_20M();

  // PA6 out for debug
  #ifdef DEBUG_SERIAL
    uart_init(115200, 0);
  #endif
  #ifdef DEBUG_OUT
    PORTA.DIRSET |= PIN6_bm;
  #endif

	// 楽譜演奏用変数
	uint8_t NoteQueue = 0;			// 発音状態カウンタ mod MAX_NOTE
	uint8_t NoteOn = 0;				// 発音指示 Bit Map [MAX_NODE -1 : 0]
	uint8_t DesignatedNote[MAX_NOTE];
	uint16_t tickCntr = 0;			// 発音タイミングカウンタ
	uint16_t NoteNumber = 0;		// ノート番号
	uint16_t envelope[MAX_NOTE] = {0};

	initTeeth();	// teeth 構造体初期化
	// 終了マーカーが無い場合の暫定終了ポイント
	uint16_t maxNoteNumber = sizeof(score_note)/sizeof(score_note[0]);
	uint16_t estimatedEndTicks = score_tick[maxNoteNumber - 1] + 600;	// 約3秒
	uint8_t estimatedEndFlag = 0;

	// TCA PWM setting
	setPWM_PINS();
	setTCA_SSPWM();
	setCCL_LUT1();
	// Timer
	setTCB_PeriodicInterrupt();
	// 割り込み許可
	sei();

    /* Replace with your application code */
    while (1) 
    {
		if(ms5Flag) {
			#ifdef DEBUG_OUT
			PORTA.OUTSET = PIN6_bm;
			#endif
		
			ms5Flag = 0;
			
			// NoteOn情報を調べて発音を開始
			for(uint8_t i = 0; i<MAX_NOTE; i++) {
				if(NoteOn & NoteOn_bm[i]) {
					teeth[i].Pitch = Note[DesignatedNote[i]];
					teeth[i].waveIdx = 0;
					teeth[i].EnvDat = 255;		// Envelope[0] = 255
				}
			}

			#ifdef DEBUG_OUT
			PORTA.OUTCLR = PIN6_bm;
			#endif
		
			// 楽譜のtickとnoteをチェックして発音情報を登録
			// 発音数がMAX_NOTEを超えた場合でも発音状態を無視して順番に再発音する
			NoteOn = 0;
			while(score_tick[NoteNumber]<=tickCntr) {
				// 発音情報の登録
				NoteOn |= 0x01 << NoteQueue;
				if(score_note[NoteNumber]>=64) {	// 楽譜終了処理
					NoteQueue = 0;			// 発音状態カウンタ mod MAX_NOTE
					NoteOn = 0;				// 発音指示 Bit Map [MAX_NODE -1 : 0]
					tickCntr = 0;			// 発音タイミングカウンタ
					NoteNumber = 0;			// ノート番号
					initTeeth();
					break;
				}
				DesignatedNote[NoteQueue] = score_note[NoteNumber];
				envelope[NoteQueue] = 0;
				// 次の音符へ
				NoteNumber++;
				if(NoteNumber>=maxNoteNumber) {	// 暫定終了処理へ
					estimatedEndFlag = 1;
					break;
				}
				NoteQueue++;
				if(NoteQueue>=MAX_NOTE) {
					NoteQueue = 0;
				}
			}

			#ifdef DEBUG_SERIAL
			Serial_rprint_int(tickCntr, 10);
			Serial_print(" : ");
			Serial_rprintln_int(NoteOn, 16);
			#endif
			
			// 各ノートのエンベロープを更新
			for(uint8_t i = 0; i<MAX_NOTE; i++) {
				if(teeth[i].EnvDat!=0 && !(NoteOn & NoteOn_bm[i])) {	// NoteOnトリガー時は更新しない
					// エンベロープの更新
					uint8_t EnvIdx = calEnvelope(teeth[i].Pitch, &envelope[i]);
					teeth[i].EnvDat = Envelope[EnvIdx];
				}
			}
			
			// TICカウンタの更新
			tickCntr++;
			
			// 暫定終了処理
			if(estimatedEndFlag && (tickCntr>=estimatedEndTicks)) {
				estimatedEndFlag = 0;
				NoteQueue = 0;			// 発音状態カウンタ mod MAX_NOTE
				NoteOn = 0;				// 発音指示 Bit Map [MAX_NODE -1 : 0]
				tickCntr = 0;			// 発音タイミングカウンタ
				NoteNumber = 0;			// ノート番号
				initTeeth();
			}
		}
	}
}

// Clock setup
void setCLK_PER_20M(void)
{
	// A selector for CLK_MAIN
	//	_PROTECTED_WRITE(CLKCTRL.MCLKCTRLA, 1);	// 20MHz OSC select (default)
	// A PreScaler for CLK_CPU & CLK_PER
	_PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, 0);	// skip PreScaler
}

// TCA0 comparator outputs, CCL inverted output
// WO0 : PKG 7pin PA3	CMP0
// CCL : PKG 3pin PA7	LUT1
void setPWM_PINS(void)
{
	// WO0(PA3),CCL1(PA7)を出力に設定
	PORTA.DIRSET = PIN3_bm | PIN7_bm;
}

// TCA configuration
// CMP0 & CMP1 enable
// single-slope PWM
void setTCA_SSPWM(void)
{
	// TCA0設定：Single-slope PWM, CMP0出力有効, Auto Lock Up Enable
	TCA0.SINGLE.CTRLB = TCA_SINGLE_CMP0EN_bm | TCA_SINGLE_WGMODE_SINGLESLOPE_gc;
	// PWM周期設定
	TCA0.SINGLE.PER = TCA_PER - 1;
	// デューティ比設定（50%）
	TCA0.SINGLE.CMP0 = 128;
	// クロック選択：DIV1（分周なし）で20MHz動作
	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc | TCA_SINGLE_ENABLE_bm;
}

// CCL1でTCA.CMP0の反転信号を出力する
void setCCL_LUT1(void)
{
	// LUT入力選択
	CCL.LUT1CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_TCA0_gc;		// TCA0.WO0を入力に使用
	CCL.LUT1CTRLC = CCL_INSEL2_MASK_gc;
	// LUT1 Truth Table
	CCL.TRUTH1 = 0x55;		// INSEL0の反転
	// 出力設定
	CCL.LUT1CTRLA = CCL_OUTEN_bm | CCL_ENABLE_bm;	// ENABLE
	CCL.CTRLA = CCL_ENABLE_bm;				// Enable
}

// TCB 周期的割り込み 設定
// fSMP = 26.0416666666667kHz, fSMP = fPER/3
void setTCB_PeriodicInterrupt(void)
{
	// TCBを一時停止
	TCB0.CTRLA = 0;
	// 周期モードに設定(default)
	TCB0.CTRLB = TCB_CNTMODE_INT_gc;
	// 割り込み周期を設定 : TCA PWM周期の整数倍で設定
	TCB0.CCMP = TCB_PER;
	// 割り込みを許可
	TCB0.INTCTRL = TCB_CAPT_bm;
	// TCBを有効化 クロックは CLK_PERを選択
	TCB0.CTRLA = TCB_ENABLE_bm;
}


// teeth 構造体初期化
void initTeeth(void)
{
	for(uint8_t i = 0; i<MAX_NOTE; i++) {
		teeth[i].Pitch = 0;
		teeth[i].waveIdx = 0;
		//		teeth[i].EvlIdx = 255;
		teeth[i].EnvDat = 0;
	}
}


uint8_t calEnvelope(const uint16_t pitch, uint16_t* envelope)
{
	uint16_t delta = (((pitch - 138) >> 4) + 103) >> 1;	// 小数点以下7ビット
	*envelope = *envelope + delta;
	if(*envelope>=32768) {
		*envelope = 32768;
		return 255;
		} else {
		return *envelope >> 7;
	}
}
