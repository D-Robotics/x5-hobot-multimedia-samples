#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <libavformat/avformat.h>

#define DEBUG_DUMP
// #define DEBUG_KBHIT
#define DEBUG_CLTRLC
// #define DEBUG_DUMP_FRAMES

static int g_cltrlc = 0;
struct termios orig_termios;

void reset_terminal_mode()
{
	tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
	struct termios new_termios;

	/* take two copies - one for now, one for later */
	tcgetattr(0, &orig_termios);
	memcpy(&new_termios, &orig_termios, sizeof(new_termios));

	/* register cleanup handler, and set the new terminal mode */
	atexit(reset_terminal_mode);
	cfmakeraw(&new_termios);
	tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
	int r;
	unsigned char c;
	if ((r = read(0, &c, sizeof(c))) < 0) {
		return r;
	} else {
		return c;
	}
}

void cltrlc_handler()
{
	printf("cltrl-c pressed!\n");
	g_cltrlc = 1;
}

void usage()
{
	printf("usage: ffdemux file_name\n");

	exit(1);
}

int main(int argc, char *argv[])
{
	AVFormatContext *fmt_ctx = NULL;
	AVPacket pkt;
	int r, video_stream;
#ifdef DEBUG_DUMP
	int dump_fd = 0;
#endif
#ifdef DEBUG_DUMP_FRAMES
	int frame_fd = 0;
	char frame_file[32];
	int idx = 0;
#endif
	char *file_name;

	if (argc != 2)
		usage();

	file_name = argv[1];

	/* open input file, and allocate format context */
	r = avformat_open_input(&fmt_ctx, file_name, NULL, NULL);
	if (r < 0) {
		fprintf(stderr, "avformat_open_input failed. r(%d)\n", r);
		return 1;
	}

	/* retrieve stream information */
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		return 1;
	}

	printf("probesize: %ld\n", fmt_ctx->probesize);

	/* dump input information to stderr */
	av_dump_format(fmt_ctx, 0, file_name, 0);

	/* select the video stream */
	r = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (r < 0) {
		av_log(NULL, AV_LOG_ERROR,
		       "Cannot find a video stream in the input file\n");
		return r;
	}

	video_stream = r;

	/* av_init_packet api is deprecated, use av_packet_unref instead to init packet */
	av_packet_unref(&pkt);

	/* prepare dump file */
#ifdef DEBUG_DUMP
	dump_fd = open("./dump.data", O_CREAT | O_RDWR, 0755);
#endif

#ifdef DEBUG_KBHIT
	set_conio_terminal_mode();
	printf("'q' for exit\n");
#endif

#ifdef DEBUG_CLTRLC
	printf("cltrl-c for exit\n");
	signal(SIGINT, cltrlc_handler);
#endif

	/* read frames from the file */
	while (av_read_frame(fmt_ctx, &pkt) >= 0) {
#ifdef DEBUG_KBHIT
		if (kbhit() && getch() == 'q') {
			break;
		}
#endif
		if (g_cltrlc)
			break;

		if (pkt.stream_index == video_stream) {
			printf
			    ("get video stream, id(%d), data: %p, size: %d.\n\n",
			     pkt.stream_index, pkt.data, pkt.size);
#ifdef DEBUG_DUMP
			if (dump_fd && pkt.size)
				write(dump_fd, pkt.data, pkt.size);
#endif
#ifdef DEBUG_DUMP_FRAMES
			snprintf(frame_file, 32, "%05d.frame", ++idx);
			frame_file[31] = '\0';
			frame_fd = open(frame_file, O_CREAT | O_RDWR, 0755);
			if (frame_fd)
				write(frame_fd, pkt.data, pkt.size);
			close(frame_fd);
#endif
		}
		// usleep(25*1000);
	}

#ifdef DEBUG_DUMP
	if (dump_fd)
		close(dump_fd);
#endif
	avformat_close_input(&fmt_ctx);

	printf("exit main loop\n");

	return 0;
}
