//
//  wav_to_flac.h
//
//  Created by Jason Hurt on 7/6/12.
#ifdef __cplusplus
extern "C"
#endif

#ifndef Lucidity_wav_to_flac_h
#define Lucidity_wav_to_flac_h

/**
 ** wave_file: the full path and filename of the input wave file
 ** flac_file: the full path and filename of the output flac file, without the .flac extension
 ** split_interval_seconds: if > 0, there will be multiple flac files created every split_interval_seconds, 
      so a 30 second file with split_interval_seconds == 10 will have 3 output files. Pass 0 to encode 1 output file.
 ** out_flac_files: a list of char* to the output files created, should have enough space up to a max of 1024 output files
 **/
int convertWavToFlac(const char *wave_file_in, const char *flac_file_out, int split_interval_seconds, char** out_flac_files);

#endif
