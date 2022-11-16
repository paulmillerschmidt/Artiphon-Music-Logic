//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Sat Jun 30 11:52:06 PDT 2001
// Last Modified: Mon Feb  9 21:14:03 PST 2015 Updated for C++11.
// Filename:      ...sig/examples/all/midi2melody.cpp
// Web Address:   http://sig.sapp.org/examples/museinfo/humdrum/midi2melody.cpp
// Syntax:        C++; museinfo
//
// Description:   Converts a single melody MIDI file/track into an ASCII text
//                format with starting time and pitch.
//

#include "MidiFile.h"
#include "Options.h"
#include <vector>
#include <iostream>
#include "VecMath.h"
#include "Section.h"
#include "CMMG.h"

using namespace std;
using namespace smf;

//Ticks per quarter note. MIDI default is 48. 
#define TPQ 120

class Melody {
   public:
      double tick;
      double duration;
      int    pitch;
};

// user interface variables
Options options;
int     track = 0;          // used with the -t option

// function declarations:
void      checkOptions      (Options& opts, int argc, char** argv);
void      example           (void);
void      usage             (const char* command);
void      convertToMelody   (MidiFile& midifile, vector<Melody>& melody);
void      printMelody       (vector<Melody>& melody, int tpq);
void      sortMelody        (vector<Melody>& melody);
int       notecompare       (const void* a, const void* b);

//my functions
std::vector<int> cacheMelody(std::vector<Melody>& melody, int tpq);
void note_on(MidiFile *file, int channel, float time, int note, char velocity);
void note_off(MidiFile *file, int channel, float time, int note, char velocity);
void add_note(MidiFile *file, int channel, Note note, float time);
void add_notes(MidiFile *file, int channel, std::vector<Note> notes, float time);
void add_consecutive_notes(MidiFile *file, int channel, std::vector<Note> notes, float init_time);
void add_progression(MidiFile *file, int channel, ChordProgression cp, float init_time);
//////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
   checkOptions(options, argc, argv);
   MidiFile midifile(options.getArg(1));
   if (options.getBoolean("track-count")) {
      cout << midifile.getTrackCount() << endl;
      return 0;
   }
   if (!options.getBoolean("track")) {
      midifile.joinTracks();
   } 

   vector<Melody> melody;
   convertToMelody(midifile, melody);
   sortMelody(melody);
   printMelody(melody, midifile.getTicksPerQuarterNote());

   return 0;
}



//cache melody
std::vector<int> cacheMelody(std::vector<Melody>& melody, int tpq) {
   std::vector<int> out;
   if (melody.size() < 1) {
      return {0};
   }
   Melody temp;
   temp.tick = melody[melody.size()-1].tick +
               melody[melody.size()-1].duration;
   temp.pitch = 0;
   temp.duration = 0;
   melody.push_back(temp);

   for (int i=0; i<(int)melody.size()-1; i++) {
      double delta = melody[i+1].tick - melody[i].tick;
      if (delta == 0) {
         continue;
      }
      // cout << melody[i].pitch << "\n";
      out.push_back(melody[i].pitch);

   }
   return out;

}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////
//
// sortMelody --
//

void sortMelody(vector<Melody>& melody) {
   qsort(melody.data(), melody.size(), sizeof(Melody), notecompare);
}



//////////////////////////////
//
// printMelody --
//   only print the highest voice if multiple notes played together.
//

void printMelody(vector<Melody>& melody, int tpq) {
   if (melody.size() < 1) {
      return;
   }

   Melody temp;
   temp.tick = melody[melody.size()-1].tick +
               melody[melody.size()-1].duration;
   temp.pitch = 0;
   temp.duration = 0;
   melody.push_back(temp);

   for (int i=0; i<(int)melody.size()-1; i++) {
      double delta = melody[i+1].tick - melody[i].tick;
      if (delta == 0) {
         continue;
      }

      cout << (double)melody[i].tick/tpq
           << "\t" << melody[i].pitch
           // << "\t" << (double)melody[i].duration/tpq
           << "\n";
      if (delta > melody[i].duration) {
         cout << (melody[i+1].tick - (delta - melody[i].duration))/(double)tpq
              << "\t" << 0
              << "\n";
      }
   }
   cout << (double)melody[melody.size()-1].tick/tpq
        << "\t" << 0
        << "\n";
}



//////////////////////////////
//
// convertToMelody --
//

void convertToMelody(MidiFile& midifile, vector<Melody>& melody) {
   midifile.absoluteTicks();
   if (track < 0 || track >= midifile.getNumTracks()) {
      cout << "Invalid track: " << track << " Maximum track is: "
           << midifile.getNumTracks() - 1 << endl;
   }
   int numEvents = midifile.getNumEvents(track);

   vector<int> state(128);   // for keeping track of the note states

   int i;
   for (i=0; i<128; i++) {
      state[i] = -1;
   }

   melody.reserve(numEvents);
   melody.clear();

   Melody mtemp;
   int command;
   int pitch;
   int velocity;

   for (i=0; i<numEvents; i++) {
      command = midifile[track][i][0] & 0xf0;
      if (command == 0x90) {
         pitch = midifile[track][i][1];
         velocity = midifile[track][i][2];
         if (velocity == 0) {
            // note off
            goto noteoff;
         } else {
            // note on
            state[pitch] = midifile[track][i].tick;
         }
      } else if (command == 0x80) {
         // note off
         pitch = midifile[track][i][1];
noteoff:
         if (state[pitch] == -1) {
            continue;
         }
         mtemp.tick = state[pitch];
         mtemp.duration = midifile[track][i].tick - state[pitch];
         mtemp.pitch = pitch;
         melody.push_back(mtemp);
         state[pitch] = -1;
      }
   }
}



//////////////////////////////
//
// checkOptions --
//

void checkOptions(Options& opts, int argc, char* argv[]) {
   opts.define("t|track=i:0",        "Track from which to extract melody");
   options.define("c|track-count=b", "List number of tracks");

   opts.define("author=b",  "author of program");
   opts.define("version=b", "compilation info");
   opts.define("example=b", "example usages");
   opts.define("h|help=b",  "short description");
   opts.process(argc, argv);

   // handle basic options:
   if (opts.getBoolean("author")) {
      cout << "Written by Craig Stuart Sapp, "
           << "craig@ccrma.stanford.edu, 30 June 2001" << endl;
      exit(0);
   } else if (opts.getBoolean("version")) {
      cout << argv[0] << ", version: June 2001" << endl;
      cout << "compiled: " << __DATE__ << endl;
      exit(0);
   } else if (opts.getBoolean("help")) {
      usage(opts.getCommand().c_str());
      exit(0);
   } else if (opts.getBoolean("example")) {
      example();
      exit(0);
   }

   if (opts.getArgCount() != 1) {
      usage(opts.getCommand().c_str());
      exit(1);
   }

   track = opts.getInteger("track");
}



//////////////////////////////
//
// example --
//

void example(void) {

}



//////////////////////////////
//
// usage --
//

void usage(const char* command) {

}



///////////////////////////////
//
// notecompare -- for sorting the times of Melody messages
//

int notecompare(const void* a, const void* b) {
   const Melody& aa = *static_cast<const Melody*>(a);
   const Melody& bb = *static_cast<const Melody*>(b);

   if (aa.tick < bb.tick) {
      return -1;
   } else if (aa.tick > bb.tick) {
      return 1;
   } else {
      // highest note comes first
      if (aa.pitch > bb.pitch) {
         return 1;
      } else if (aa.pitch < bb.pitch) {
         return -1;
      } else {
         return 0;
      }
   }
}


/* note_on
@brief - add note on message to MIDI file. 
@param - MidiFile to add note to, the channel, and the parameters of the note. 
@return - void.
*/
void note_on(MidiFile *file, int channel, float time, int note, char velocity) {
    vector<uchar> midievent;     // temporary storage for MIDI events
    midievent.resize(3); 
    midievent[0] = 0x90; //note on
    midievent[1] = note;
    midievent[2] = velocity;
    file->addEvent(channel, time, midievent);
};
/* note_off
@brief - add note off message to MIDI file. 
@param - MidiFile to add note to, the channel, and the parameters of the note. 
@return - void.
*/
void note_off(MidiFile *file, int channel, float time, int note, char velocity) {
    vector<uchar> midievent;     // temporary storage for MIDI events
    midievent.resize(3);  
    midievent[0] = 0x80; //note off
    midievent[1] = note;
    midievent[2] = velocity;
    file->addEvent(channel, time, midievent);
};
/* add_note
@brief - adds note to file. 
@param - MidiFile and channel to add to, Note to be added and bpm of track. 
@return - void. 
*/
void add_note(MidiFile *file, int channel, Note note, float time) {
    note_on(file, 1, time * TPQ, note.get_tone(), note.get_vel());
    note_off(file, 1, (time + note.get_length()) * TPQ, note.get_tone(), note.get_vel());

};
/* add_notes
@brief - adds a collection of notes to file. 
@param - MidiFile and channel to add to, notes to be added and bpm of track. 
@return - void. 
*/
void add_notes(MidiFile *file, int channel, std::vector<Note> notes, float time) {
    for (Note note : notes) 
        add_note(file, channel, note, time);
};
/* add_notes consecutively 
@brief - adds a collection of notes to file. 
@param - MidiFile and channel to add to, notes to be added and bpm of track. 
@return - void. 
*/
void add_consecutive_notes(MidiFile *file, int channel, std::vector<Note> notes, float init_time) {
    float time = init_time;
    for (Note note : notes) {
        add_note(file, channel, note, time);
        time += note.get_length();
    };
};
/*add progression 
@brief - add entire chord progression, consecutively.
*/
void add_progression(MidiFile *file, int channel, ChordProgression cp, float init_time) {
    float time = init_time;
    for (Chord c : cp.get_cp()) {
        add_notes(file, channel, c.get_chord(), time);
        time += c.get_length();
    };
};