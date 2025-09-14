# TinyWTmusicbox
ATtiny402 based Wave Table Music Player which has Orgel Tones

## ATtiny402 オルゴール風 Wave Table プレーヤー
- Chan氏の有名な[WaveTable電子オルゴール](https://elm-chan.org/works/mxb/report_j.html)をテクストとしたプロジェクト
- WaveTableデータ、Envelopeデータはそのまま流用させていただいた（Angular値の補正でfSの差異を吸収）
- MPUをATtiny45からATtiny402に変更（たまたま手元にあった）
- コードをアセンブラからC言語に変更（アセンブラわからない）
- 開発環境はMicrochip Studio（ベアメタル）
- PWM周波数は250KHzから62.5KHzに変更（タイマー/カウンタのクロック周波数による制限）
- サンプリング周波数(fS)を32KHzから20.833KHzに変更（再生帯域とPWM周波数から適当に決定）
- 同時発音数は8音（単純なラウンドロビンキュー方式のため音切れが目立つ場合有り）
- 音域はC-2からEb-7の64音
- サスティーンは最低音約3.2秒～最高音約0.7秒に設定（音階に対する傾きはAngular値の傾きの1/16）
- fS割り込み単位でWaveTableデータとEnvelope値からPWMレジスタ値を求めて積算後にレジスタ（バッファを使用）を更新
- メインループでは5ms周期で発音タイミングとEnvelope値の更新をおこなっている
- ATtiny402のCCLでPWM比較出力信号の反転出力を作成しスピーカーをプッシュプル（？）で駆動
- 楽譜データをMIDIファイルから変換できるスクリプトを作成（次項）

## 楽譜データの作成
- PythonのMIDOライブラリを利用してMIDIファイルから専用楽譜データを作成可能（./midi2score/midi2tiny_v00.py）
- 変換できるMIDIファイルにはいろいろ制限あり（オルゴールっぽく聞こえるアレンジを含め実際に試してみるより他に…）

## 音色について
- PWM周期とfSのせいかオルゴール特有の金属的な響きが薄れてしまっているような気が (-_-;)
- オルゴールっぽく聞かせるには元データのアレンジも必要（未対応）
- スピーカーを入れる箱次第でそれっぽく聞こえる場合もある？

## 謝辞
Chan様 ありがとうございました</br>
[ニコニコ動画 AVRでオルゴール作ってみた](https://www.nicovideo.jp/watch/sm14245861) ありがとうございました