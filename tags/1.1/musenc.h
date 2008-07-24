/* -*- TABSIZE = 4 -*- */
/*
 *	for new GOGO-no-coda (1999/09,10)
 *	Copyright (C) 1999 PEN@MarineCat
 */
#ifndef __MUSUI_H__
#define __MUSUI_H__

#include <limits.h>

typedef	signed int				MERET;
typedef	unsigned long				MPARAM;
typedef	unsigned long				UPARAM;

#ifdef GOGO_DLL_EXPORTS
#define		EXPORT				__declspec(dllexport) 
#else
#define		EXPORT					
#endif


#define	ME_NOERR				(0)	// return normally;正常終了
#define	ME_EMPTYSTREAM				(1)	// stream becomes empty;ストリームが最後に達した
#define	ME_HALTED				(2)	// stopped by user;(ユーザーの手により)中断された
#define	ME_INTERNALERROR			(10)	// internal error; 内部エラー
#define	ME_PARAMERROR				(11)	// parameters error;設定でパラメーターエラー
#define	ME_NOFPU				(12)	// no FPU;FPUを装着していない!!
#define	ME_INFILE_NOFOUND			(13)	// can't open input file;入力ファイルを正しく開けない
#define	ME_OUTFILE_NOFOUND			(14)	// can't open output file;出力ファイルを正しく開けない
#define	ME_FREQERROR				(15)	// frequency is not good;入出力周波数が正しくない
#define	ME_BITRATEERROR				(16)	// bitrate is not good;出力ビットレートが正しくない
#define	ME_WAVETYPE_ERR				(17)	// WAV format is not good;ウェーブタイプが正しくない
#define	ME_CANNOT_SEEK				(18)	// can't seek;正しくシーク出来ない
#define	ME_BITRATE_ERR				(19)	// only for compatibility;ビットレート設定が正しくない
#define	ME_BADMODEORLAYER			(20)	// mode/layer not good;モード・レイヤの設定異常
#define	ME_NOMEMORY				(21)	// fail to allocate memory;メモリアローケーション失敗
#define	ME_CANNOT_SET_SCOPE			(22)	// thread error;スレッド属性エラー(pthread only)
#define	ME_CANNOT_CREATE_THREAD			(23)	// fail to create thear;スレッド生成エラー
#define	ME_WRITEERROR				(24)	// lock of capacity of disk;記憶媒体の容量不足


// definition of call-back function for user;ユーザーのコールバック関数定義
typedef	MERET	(*MPGE_USERFUNC)(void *buf, unsigned long nLength );
#define MPGE_NULL_FUNC (MPGE_USERFUNC)NULL	// for HighC

///////////////////////////////////////////////////////////////////////////
// Configuration
///////////////////////////////////////////////////////////////////////////
// for INPUT
#define		MC_INPUTFILE			(1)
// para1 choice of input device
	#define		MC_INPDEV_FILE		(0)		// input device is file;入力デバイスはファイル
	#define		MC_INPDEV_STDIO		(1)		//                 stdin;入力デバイスは標準入力
	#define		MC_INPDEV_USERFUNC	(2)		//       defined by user;入力デバイスはユーザー定義
	// para2 (必要であれば)ファイル名。ポインタを指定する
	// メモリよりエンコードの時は以下の構造体のポインタを指定する.
	struct MCP_INPDEV_USERFUNC {
		MPGE_USERFUNC	pUserFunc;	// pointer to user-function for call-back or MPGE_NULL_FUNC if none
						// コールバック対象のユーザー関数。未定義時MPGE_NULL_FUNCを代入
		unsigned int	nSize;		// size of file or MC_INPDEV_MEMORY_NOSIZE if unknown
						// ファイルサイズ。不定の時は MC_INPDEV_MEMORY_NOSIZEを指定
		int				nBit;	// nBit = 8 or 16 ; PCMビット深度を指定
		int				nFreq;	// input frequency ; 入力周波数の指定
		int				nChn;	// number of channel(1 or 2) ; チャネル数
	};
	#define		MC_INPDEV_MEMORY_NOSIZE		(UINT_MAX)

///////////////////////////////////////////////////////////////////////////
// for OUTPUT ( now stdout is not support )
#define		MC_OUTPUTFILE			(2)
// para1 choice of output device
	#define		MC_OUTDEV_FILE		(0)		// output device is file;出力デバイスはファイル
	#define		MC_OUTDEV_STDOUT	(1)		//                  stdout; 出力デバイスは標準出力
	#define		MC_OUTDEV_USERFUNC	(2)		//        defined by user;出力デバイスはユーザー定義
// para2 pointer to file if necessary ;(必要であれば)ファイル名。ポインタ指定

///////////////////////////////////////////////////////////////////////////
// mode of encoding ;エンコードタイプ
#define		MC_ENCODEMODE			(3)
// para1 mode;モード設定
	#define		MC_MODE_MONO		(0)		// mono;モノラル
	#define		MC_MODE_STEREO		(1)		// stereo;ステレオ
	#define		MC_MODE_JOINT		(2)		// joint-stereo;ジョイント
	#define		MC_MODE_MSSTEREO	(3)		// mid/side stereo;ミッドサイド
	#define		MC_MODE_DUALCHANNEL	(4)		// dual channel;デュアルチャネル

///////////////////////////////////////////////////////////////////////////
// bitrate;ビットレート設定
#define		MC_BITRATE				(4)
// para1 bitrate;ビットレート 即値指定


///////////////////////////////////////////////////////////////////////////
// frequency of input file (force);入力で用いるサンプル周波数の強制指定
#define		MC_INPFREQ				(5)
// para1 frequency;入出力で用いるデータ

///////////////////////////////////////////////////////////////////////////
// frequency of output mp3 (force);出力で用いるサンプル周波数の強制指定
#define		MC_OUTFREQ				(6)
// para1 frequency;入出力で用いるデータ

///////////////////////////////////////////////////////////////////////////
// size ofheader if you ignore WAV-header (for example cda);エンコード開始位置の強制指定(ヘッダを無視する時)
#define		MC_STARTOFFSET			(7)

///////////////////////////////////////////////////////////////////////////
// psycho-acoustics ON/OFF;心理解析 ON/OFF
#define		MC_USEPSY				(8)
// PARA1 boolean(TRUE/FALSE)

///////////////////////////////////////////////////////////////////////////
// 16kHz low-pass filter ON/OFF;16KHz低帯域フィルタ ON/OFF
#define		MC_USELPF16				(9)
// PARA1 boolean(TRUE/FALSE)

///////////////////////////////////////////////////////////////////////////
// use special UNIT, para1:boolean; ユニット指定 para1:BOOL値
#define		MC_USEMMX				(10)	// MMX
#define		MC_USE3DNOW				(11)	// 3DNow!
#define		MC_USEKNI				(12)	// SSE(KNI)
#define		MC_USEE3DNOW				(13)	// Enhanced 3D Now!
#define		MC_USESPC1				(14)	// special switch for debug
#define		MC_USESPC2				(15)	// special switch for debug

///////////////////////////////////////////////////////////////////////////
// addition of TAG; ファイルタグ情報付加
#define		MC_ADDTAG				(16)
// dwPara1  length of TAG;タグ長  
// dwPara2  pointer to TAG;タグデータのポインタ

///////////////////////////////////////////////////////////////////////////
// emphasis;エンファシスタイプの設定
#define		MC_EMPHASIS				(17)	
// para1 type of emphasis;エンファシスタイプの設定
	#define		MC_EMP_NONE		(0)		// no empahsis;エンファシスなし(dflt)
	#define		MC_EMP_5015MS		(1)		// 50/15ms    ;エンファシス50/15ms
	#define		MC_EMP_CCITT		(3)		// CCITT      ;エンファシスCCITT

///////////////////////////////////////////////////////////////////////////
// use VBR;VBRタイプの設定
#define		MC_VBR				(18)

///////////////////////////////////////////////////////////////////////////
// SMP support para1: interger
#define		MC_CPU				(19)

///////////////////////////////////////////////////////////////////////////
// for RAW-PCM; 以下4つはRAW-PCMの設定のため
// byte swapping for 16bitPCM; PCM入力時のlow, high bit 変換
#define		MC_BYTE_SWAP			(20)

///////////////////////////////////////////////////////////////////////////
// for 8bit PCM
#define		MC_8BIT_PCM			(21)

///////////////////////////////////////////////////////////////////////////
// for mono PCM
#define		MC_MONO_PCM			(22)

///////////////////////////////////////////////////////////////////////////
// for Towns SND
#define		MC_TOWNS_SND			(23)

///////////////////////////////////////////////////////////////////////////
// BeOS Encode thread priority
#define		MC_THREAD_PRIORITY		(24)

///////////////////////////////////////////////////////////////////////////
// BeOS Read thread priority
//#if	defined(USE_BTHREAD)
#define		MC_READTHREAD_PRIORITY		(25)
//#endif

MERET	EXPORT	MPGE_initializeWork();
MERET	EXPORT	MPGE_setConfigure(MPARAM mode, UPARAM dwPara1, UPARAM dwPara2 );
MERET	EXPORT	MPGE_getConfigure(MPARAM mode, void *para1 );
MERET	EXPORT	MPGE_detectConfigure();
#ifdef USE_BETHREAD
MERET	EXPORT	MPGE_processFrame(int *frameNum);
#else
MERET	EXPORT	MPGE_processFrame();
#endif
MERET	EXPORT	MPGE_closeCoder();
MERET	EXPORT	MPGE_endCoder();
MERET	EXPORT	MPGE_getUnitStates( unsigned long *unit );

// This function is effective for gogo.dll;このファンクションはDLLバージョンのみ有効
MERET	EXPORT	MPGE_getVersion( unsigned long *vercode,  char *verstring );
// vercode = 0x125 ->  version 1.25
// verstring       ->  "ver 1.25 1999/09/25" (allocate abobe 260bytes buffer)



////////////////////////////////////////////////////////////////////////////
// for getting configuration
////////////////////////////////////////////////////////////////////////////

#define		MG_INPUTFILE			(1)	// name of input file ;入力ファイル名取得
#define		MG_OUTPUTFILE			(2)	// name of output file;出力ファイル名取得
#define		MG_ENCODEMODE			(3)	// type of encoding   ;エンコードモード
#define		MG_BITRATE			(4)	// bitrate            ;ビットレート
#define		MG_INPFREQ			(5)	// input frequency    ;入力周波数
#define		MG_OUTFREQ			(6)	// output frequency   ;出力周波数
#define		MG_STARTOFFSET			(7)	// offset of input PCM;スタートオフセット
#define		MG_USEPSY			(8)	// psycho-acoustics   ;心理解析を使用する/しない
#define		MG_USEMMX			(9)	// MMX
#define		MG_USE3DNOW			(10)	// 3DNow!
#define		MG_USEKNI			(11)	// SSE(KNI)
#define		MG_USEE3DNOW			(12)	// Enhanced 3DNow!

#define		MG_USESPC1			(13)	// special switch for debug
#define		MG_USESPC2			(14)	// special switch for debug
#define		MG_COUNT_FRAME			(15)	// amount of frame
#define		MG_NUM_OF_SAMPLES		(16)	// number of sample for 1 frame;1フレームあたりのサンプル数
#define		MG_MPEG_VERSION			(17)	// MPEG VERSION
#define		MG_READTHREAD_PRIORITY		(18)	// thread priority to read for BeOS

#endif /* __MUSUI_H__ */
