#include <stdlib.h>
#include <string.h>

#define NOT_NULL_FUNC(_x, _y) \
	if((_x) != NULL) { \
		_y(_x); \
		_x = NULL; \
	}

#define NOT_NULL_FREE(_x) \
	NOT_NULL_FUNC(_x, free)

#define NULL_RET(_x) \
	if((_x) == NULL) { \
		return; \
	}

#define NULL_RET_NULL(_x) \
	if((_x) == NULL) { \
		return NULL; \
	}

typedef struct {
	float x;
	float y;
} vec_t;

typedef struct {
	vec_t pos;
	vec_t size;
} select_t;

typedef struct {
	unsigned int *data;
	vec_t size;
} buffer_t;

struct frame_struct;
typedef struct {
	void *ptr;
	int size;
} params_t;

params_t *params_make(void *ptr, int size) {
	params_t *params = malloc(sizeof(params_t));
	NULL_RET_NULL(params);
	params->ptr = malloc(size);
	if(params->ptr == NULL) {
		free(params);
		return NULL;
	}
	
	memcpy(params->ptr, ptr, size);
	params->size = size;
	return params;
}

void params_free(params_t **params) {
	NULL_RET(params);
	NULL_RET(*params);
	NOT_NULL_FREE((*params)->ptr);
	NOT_NULL_FREE(*params);
}

typedef void (*callback_t)(struct frame_struct *, params_t *params);
typedef struct frame_struct {
	select_t select; // select is only used if is_sub is true.
	buffer_t *buffer;
	int is_sub;
	callback_t *callbacks;
	params_t **params;
	int callbacks_size;
} frame_t;

buffer_t *buffer_make(vec_t size) {
	buffer_t *buffer = malloc(sizeof(buffer_t));
	NULL_RET_NULL(buffer);
	buffer->data = malloc(sizeof(int) * (int)(size.x) * (int)(size.y));
	if(buffer->data == NULL) {
		free(buffer);
		return NULL;
	}
	
	buffer->size = size;
	return buffer;
}

void buffer_free(buffer_t **buffer) {
	NULL_RET(buffer);
	NULL_RET(*buffer);
	NOT_NULL_FREE((*buffer)->data);
	NOT_NULL_FREE(*buffer);
}

frame_t *frame_make(buffer_t *buffer) {
	NULL_RET_NULL(buffer);
	frame_t *frame = malloc(sizeof(frame_t));
	NULL_RET_NULL(frame);
	frame->select = (select_t) { (vec_t) { 0, 0 }, buffer->size };
	frame->buffer = buffer;
	frame->is_sub = 0;
	frame->callbacks = NULL;
	frame->params = NULL;
	frame->callbacks_size = 0;
	return frame;
}

void frame_free(frame_t **frame) {
	NULL_RET(frame);
	NULL_RET(*frame);
	for(int i = 0; i < (*frame)->callbacks_size && (*frame)->params; i++) {
		if((*frame)->params[i] == NULL) {
			continue;
		}

		params_free(&((*frame)->params[i]));
	}

	NOT_NULL_FREE((*frame)->params);
	NOT_NULL_FREE((*frame)->callbacks);
	NOT_NULL_FREE(*frame);
}

void *safe_realloc(void *ptr, int size) {
	void *tmp = realloc(ptr, size);
	if(tmp == NULL) {
		free(ptr);
		return NULL;
	}

	return tmp;
}

void frame_push_callback(frame_t *frame, callback_t callback, params_t *params) {
	NULL_RET(frame);
	frame->callbacks_size++;
	frame->callbacks = safe_realloc(frame->callbacks, sizeof(callback_t) * frame->callbacks_size);
	if(frame->callbacks == NULL) {
		NOT_NULL_FREE(frame->params);
		frame->callbacks_size = 0;
		return;
	}
	
	frame->callbacks[frame->callbacks_size - 1] = callback;
	frame->params = safe_realloc(frame->params, sizeof(params_t *) * frame->callbacks_size);
	if(frame->params == NULL) {
		free(frame->callbacks);
		frame->callbacks_size = 0;
		return;
	}

	frame->params[frame->callbacks_size - 1] = params;
}

void frame_pop_callback(frame_t *frame) {
	NULL_RET(frame);
	if(frame->callbacks_size <= 0) {
		return;
	}

	frame->callbacks_size--;
	frame->callbacks[frame->callbacks_size] = NULL;
	frame->callbacks = safe_realloc(frame->callbacks, sizeof(callback_t) * frame->callbacks_size);
	frame->params = safe_realloc(frame->params, sizeof(params_t *) * frame->callbacks_size);
	if(frame->callbacks == NULL || frame->params == NULL) {
		NOT_NULL_FREE(frame->callbacks);
		NOT_NULL_FREE(frame->params);
		frame->callbacks_size = 0;
	}
}

void frame_remove_callback_index(frame_t *frame, int index) {
	if(index >= frame->callbacks_size - 1) {
		return frame_pop_callback(frame);
	}

	index = index < 0 ? 0 : index;
	for(int i = index; i < frame->callbacks_size - 1; i++) {
		frame->callbacks[i] = frame->callbacks[i + 1];
	}

	frame_pop_callback(frame);
}

void frame_remove_callback(frame_t *frame, callback_t callback) {
	for(int i = 0; i < frame->callbacks_size; i++) {
		if(frame->callbacks[i] == callback) {
			frame_remove_callback_index(frame, i);
		}
	}
}

#define RGB(_r, _g, _b) \
	(_r | (_g << 8) | (_b << 16))

void frame_pixel(frame_t *frame, vec_t pixel, unsigned int color) {
	if(
		pixel.x < 0 ||
		pixel.y < 0 ||
		pixel.x > frame->buffer->size.x ||
		pixel.y > frame->buffer->size.y || (
			frame->is_sub && (
				pixel.x > frame->select.size.x ||
				pixel.y > frame->select.size.y ||
				frame->select.pos.x < 0 ||
				frame->select.pos.y < 0 ||
				frame->select.pos.x + frame->select.size.x > frame->buffer->size.x ||
				frame->select.pos.y + frame->select.size.y > frame->buffer->size.y
			)
		)
	) {
		return;
	}

	if(frame->is_sub) {
		pixel.x += frame->select.pos.x;
		pixel.y += frame->select.pos.y;
	}

	frame->buffer->data[((int) pixel.y * (int) frame->buffer->size.x) + (int) pixel.x] = color;
}

void frame_rect(frame_t *frame, vec_t pos, vec_t size, unsigned int color) {
	for(int x = 0; x < size.x; x++) {
		for(int y = 0; y < size.y; y++) {
			frame_pixel(frame, (vec_t) { x + (int)pos.x, y + (int)pos.y }, color);
		}
	}
}

void frame_background(frame_t *frame, unsigned int color) {
	frame_rect(frame, (vec_t) { 0, 0 }, frame->select.size, color);
}

void frame_clear(frame_t *frame) {
	frame_background(frame, RGB(0, 0, 0));
}

void frame_update(frame_t *frame) {
	frame_clear(frame);
	for(int i = 0; i < frame->callbacks_size; i++) {
		frame->callbacks[i](frame, frame->params[i]);
	}
}
