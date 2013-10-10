#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define reg_a(x) reg[((x) >> 6) & 7]
#define reg_b(x) reg[((x) >> 3) & 7]
#define reg_c(x) reg[((x) >> 0) & 7]

#define op(x) (((x) >> 28) & 0xf)

enum {
	kUM_ID_INIT_COUNT = 256 - 1
};

struct um_arr {
	uint32_t count;
	uint32_t el[1];
};

struct um_id {
	uint32_t count;
	uint32_t noccupied;
	uint32_t **wid;
};

static __inline__ int is_little_endian(void)
{
	union {
		uint32_t i;
		uint8_t c[4];
	} x = { 1 };

	return x.c[0] == 1;
}

static __inline__ uint32_t bswap32(uint32_t x)
{
	return 0
		| ((x & 0x000000ff) << 24)
		| ((x & 0x0000ff00) << 8)
		| ((x & 0x00ff0000) >> 8)
		| ((x & 0xff000000) >> 24);
}

static __inline__ uint32_t to_be32(uint32_t x)
{
	return is_little_endian() ? bswap32(x) : x;
}

static uint32_t *um_alloc(uint32_t nmemb)
{
	struct um_arr *p = calloc(nmemb + 1, sizeof(uint32_t));

	if (p == NULL) {
		return NULL;
	}

	p->count = nmemb;

	return p->el;
}

static void um_free(uint32_t *p)
{
	free(p - 1);
}

static uint32_t add_wid(struct um_id *um_id, uint32_t *wid)
{
	uint32_t id = 0;
	uint32_t i = 0;

	if ((um_id == NULL) || (wid == NULL)) {
		return 0;
	}

	if (um_id->noccupied == um_id->count) {
		uint32_t n = um_id->count;

		um_id->count *= 2;
		um_id->wid = realloc(um_id->wid, sizeof(um_id->wid[0]) * um_id->count);

		if (um_id->wid == NULL) {
			return 0;
		}

		memset(um_id->wid + n, 0x00, sizeof(um_id->wid[0]) * n);
	}

	for (i = 1; i < um_id->count; ++i) {
		if (um_id->wid[i] == NULL) {
			id = i;
			um_id->wid[i] = wid;
			++um_id->noccupied;
			break;
		}
	}

	return id;
}

static uint32_t *delete_id(struct um_id *um_id, uint32_t id)
{
	uint32_t *wid = NULL;

	if ((um_id == NULL) || (id == 0)) {
		return NULL;
	}

	wid = um_id->wid[id];
	um_id->wid[id] = NULL;
	--um_id->noccupied;

	if (um_id->noccupied <= um_id->count / 4) {
		/* TODO: do some optimization */
	}

	return wid;
}

static uint32_t *get_wid(struct um_id *um_id, uint32_t id)
{
	if ((um_id == NULL) || (id == 0)) {
		return NULL;
	}

	return um_id->wid[id];
}

static int32_t alloc_wid(struct um_id *um_id)
{
	um_id->count = kUM_ID_INIT_COUNT + 1;
	um_id->wid = malloc(um_id->count * sizeof(um_id->wid[0]));

	if (um_id->wid == NULL) {
		return -1;
	}

	memset(um_id->wid, 0x00, um_id->count * sizeof(um_id->wid[0]));

	um_id->noccupied = 1;		/* reserve first node */

	return 0;
}

static void free_wid(struct um_id *um_id)
{
	int i;

	for (i = 1; i < um_id->count; ++i) {
		if (um_id->wid[i] != NULL) {
			um_free(um_id->wid[i]);
		}
	}
}

static void print_usage(const char *name)
{
	fprintf(stderr, "Usage: %s scroll\n", name);
}

int main(int argc, char *argv[])
{
	uint32_t x = 0;
	uint32_t fp = 0;
	FILE *scroll = NULL;
	uint32_t scroll_size = 0;
	uint32_t reg[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	uint32_t *arr0 = NULL;
	struct um_id um_id;
	uint8_t should_stop = 0;

	/* check arguments */

	if (argc < 2) {
		print_usage(argv[0]);
		return -1;
	}

	scroll = fopen(argv[1], "rb");

	if (scroll == NULL) {
		fprintf(stderr, "::::| error: could not open '%s' file\n", argv[1]);
		return -1;
	}

	fseek(scroll, 0L, SEEK_END);
	scroll_size = (uint32_t)ftell(scroll);

	arr0 = um_alloc(scroll_size / sizeof(x));

	if (arr0 == NULL) {
		fprintf(stderr, "::::| error: no memory for um\n");
		return -1;
	}

	fseek(scroll, 0L, SEEK_SET);

	while (fread(&x, sizeof(x), 1, scroll) > 0) {
		arr0[fp++] = to_be32(x);
	}

	fclose(scroll);

	if (alloc_wid(&um_id) != 0) {
		fprintf(stderr, "::::| error: no memory for wid\n");
		um_free(arr0);
	}

	fp = 0;

	/* UM loop */

	while (should_stop == 0) {
		uint32_t op_num;

		x = arr0[fp++];
		op_num = op(x);

		switch(op_num) {
			case 0: {
				/* Conditional Move
				 *
				 * The register A receives the value in register B,
				 * unless the register C contains 0.
				 */
				if (reg_c(x)) {
					reg_a(x) = reg_b(x);
				}
			} break;

			case 1: {
				/* Array Index
				 *
				 * The register A receives the value stored at offset
				 * in register C in the array identified by B.
				 */
				uint32_t *arr = arr0;

				if (reg_b(x)) {
					arr = get_wid(&um_id, reg_b(x));
				}

				reg_a(x) = arr[reg_c(x)];
			} break;

			case 2: {
				/* Array Amendment
				 *
				 * The array identified by A is amended at the offset
				 * in register B to store the value in register C.
				 */
				uint32_t *arr = arr0;

				if (reg_a(x)) {
					arr = get_wid(&um_id, reg_a(x));
				}

				arr[reg_b(x)] = reg_c(x);
			} break;

			case 3: {
				/* Addition
				 *
				 * The register A receives the value in register B plus
				 * the value in register C, modulo 2^32.
				 */
				reg_a(x) = reg_b(x) + reg_c(x);
			} break;

			case 4: {
				/* Multiplication
				 *
				 * The register A receives the value in register B times
				 * the value in register C, modulo 2^32.
				 */
				reg_a(x) = reg_b(x) * reg_c(x);
			} break;

			case 5: {
				/* Division
				 *
				 * The register A receives the value in register B
				 * divided by the value in register C, if any, where
				 * each quantity is treated treated as an unsigned 32
				 * bit number.
				 */
				if (reg_c(x) == 0) {
					should_stop = 1;
				} else {
					reg_a(x) = reg_b(x) / reg_c(x);
				}
			} break;

			case 6: {
				/* Not-And
				 *
				 * Each bit in the register A receives the 1 bit if
				 * either register B or register C has a 0 bit in that
				 * position.  Otherwise the bit in register A receives
				 * the 0 bit.
				 */
				reg_a(x) = ~(reg_b(x) & reg_c(x));
			} break;

			case 7: {
				/* Halt
				 *
				 * The universal machine stops computation.
				 */
				fprintf(stderr, "HALT\n");
				should_stop = 1;
			} break;

			case 8: {
				/* Allocation
				 *
				 * A new array is created with a capacity of platters
				 * commensurate to the value in the register C. This
				 * new array is initialized entirely with platters
				 * holding the value 0. A bit pattern not consisting of
				 * exclusively the 0 bit, and that identifies no other
				 * active allocated array, is placed in the B register.
				 */
				uint32_t *p = um_alloc(reg_c(x));

				if (p == NULL) {
					fprintf(stderr, "::::| error: no memory for um\n");
					should_stop = 1;
				}

				reg_b(x) = add_wid(&um_id, p);

				if (reg_b(x) == 0) {
					fprintf(stderr, "::::| error: failed to allocate wid\n");
					should_stop = 1;
				}
			} break;

			case 9: {
				/* Abandonment
				 *
				 * The array identified by the register C is abandoned.
				 * Future allocations may then reuse that identifier.
				 */
				um_free(delete_id(&um_id, reg_c(x)));
			} break;

			case 10: {
				/* Output
				 *
				 * The value in the register C is displayed on the console
				 * immediately. Only values between and including 0 and 255
				 * are allowed.
				 */
				putchar(reg_c(x) & 0xff);
			} break;

			case 11: {
				/* Input
				 *
				 * The universal machine waits for input on the console.
				 * When input arrives, the register C is loaded with the
				 * input, which must be between and including 0 and 255.
				 * If the end of input has been signaled, then the
				 * register C is endowed with a uniform value pattern
				 * where every place is pregnant with the 1 bit.
				 */
				int c = getchar();

				reg_c(x) = feof(stdin) ? 0xffffffffU : ((uint32_t)c & 0xff);
			} break;

			case 12: {
				/* Load Program
				 *
				 * The array identified by the B register is duplicated
				 * and the duplicate shall replace the '0' array,
				 * regardless of size. The execution finger is placed
				 * to indicate the platter of this array that is
				 * described by the offset given in C, where the value
				 * 0 denotes the first platter, 1 the second, et
				 * cetera.
				 *
				 * The '0' array shall be the most sublime choice for
				 * loading, and shall be handled with the utmost
				 * velocity.
				 */
				if (reg_b(x)) {
					uint32_t *p = get_wid(&um_id, reg_b(x));
					struct um_arr *arr = (struct um_arr *)(p - 1);

					um_free(arr0);
					arr0 = um_alloc(arr->count);

					if (arr0 != NULL) {
						memcpy(arr0, arr->el, sizeof(x) * arr->count);
					} else {
						fprintf(stderr, "::::| error: no memory for zero array\n");
						should_stop = 1;
					}
				}

				fp = reg_c(x);
			} break;

			case 13: {
				/* Orthography
				 *
				 * The value indicated is loaded into the register A
				 * forthwith.
				 */
				reg[(x >> 25) & 7] = x & 0x01ffffff;
			} break;
		}
	}

	free_wid(&um_id);
	um_free(arr0);
	free(um_id.wid);

	return 0;
}
