/*
 * Use libFLAC to convert an Apple WAVE file to one or more flac encoded files
 * Created by Jason Hurt on 7/6/12.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FLACiOS/metadata.h"
#include "FLACiOS/stream_encoder.h"

static void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data);

#define READSIZE 1024

static FLAC__byte buffer[READSIZE/*samples*/ * 2/*bytes_per_sample*/ * 2/*channels*/];
static FLAC__int32 pcm[READSIZE/*samples*/ * 2/*channels*/];

int convertWavToFlac(const char *wave_file, const char *flac_file, int split_interval_seconds, char** out_flac_files) {
    FILE *fin;
    if((fin = fopen(wave_file, "rb")) == NULL) {
        fprintf(stderr, "ERROR: opening %s for output\n", wave_file);
        return 1;
    }
    
    // read wav header and validate it, note this will most likely fail for WAVE files not created by Apple
    if(fread(buffer, 1, 44, fin) != 44 ||
       memcmp(buffer, "RIFF", 4) ||
       memcmp(buffer+36, "FLLR", 4)) {
        fprintf(stderr, "ERROR: invalid/unsupported WAVE file\n");
        fclose(fin);
        return 1;
    }
    unsigned num_channels = ((unsigned)buffer[23] << 8) | buffer[22];;
    unsigned sample_rate = ((((((unsigned)buffer[27] << 8) | buffer[26]) << 8) | buffer[25]) << 8) | buffer[24];
    //unsigned byte_rate = ((((((unsigned)buffer[31] << 8) | buffer[30]) << 8) | buffer[29]) << 8) | buffer[28];
    //unsigned block_align = ((unsigned)buffer[33] << 8) | buffer[32];
    unsigned bps = ((unsigned)buffer[35] << 8) | buffer[34];
    
    //Apple puts the number of filler bytes in the 2 bytes following FLLR in the filler chunk
    //get the int value of the hex
    unsigned filler_byte_count = ((unsigned)buffer[41] << 8) | buffer[40];
    //swallow the filler bytes, exiting if there were not enough
    if(fread(buffer, 1, filler_byte_count, fin) != filler_byte_count) {
        fprintf(stderr, "ERROR: invalid number of filler bytes\n");
        return 1;
    }
    //swallow the beginning of the data chunk, i.e. the word 'data'
    unsigned data_subchunk_size = 0;
    if(fread(buffer, 1, 8, fin) != 8 || memcmp(buffer, "data", 4))  {
        fprintf(stderr, "ERROR: bad data start section\n");
        return 1;
    }
    else {
        //Subchunk2Size == NumSamples * NumChannels * BitsPerSample/8
        data_subchunk_size = ((((((unsigned)buffer[7] << 8) | buffer[6]) << 8) | buffer[5]) << 8) | buffer[4];
    }

    //create the flac encoder
    FLAC__StreamEncoder *encoder = FLAC__stream_encoder_new();
    FLAC__stream_encoder_set_verify(encoder, true);
    FLAC__stream_encoder_set_compression_level(encoder, 5);
    FLAC__stream_encoder_set_channels(encoder, num_channels);
    FLAC__stream_encoder_set_bits_per_sample(encoder, bps);
    FLAC__stream_encoder_set_sample_rate(encoder, sample_rate);
    //unknown total samples
    FLAC__stream_encoder_set_total_samples_estimate(encoder, 0);
    char* next_flac_file = malloc(sizeof(char) * 1024);
    sprintf(next_flac_file, "%s.flac", flac_file);
    fprintf(stderr, "writing to new flac file %s\n", next_flac_file);
    FLAC__stream_encoder_init_file(encoder, next_flac_file, progress_callback, NULL);
    
    long total_bytes_read = 0;
    int did_split_at_interval[1024];
    for(int i = 0; i < 1024; i++) {
        did_split_at_interval[i] = 0;
    }

    //read the wav file data chunk until we reach the end of the file.
    size_t bytes_read = 0;
    size_t need = (size_t)READSIZE;
    int flac_file_index = 0;
    while((bytes_read = fread(buffer, num_channels * (bps/8), need, fin)) != 0) {
        /* convert the packed little-endian 16-bit PCM samples from WAVE into an interleaved FLAC__int32 buffer for libFLAC */
        size_t i;
        for(i = 0; i < bytes_read*num_channels; i++) {
            /* inefficient but simple and works on big- or little-endian machines */
            pcm[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)buffer[2*i+1] << 8) | (FLAC__int16)buffer[2*i]);
        }
        /* feed samples to encoder */
        FLAC__stream_encoder_process_interleaved(encoder, pcm, bytes_read);
        total_bytes_read += bytes_read;
        
        if(split_interval_seconds > 0) {
            double elapsed_time_seconds = (total_bytes_read * 16) / (bps * sample_rate);
            int interval = elapsed_time_seconds / split_interval_seconds;
            if(interval > 0) {
                if(!did_split_at_interval[interval-1]) {
                    //finish encoding the current flac file
                    FLAC__stream_encoder_finish(encoder);
                    FLAC__stream_encoder_delete(encoder);
                    
                    //add the flac file to the out_flac_files output parameter
                    *(out_flac_files + flac_file_index) = next_flac_file;
                    flac_file_index += 1;
                    
                    //get a new flac file name
                    //free(next_flac_file);
                    next_flac_file = malloc(sizeof(char) * 1024);
                    sprintf(next_flac_file, "%s_%d.flac", flac_file, interval);
                    fprintf(stderr, "writing to new flac file %s\n", next_flac_file);
                    
                    //create a new encoder
                    encoder = FLAC__stream_encoder_new();
                    FLAC__stream_encoder_set_verify(encoder, true);
                    FLAC__stream_encoder_set_compression_level(encoder, 5);
                    FLAC__stream_encoder_set_channels(encoder, num_channels);
                    FLAC__stream_encoder_set_bits_per_sample(encoder, bps);
                    FLAC__stream_encoder_set_sample_rate(encoder, sample_rate);
                    FLAC__stream_encoder_set_total_samples_estimate(encoder, 0);
                    FLAC__stream_encoder_init_file(encoder, next_flac_file, progress_callback, NULL);
                    
                    //mark the interval as split
                    did_split_at_interval[interval-1] = 1;
                }
            }
        }
    }
    fprintf(stderr, "total bytes read: %ld\nbits per sample: %d\nsample rate: %d\n", total_bytes_read, bps, sample_rate);

    *(out_flac_files + flac_file_index) = next_flac_file;

    //cleanup
    FLAC__stream_encoder_finish(encoder);
    FLAC__stream_encoder_delete(encoder);
    fclose(fin);
    
    return 0;
}

void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data) {
    //fprintf(stderr, "wrote %llu bytes\n", bytes_written);
}
