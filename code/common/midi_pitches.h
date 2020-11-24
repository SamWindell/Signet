#pragma once

#include <array>
#include <string>

struct MIDIPitch {
    std::string ToString() const {
        std::array<char, 32> buf {};
        std::snprintf(buf.data(), buf.size(), "%.2f", pitch);
        return std::string(name) + " (" + std::string(buf.data()) + "Hz - MIDI " + std::to_string(midi_note) +
               ")";
    }

    std::string GetPitchString() const {
        std::array<char, 32> buf {};
        std::snprintf(buf.data(), buf.size(), "%.0f", pitch);
        return buf.data();
    }

    int midi_note;
    const char *name;
    double pitch;
};

static constexpr MIDIPitch g_midi_pitches[] = {
    {0, "C-1", 8.18},      {1, "C#-1", 8.66},     {2, "D-1", 9.18},       {3, "D#-1", 9.72},
    {4, "E-1", 10.3},      {5, "F-1", 10.91},     {6, "F#-1", 11.56},     {7, "G-1", 12.25},
    {8, "G#-1", 12.98},    {9, "A-1", 13.75},     {10, "A#-1", 14.57},    {11, "B-1", 15.43},
    {12, "C0", 16.35},     {13, "C#0", 17.32},    {14, "D0", 18.35},      {15, "D#0", 19.45},
    {16, "E0", 20.6},      {17, "F0", 21.83},     {18, "F#0", 23.12},     {19, "G0", 24.5},
    {20, "G#0", 25.96},    {21, "A0", 27.5},      {22, "A#0", 29.14},     {23, "B0", 30.87},
    {24, "C1", 32.7},      {25, "C#1", 34.65},    {26, "D1", 36.71},      {27, "D#1", 38.89},
    {28, "E1", 41.2},      {29, "F1", 43.65},     {30, "F#1", 46.25},     {31, "G1", 49},
    {32, "G#1", 51.91},    {33, "A1", 55},        {34, "A#1", 58.27},     {35, "B1", 61.74},
    {36, "C2", 65.41},     {37, "C#2", 69.3},     {38, "D2", 73.42},      {39, "D#2", 77.78},
    {40, "E2", 82.41},     {41, "F2", 87.31},     {42, "F#2", 92.5},      {43, "G2", 98},
    {44, "G#2", 103.83},   {45, "A2", 110},       {46, "A#2", 116.54},    {47, "B2", 123.47},
    {48, "C3", 130.81},    {49, "C#3", 138.59},   {50, "D3", 146.83},     {51, "D#3", 155.56},
    {52, "E3", 164.81},    {53, "F3", 174.61},    {54, "F#3", 185},       {55, "G3", 196},
    {56, "G#3", 207.65},   {57, "A3", 220},       {58, "A#3", 233.08},    {59, "B3", 246.94},
    {60, "C4", 261.63},    {61, "C#4", 277.18},   {62, "D4", 293.66},     {63, "D#4", 311.13},
    {64, "E4", 329.63},    {65, "F4", 349.23},    {66, "F#4", 369.99},    {67, "G4", 392},
    {68, "G#4", 415.3},    {69, "A4", 440},       {70, "A#4", 466.16},    {71, "B4", 493.88},
    {72, "C5", 523.25},    {73, "C#5", 554.37},   {74, "D5", 587.33},     {75, "D#5", 622.25},
    {76, "E5", 659.26},    {77, "F5", 698.46},    {78, "F#5", 739.99},    {79, "G5", 783.99},
    {80, "G#5", 830.61},   {81, "A5", 880},       {82, "A#5", 932.33},    {83, "B5", 987.77},
    {84, "C6", 1046.5},    {85, "C#6", 1108.73},  {86, "D6", 1174.66},    {87, "D#6", 1244.51},
    {88, "E6", 1318.51},   {89, "F6", 1396.91},   {90, "F#6", 1479.98},   {91, "G6", 1567.98},
    {92, "G#6", 1661.22},  {93, "A6", 1760},      {94, "A#6", 1864.66},   {95, "B6", 1975.53},
    {96, "C7", 2093},      {97, "C#7", 2217.46},  {98, "D7", 2349.32},    {99, "D#7", 2489.02},
    {100, "E7", 2637.02},  {101, "F7", 2793.83},  {102, "F#7", 2959.96},  {103, "G7", 3135.96},
    {104, "G#7", 3322.44}, {105, "A7", 3520},     {106, "A#7", 3729.31},  {107, "B7", 3951.07},
    {108, "C8", 4186.01},  {109, "C#8", 4434.92}, {110, "D8", 4698.64},   {111, "D#8", 4978.03},
    {112, "E8", 5274.04},  {113, "F8", 5587.65},  {114, "F#8", 5919.91},  {115, "G8", 6271.93},
    {116, "G#8", 6644.88}, {117, "A8", 7040},     {118, "A#8", 7458.62},  {119, "B8", 7902.13},
    {120, "C9", 8372.02},  {121, "C#9", 8869.84}, {122, "D9", 9397.27},   {123, "D#9", 9956.06},
    {124, "E9", 10548.08}, {125, "F9", 11175.3},  {126, "F#9", 11839.82}, {127, "G9", 12543.85},
};

MIDIPitch FindClosestMidiPitch(const double freq);
int ScaleByOctavesToBeNearestToMiddleC(int midi_note);
