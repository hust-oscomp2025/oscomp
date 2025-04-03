// See LICENSE for license details.

#include <ctype.h>
#include <kernel/types.h>
#include <kernel/util/string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

static int vsnprintf_internal(char* str, size_t size, const char* format, va_list args);

void* memcpy(void* dest, const void* src, size_t len) {
	const char* s = src;
	char* d = dest;

	if ((((uintptr_t)dest | (uintptr_t)src) & (sizeof(uintptr_t) - 1)) == 0) {
		while ((void*)d < (dest + len - (sizeof(uintptr_t) - 1))) {
			*(uintptr_t*)d = *(const uintptr_t*)s;
			d += sizeof(uintptr_t);
			s += sizeof(uintptr_t);
		}
	}

	while (d < (char*)(dest + len)) *d++ = *s++;

	return dest;
}

void* memset(void* dest, int byte, size_t len) {
	if ((((uintptr_t)dest | len) & (sizeof(uintptr_t) - 1)) == 0) {
		uintptr_t word = byte & 0xFF;
		word |= word << 8;
		word |= word << 16;
		word |= word << 16 << 16;

		uintptr_t* d = dest;
		while (d < (uintptr_t*)(dest + len)) *d++ = word;
	} else {
		char* d = dest;
		while (d < (char*)(dest + len)) *d++ = byte;
	}
	return dest;
}

size_t strlen(const char* s) {
	const char* p = s;
	while (*p) p++;
	return p - s;
}

int strcmp(const char* s1, const char* s2) {
	unsigned char c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;
	} while (c1 != 0 && c1 == c2);

	return c1 - c2;
}

char* strcpy(char* dest, const char* src) {
	char* d = dest;
	while ((*d++ = *src++));
	return dest;
}

char* strchr(const char* p, int ch) {
	char c;
	c = ch;
	for (;; ++p) {
		if (*p == c) return ((char*)p);
		if (*p == '\0') return (NULL);
	}
}

char* strtok(char* str, const char* delim) {
	static char* current;
	if (str != NULL) current = str;
	if (current == NULL) return NULL;

	char* start = current;
	while (*start != '\0' && strchr(delim, *start) != NULL) start++;

	if (*start == '\0') {
		current = NULL;
		return current;
	}

	char* end = start;
	while (*end != '\0' && strchr(delim, *end) == NULL) end++;

	if (*end != '\0') {
		*end = '\0';
		current = end + 1;
	} else
		current = NULL;
	return start;
}

char* strcat(char* dst, const char* src) {
	strcpy(dst + strlen(dst), src);
	return dst;
}

long atol(const char* str) {
	long res = 0;
	int sign = 0;

	while (*str == ' ') str++;

	if (*str == '-' || *str == '+') {
		sign = *str == '-';
		str++;
	}

	while (*str) {
		res *= 10;
		res += *str++ - '0';
	}

	return sign ? -res : res;
}

void* memmove(void* dst, const void* src, size_t n) {
	const char* s;
	char* d;

	s = src;
	d = dst;
	if (s < d && s + n > d) {
		s += n;
		d += n;
		while (n-- > 0) *--d = *--s;
	} else
		while (n-- > 0) *d++ = *s++;

	return dst;
}

/**
 * strncpy - 将源字符串的前n个字符复制到目标字符串
 * @dest: 目标字符串
 * @src: 源字符串
 * @n: 最多复制的字符数
 *
 * 如果src的长度小于n，则用'\0'填充剩余空间
 * 如果src的长度大于等于n，则不会自动添加'\0'
 *
 * 返回值: 目标字符串指针
 */
char* strncpy(char* dest, const char* src, size_t n) {
	size_t i;

	// 复制最多n个字符
	for (i = 0; i < n && src[i] != '\0'; i++) { dest[i] = src[i]; }

	// 如果源字符串长度小于n，用'\0'填充剩余空间
	for (; i < n; i++) { dest[i] = '\0'; }

	return dest;
}

/**
 * snprintf - 格式化字符串并将结果写入缓冲区，同时确保不会溢出
 * @str: 目标缓冲区
 * @size: 缓冲区的大小（包括终止符'\0'）
 * @format: 格式化字符串
 * @...: 可变参数列表
 *
 * 将格式化字符串写入到指定缓冲区，最多写入size-1个字符，
 * 并确保缓冲区以'\0'结尾。
 *
 * 返回值: 如果缓冲区足够大，返回写入的字符数（不包括终止符'\0'）；
 *         否则返回格式化完成后本应该写入的字符数（不包括终止符'\0'）
 */
int snprintf(char* str, size_t size, const char* format, ...) {
	va_list args;
	int result;

	// 如果缓冲区大小为0或为NULL，不进行任何写入
	if (size == 0 || str == NULL) { return 0; }

	va_start(args, format);
	result = vsnprintf_internal(str, size, format, args);
	va_end(args);

	return result;
}

// 内部实现，处理格式化并写入字符
static int vsnprintf_internal(char* str, size_t size, const char* format, va_list args) {
	size_t count = 0; // 已写入或需要写入的字符数
	char* s;
	int num, len;
	char padding, temp[32];
	uint32 unum;

	if (size == 0) { return 0; }

	// 确保至少有一个字节用于终止符
	size--;

	while (*format && count < size) {
		if (*format != '%') {
			// 普通字符直接写入
			str[count++] = *format++;
			continue;
		}

		// 处理格式说明符
		format++;

		// 处理填充（暂时只支持0填充）
		padding = ' ';
		if (*format == '0') {
			padding = '0';
			format++;
		}

		// 处理不同类型的格式化
		switch (*format) {
		case 's': // 字符串
			s = va_arg(args, char*);
			if (s == NULL) { s = "(null)"; }
			len = strlen(s);
			// 复制字符串，但不超过剩余空间
			while (*s && count < size) { str[count++] = *s++; }
			// 即使无法完全写入，也要计算总长度
			count += strlen(s);
			break;

		case 'd': // 有符号整数
		case 'i':
			num = va_arg(args, int);

			// 处理负数
			if (num < 0) {
				if (count < size) {
					str[count++] = '-';
				} else {
					count++;
				}
				num = -num;
			}

			// 转换为字符串
			len = 0;
			do {
				temp[len++] = '0' + (num % 10);
				num /= 10;
			} while (num > 0);

			// 反向写入
			while (len > 0 && count < size) { str[count++] = temp[--len]; }
			// 计算剩余未写入的字符
			count += len;
			break;

		case 'u': // 无符号整数
			unum = va_arg(args, uint32);

			// 转换为字符串
			len = 0;
			do {
				temp[len++] = '0' + (unum % 10);
				unum /= 10;
			} while (unum > 0);

			// 反向写入
			while (len > 0 && count < size) { str[count++] = temp[--len]; }
			// 计算剩余未写入的字符
			count += len;
			break;

		case 'x': // 十六进制（小写）
		case 'X': // 十六进制（大写）
			unum = va_arg(args, uint32);

			// 转换为字符串
			len = 0;
			do {
				int digit = unum % 16;
				if (digit < 10) {
					temp[len++] = '0' + digit;
				} else {
					temp[len++] = (*format == 'x' ? 'a' : 'A') + (digit - 10);
				}
				unum /= 16;
			} while (unum > 0);

			// 反向写入
			while (len > 0 && count < size) { str[count++] = temp[--len]; }
			// 计算剩余未写入的字符
			count += len;
			break;

		case 'c': // 字符
			if (count < size) {
				str[count++] = (char)va_arg(args, int);
			} else {
				count++;
				va_arg(args, int); // 跳过参数
			}
			break;

		case '%': // 百分号
			if (count < size) {
				str[count++] = '%';
			} else {
				count++;
			}
			break;

		default: // 不支持的格式，原样输出
			if (count < size) {
				str[count++] = *format;
			} else {
				count++;
			}
		}

		format++;
	}

	// 添加终止符
	if (size > 0) { str[count < size ? count : size] = '\0'; }

	// 计算剩余格式字符串会产生的字符数
	while (*format) {
		if (*format == '%') {
			format++;
			if (*format == 's') {
				s = va_arg(args, char*);
				if (s) { count += strlen(s); }
			} else if (*format == 'c' || *format == 'd' || *format == 'i' || *format == 'u' || *format == 'x' || *format == 'X') {
				va_arg(args, int); // 跳过参数
				count++;
			} else if (*format == '%') {
				count++;
			}
		} else {
			count++;
		}
		format++;
	}

	return count;
}

/**
 * memcmp - Compare two memory regions
 * @s1: First memory region
 * @s2: Second memory region
 * @n: Number of bytes to compare
 *
 * Compares two memory regions byte by byte.
 *
 * Returns:
 *   0 if the regions are identical
 *   < 0 if the first differing byte in s1 is less than in s2
 *   > 0 if the first differing byte in s1 is greater than in s2
 */
int memcmp(const void* s1, const void* s2, size_t n) {
	const unsigned char* p1 = s1;
	const unsigned char* p2 = s2;

	if (s1 == s2 || n == 0) return 0;

	/* Compare byte by byte */
	while (n--) {
		if (*p1 != *p2) return *p1 - *p2;
		p1++;
		p2++;
	}

	return 0;
}

int32_t vsnprintf(char* out, size_t n, const char* s, va_list vl) {
	bool format = false;
	bool longarg = false;
	size_t pos = 0;

	for (; *s; s++) {
		if (format) {
			switch (*s) {
			case 'l':
				longarg = true;
				break;
			case 'p':
				longarg = true;
				if (++pos < n) out[pos - 1] = '0';
				if (++pos < n) out[pos - 1] = 'x';
			case 'x': {
				int64 num = longarg ? va_arg(vl, int64) : va_arg(vl, int);
				for (int i = 2 * (longarg ? sizeof(int64) : sizeof(int)) - 1; i >= 0; i--) {
					int d = (num >> (4 * i)) & 0xF;
					if (++pos < n) out[pos - 1] = (d < 10 ? '0' + d : 'a' + d - 10);
				}
				longarg = false;
				format = false;
				break;
			}
			case 'd': {
				int64 num = longarg ? va_arg(vl, int64) : va_arg(vl, int);
				if (num < 0) {
					num = -num;
					if (++pos < n) out[pos - 1] = '-';
				}
				int64 digits = 1;
				for (int64 nn = num; nn /= 10; digits++);
				for (int i = digits - 1; i >= 0; i--) {
					if (pos + i + 1 < n) out[pos + i] = '0' + (num % 10);
					num /= 10;
				}
				pos += digits;
				longarg = false;
				format = false;
				break;
			}
			case 's': {
				const char* s2 = va_arg(vl, const char*);
				while (*s2) {
					if (++pos < n) out[pos - 1] = *s2;
					s2++;
				}
				longarg = false;
				format = false;
				break;
			}
			case 'c': {
				if (++pos < n) out[pos - 1] = (char)va_arg(vl, int);
				longarg = false;
				format = false;
				break;
			}
			default:
				break;
			}
		} else if (*s == '%')
			format = true;
		else if (++pos < n)
			out[pos - 1] = *s;
	}
	if (pos < n)
		out[pos] = 0;
	else if (n)
		out[n - 1] = 0;
	return pos;
}
