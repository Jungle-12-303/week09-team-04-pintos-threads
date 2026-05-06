# PintOS string.h / string.c 참고표

대상 파일:

- `pintos/include/lib/string.h`
- `pintos/lib/string.c`

PintOS의 문자열은 C처럼 `char[]` 또는 `char *`로 다루고, 문자열 끝은 `\0` 문자로 표시한다. 아래 함수들은 대부분 인자가 `NULL`이면 안 되며, 구현 안에서 `ASSERT`로 검사한다.

## 기본 개념

| 항목 | 타입 / 형태 | 의미 | 예시 | 주의점 |
|---|---|---|---|---|
| 문자열 배열 | `char buf[128]` | 실제 문자를 저장하는 공간 | `char buf[128] = "";` | 수정 가능 |
| 문자열 포인터 | `char *p` | 문자열 시작 주소를 가리키는 값 | `char *p = buf;` | 포인터 자체는 저장 공간이 아님 |
| 문자열 종료 문자 | `'\0'` | 문자열의 끝을 나타내는 문자 값 0 | `buf[0] = '\0';` | `NULL`과 용도가 다름 |
| 널 포인터 | `NULL` | 포인터가 아무것도 가리키지 않음 | `char *p = NULL;` | 문자열 종료 문자가 아님 |
| 크기 타입 | `size_t` | 크기, 길이, byte 수를 나타내는 부호 없는 정수 | `size_t len = strlen (s);` | 음수 표현용이 아님 |

## 함수 전체 요약

| 함수 | 선언 | 용도 | 반환값 | 핵심 주의점 |
|---|---|---|---|---|
| `memcpy` | `void *memcpy (void *dst, const void *src, size_t size);` | 메모리 `size` byte 복사 | `dst` | `dst`와 `src`가 겹치면 안 됨 |
| `memmove` | `void *memmove (void *dst, const void *src, size_t size);` | 겹칠 수 있는 메모리 복사 | `dst` | 겹침 가능하면 `memcpy` 대신 사용 |
| `memcmp` | `int memcmp (const void *a, const void *b, size_t size);` | 메모리 `size` byte 비교 | 같으면 `0`, `a`가 크면 양수, `b`가 크면 음수 | `\0`을 특별 취급하지 않음 |
| `memchr` | `void *memchr (const void *block, int ch, size_t size);` | 메모리에서 byte 값 검색 | 찾은 주소 또는 `NULL` | 문자열이 아니라 byte 블록 검색 |
| `memset` | `void *memset (void *dst, int value, size_t size);` | 메모리를 특정 byte 값으로 채움 | `dst` | `value`는 byte 단위로 들어감 |
| `strcmp` | `int strcmp (const char *a, const char *b);` | 문자열 비교 | 같으면 `0`, `a`가 크면 양수, `b`가 크면 음수 | `\0`까지 비교 |
| `strlen` | `size_t strlen (const char *string);` | 문자열 길이 계산 | `\0` 제외 길이 | `\0`이 없으면 위험 |
| `strnlen` | `size_t strnlen (const char *string, size_t maxlen);` | 최대 `maxlen`까지만 문자열 길이 계산 | 실제 길이 또는 `maxlen` | 방어적 길이 계산에 좋음 |
| `strchr` | `char *strchr (const char *string, int c);` | 문자열에서 첫 문자 검색 | 찾은 주소 또는 `NULL` | `c == '\0'`이면 끝 문자 주소 반환 |
| `strrchr` | `char *strrchr (const char *string, int c);` | 문자열에서 마지막 문자 검색 | 찾은 주소 또는 `NULL` | 마지막 등장 위치 |
| `strstr` | `char *strstr (const char *haystack, const char *needle);` | 부분 문자열 검색 | 찾은 시작 주소 또는 `NULL` | 내부적으로 `strlen`, `memcmp` 사용 |
| `strpbrk` | `char *strpbrk (const char *string, const char *stop);` | `stop`에 포함된 문자 중 첫 등장 검색 | 찾은 주소 또는 `NULL` | 여러 구분자 중 하나 찾을 때 유용 |
| `strspn` | `size_t strspn (const char *string, const char *skip);` | 처음부터 `skip` 문자들만 이어지는 길이 | prefix 길이 | 허용 문자 집합 검사 |
| `strcspn` | `size_t strcspn (const char *string, const char *stop);` | 처음부터 `stop` 문자가 나오기 전까지 길이 | prefix 길이 | 금지 문자 집합 검사 |
| `strlcpy` | `size_t strlcpy (char *dst, const char *src, size_t size);` | 안전한 문자열 복사 | `src` 전체 길이 | `size > 0`이면 항상 `\0` 종료 |
| `strlcat` | `size_t strlcat (char *dst, const char *src, size_t size);` | 안전한 문자열 이어붙이기 | 충분했다면 되었을 전체 길이 | `dst`는 이미 문자열이어야 함 |
| `strtok_r` | `char *strtok_r (char *s, const char *delimiters, char **save_ptr);` | 문자열 토큰화 | 다음 토큰 주소 또는 `NULL` | 원본 문자열을 수정함 |
| `strncat` | `char *strncat (char *dst, const char *src, size_t size);` | 문자열 일부 이어붙이기 | `dst` | 헤더에서 사용 금지 매크로로 막힘 |

## 인자 상세표

### 메모리 함수

| 함수 | 인자 | 타입 | 의미 |
|---|---|---|---|
| `memcpy` | `dst` | `void *` | 복사받을 목적지 메모리 |
| `memcpy` | `src` | `const void *` | 복사할 원본 메모리 |
| `memcpy` | `size` | `size_t` | 복사할 byte 수 |
| `memmove` | `dst` | `void *` | 복사받을 목적지 메모리 |
| `memmove` | `src` | `const void *` | 복사할 원본 메모리 |
| `memmove` | `size` | `size_t` | 복사할 byte 수 |
| `memcmp` | `a` | `const void *` | 비교할 첫 번째 메모리 |
| `memcmp` | `b` | `const void *` | 비교할 두 번째 메모리 |
| `memcmp` | `size` | `size_t` | 비교할 byte 수 |
| `memchr` | `block` | `const void *` | 검색할 메모리 시작 주소 |
| `memchr` | `ch` | `int` | 찾을 byte 값 |
| `memchr` | `size` | `size_t` | 검색할 byte 수 |
| `memset` | `dst` | `void *` | 값을 채울 메모리 |
| `memset` | `value` | `int` | 채울 byte 값 |
| `memset` | `size` | `size_t` | 채울 byte 수 |

### 문자열 비교 / 길이 함수

| 함수 | 인자 | 타입 | 의미 |
|---|---|---|---|
| `strcmp` | `a` | `const char *` | 비교할 첫 번째 문자열 |
| `strcmp` | `b` | `const char *` | 비교할 두 번째 문자열 |
| `strlen` | `string` | `const char *` | 길이를 구할 문자열 |
| `strnlen` | `string` | `const char *` | 길이를 구할 문자열 |
| `strnlen` | `maxlen` | `size_t` | 최대 검사 길이 |

### 문자열 검색 함수

| 함수 | 인자 | 타입 | 의미 |
|---|---|---|---|
| `strchr` | `string` | `const char *` | 검색 대상 문자열 |
| `strchr` | `c` | `int` | 찾을 문자 |
| `strrchr` | `string` | `const char *` | 검색 대상 문자열 |
| `strrchr` | `c` | `int` | 찾을 문자 |
| `strstr` | `haystack` | `const char *` | 검색 대상 문자열 |
| `strstr` | `needle` | `const char *` | 찾을 부분 문자열 |
| `strpbrk` | `string` | `const char *` | 검색 대상 문자열 |
| `strpbrk` | `stop` | `const char *` | 찾을 문자들의 집합 |
| `strspn` | `string` | `const char *` | 검사 대상 문자열 |
| `strspn` | `skip` | `const char *` | 허용할 문자들의 집합 |
| `strcspn` | `string` | `const char *` | 검사 대상 문자열 |
| `strcspn` | `stop` | `const char *` | 만나면 멈출 문자들의 집합 |

### 문자열 복사 / 결합 / 토큰화 함수

| 함수 | 인자 | 타입 | 의미 |
|---|---|---|---|
| `strlcpy` | `dst` | `char *` | 복사받을 목적지 버퍼 |
| `strlcpy` | `src` | `const char *` | 복사할 원본 문자열 |
| `strlcpy` | `size` | `size_t` | `dst` 버퍼 전체 크기 |
| `strlcat` | `dst` | `char *` | 뒤에 붙일 목적지 버퍼 |
| `strlcat` | `src` | `const char *` | 뒤에 붙일 원본 문자열 |
| `strlcat` | `size` | `size_t` | `dst` 버퍼 전체 크기 |
| `strtok_r` | `s` | `char *` | 처음 호출 때 토큰화할 수정 가능한 문자열 |
| `strtok_r` | `delimiters` | `const char *` | 구분자로 사용할 문자들의 집합 |
| `strtok_r` | `save_ptr` | `char **` | 현재 토큰화 위치를 저장할 포인터 변수의 주소 |
| `strncat` | `dst` | `char *` | 이어붙일 목적지 문자열 |
| `strncat` | `src` | `const char *` | 이어붙일 원본 문자열 |
| `strncat` | `size` | `size_t` | 최대 이어붙일 문자 수 |

## 반환값 상세표

| 함수 | 반환 타입 | 반환값 의미 | 실패 / 없음 |
|---|---|---|---|
| `memcpy` | `void *` | `dst` | 없음 |
| `memmove` | `void *` | `dst` | 없음 |
| `memcmp` | `int` | 같으면 `0`, 첫 차이에서 `a > b`면 양수, `a < b`면 음수 | 없음 |
| `memchr` | `void *` | 찾은 byte 주소 | 못 찾으면 `NULL` |
| `memset` | `void *` | `dst` | 없음 |
| `strcmp` | `int` | 같으면 `0`, 첫 차이에서 `a > b`면 양수, `a < b`면 음수 | 없음 |
| `strlen` | `size_t` | `\0` 제외 문자열 길이 | 없음 |
| `strnlen` | `size_t` | 실제 길이 또는 `maxlen` | 없음 |
| `strchr` | `char *` | 찾은 문자 주소 | 못 찾으면 `NULL` |
| `strrchr` | `char *` | 마지막으로 찾은 문자 주소 | 못 찾으면 `NULL` |
| `strstr` | `char *` | 부분 문자열 시작 주소 | 못 찾으면 `NULL` |
| `strpbrk` | `char *` | `stop` 문자 중 처음 발견한 주소 | 못 찾으면 `NULL` |
| `strspn` | `size_t` | 허용 문자 prefix 길이 | 해당 없으면 `0` |
| `strcspn` | `size_t` | 금지 문자 전까지 prefix 길이 | 첫 글자가 금지 문자면 `0` |
| `strlcpy` | `size_t` | `src` 전체 길이 | 반환값이 `size` 이상이면 잘림 |
| `strlcat` | `size_t` | 기존 `dst` 길이 + `src` 길이 | 반환값이 `size` 이상이면 잘림 |
| `strtok_r` | `char *` | 다음 토큰 시작 주소 | 토큰 없으면 `NULL` |
| `strncat` | `char *` | `dst` | 이 코드베이스에서는 사용 지양 |

## 금지 유도 매크로

| 금지 이름 | 매크로 치환 | 대신 쓸 함수 | 이유 |
|---|---|---|---|
| `strcpy` | `dont_use_strcpy_use_strlcpy` | `strlcpy` | 목적지 크기를 받지 않아 overflow 위험 |
| `strncpy` | `dont_use_strncpy_use_strlcpy` | `strlcpy` | `\0` 종료가 직관적이지 않음 |
| `strcat` | `dont_use_strcat_use_strlcat` | `strlcat` | 목적지 크기를 받지 않아 overflow 위험 |
| `strncat` | `dont_use_strncat_use_strlcat` | `strlcat` | 전체 버퍼 크기 기준이 아니라 실수하기 쉬움 |
| `strtok` | `dont_use_strtok_use_strtok_r` | `strtok_r` | 내부 전역 상태를 써서 안전하지 않음 |

원문 매크로:

```c
#define strcpy dont_use_strcpy_use_strlcpy
#define strncpy dont_use_strncpy_use_strlcpy
#define strcat dont_use_strcat_use_strlcat
#define strncat dont_use_strncat_use_strlcat
#define strtok dont_use_strtok_use_strtok_r
```

## 자주 쓰는 패턴

| 상황 | 코드 | 설명 |
|---|---|---|
| 빈 문자열 만들기 | `char buf[128] = "";` | `buf[0]`이 `\0` |
| 기존 문자열 비우기 | `buf[0] = '\0';` | 저장 공간은 그대로 두고 문자열만 빈 상태 |
| 배열에 복사 | `strlcpy (buf, src, sizeof buf);` | `buf`가 배열이면 `sizeof buf`가 전체 크기 |
| 포인터 목적지에 복사 | `strlcpy (p, src, PGSIZE);` | `p`가 실제 쓰기 가능한 공간을 가리켜야 함 |
| 페이지 버퍼 복사 | `strlcpy (copy, file_name, PGSIZE);` | `copy = palloc_get_page (...)` 같은 경우 |
| 토큰화 | `token = strtok_r (cmd, " ", &save_ptr);` | `cmd` 내부 구분자가 `\0`으로 바뀜 |
| 메모리 0 초기화 | `memset (buf, 0, sizeof buf);` | 배열 전체를 0으로 채움 |

## 헷갈리는 포인트

| 주제 | 맞는 사용 | 위험한 사용 | 이유 |
|---|---|---|---|
| `NULL` vs `\0` | `char *p = NULL;`, `buf[i] = '\0';` | `buf[i] = NULL;` | `NULL`은 포인터용, `\0`은 문자용 |
| 수정 가능한 문자열 | `char s[] = "hello";` | `char *s = "hello"; s[0] = 'H';` | 문자열 리터럴은 수정하면 안 됨 |
| 포인터 목적지 | `char *p = buf; strlcpy (p, src, sizeof buf);` | `char *p; strlcpy (p, src, 128);` | 초기화 안 된 포인터는 유효한 공간을 가리키지 않음 |
| 포인터의 `sizeof` | `strlcpy (p, src, PGSIZE);` | `strlcpy (p, src, sizeof p);` | `sizeof p`는 버퍼 크기가 아니라 포인터 크기 |
| `strtok_r` 원본 | `char cmd[] = "a b"; strtok_r (cmd, " ", &save);` | `strtok_r ("a b", " ", &save);` | `strtok_r`은 원본 문자열을 수정함 |

## PintOS argument passing에서 특히 유용한 함수

| 함수 | 왜 유용한가 | 예시 상황 |
|---|---|---|
| `strlcpy` | 커맨드라인 문자열을 안전하게 복사 | `file_name`을 임시 버퍼에 복사 |
| `strtok_r` | 공백 기준으로 프로그램 이름과 인자를 나눔 | `"echo hello"`를 `echo`, `hello`로 토큰화 |
| `strlen` | 스택에 복사할 문자열 길이 계산 | `strlen (argv[i]) + 1` |
| `memcpy` | 문자열 byte를 유저 스택에 직접 복사 | 스택 포인터를 내린 뒤 문자열 복사 |
| `memset` | 버퍼나 구조체 초기화 | 임시 배열 0 초기화 |
