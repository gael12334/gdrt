/*
 * Copyright 2026 (c) Gaël Fortier. All right reserved. gael.fortier.1@ens.etsmtl.ca
 */

/* ----------
 * References
 * ----------
 * File markers: https://filesig.search.org/
 */

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*****************************************************************
 * Utility structs
 *****************************************************************/

typedef struct data {
	char* ptr;
	long size;	
} DATA;

typedef struct stream {
	FILE* stream;
	long size;
} STREAM;

typedef enum arg_type {
	ARG_TEXT,
	ARG_INTEGER,
	ARG_RAW_DATA,
	ARG_IFS, // input file stream
	ARG_OFS, // output file stream
	ARG_IBFS, // input binary file stream
	ARG_OBFS, // output binary file stream
} ARG_TYPE;

typedef unsigned long ULONG;
typedef char PATH[256];

/*****************************************************************
 * Utility - Other
 *****************************************************************/

#define declstr(name, literal) char name[sizeof(literal)]; strcpy(name, literal);

/*****************************************************************
 * Utility - Argument parsers
 *****************************************************************/

typedef int(*ARG_PARSER)(char* argument, void* value);
#define ARG_PARSER_PARAM char* argument, void* value
#define ELIF else if

int parse_text(ARG_PARSER_PARAM) {
	*((char**)value) = argument;
	return 0;
}

int parse_integer(ARG_PARSER_PARAM) {
	int count = sscanf(argument, "%li", (long*)value);
	if(count == 1)
		return 0;
	count = sscanf(argument, "%lx", (ULONG*)value);
	if(count == 1)
		return 0;
	return 1;
}

int parse_rawdata(ARG_PARSER_PARAM) {
	static const char table[132] = 
		"................................"
		"................\x0\x1\x2\x3\x4\x5\x6\x7\x8\x9......"
		".\xA\xB\xC\xD\xE\xF............."
		".\xA\xB\xC\xD\xE\xF.............";
		
	char* it;
	for(it = argument; *it != '\0'; it++) {
		*it = table[(int)*it];
		if(*it > 0xF)
			return 1;
	}

	size_t len = it - argument;
	for(size_t i = len; i > (len & 1); i -= 2) {
		it[-1] = argument[i - 1];
		it[-1] += argument[i - 2] << 4;
		it--;
	}

	if(len & 1) {
		it[-1] = argument[0];
		it--;
	}

	DATA* data = value;
	data->ptr = argument;
	data->size = (len + (len & 1)) >> 1;

	for(long i = 0; i < data->size; i++) {
		data->ptr[i] = it[i];
		it[i] = '\0';
	}
	
	return 0;
}

int parse_ofs(ARG_PARSER_PARAM) {
	STREAM* stream = value;
	stream->stream = fopen(argument, "w");
	stream->size = 0;
	return (stream == NULL);
}

int parse_ifs(ARG_PARSER_PARAM) {
	STREAM* stream = value;
	memset(stream, 0, sizeof(*stream));
	stream->stream = fopen(argument, "r");
	if(stream->stream != NULL) {
		fseek(stream->stream, 0, SEEK_END);
		stream->size = ftell(stream->stream);
		rewind(stream->stream);
	}
	return (stream == NULL);
}

int parse_obfs(ARG_PARSER_PARAM) {
	STREAM* stream = value;
	stream->stream = fopen(argument, "wb");
	stream->size = 0;
	return (stream == NULL);
}

int parse_ibfs(ARG_PARSER_PARAM) {
	STREAM* stream = value;
	memset(stream, 0, sizeof(*stream));
	stream->stream = fopen(argument, "rb");
	if(stream->stream != NULL) {
		fseek(stream->stream, 0, SEEK_END);
		stream->size = ftell(stream->stream);
		rewind(stream->stream);
	}
	return (stream == NULL);
}

int parse_argument(char* argument, ARG_TYPE type, void* value) {
	ARG_PARSER parser[] = {
		parse_text,
		parse_integer,
		parse_rawdata,
		parse_ifs,
		parse_ofs,
		parse_ibfs,
		parse_obfs	
	};

	return parser[type](argument, value);
}

/*****************************************************************
 * Program - finder 
 *****************************************************************/
typedef struct program {
	PATH srcpath;
	STREAM input;
	STREAM output;
	long offset;
	long length;
	DATA marker; 
	DATA trailer;
} PROGRAM;

typedef struct entry {
	int64_t offset;
	int64_t size;	
} ENTRY;

enum program_error {
	PROGRAM_SUCCESS,
	PROGRAM_INVALID_SOURCE,
	PROGRAM_INVALID_OUTPUT,
	PROGRAM_INVALID_OFFSET,
	PROGRAM_INVALID_LENGTH,
	PROGRAM_INVALID_MARKER,
	PROGRAM_INVALID_ENDING,
	PROGRAM_INVALID_ARGC,
	PROGRAM_FREAD_SOURCE,
	PROGRAM_FWRITE_OUTPUT,
	PROGRAM_OUT_OF_RANGE,
	PROGRAM_ERROR_NUMBER
};

int program_logerr_finder(const char* func, int err) {
	const char* errmsg[PROGRAM_ERROR_NUMBER] = {
		"success",
		"invalid source path",
		"invalid output path",
		"offset has an invalid format",
		"length has an invalid format",
		"marker has an invalid format",
		"ending has an invalid format",
		"number of arguments provided is incorrect",
		"offset and size points outside source file",
		"an error occured when writing to output",
		"offset + length points outside region",
	};

	printf("[%s]: error # %i: '%s'\n", func, err, errmsg[err]);
	return err;
}

int program_init_finder(PROGRAM* p, int argc, char** argv) {
	if(argc != 8) 
		return program_logerr_finder(__func__, PROGRAM_INVALID_ARGC);
	if(parse_argument(argv[2], ARG_IBFS, &p->input)) 
		return program_logerr_finder(__func__, PROGRAM_INVALID_SOURCE);
	if(parse_argument(argv[3], ARG_OFS, &p->output))
		return program_logerr_finder(__func__, PROGRAM_INVALID_OUTPUT);
	if(parse_argument(argv[4], ARG_RAW_DATA, &p->marker))
		return program_logerr_finder(__func__, PROGRAM_INVALID_MARKER);
	if(parse_argument(argv[5], ARG_RAW_DATA, &p->trailer))
		return program_logerr_finder(__func__, PROGRAM_INVALID_ENDING);
	if(parse_argument(argv[6], ARG_INTEGER, &p->offset))
		return program_logerr_finder(__func__, PROGRAM_INVALID_OFFSET);
	if(parse_argument(argv[7], ARG_INTEGER, &p->length))
		return program_logerr_finder(__func__, PROGRAM_INVALID_LENGTH);		

	if(p->offset + p->length > p->input.size)
		return program_logerr_finder(__func__, 	PROGRAM_OUT_OF_RANGE);

	p->input.size = p->length;
	strncpy(p->srcpath, argv[2], sizeof(p->srcpath));
	if(fseek(p->input.stream, p->offset, SEEK_CUR)) 
		return program_logerr_finder(__func__, 	PROGRAM_INVALID_OFFSET);
	
	return PROGRAM_SUCCESS;
}

long search_stream_sequence(int fd, long* position, long limit, const char* sequence, long sequence_length) {
	static char cache[0x100000];
	long initial_position = *position;
	long limit_position = initial_position + limit;
	long matching_length = 0;

	while(*position < limit_position) {
		long remaining = limit_position - *position;
		long read = pread(fd, cache, sizeof(cache), *position);
		long length = (read > remaining) ? remaining : read;

		for(long i = 0; i < length; i++) {
			if(cache[i] != sequence[matching_length]) {
				if(matching_length > 0) {
					i--;
				}
				matching_length = 0;
				continue;
			}

			matching_length++;
			//printf("%hhx ", cache[i]);
			if(matching_length == sequence_length) {
				long offset = *position + i - sequence_length + 1;
				*position += length;
				return offset;
			}
		}
		//puts(".");

		*position += length;
		if(*position >= limit_position) {
			break;
		}
		*position -= sequence_length;
	}

	return -1;
}

int program_run_finder(PROGRAM* p) {
	long finder_result = 0;
	long count = 0;

	fwrite(p->srcpath, 1, sizeof(p->srcpath), p->output.stream);
	fflush(p->output.stream);

	puts("=============================");
	printf("source: %s\n", p->srcpath);
	printf("size / limit: %li\n", p->input.size);
	printf("marker size: %li\n", p->marker.size);
	printf("trailer size: %li\n", p->trailer.size);
	puts("=============================");

	long position = p->offset;
	long limit = position + p->input.size;
	while(position < limit) {
		long start = search_stream_sequence(fileno(p->input.stream), &position, p->input.size, p->marker.ptr, p->marker.size);
		if (start == -1) {
			continue;
		}

		printf("[%s]: #%li\n", __func__, count);
		printf("[%s]: marker @ 0x%lx (%li)\n", __func__, (uint64_t)start, start);
		printf("[%s]: position=%li\n", __func__, position);

		long end = search_stream_sequence(fileno(p->input.stream), &position, p->input.size, p->trailer.ptr, p->trailer.size);
		if(end == -1) {
			continue;
		}

		if(end - start < 0) {
			puts("end < start");
			break;
		}

		printf("[%s]: trailer @ 0x%lx (%li)\n", __func__, (uint64_t)end, end);
		printf("[%s]: size = 0x%lx (%li)\n\n", __func__, (uint64_t)(end - start), end - start);

		ENTRY e = {.offset = start, .size = end - start};
		fwrite(&e, 1, sizeof(e), p->output.stream);
		fflush(p->output.stream);
		count++;
	}

	printf("[%s]: count %li\n", __func__, count);
	return count;
}

void program_deinit_finder(PROGRAM* p) {
	if(p->input.stream)
		fclose(p->input.stream);
	if(p->output.stream)
		fclose(p->output.stream);
	memset(p, 0, sizeof(*p));
}

/*****************************************************************
 * Program - extractor
 *****************************************************************/

typedef struct extractor {
	long index;
	STREAM report;
	STREAM output;
	STREAM source;
	ENTRY entry;
	PATH srcpath;
} EXTRACTOR;

enum extractor_error {
	EXTRACTOR_SUCCESS,
	EXTRACTOR_INVALID_ARGC,
	EXTRACTOR_INVALID_INDEX,
	EXTRACTOR_INVALID_INPUT,
	EXTRACTOR_INVALID_OUTPUT,
	EXTRACTOR_FSEEK_INPUT_FAILED,
	EXTRACTOR_FREAD_INPUT_FAILED,
	EXTRACTOR_FSEEK_SRC_FAILED,
	EXTRACTOR_FOPEN_SRC_FAILED,
	EXTRACTOR_FREAD_SRC_FAILED,
	EXTRACTOR_FWRITE_OUTPUT_FAILED,
};

int program_logerr_extractor(const char* func, int err) {
	const char* errmsg[] = {
		"success",
		"number of arguments provided is incorrect",
		"failed to parse provided index",
		"failed to open input file",
		"failed to open output file",
		"index points outside input file",
		"number of bytes read was unexpected",
		"offset and size points outside source file",
		"failed to open source file",
		"failed to read source file",
		"number of bytes written was unexpected"
	};

	printf("[%s]: error # %i: '%s'\n", func, err, errmsg[err]);
	return err;
}

int program_init_extractor(EXTRACTOR* e, int argc, char** argv) {
	if(argc != 5) 
		return program_logerr_extractor(__func__, EXTRACTOR_INVALID_ARGC);
	if(parse_argument(argv[2], ARG_IFS, &e->report))
		return program_logerr_extractor(__func__, EXTRACTOR_INVALID_INPUT);
	if(parse_argument(argv[3], ARG_OBFS, &e->output)) 
		return program_logerr_extractor(__func__, EXTRACTOR_INVALID_OUTPUT);
	if(parse_argument(argv[4], ARG_INTEGER, &e->index))
		return program_logerr_extractor(__func__, EXTRACTOR_INVALID_INDEX);

	long read = fread(e->srcpath, 1, sizeof(e->srcpath), e->report.stream);
	if(read != sizeof(e->srcpath))
		return program_logerr_extractor(__func__, EXTRACTOR_FREAD_INPUT_FAILED);
	if(parse_argument(e->srcpath, ARG_IBFS, &e->source))
		return program_logerr_extractor(__func__, EXTRACTOR_FOPEN_SRC_FAILED);

	long offset = e->index * sizeof(ENTRY) + sizeof(PATH);
	if(fseek(e->report.stream, offset, SEEK_SET))
		return program_logerr_extractor(__func__, EXTRACTOR_FSEEK_INPUT_FAILED);
	read = fread(&e->entry, 1, sizeof(e->entry), e->report.stream);
	if(read != sizeof(e->entry))
		return program_logerr_extractor(__func__, EXTRACTOR_FREAD_INPUT_FAILED);

	printf("[%s]: %li %li\n", __func__, e->entry.offset, e->entry.size);	
	return EXTRACTOR_SUCCESS;
}

int program_run_extractor(EXTRACTOR* e) {
	static char large_buffer[0x100000]; // 1 MiB buffer

	if(fseek(e->source.stream, e->entry.offset, SEEK_SET))
		return program_logerr_extractor(__func__, EXTRACTOR_FSEEK_SRC_FAILED);

	long read = 0;
	long wrote = 0;
	long left = e->entry.size;
	long size = 0;
	
	while(left > 0) {
		size = (left < (long)sizeof(large_buffer)) ? left : (long)sizeof(large_buffer);
		read = fread(large_buffer, 1, size, e->source.stream);
		if(read != size) 
			return program_logerr_extractor(__func__, EXTRACTOR_FREAD_SRC_FAILED);

		putc('.', stdout);
		wrote = fwrite(large_buffer, 1, read, e->output.stream);
		if(wrote != read)
			return program_logerr_extractor(__func__, EXTRACTOR_FWRITE_OUTPUT_FAILED);
			
		fflush(e->output.stream);
		left -= size;
	}
	
	return EXTRACTOR_SUCCESS;
}

void program_deinit_extractor(EXTRACTOR* e) {
	if(e->report.stream) fclose(e->report.stream);
	if(e->output.stream) fclose(e->output.stream);
	if(e->source.stream) fclose(e->source.stream);
	memset(e, 0, sizeof(*e));
}

int program_extractor(int argc, char** argv) {
	EXTRACTOR e;
	memset(&e, 0, sizeof(e));

	if(program_init_extractor(&e, argc, argv) == EXTRACTOR_SUCCESS)
		program_run_extractor(&e);
		
	program_deinit_extractor(&e);
	return 0;
}

/*****************************************************************
 * Program - entrypoint (main)
 *****************************************************************/

int help(void) {
	puts(
		"options:\n\t"
		"-f: finder\n\t"
		"-ij: finder (jpeg)\n\t"
		"-ip: finder (png)\n\t"
		"-vm: finder (mp3)\n\t"
		"-pd: finder (pdf)\n\t"
		"-e: extractor\n\t"
		"-h: help"
	);
	puts("extractor:\n\t-e <report> <output> <index>");
	puts("finder:\n\t-f <input> <output> <marker> <trailer> <offset> <length>");
	puts("finder (-i*, -v*, -p*):\n\t(-i* | -v* | -p*) <input> <output> <offset> <length>");
	return -1;
}

int finder(int argc, char** argv) {
	PROGRAM p = {0};
	int error = program_init_finder(&p, argc, argv);
	if(!error) {
		error = program_run_finder(&p);
	}
	program_deinit_finder(&p);
	return error;
}

int finder_shortcut(int argc, char** argv, char* marker, char* trailer) {
	if(argc != 6) {
		return help();
	}

	char* arguments[] = {argv[0], argv[1], argv[2], argv[3], marker, trailer, argv[4], argv[5]};
	return finder(8, arguments);
}

int main(int argc, char** argv) {
	int error = 0;

	if(argc < 2) {
		return help();
	}

	if(argv[1][0] == '-') {
		switch(argv[1][1]) {
			case 'f': {
				return finder(argc, argv);
			}

			case 'i': {
				switch(argv[1][2]) {
					case 'j': { // JPEG
						declstr(marker, "FFD8FF");
						declstr(trailer, "FFD9");
						return finder_shortcut(argc, argv, marker, trailer);
					}
					case 'p': { // PNG
						declstr(marker, "89504E470D0A1A0A");
						declstr(trailer, "49454E44AE426082");
						return finder_shortcut(argc, argv, marker, trailer);
					}
					default: {
						return help();
					}
				}
			}

			case 'v': {
				switch(argv[1][2]) {
					case 'm': { // MPEG
						declstr(marker, "000001B3");
						declstr(trailer, "000001B7");
						return finder_shortcut(argc, argv, marker, trailer);
					}
					default: {
						return help();
					}
				}
			}

			case 'd': {
				switch(argv[1][2]) {
					case 'p': { // PDF
						declstr(marker, "25504446");
						declstr(trailer, "2525454F46");
						return finder_shortcut(argc, argv, marker, trailer);
					}
					default: {
						return help();
					}
				}
			}

			case 'e': {
				return program_extractor(argc, argv);
			}
		}
	}

	return help();
}
