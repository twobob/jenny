# jenny

Jenny turns a ChordStream token stream into a MIDI chord part.

She reads either ChordStream output, the table or `--tsv`, from a file or a pipe, and writes a single-track MIDI file.

```
chordstream test.mid | jenny -o chords.mid
chordstream --tsv test.mid | jenny --gm 5 --oct 3 --beat 1x1x0 --accent +20 -o chords.mid
```

## Options

| option | meaning | default |
|---|---|---|
| `-o FILE` | output MIDI file | `jenny.mid` |
| `--gm N` | General MIDI program, 1 to 128 | 1, Acoustic Grand Piano |
| `--oct N` | octave of the chord root, C4 = 60 | 4 |
| `--beat PAT` | one character per sixteenth: `1` strikes, `x` holds, `0` is off; up to 128 long, padded with `0` to whole bars | `1xxxxxxxxxxxxxx0` |
| `--vel N` | hit velocity, 1 to 127 | 75 |
| `--accent V` | downbeat velocity: `+N` or `-N` relative to `--vel`, or `N` absolute | same as `--vel` |
| `--metre N/D` | time signature, sets the sixteenths per bar for `--beat` | 4/4 |
| `--bpm N` | tempo | 120 |

Silence and solo tokens play nothing. A chord change on an `x` cell strikes the new chord.

## Build

Pure C11. With Visual Studio 2022, run `build_jenny.bat`. Elsewhere:

```
cc -std=c11 -O2 -o jenny jenny.c
```
