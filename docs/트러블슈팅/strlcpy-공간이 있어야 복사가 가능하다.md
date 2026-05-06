# `temp_name`과 `strlcpy` 트러블슈팅

## 상황

`load()` 함수에서 실행 파일 이름만 분리하기 위해 `file_name`을 복사한 뒤 `strtok_r()`로 파싱하려고 했다.

최종적으로 의도한 흐름은 다음과 같다.

```c
char *save_ptr;
char temp_name[strlen(file_name) + 1];
strlcpy(temp_name, file_name, strlen(file_name) + 1);
char *program_name = strtok_r(temp_name, " ", &save_ptr);
```

## 헷갈렸던 점

처음에는 다음처럼 작성하려고 했다.

```c
char *temp_name;
strlcpy(temp_name, file_name, strlen(file_name) + 1);
```

하지만 이 코드는 위험하다.

`char *temp_name;`은 문자열을 담을 공간을 만든 것이 아니다. 단지 문자열이 있는 주소를 저장할 수 있는 포인터 변수 하나를 만든 것이다.

즉, 이 상태의 `temp_name`은 아직 유효한 메모리 공간을 가리키지 않는다.

```text
temp_name
+----------------+
| 알 수 없는 주소 |
+----------------+
```

그런데 `strlcpy()`는 첫 번째 인자로 받은 주소에 문자열을 복사한다.

```c
strlcpy(temp_name, file_name, strlen(file_name) + 1);
```

따라서 위 코드는 `temp_name`이 가리키는 알 수 없는 위치에 문자열을 쓰려고 하게 된다.

## `strlcpy()`의 세 번째 인자는 공간을 만들지 않는다

헷갈릴 수 있는 지점은 `strlcpy()`의 첫 번째 인자가 어차피 포인터처럼 보인다는 점이다.

```c
strlcpy(dst, src, size);
```

여기서 `size`는 `strlcpy()`에게 이만큼의 공간을 새로 확보하라고 요청하는 값이 아니다.

`size`는 `dst`가 이미 가리키고 있는 공간의 크기를 알려주는 값이다.

즉, `strlcpy()`는 다음처럼 동작한다고 이해하면 된다.

```text
dst가 이미 size만큼의 유효한 공간을 가리키고 있다고 믿고,
src 문자열을 그 범위 안에서 복사한다.
```

따라서 아래 코드는 여전히 위험하다.

```c
char *temp_name;
strlcpy(temp_name, file_name, strlen(file_name) + 1);
```

세 번째 인자로 `strlen(file_name) + 1`을 넘겼더라도, 그 크기만큼의 메모리가 자동으로 생기지는 않는다.

`strlcpy()`는 메모리 할당 함수가 아니라 복사 함수다.

반면 아래 코드는 실제 배열 공간을 먼저 만든다.

```c
char temp_name[strlen(file_name) + 1];
strlcpy(temp_name, file_name, strlen(file_name) + 1);
```

이 경우 `temp_name` 배열은 `strlen(file_name) + 1`바이트짜리 실제 저장 공간이다. 배열 이름 `temp_name`은 함수 인자로 전달될 때 그 배열의 첫 번째 칸 주소처럼 사용된다.

그래서 두 코드 모두 `strlcpy()`에 들어갈 때는 포인터처럼 보이지만, 의미는 다르다.

```text
char *temp_name;
  -> 주소를 담는 변수만 있음
  -> 실제 문자열 공간은 아직 없음

char temp_name[N];
  -> N바이트짜리 실제 배열 공간이 있음
  -> 함수에 넘기면 그 공간의 시작 주소가 전달됨
```

핵심은 다음과 같다.

```text
포인터는 주소다.
주소를 담는 변수가 있다고 해서 문자열을 저장할 공간이 생기는 것은 아니다.
```

## 해결

문자열을 복사하려면 먼저 실제 저장 공간이 있어야 한다.

이번 경우에는 배열을 사용해서 `file_name` 길이만큼의 임시 공간을 만들었다.

```c
char temp_name[strlen(file_name) + 1];
```

여기서 `+ 1`은 문자열 마지막의 널 문자 `'\0'`을 위한 공간이다.

그 다음 `strlcpy()`로 복사한다.

```c
strlcpy(temp_name, file_name, strlen(file_name) + 1);
```

이제 `temp_name`은 실제 배열 공간이므로 `strlcpy()`의 목적지로 사용할 수 있다.

## 왜 복사본이 필요한가

`strtok_r()`는 원본 문자열을 수정한다.

예를 들어:

```text
"echo hello"
```

를 공백 기준으로 파싱하면, 내부적으로 공백을 `'\0'`으로 바꿔서 다음처럼 만든다.

```text
"echo\0hello"
```

따라서 `file_name` 원본을 직접 자르기보다, `temp_name` 복사본을 만든 뒤 그 복사본을 파싱하는 편이 안전하다.

## `strtok_r()` 반환값

```c
char *program_name = strtok_r(temp_name, " ", &save_ptr);
```

`strtok_r()`의 반환값은 새 문자열이 아니라, `temp_name` 안에서 이번에 찾은 토큰의 시작 주소다.

예를 들어 `file_name`이 다음과 같다면:

```text
"   echo hello"
```

`temp_name`은 배열의 맨 앞 공백부터 시작하지만, `program_name`은 공백을 건너뛴 `echo`의 시작 위치를 가리킨다.

그래서 실행 파일을 열 때는 반환값을 사용하는 것이 의미상 정확하다.

```c
file = filesys_open(program_name);
```

## 정리

- `char *temp_name;`은 공간이 아니라 주소를 담는 변수다.
- 문자열을 복사하려면 실제 저장 공간이 필요하다.
- `char temp_name[strlen(file_name) + 1];`은 실제 배열 공간을 만든다.
- `strlcpy()`의 세 번째 인자는 목적지 버퍼의 크기이며, 공간을 새로 할당하지 않는다.
- `strtok_r()`는 문자열을 직접 수정하므로 복사본을 파싱하는 것이 좋다.
- `strtok_r()`의 반환값은 이번 토큰의 시작 주소다.
