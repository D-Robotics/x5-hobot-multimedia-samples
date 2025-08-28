/***************************************************************************
 * COPYRIGHT NOTICE
 * Copyright 2024 D-Robotics, Inc.
 * All rights reserved.
 ***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include <alsa/asoundlib.h>
#include <time.h>

// 定义一个Audio设备基础信息结构体
typedef struct {
    char capture_device[32];
    char playback_device[32];
    unsigned int rate;
    unsigned int bits;
    unsigned int channels;
    unsigned int duration;
    char record_file[64];
} AudioConfig;

// 初始化默认配置
AudioConfig config = {
    // .capture_device = "default", // 默认录音设备
    // .playback_device = "default", // 默认播放设备
    .capture_device = "hw:0,0",
    .playback_device = "hw:0,0",
    .rate = 48000,
    .bits = 16,
    .channels = 2,
    .duration = 5,
    .record_file = "record_test.wav"
};

// 枚举音频格式数组，可以用来检查平台是否支持
snd_pcm_format_t formats[] = {
    SND_PCM_FORMAT_S8,
    SND_PCM_FORMAT_U8,
    SND_PCM_FORMAT_S16_LE,
    SND_PCM_FORMAT_S16_BE,
    SND_PCM_FORMAT_U16_LE,
    SND_PCM_FORMAT_U16_BE,
    SND_PCM_FORMAT_S24_LE,
    SND_PCM_FORMAT_S24_BE,
    SND_PCM_FORMAT_U24_LE,
    SND_PCM_FORMAT_U24_BE,
    SND_PCM_FORMAT_S32_LE,
    SND_PCM_FORMAT_S32_BE,
    SND_PCM_FORMAT_U32_LE,
    SND_PCM_FORMAT_U32_BE,
    SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
    SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
    SND_PCM_FORMAT_MU_LAW,
    SND_PCM_FORMAT_A_LAW,
    SND_PCM_FORMAT_IMA_ADPCM,
    SND_PCM_FORMAT_MPEG,
    SND_PCM_FORMAT_GSM,
    SND_PCM_FORMAT_UNKNOWN  // 表示结束
};
/*
SND_PCM_FORMAT_S8,
SND_PCM_FORMAT_U8,
SND_PCM_FORMAT_S16_LE,
SND_PCM_FORMAT_S16_BE,
SND_PCM_FORMAT_U16_LE,
SND_PCM_FORMAT_U16_BE,
SND_PCM_FORMAT_S24_LE,
SND_PCM_FORMAT_S24_BE,
SND_PCM_FORMAT_U24_LE,
SND_PCM_FORMAT_U24_BE,
SND_PCM_FORMAT_S32_LE,
SND_PCM_FORMAT_S32_BE,
SND_PCM_FORMAT_U32_LE,
SND_PCM_FORMAT_U32_BE,
SND_PCM_FORMAT_F32_LE,
SND_PCM_FORMAT_F32_BE,
SND_PCM_FORMAT_F64_LE,
SND_PCM_FORMAT_F64_BE,
SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
SND_PCM_FORMAT_MU_LAW,
SND_PCM_FORMAT_A_LAW,
SND_PCM_FORMAT_IMA_ADPCM,
SND_PCM_FORMAT_MPEG,
SND_PCM_FORMAT_GSM,
*/


// 打印帮助信息
static void print_help() {
    printf("Usage: sample_audio [OPTIONS]\n");
    printf("Options:\n");
    printf("  -r <Sampling rate>            Specify sample rate for record or playback\n");
    printf("  -b <Bit depth>                Specify bit depth for record or playback\n");
    printf("  -c <Number of channels>       Specify channels for record or playback\n");
    printf("  -d <Duration>                 Specify duration for record or playback\n");
    printf("  -f <File name>                Specify file for record or playback\n");
    printf("  -h                            Show this help message\n");
}

// 打印命令提示信息
static void command_help() {
    printf("\n***************  Command Lists  ***************\n");
    printf(" q  -- Quit\n");
    printf(" r  -- Start recording\n");
    printf(" p  -- Playback\n");
    printf(" c  -- Check hardware support\n");
    printf(" h  -- Print help message\n");
}


// 定义一个 WAV 文件头结构，方便后续观察录制和回放效果
typedef struct {
    char riff[4];               // 'RIFF'
    unsigned int chunk_size;    // 文件大小
    char wave[4];               // 'WAVE'
    char fmt[4];                // 'fmt '
    unsigned int fmt_size;      // fmt 子块的大小
    unsigned short format_type; // 格式类型
    unsigned short channels;    // 声道数
    unsigned int sample_rate;   // 采样率
    unsigned int byte_rate;     // 每秒字节数 (sample_rate * channels * bits_per_sample / 8)
    unsigned short block_align; // 每块对齐字节数 (channels * bits_per_sample / 8)
    unsigned short bits_per_sample; // 每个样本的位数
    char data[4];               // `data`字段
    unsigned int data_size;     // `data_size`字段
} WavHeader;

// 填写WAV头部信息
void write_wav_header(FILE *file, const AudioConfig *config, unsigned int data_size) {
    WavHeader header;

    memcpy(header.riff, "RIFF", 4);
    header.chunk_size = 36 + data_size;  // 总大小 = header + data size
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    header.fmt_size = 16;
    header.format_type = 1; // PCM格式
    header.channels = config->channels;
    header.sample_rate = config->rate;
    header.byte_rate = config->rate * config->channels * (config->bits / 8);
    header.block_align = config->channels * (config->bits / 8);
    header.bits_per_sample = config->bits;
    memcpy(header.data, "data", 4);
    header.data_size = data_size;

    // 写入WAV头部
    fwrite(&header, sizeof(WavHeader), 1, file);
}

// 设置控制值函数（如设置增益）
int set_control_value(const char *control_name, long value) {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;

    if (snd_mixer_open(&handle, 0) < 0) {
        fprintf(stderr, "Unable to open mixer\n");
        return -1;
    }

    if (snd_mixer_attach(handle, "default") < 0) {
        fprintf(stderr, "Unable to attach mixer\n");
        snd_mixer_close(handle);
        return -1;
    }

    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_name(sid, control_name);
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
        fprintf(stderr, "Unable to find control '%s'\n", control_name);
        snd_mixer_close(handle);
        return -1;
    }

    snd_mixer_selem_set_playback_volume_all(elem, value);
    snd_mixer_close(handle);
    return 0;
}


// 音频格式映射函数：将位深转换为合适的 PCM 格式
snd_pcm_format_t map_format(int bits) {
    switch (bits) {
        case 8:
            return SND_PCM_FORMAT_S8;
        case 16:
            return SND_PCM_FORMAT_S16_LE;
        case 24:
            return SND_PCM_FORMAT_S24_LE;
        case 32:
            return SND_PCM_FORMAT_S32_LE;
        default:
            return SND_PCM_FORMAT_S16_LE;  // 默认16-bit格式
    }
}

// 检查硬件支持的音频格式
int check_supported_format(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *params, snd_pcm_format_t format) {
    int ret = snd_pcm_hw_params_test_format(pcm_handle, params, format);
    return (ret == 0) ? 1 : 0;  // 如果返回0，表示支持
}

// 返回采样位深度
snd_pcm_format_t set_supported_format(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *params, int bits) {
    snd_pcm_format_t format = map_format(bits);  // 将位深转换为实际的PCM格式
    if (check_supported_format(pcm_handle, params, format)) {
        return format;  // 如果支持直接返回
    } else {
        printf("Format with %d bits is not supported, setting to SND_PCM_FORMAT_S16_LE\n", bits);
        return SND_PCM_FORMAT_S16_LE;  // 如果不支持则设置为默认格式 SND_PCM_FORMAT_S16_LE
    }
}

// 检查硬件支持的采样率
int check_supported_rate(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *params, unsigned int rate) {
    int ret = snd_pcm_hw_params_test_rate(pcm_handle, params, rate, 0);
    return (ret == 0) ? 1 : 0;  // 如果返回0，表示支持
}

// 返回采样率
unsigned int set_supported_rate(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *params, unsigned int rate) {
    if (check_supported_rate(pcm_handle, params, rate)) {
        return rate;  // 如果支持直接返回
    } else {
        printf("Rate %d is not supported, setting to 44100\n", rate);
        return 44100;  // 如果不支持则设置为默认采样率 44100
    }
}


// 录音函数
void record_audio(const AudioConfig *config) {
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    int ret;
    char *buffer;
    float gain = 10;//文件操作设置数字增益，改动的是数据幅值，但有限制，后续可以根据实际情况调整或者删除

    const char *control_name = "ADC PGA Gain";//ADC PGA Gain,需要根据实际情况改写，可放文件开头做全局变量
    int value_adc_pga_gain = 8;//ADC PGA Gain value,需要根据实际情况改写，可放文件开头做全局变量
    unsigned int rate = config->rate;

    if (set_control_value(control_name, value_adc_pga_gain) != 0) {
        fprintf(stderr, "Failed to set control value\n");
        exit(1);
    }

    // 打开 PCM 设备
    ret = snd_pcm_open(&pcm_handle, config->capture_device, SND_PCM_STREAM_CAPTURE, 0);
    if (ret < 0) {
        fprintf(stderr, "Unable to open PCM device: %s\n", snd_strerror(ret));
        exit(1);
    }

    // 配置硬件参数
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);//一般都是用的左右交错的方式，这里暂时没必要做客制化

    // 确保硬件支持的采样位深
    snd_pcm_format_t format = set_supported_format(pcm_handle, params, config->bits);
    // 设置硬件参数
    snd_pcm_hw_params_set_format(pcm_handle, params, format);
    // 确保硬件支持的采样率
    rate = set_supported_rate(pcm_handle, params, rate);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);
    // 设置声道数
    snd_pcm_hw_params_set_channels(pcm_handle, params, config->channels);

    // 写入硬件参数到设备
    ret = snd_pcm_hw_params(pcm_handle, params);
    if (ret < 0) {
        fprintf(stderr, "Unable to set hardware parameters: %s\n", snd_strerror(ret));
        exit(1);
    }

    // 获取缓冲区大小
    snd_pcm_hw_params_get_period_size(params, &frames, 0);
    buffer = (char *) malloc(frames * 4);  // 2 声道，每个采样 2 字节

    // 打开文件用于保存录音数据
    FILE *file = fopen(config->record_file, "wb");
    if (!file) {
        fprintf(stderr, "Unable to open file %s for writing.\n", config->record_file);
        exit(1);
    }

    // 录音并保存到文件
    printf("Start recording...\n");
    unsigned int data_size = 0;  // 音频数据大小

    // 开始录音并保存到文件
    time_t start_time = time(NULL);
    time_t end_time = start_time + config->duration;  // 计算录音结束时间

    while (time(NULL) < end_time) {
        ret = snd_pcm_readi(pcm_handle, buffer, frames);
        if (ret == -EPIPE) {
            fprintf(stderr, "Buffer overflow error occurred!\n");
            snd_pcm_prepare(pcm_handle);
        } else if (ret < 0) {
            fprintf(stderr, "Recording read error: %s\n", snd_strerror(ret));
        // } else {
        //     printf("Get %d frames\n", ret);
        //     fwrite(buffer, 1, ret * 4, file);  // 将录音数据写入文件
        //     data_size += ret * 4;  // 增加数据大小
        // }
        } else {
            // 可以通过增加数字增益，这一段可以根据情况注释掉，使用上面未注释的。后续改进
            // printf("Get %d frames\n", ret);
            int16_t *samples = (int16_t *)buffer;  // 假设是 16 位采样
            size_t sample_count = ret * config->channels;
            for (size_t j = 0; j < sample_count; j++) {
                int sample = samples[j] * gain;
                if (sample > 32767) sample = 32767;  // 防止溢出
                if (sample < -32768) sample = -32768;
                samples[j] = (int16_t)sample;
            }
            fwrite(buffer, 1, ret * config->channels * (config->bits / 8), file);
        }
    }

    // 写入WAV头部
    fseek(file, 0, SEEK_SET);  // 回到文件开头写入头部
    write_wav_header(file, config, data_size);

    // 关闭 PCM 设备和文件
    snd_pcm_close(pcm_handle);
    fclose(file);
    free(buffer);

    printf("Recording finished, WAV file saved as:%s\n", config->record_file);
}

// 播放函数
void play_audio(const AudioConfig *config) {
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    FILE *file;
    char *buffer;
    int ret;
    unsigned int rate = config->rate;

    const char *control_name = "DAC";//也是codec控件
    int value_dac = 191;//DAC Vaule数值，可以通过amixer等命令查看详细范围，或者其他接口查看

    if (set_control_value(control_name, value_dac) != 0) {
        fprintf(stderr, "Failed to set control value\n");
        exit(1);
    }


    // 打开 PCM 设备
    ret = snd_pcm_open(&pcm_handle, config->playback_device, SND_PCM_STREAM_PLAYBACK, 0);
    if (ret < 0) {
        fprintf(stderr, "Unable to open PCM device:%s\n", snd_strerror(ret));
        exit(1);
    }

    // 配置硬件参数
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    // 确保硬件支持的采样位深
    snd_pcm_format_t format = set_supported_format(pcm_handle, params, config->bits);
    // 设置硬件参数
    snd_pcm_hw_params_set_format(pcm_handle, params, format);
    // 确保硬件支持的采样率
    rate = set_supported_rate(pcm_handle, params, rate);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);
    // 设置声道数
    snd_pcm_hw_params_set_channels(pcm_handle, params, config->channels);

    // 写入硬件参数到设备
    ret = snd_pcm_hw_params(pcm_handle, params);
    if (ret < 0) {
        fprintf(stderr, "Unable to set hardware parameters: %s\n", snd_strerror(ret));
        exit(1);
    }

    // 获取缓冲区大小
    snd_pcm_hw_params_get_period_size(params, &frames, 0);
    buffer = (char *) malloc(frames * config->channels * (config->bits / 8));  // 动态分配缓冲区大小

    // 打开文件
    file = fopen(config->record_file, "rb");
    if (!file) {
        fprintf(stderr, "Unable to open file: %s\n", config->record_file);
        exit(1);
    }

    // 跳过 WAV 文件头
    fseek(file, 44, SEEK_SET);  // WAV 文件头通常为 44 字节

    // 播放音频
    printf("playing...\n");
    size_t read_size;
    while ((read_size = fread(buffer, 1, frames * config->channels * (config->bits / 8), file)) > 0) {
        ret = snd_pcm_writei(pcm_handle, buffer, read_size / (config->channels * (config->bits / 8)));
        if (ret == -EPIPE) {
            fprintf(stderr, "Buffer Overflow Error Occurred!\n");
            snd_pcm_prepare(pcm_handle);
        } else if (ret < 0) {
            fprintf(stderr, "Error playing audio: %s\n", snd_strerror(ret));
        } else if (ret != (read_size / (config->channels * (config->bits / 8)))) {
            fprintf(stderr, "The number of frames written does not match the number of frames read.\n");
        }
    }

    // 关闭 PCM 设备和文件
    snd_pcm_close(pcm_handle);
    fclose(file);
    free(buffer);

    printf("play end.\n");
}

//打印格式化一下，提高阅读性
void print_supported_formats(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *params) {
    printf("Supported formats:\n");
    printf("%-30s%-50s%-15s\n", "Format", "Support", "Description");
    printf("----------------------------------------------------------------------------------------------\n");

    for (int i = 0; formats[i] != SND_PCM_FORMAT_UNKNOWN; i++) {
        const char *format_name = snd_pcm_format_name(formats[i]);
        const char *format_desc = snd_pcm_format_description(formats[i]);
        int ret = snd_pcm_hw_params_test_format(pcm_handle, params, formats[i]);

        printf("%-30s%-50s%-15s\n",
                format_name ? format_name : "Unknown",
                format_desc ? format_desc : "No description",
                ret == 0 ? "Supported" : "Not Supported");
    }
}

void print_supported_channels(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *params) {
    unsigned int min_channels, max_channels;
    int ret;

    // 获取支持的最小和最大声道数
    ret = snd_pcm_hw_params_get_channels_min(params, &min_channels);
    if (ret < 0) {
        fprintf(stderr, "Error getting minimum channels: %s\n", snd_strerror(ret));
        return;
    }

    ret = snd_pcm_hw_params_get_channels_max(params, &max_channels);
    if (ret < 0) {
        fprintf(stderr, "Error getting maximum channels: %s\n", snd_strerror(ret));
        return;
    }

    printf("\nChannels                      SupportNum\n");
    printf("--------------------------------------------------------\n");
    printf("Max                              %u\n", max_channels);
    printf("Min                              %u\n", min_channels);
}


void print_supported_sampling_rates(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *params) {
    unsigned int rates[] = {8000, 16000, 22050, 44100, 48000, 96000, 192000};
    size_t num_rates = sizeof(rates) / sizeof(rates[0]);

    printf("\nSampling Rate (Hz)            Support\n");
    printf("--------------------------------------------------------\n");
    for (size_t i = 0; i < num_rates; i++) {
        unsigned int rate = rates[i];
        int ret = snd_pcm_hw_params_test_rate(pcm_handle, params, rate, 0);
        printf("%-30u %-15s\n", rate, ret == 0 ? "Supported" : "Not Supported");
    }
}


//获取设备信息函数
void get_pcm_format(const AudioConfig *config) {
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    int ret;

    // 打开录音 PCM 设备
    printf("capture_device:\n");
    ret = snd_pcm_open(&pcm_handle, config->capture_device, SND_PCM_STREAM_CAPTURE, 0);
    if (ret < 0) {
        fprintf(stderr, "Unable to open PCM device: %s\n", snd_strerror(ret));
        exit(1);
    }

    // 分配硬件参数对象
    snd_pcm_hw_params_alloca(&params);

    // 初始化硬件参数对象
    ret = snd_pcm_hw_params_any(pcm_handle, params);
    if (ret < 0) {
        fprintf(stderr, "Error initializing hardware parameters: %s\n", snd_strerror(ret));
        snd_pcm_close(pcm_handle);
        exit(1);
    }

    // 打印支持的音频格式
    print_supported_formats(pcm_handle, params);

    // 打印支持的声道范围
    print_supported_channels(pcm_handle, params);

    // 打印支持的采样率
    print_supported_sampling_rates(pcm_handle, params);

    snd_pcm_close(pcm_handle);  // 关闭设备，释放资源

    // 打开播放 PCM 设备
    printf("playback_device:\n");
    ret = snd_pcm_open(&pcm_handle, config->playback_device, SND_PCM_STREAM_PLAYBACK, 0);
    if (ret < 0) {
        fprintf(stderr, "Unable to open PCM device: %s\n", snd_strerror(ret));
        exit(1);
    }

    // 分配硬件参数对象
    snd_pcm_hw_params_alloca(&params);

    // 初始化硬件参数对象
    ret = snd_pcm_hw_params_any(pcm_handle, params);
    if (ret < 0) {
        fprintf(stderr, "Error initializing hardware parameters: %s\n", snd_strerror(ret));
        snd_pcm_close(pcm_handle);
        exit(1);
    }

    // 打印支持的音频格式
    print_supported_formats(pcm_handle, params);

    // 打印支持的声道范围
    print_supported_channels(pcm_handle, params);

    // 打印支持的采样率
    print_supported_sampling_rates(pcm_handle, params);

    snd_pcm_close(pcm_handle);  // 关闭设备，释放资源

    command_help();

}

// 处理用户输入命令
static void handle_user_command(const AudioConfig *config) {
    char option;
    int running = 1;

    command_help();
    printf("\nCommand: ");
    while (running && ((option = getchar()) != EOF)) {
        switch (option) {
            case 'q':
                printf("Quit\n");
                running = 0;
                break;
            case 'r':
                printf("Recording...\n");
                record_audio(config);
                break;
            case 'p':
                printf("Playing...\n");
                play_audio(config);
                break;
            case 'c':
                //检查硬件信息，设置参数的时候可以查询，一定程度避免设置的时候没有参考
                get_pcm_format(config);
                break;
            case 'h':
                print_help();
                break;
            case '\n':
            case '\r':
                continue;
            default:
                printf("Command not supported!\n");
                command_help();
                break;
        }
        printf("\nCommand: ");
    }
}


int main(int argc, char **argv) {
    int opt;

    while ((opt = getopt(argc, argv, "r:b:c:d:f:h")) != -1) {
        switch (opt) {
            case 'r':
                config.rate = atoi(optarg);
                break;
            case 'b':
                config.bits = atoi(optarg);
                break;
            case 'c':
                config.channels = atoi(optarg);
                break;
            case 'd':
                config.duration = atoi(optarg);
                break;
            case 'f':
                strncpy(config.record_file, optarg, sizeof(config.record_file) - 1);
                break;
            case 'h':
            default:
                print_help();
                return 0;
        }
    }

    printf("Audio Recording and Playback Program\n");
    printf("Settings:\n");
    printf("  Capture Device        : %s\n", config.capture_device);
    printf("  Playback Device       : %s\n", config.playback_device);
    printf("  Sampling Rate         : %d Hz\n", config.rate);
    printf("  Bit Depth             : %d bit\n", config.bits);
    printf("  Channels              : %d\n", config.channels);
    printf("  Duration              : %d seconds\n", config.duration);
    printf("  File Name             : %s\n", config.record_file);

    handle_user_command(&config);
    return 0;
}