/**
 *
 *  Copyright 2016-2017 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */

#ifdef FFMPEG_IMPORT
#include <wchar.h>
#include <locale.h>

#define MAX_PATH 4096
#endif

#include <cstdio>
#include <stdlib.h>
#include <exception>
#include <string>
#include <algorithm>
#include <cstring>
#include "libvmaf.h"

extern "C" {
#include "common/frame.h"
}

#define read_image_b       read_image_b2s
#define read_image_w       read_image_w2s

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

void print_usage(int argc, char *argv[])
{
#ifdef FFMPEG_IMPORT
    fprintf(stderr, "Usage: %s fmt width height ref_path dis_path model_path [--log log_path] [--log-fmt log_fmt] [--disable-clip] [--disable-avx] [--psnr] [--ssim] [--ms-ssim] [--ref-is-raw] [--dis-is-raw] [--ref-start-offset-seconds XXX] [--dis-start-offset-seconds XXX] [--frames XXX] [--ref-deinterlace] [--dis-deinterlace] [--phone-model]\n", argv[0]);
#else
    fprintf(stderr, "Usage: %s fmt width height ref_path dis_path model_path [--log log_path] [--log-fmt log_fmt] [--disable-clip] [--disable-avx] [--psnr] [--ssim] [--ms-ssim] [--phone-model]\n", argv[0]);
#endif
    fprintf(stderr, "fmt:\n\tyuv420p\n\tyuv422p\n\tyuv444p\n\tyuv420p10le\n\tyuv422p10le\n\tyuv444p10le\n\n");
    fprintf(stderr, "log_fmt:\n\txml (default)\n\tjson\n\n");
}

#ifdef FFMPEG_IMPORT
int run_wrapper(char *fmt, int width, int height, char *ref_path, char *ref_start_offset_seconds, bool ref_is_raw, bool ref_deinterlace, char *dis_path, char *dis_start_offset_seconds, bool dis_is_raw, bool dis_deinterlace, char *max_frames, char *model_path,
        char *log_path, char *log_fmt, bool disable_clip, bool disable_avx, bool enable_transform, bool phone_model,
        bool do_psnr, bool do_ssim, bool do_ms_ssim, char *pool_method)
#else
int run_wrapper(char *fmt, int width, int height, char *ref_path, char *dis_path, char *model_path,
        char *log_path, char *log_fmt, bool disable_clip, bool disable_avx, bool enable_transform, bool phone_model,
        bool do_psnr, bool do_ssim, bool do_ms_ssim, char *pool_method)
#endif
{
    double score;

    int ret = 0;
    struct data *s;
    s = (struct data *)malloc(sizeof(struct data));
    s->format = fmt;
    s->width = width;
    s->height = height;

    if (!strcmp(fmt, "yuv420p") || !strcmp(fmt, "yuv420p10le"))
    {
        if ((width * height) % 2 != 0)
        {
            fprintf(stderr, "(width * height) %% 2 != 0, width = %d, height = %d.\n", width, height);
            ret = 1;
            goto fail_or_end;
        }
        s->offset = width * height / 2;
    }
    else if (!strcmp(fmt, "yuv422p") || !strcmp(fmt, "yuv422p10le"))
    {
        s->offset = width * height;
    }
    else if (!strcmp(fmt, "yuv444p") || !strcmp(fmt, "yuv444p10le"))
    {
        s->offset = width * height * 2;
    }
    else
    {
        fprintf(stderr, "unknown format %s.\n", fmt);
        ret = 1;
        goto fail_or_end;
    }

#ifdef FFMPEG_IMPORT
    wchar_t unicode_ffmpeg_command[MAX_PATH];
    wchar_t unicode_filename[MAX_PATH];
    char converted_unicode_command[MAX_PATH];

    if (ref_is_raw == false)
    {
        // build ffmpeg command using UTF8 character set
        mbstowcs(unicode_filename, ref_path, MAX_PATH);
        swprintf(unicode_ffmpeg_command, MAX_PATH, L"ffmpeg -i \"%ls\" -s %dx%d -f image2pipe -vcodec rawvideo -pix_fmt yuv420p", unicode_filename, width, height);
        wcstombs(converted_unicode_command, unicode_ffmpeg_command, MAX_PATH);

        // offset in seconds from start of video
        if (ref_start_offset_seconds != NULL)
        {
            strcat(converted_unicode_command, " -ss ");
            strcat(converted_unicode_command, ref_start_offset_seconds);
        }

        // if a maximum number of frames was specified then add the parameter to ffmpeg
        if (max_frames != NULL)
        {
            strcat(converted_unicode_command, " -vframes ");
            strcat(converted_unicode_command, max_frames);
        }

        // shall we deinterlace the reference video?
        if (ref_deinterlace == true)
        {
            strcat(converted_unicode_command, " -vf yadif ");
        }

        // append the dash for ffmpeg to write to output pipe
        strcat(converted_unicode_command, " -");

        // open reference file using ffmpeg
        if (!(s->ref_rfile = popen(converted_unicode_command, "r")))
        {
            fprintf(stderr, "fopen ref_path %s failed.\n", ref_path);
            ret = 1;
            goto fail_or_end;
        }
    }
    else
#endif
    if (!(s->ref_rfile = fopen(ref_path, "rb")))
    {
        fprintf(stderr, "fopen ref_path %s failed.\n", ref_path);
        ret = 1;
        goto fail_or_end;
    }

#ifdef FFMPEG_IMPORT
    if (dis_is_raw == false)
    {
        // build ffmpeg command using UTF8 character set
        mbstowcs(unicode_filename, dis_path, MAX_PATH);
        swprintf(unicode_ffmpeg_command, MAX_PATH, L"ffmpeg -i \"%ls\" -s %dx%d -f image2pipe -vcodec rawvideo -pix_fmt yuv420p", unicode_filename, width, height);
        wcstombs(converted_unicode_command, unicode_ffmpeg_command, MAX_PATH);

        // offset in seconds from start of video
        if (dis_start_offset_seconds != NULL)
        {
            strcat(converted_unicode_command, " -ss ");
            strcat(converted_unicode_command, dis_start_offset_seconds);
        }

        // if a maximum number of frames was specified then add the parameter to ffmpeg
        if (max_frames != NULL)
        {
            strcat(converted_unicode_command, " -vframes ");
            strcat(converted_unicode_command, max_frames);
        }

        // shall we deinterlace the encoded video?
        if (dis_deinterlace == true)
        {
            strcat(converted_unicode_command, " -vf yadif ");
        }

        // append the dash for ffmpeg to write to output pipe
        strcat(converted_unicode_command, " -");

        // open distorted file using ffmpeg
        if (!(s->dis_rfile = popen(converted_unicode_command, "r")))
        {
            fprintf(stderr, "fopen ref_path %s failed.\n", dis_path);
            ret = 1;
            goto fail_or_end;
        }
    }
    else
#endif
    if (!(s->dis_rfile = fopen(dis_path, "rb")))
    {
        fprintf(stderr, "fopen ref_path %s failed.\n", dis_path);
        ret = 1;
        goto fail_or_end;
    }

    /* Run VMAF */
    ret = compute_vmaf(&score, fmt, width, height, read_frame, s, model_path, log_path, log_fmt, disable_clip, disable_avx, enable_transform, phone_model, do_psnr, do_ssim, do_ms_ssim, pool_method);

fail_or_end:
    if (s->ref_rfile)
    {
        fclose(s->ref_rfile);
    }
    if (s->dis_rfile)
    {
        fclose(s->dis_rfile);
    }
    if (s)
    {
        free(s);
    }
    return ret;
}

int main(int argc, char *argv[])
{
    char* fmt;
    int width;
    int height;
    char* ref_path;
    char* dis_path;
    char *model_path;
    char *log_path = NULL;
    char *log_fmt = NULL;
    bool disable_clip = false;
    bool disable_avx = false;
    bool enable_transform = false;
    bool phone_model = false;
    bool do_psnr = false;
    bool do_ssim = false;
    bool do_ms_ssim = false;
    char *pool_method = NULL;
#ifdef FFMPEG_IMPORT
    bool ref_is_raw = false;
    bool dis_is_raw = false;
    bool ref_deinterlace = false;
    bool dis_deintrlace = false;
    char *max_frames = NULL;
    char *ref_start_offset_seconds = NULL;
    char *dis_start_offset_seconds = NULL;
#endif

    /* Check parameters */

    if (argc < 7)
    {
        print_usage(argc, argv);
        return -1;
    }

#ifdef FFMPEG_IMPORT
    std::setlocale(LC_ALL, "en_US.UTF-8");
#endif

    fmt = argv[1];
    width = std::stoi(argv[2]);
    height = std::stoi(argv[3]);
    ref_path = argv[4];
    dis_path = argv[5];
    model_path = argv[6];

    if (width <= 0 || height <= 0)
    {
        fprintf(stderr, "Error: Invalid frame resolution %d x %d\n", width, height);
        print_usage(argc, argv);
        return -1;
    }

    log_path = getCmdOption(argv + 7, argv + argc, "--log");

    log_fmt = getCmdOption(argv + 7, argv + argc, "--log-fmt");
    if (log_fmt != NULL && !(strcmp(log_fmt, "xml")==0 || strcmp(log_fmt, "json")==0))
    {
        fprintf(stderr, "Error: log_fmt must be xml or json, but is %s\n", log_fmt);
        return -1;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--disable-clip"))
    {
        disable_clip = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--disable-avx"))
    {
        disable_avx = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--enable-transform"))
    {
        enable_transform = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--phone-model"))
    {
        phone_model = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--psnr"))
    {
        do_psnr = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--ssim"))
    {
        do_ssim = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--ms-ssim"))
    {
        do_ms_ssim = true;
    }

#ifdef FFMPEG_IMPORT
    if (cmdOptionExists(argv + 7, argv + argc, "--ref-is-raw"))
    {
        ref_is_raw = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--dis-is-raw"))
    {
        dis_is_raw = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--ref-deinterlace"))
    {
        ref_deinterlace = true;
    }

    if (cmdOptionExists(argv + 7, argv + argc, "--dis-deinterlace"))
    {
        dis_deintrlace = true;
    }

    max_frames = getCmdOption(argv + 7, argv + argc, "--frames");

    ref_start_offset_seconds = getCmdOption(argv + 7, argv + argc, "--ref-start-offset-seconds");
    dis_start_offset_seconds = getCmdOption(argv + 7, argv + argc, "--dis-start-offset-seconds");
#endif

    pool_method = getCmdOption(argv + 7, argv + argc, "--pool");
    if (pool_method != NULL && !(strcmp(pool_method, "min")==0 || strcmp(pool_method, "harmonic_mean")==0 || strcmp(pool_method, "mean")==0))
    {
        fprintf(stderr, "Error: pool_method must be min, harmonic_mean or mean, but is %s\n", pool_method);
        return -1;
    }
        
    try
    {
#ifdef FFMPEG_IMPORT
        return run_wrapper(fmt, width, height, ref_path, ref_start_offset_seconds, ref_is_raw, ref_deinterlace, dis_path, dis_start_offset_seconds, dis_is_raw, dis_deintrlace, max_frames, model_path,
                log_path, log_fmt, disable_clip, disable_avx, enable_transform, phone_model,
                do_psnr, do_ssim, do_ms_ssim, pool_method);
#else
        return run_wrapper(fmt, width, height, ref_path, dis_path, model_path,
                log_path, log_fmt, disable_clip, disable_avx, enable_transform, phone_model,
                do_psnr, do_ssim, do_ms_ssim, pool_method);
#endif
    }
    catch (const std::exception &e)
    {
        fprintf(stderr, "Error: %s\n", e.what());
        print_usage(argc, argv);
        return -1;
    }
    
}
