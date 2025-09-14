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
 *	CCL��WO0�𔽓]����PA7(CCL)��PA3(WO0)�ō����o�͂��\��
*/

/*
  Beta30
	struct _teeth�@�̃G���x���[�v���|�C���^������f�[�^�ɕύX
	���C�����[�v�̃G���x���[�v�������Ɏ��f�[�^�������邱�Ƃɂ���
*/

//PA6(PKG.2PIN) Debug out
//#define DEBUG_OUT

#define F_CPU 20000000UL  // ATtiny202�̓����N���b�N�i20MHz�j
#define F_CLK_PER 20000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include "OrgelWave.h"
//#include "score_test.h"
//#include "score_Etude_op10no3.h"
#include "score_gnoss1_116bpm.h"

#define MAX_NOTE 8		// �ő�8�܂�
#define TCA_PER (256)	// PWM���� PER = 255, fPER = 78.125KHz
#define TCB_PER (TCA_PER * 3 - 1)	// TCB���荞�ݎ��� fSMP = fPER/3

// NoteOn�r�b�g�}�X�N
const uint8_t NoteOn_bm[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

// �����\����
typedef struct _teeth {
	uint16_t Pitch;			// �|�C���^�ϓ�/�T���v������
	uint16_t waveIdx;		// Wave table �|�C���^(�����_�ȉ�8�r�b�g)
	uint8_t EnvDat;			// Envelope �f�[�^
} TEETH_t;

// ISR�Ŏg����ϐ�
volatile uint8_t primeCntr = 0;			// �^�C�~���O�����p�J�E���^
volatile uint8_t ms5Flag = 0;			// 5�~���b(4.992ms)�J�E���g�t���O
volatile TEETH_t teeth[MAX_NOTE];		// �e�������

// �v���g�^�C�v
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
// �����_�ȉ��؂�̂ĂĐώZ
	int16_t per = 0;
	for(uint8_t i = 0; i<MAX_NOTE; i++) {
		// WaveTable��ǂݏo���G���x���[�v����
		per += ((int16_t)Wave[teeth[i].waveIdx >> 8] * (int16_t)(teeth[i].EnvDat)) >> 8;
		// WaveTable�C���f�b�N�X�X�V
		teeth[i].waveIdx += teeth[i].Pitch;
		if(teeth[i].waveIdx>262143) {
			teeth[i].waveIdx -= 32768;
		}
	}
	// ���~�b�g�������PWM PERBUF�֏�������
	if(per>127) {
		per = 127;
		} else if(per<-127) {
		per = -127;
	}
	TCA0.SINGLE.CMP0BUF = per + 128;

	// 5�~���b�J�E���^�X�V(���ۂ�4.99ms)
	primeCntr++;
	if(primeCntr>=130) {
		ms5Flag = 1;
		primeCntr = 0;
	}

	// ���荞�݃t���O���N���A
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

	// �y�����t�p�ϐ�
	uint8_t NoteQueue = 0;			// ������ԃJ�E���^ mod MAX_NOTE
	uint8_t NoteOn = 0;				// �����w�� Bit Map [MAX_NODE -1 : 0]
	uint8_t DesignatedNote[MAX_NOTE];
	uint16_t tickCntr = 0;			// �����^�C�~���O�J�E���^
	uint16_t NoteNumber = 0;		// �m�[�g�ԍ�
	uint16_t envelope[MAX_NOTE] = {0};

	initTeeth();	// teeth �\���̏�����
	// �I���}�[�J�[�������ꍇ�̎b��I���|�C���g
	uint16_t maxNoteNumber = sizeof(score_note)/sizeof(score_note[0]);
	uint16_t estimatedEndTicks = score_tick[maxNoteNumber - 1] + 600;	// ��3�b
	uint8_t estimatedEndFlag = 0;

	// TCA PWM setting
	setPWM_PINS();
	setTCA_SSPWM();
	setCCL_LUT1();
	// Timer
	setTCB_PeriodicInterrupt();
	// ���荞�݋���
	sei();

    /* Replace with your application code */
    while (1) 
    {
		if(ms5Flag) {
			#ifdef DEBUG_OUT
			PORTA.OUTSET = PIN6_bm;
			#endif
		
			ms5Flag = 0;
			
			// NoteOn���𒲂ׂĔ������J�n
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
		
			// �y����tick��note���`�F�b�N���Ĕ�������o�^
			// ��������MAX_NOTE�𒴂����ꍇ�ł�������Ԃ𖳎����ď��ԂɍĔ�������
			NoteOn = 0;
			while(score_tick[NoteNumber]<=tickCntr) {
				// �������̓o�^
				NoteOn |= 0x01 << NoteQueue;
				if(score_note[NoteNumber]>=64) {	// �y���I������
					NoteQueue = 0;			// ������ԃJ�E���^ mod MAX_NOTE
					NoteOn = 0;				// �����w�� Bit Map [MAX_NODE -1 : 0]
					tickCntr = 0;			// �����^�C�~���O�J�E���^
					NoteNumber = 0;			// �m�[�g�ԍ�
					initTeeth();
					break;
				}
				DesignatedNote[NoteQueue] = score_note[NoteNumber];
				envelope[NoteQueue] = 0;
				// ���̉�����
				NoteNumber++;
				if(NoteNumber>=maxNoteNumber) {	// �b��I��������
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
			
			// �e�m�[�g�̃G���x���[�v���X�V
			for(uint8_t i = 0; i<MAX_NOTE; i++) {
				if(teeth[i].EnvDat!=0 && !(NoteOn & NoteOn_bm[i])) {	// NoteOn�g���K�[���͍X�V���Ȃ�
					// �G���x���[�v�̍X�V
					uint8_t EnvIdx = calEnvelope(teeth[i].Pitch, &envelope[i]);
					teeth[i].EnvDat = Envelope[EnvIdx];
				}
			}
			
			// TIC�J�E���^�̍X�V
			tickCntr++;
			
			// �b��I������
			if(estimatedEndFlag && (tickCntr>=estimatedEndTicks)) {
				estimatedEndFlag = 0;
				NoteQueue = 0;			// ������ԃJ�E���^ mod MAX_NOTE
				NoteOn = 0;				// �����w�� Bit Map [MAX_NODE -1 : 0]
				tickCntr = 0;			// �����^�C�~���O�J�E���^
				NoteNumber = 0;			// �m�[�g�ԍ�
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
	// WO0(PA3),CCL1(PA7)���o�͂ɐݒ�
	PORTA.DIRSET = PIN3_bm | PIN7_bm;
}

// TCA configuration
// CMP0 & CMP1 enable
// single-slope PWM
void setTCA_SSPWM(void)
{
	// TCA0�ݒ�FSingle-slope PWM, CMP0�o�͗L��, Auto Lock Up Enable
	TCA0.SINGLE.CTRLB = TCA_SINGLE_CMP0EN_bm | TCA_SINGLE_WGMODE_SINGLESLOPE_gc;
	// PWM�����ݒ�
	TCA0.SINGLE.PER = TCA_PER - 1;
	// �f���[�e�B��ݒ�i50%�j
	TCA0.SINGLE.CMP0 = 128;
	// �N���b�N�I���FDIV1�i�����Ȃ��j��20MHz����
	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc | TCA_SINGLE_ENABLE_bm;
}

// CCL1��TCA.CMP0�̔��]�M�����o�͂���
void setCCL_LUT1(void)
{
	// LUT���͑I��
	CCL.LUT1CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_TCA0_gc;		// TCA0.WO0����͂Ɏg�p
	CCL.LUT1CTRLC = CCL_INSEL2_MASK_gc;
	// LUT1 Truth Table
	CCL.TRUTH1 = 0x55;		// INSEL0�̔��]
	// �o�͐ݒ�
	CCL.LUT1CTRLA = CCL_OUTEN_bm | CCL_ENABLE_bm;	// ENABLE
	CCL.CTRLA = CCL_ENABLE_bm;				// Enable
}

// TCB �����I���荞�� �ݒ�
// fSMP = 26.0416666666667kHz, fSMP = fPER/3
void setTCB_PeriodicInterrupt(void)
{
	// TCB���ꎞ��~
	TCB0.CTRLA = 0;
	// �������[�h�ɐݒ�(default)
	TCB0.CTRLB = TCB_CNTMODE_INT_gc;
	// ���荞�ݎ�����ݒ� : TCA PWM�����̐����{�Őݒ�
	TCB0.CCMP = TCB_PER;
	// ���荞�݂�����
	TCB0.INTCTRL = TCB_CAPT_bm;
	// TCB��L���� �N���b�N�� CLK_PER��I��
	TCB0.CTRLA = TCB_ENABLE_bm;
}


// teeth �\���̏�����
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
	uint16_t delta = (((pitch - 138) >> 4) + 103) >> 1;	// �����_�ȉ�7�r�b�g
	*envelope = *envelope + delta;
	if(*envelope>=32768) {
		*envelope = 32768;
		return 255;
		} else {
		return *envelope >> 7;
	}
}
