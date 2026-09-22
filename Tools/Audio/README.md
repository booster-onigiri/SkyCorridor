# Original procedural score

The three music cues keep Sky Corridor's original notes, open chord voicings,
sparse phrase structure, and unresolved endings. All sounds are synthesized
from equations: softly damped, slightly inharmonic string modes; a quiet
additive harmonic halo; generated hammer noise; and a generated diffuse room.
There are no recorded instrument samples or external impulse responses.

From the repository root:

```console
python -m pip install -r Tools/Audio/requirements.txt
python Tools/Audio/compose_music89.py
```

The default output is `Project/SourceArt/Audio89/Masters-v1`, resolved relative
to this script. Use `--output another-directory` to render elsewhere. Existing
outputs cause an error and are never overwritten. The only rendering dependency
is NumPy; Python's standard library writes 48 kHz, 16-bit stereo PCM WAV files.
NumPy is a separately installed BSD-licensed dependency and is not vendored.

`score-and-masters.json` contains the complete note events, synthesis provenance,
per-cue seeds, runtime versions, WAV hashes, and PCM measurements. No sample-library
license is needed for these synthesized recordings. The enclosing repository's
license determines the terms for this source and its original score material.

This workflow never opens an audio device. Technical measurements and deterministic
rerender checks do not establish subjective listening quality; speaker/headphone
listening acceptance remains untested.
