# CSE471 Project1: A Multi-Component Music Synthesizer
A music synthesizer program that reads an XML file for input and generates output as an audio file. Includes drums (bass, snare, hihat, toms, cymbal) and a toned instrument for melodies.

## Details
**Title of the musical selection:** Lasso
**Duration:** 1 minute, 6 seconds
**Format:** 44,100 Hz, 16-bit stero WAV
**File:** `lasso.wav`
**Note count:** 609

*Note: Generative AI (ChatGPT) was used to assist with music composition.*

## Group Members
- **Cindy Huang** (solo project)
  - Responsible for: Drum Synthesizer, Effects Component, Sequencer

## Score File Format
The program accepts XML score files with the following structure:
```
<?xml version="1.0" encoding="utf-8"?>
<score bpm="120" beatspermeasure="4">
  <instrument instrument="DrumInstrument">
    <note measure="1" beat="1"   type="kick"   duration="0.5" velocity="0.95"/>
    <note measure="1" beat="2"   type="snare"  duration="0.5" velocity="0.88"/>
  	<note measure="1" beat="1"   type="hihat"  duration="0.1" velocity="0.76"/>
  </instrument>
  <instrument instrument="ToneInstrument">
    <note measure="1" beat="1"   duration="1.0" note="A4"/>
    <note measure="1" beat="2"   duration="0.5" note="C5"/>
    <note measure="1" beat="2.5" duration="0.5" note="E5"/>
    </instrument>      
</score>
```
### Attributes:
**Score level:**
- `bpm` - Beats per minute (tempo)
- `beatspermeasure` - Time signature denominator

**DrumInstrument notes:**
- `measure` - Measure number (1-based)
- `beat` - Beat within measure (1-based, can use decimals like 1.5)
- `type` - Drum type: "kick", "snare", "hihat", "tom-hi", "tom-mid", "tom-lo", "cymbal"
- `duration` - Note length in beats
- `velocity` - Volume (0.0-1.0)
- `pitch` - (Optional) Pitch offset in semitones for toms

**ToneInstrument notes:**
- `measure`, `beat`, `duration` - Same as drums
- `note` - Musical note (e.g., "C4", "F#5", "Bb3")

## Components
### Drum Synthesizer Component
**Owner:** Cindy Huang

**Description:**  
The drum synthesizer generates five distinct drum sounds from scratch using synthesis techniques:

1. **Kick Drum (Bass)** - Synthesized using a swept sine wave that starts at ~195Hz and quickly drops to 55Hz, with a short click transient at 1000Hz for attack
2. **Snare Drum** - Combination of 60% sine wave at 200Hz (drum body) and 40% filtered noise at 3000Hz (snare wire buzz)
3. **Hi-Hat** - Lightly filtered white noise at 8000Hz cutoff for a crisp "tss" sound
4. **Toms** (High, Mid, Low) - Swept pitch sine waves starting at different base frequencies (155Hz, 110Hz, 82Hz) with exponential pitch decay
5. **Cymbal** - Raw white noise with minimal filtering for metallic crash sound

**Technical Implementation:**
- **Polyphony:** Voice-based system allows multiple drums to play simultaneously (up to 64 voices)
- **Envelope Generation:** ADSR envelope with configurable attack, decay, sustain, and release
- **Synthesis:** All sounds generated from scratch using oscillators, noise generators, and one-pole filters
- **Per-voice state:** Each drum hit maintains independent oscillator phases and filter states

### Effects Component
**Owner:** Cindy Huang

**Description:**  
The effects component mixes audio streams from multiple instruments and applies audio processing. It simply passes audio currently.

## Files in Repository
- `lasso.score` - XML score for final musical selection
- `lasso.wav` - Final audio output
- `drums.score` / `drums.wav` - Drum component demonstration
- `Synthie/` - Source code
- `README.md` - This file. Includes details about the project
