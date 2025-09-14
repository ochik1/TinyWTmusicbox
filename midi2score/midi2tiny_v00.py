'''
  TinyOrgel Project
    MIDI file to score file convertor
    by K.Ochi
'''

import sys
import os
import mido

version = 'v0.0'

TOOTH_HI = 63
TOOTH_LO = 0

abstic = 0.004992	# real roop timing
transpose = 0


def usage():
    print("USAGE:")
    print(f"python {argvs[0]} <input file> <output file> [transpose]")
    print("")


def get_ticks(mid):
    return mid.ticks_per_beat

# 曲を通してテンポは一定であること
# トラック0から順にスキャンして最初のset_tempoを用いる
def get_midi_tempo(mid):
    for i, track in enumerate(mid.tracks):
#        print(f"\nTrack {i}: {track.name}")
        for msg in track:
            if msg.type == 'set_tempo':
                tempo = msg.tempo  # microseconds per beat
                return mido.tempo2bpm(tempo)

# 拍子、テンポ情報（複数あるかも）を取得して辞書のリストを返す
def get_time_signature(mid):
    # 拍子情報を格納するリスト（複数ある場合に備えて）
    time_signatures = []

    for track in mid.tracks:
        for msg in track:
            if msg.type == 'time_signature':
                time_signatures.append({
                    'numerator': msg.numerator,
                    'denominator': msg.denominator,
                    'clocks_per_click': msg.clocks_per_click,
                    'notated_32nd_notes_per_beat': msg.notated_32nd_notes_per_beat,
                    'tick': msg.time  # delta time（必要なら絶対tickに変換可能）
                })
    return time_signatures


# 小節番号と拍番号を算出
def tick_to_measure_beat(absolute_tick, ticks_per_beat, numerator, denominator):
    ticks_per_note = ticks_per_beat * 4 // denominator
    ticks_per_measure = ticks_per_note * numerator
    measure = absolute_tick // ticks_per_measure
    beat = (absolute_tick % ticks_per_measure) // ticks_per_note
    return measure, beat


''' Main Function from here'''
# 引数の取得
argvs = sys.argv
argc = len(sys.argv)
if argc <= 2:
    usage()
    sys.exit(-1)
elif argc == 3:
    in_file = argvs[1]
    out_file = argvs[2]
elif argc == 4:
    in_file = argvs[1]
    out_file = argvs[2]
    transpose = argvs[3]

if not os.path.isfile(in_file):
    usage()
    sys.exit(-1)

print("\nConvert the MIDI file into the TinyOrgel score file")
print(f"\tInput File: {in_file} Output File: {out_file} Transpose: {transpose}\n")


# 入力MIDIファイルを読み込み
mid = mido.MidiFile(argvs[1])

### 拍子とテンポを抽出（リストの先頭を使う）
time_signature = get_time_signature(mid)
#print(time_signature)
numerator = time_signature[0]['numerator']
denominator = time_signature[0]['denominator']
#print(f"拍子: {numerator}/{denominator}\n")

### BPMとTICKを抽出 
ticks = get_ticks(mid)
bpm = get_midi_tempo(mid);
# abs?timeをTinyOrgelのレートに合わせる
rate = (60 / bpm / ticks) / abstic
print("BPM: ", bpm, ", TICKS: ", ticks, ", RATE: ", rate)


### トラックのマージ
# 新しいトラックを作成して、すべてのメッセージをそこに集約
merged_track = mido.MidiTrack()


# 各トラックのメッセージを時間順に並べるため、絶対時間に変換
all_messages = []
for track in mid.tracks:
    abs_time = 0
    for msg in track:
        abs_time += msg.time
        all_messages.append((abs_time, msg))

# 絶対時間でソート
all_messages.sort(key=lambda x: x[0])

# 再び相対時間に変換して新しいトラックに追加
last_time = 0
for abs_time, msg in all_messages:
    msg.time = abs_time - last_time
    merged_track.append(msg)
    last_time = abs_time


# 抽出対象のメッセージ(note_onとnote_offのみ)を格納するリスト（絶対時間付き）
filtered_messages = []

abs_time = 0
for msg in merged_track:
    abs_time += msg.time
    if msg.type in ['note_on', 'note_off']:
        filtered_messages.append((abs_time, msg))

# 絶対時間でソート
filtered_messages.sort(key=lambda x: x[0])

### 演奏終了マーカーをスコアに追加するため最終イベントを抽出
current_tick = 0  # 各トラックごとにtickを初期化
last_note_event_tick = 0
for msg in merged_track:
    current_tick += msg.time  # delta timeを加算して絶対tickに変換
    if msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
        last_note_event_tick = max(last_note_event_tick, current_tick)


### 曲の最終小節の拍を求める
last_measure, last_beat = tick_to_measure_beat(last_note_event_tick, ticks, numerator, denominator)
print(f"最終小節-最終拍: {last_measure}-{last_beat}")

# 終了マーカーを追加するポジション
ticks_per_note = ticks * 4 // denominator
ticks_per_measure = ticks_per_note * numerator
end_marker_position = ticks_per_measure * (last_measure + 1)
print(f"終了マーカーは{end_marker_position}に追加")
# Tinyレートに変換
last_tick = round(last_note_event_tick * rate)


### score file output
header = '// TinyOrgel Score file\n//    Org. MIDI file is '
with open(out_file, mode='w') as f:
    f.write(header)
    f.write(in_file)
    f.write('\n')
    # Ticks
    f.write('const uint16_t score_tick[] = {\n')
    abs_time = 0
    for msg in merged_track:
        abs_time += msg.time
        if msg.type == 'note_on' and msg.velocity != 0:
            f.write('    ')
            f.write(str(round(abs_time * rate)))
            f.write(',\n')
    f.write('    ')
    f.write(str(last_tick))
    f.write('\n};\n\n')
    # Notes
    f.write('const uint8_t score_note[] = {\n')
    for msg in merged_track:
        if msg.type == 'note_on' and msg.velocity != 0:
            f.write('    ')
            note_index = (int)(msg.note) - 48 + (int)(transpose)
            if note_index > TOOTH_HI:
                note_index -= 12
                print('Warnning. The pich range over. set -12') 
            if note_index < TOOTH_LO:
                note_index += 12
                print('Warnning. The pich range over. set +12') 
            f.write(str(note_index))
            f.write(',\n')
    f.write('    ')
    f.write('255')
    f.write('\n};\n\n')

