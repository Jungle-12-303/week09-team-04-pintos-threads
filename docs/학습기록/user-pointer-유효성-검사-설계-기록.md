# user pointer 유효성 검사 설계 기록

이 문서는 Pintos `userprog/syscall.c`에서 syscall 인자로 들어오는 user pointer를 검증하는 기능을 구현해 가는 과정을 기록한다.

목표는 최종 정답 코드만 남기는 것이 아니라, 처음 접근에서 어떤 문제가 있었고, 그 문제를 어떤 기준으로 개선했는지 설명할 수 있게 만드는 것이다.

## 1. 문제 정의

syscall은 유저 프로그램이 커널에게 서비스를 요청하는 경계다. 이때 `write(fd, buffer, size)` 같은 syscall에서 `buffer`는 유저 프로그램이 넘긴 주소다.

커널은 이 주소를 그대로 믿으면 안 된다. 유저 프로그램은 다음과 같은 값을 넘길 수 있다.

- `NULL`
- 커널 영역 주소
- 유저 영역처럼 보이지만 매핑되지 않은 주소
- 시작 주소만 유효하고 중간 페이지가 invalid인 버퍼
- 문자열의 `\0`을 만나기 전에 invalid page로 넘어가는 주소

따라서 syscall 구현 전에 user pointer가 안전한지 검사하는 helper가 필요하다.

## 2. 초기 설계 기준

처음 기준은 두 가지였다.

- 단일 유저 주소 하나를 검사하는 함수가 필요하다.
- `buffer + size` 범위 전체를 검사하는 함수가 필요하다.

그래서 역할을 다음처럼 나누었다.

```c
static bool is_valid_user_ptr(void *uaddr);
static bool is_valid_user_buffer(void *buffer, unsigned size);
```

이 분리는 좋은 출발점이다. 단일 주소 검증과 범위 검증은 서로 다른 문제이기 때문이다.

## 3. 버전별 설계 변화

### v1. 단일 주소 검증과 버퍼 검증 분리

#### 목표

유저 포인터 하나가 안전한지 확인하고, 버퍼는 그 검사를 여러 번 적용해서 전체 범위를 검사하려고 했다.

#### 코드 또는 구조

현재 작성한 핵심 구조는 다음과 같다.

```c
static bool
is_valid_user_ptr(void *uaddr)
{
    if (uaddr == NULL ||
        pml4_get_page(thread_current()->pml4, uaddr) == NULL ||
        !is_user_vaddr(uaddr))
        return false;

    return true;
}
```

```c
static bool
is_valid_user_buffer(void *buffer, unsigned size)
{
    if(is_valid_user_ptr(buffer))
        return false;

    uintptr_t *uaddr = buffer;
    uintptr_t *end_uaddr = (uintptr_t)buffer + size;
    uintptr_t diff;

    while (uaddr <= end_uaddr)
    {
        diff = end_uaddr - uaddr;

        if(diff < PGSIZE){
            return (is_valid_user_ptr( (void *) (uaddr+size)));
        }
        else{
            uaddr += PGSIZE;
            if(!is_valid_user_ptr(uaddr))
                return false;
        }
    }

    return true;
}
```

#### 초기 접근의 장점

단일 포인터 검증과 버퍼 검증을 분리한 점은 좋다.

`is_valid_user_ptr()`는 "주소 하나가 유저 주소이고 매핑되어 있는가"를 맡고, `is_valid_user_buffer()`는 "범위 전체가 안전한가"를 맡는다. 이 역할 분리는 이후 `write`, `read`, `open`, `exec` 같은 syscall을 구현할 때 중복을 줄일 수 있다.

또한 `pml4_get_page()`를 떠올린 것도 방향이 맞다. 이 함수는 현재 프로세스의 page table에서 해당 유저 주소가 실제 페이지에 매핑되어 있는지 확인하는 데 사용할 수 있다.

#### 남은 문제

첫 번째 문제는 검사 순서다.

`pml4_get_page()` 내부에는 다음 조건이 있다.

```c
ASSERT (is_user_vaddr (uaddr));
```

따라서 커널 주소가 들어왔을 때 `pml4_get_page()`를 먼저 호출하면 `false`를 반환하는 것이 아니라 ASSERT로 커널이 멈출 수 있다. 유저 주소 여부를 먼저 확인하고, 그 다음 매핑 여부를 확인해야 한다.

두 번째 문제는 버퍼 시작 주소 검사 조건이 반대로 되어 있다.

```c
if(is_valid_user_ptr(buffer))
    return false;
```

이 코드는 시작 주소가 valid일 때 false를 반환한다. 의도는 invalid면 false를 반환하는 것이므로 `!is_valid_user_ptr(buffer)`가 되어야 한다.

세 번째 문제는 포인터 타입이다.

```c
uintptr_t *uaddr = buffer;
uintptr_t *end_uaddr = (uintptr_t)buffer + size;
```

`uintptr_t *`는 "주소값을 담는 정수 타입의 포인터"다. 여기서는 바이트 단위 주소 이동을 하고 싶으므로 `uint8_t *` 또는 `char *`가 더 적절하다.

네 번째 문제는 `uaddr += PGSIZE`의 의미다.

`uaddr`가 `uintptr_t *`이면 `uaddr += PGSIZE`는 4096바이트가 아니라 `4096 * sizeof(uintptr_t)`만큼 이동한다. 버퍼 페이지 순회에서는 치명적인 오차가 된다.

다섯 번째 문제는 끝 주소 계산이다.

`buffer + size`는 접근 범위의 마지막 바이트가 아니라 마지막 다음 주소다. 실제로 검사해야 하는 범위는 `buffer`부터 `buffer + size - 1`까지다. `size == 0`인 경우도 별도 정책이 필요하다.

#### 가능한 대안

대안 A: 바이트 단위로 모든 주소를 검사한다.

```c
for (uint8_t *p = buffer; p < (uint8_t *)buffer + size; p++) {
    if (!is_valid_user_ptr(p))
        return false;
}
```

장점은 이해하기 쉽고 정확하다는 것이다. 단점은 큰 버퍼에서 같은 페이지를 수천 번 반복 검사할 수 있다는 것이다.

대안 B: 시작 주소와 끝 주소만 검사한다.

```c
is_valid_user_ptr(buffer);
is_valid_user_ptr((uint8_t *)buffer + size - 1);
```

장점은 코드가 짧다. 단점은 버퍼가 세 페이지 이상에 걸치고 가운데 페이지가 unmapped인 경우를 놓칠 수 있다.

대안 C: 버퍼가 걸치는 모든 페이지를 페이지 단위로 검사한다.

```c
for (uint8_t *page = pg_round_down(buffer);
     page <= pg_round_down((uint8_t *)buffer + size - 1);
     page += PGSIZE) {
    if (!is_valid_user_ptr(page))
        return false;
}
```

장점은 page table의 매핑 단위와 맞고 효율적이다. 단점은 끝 주소 계산과 `size == 0` 처리를 조심해야 한다.

### v2. 검사 순서와 타입을 바로잡은 설계

#### 목표

v1에서 드러난 ASSERT 위험과 포인터 산술 문제를 제거한다.

#### 채택한 방향

대안 C를 채택한다.

이유는 Pintos의 주소 매핑이 페이지 단위이고, `pml4_get_page()`도 주소가 속한 페이지의 매핑 여부를 확인하기 때문이다. 버퍼 전체를 바이트 단위로 검사하는 것은 정확하지만 중복이 많다. 반대로 시작과 끝만 검사하는 방식은 중간 페이지를 놓친다.

따라서 범위가 걸치는 모든 페이지를 한 번씩 검사하는 방식이 가장 적절하다.

#### 코드 또는 구조

단일 주소 검증은 다음 순서를 따른다.

```c
static bool
is_valid_user_ptr(const void *uaddr)
{
    if (uaddr == NULL)
        return false;
    if (!is_user_vaddr(uaddr))
        return false;
    return pml4_get_page(thread_current()->pml4, uaddr) != NULL;
}
```

핵심은 `is_user_vaddr()`를 먼저 호출한다는 점이다. 그래야 `pml4_get_page()` 내부 ASSERT 조건을 만족시킬 수 있다.

버퍼 검증은 다음 구조로 잡는다.

```c
static bool
is_valid_user_buffer(const void *buffer, unsigned size)
{
    if (size == 0)
        return true;
    if (!is_valid_user_ptr(buffer))
        return false;

    uint8_t *start = pg_round_down(buffer);
    uint8_t *end = pg_round_down((const uint8_t *)buffer + size - 1);

    for (uint8_t *page = start; page <= end; page += PGSIZE) {
        if (!is_valid_user_ptr(page))
            return false;
    }

    return true;
}
```

#### 이전 버전에서 개선된 점

`pml4_get_page()` 호출 전에 유저 주소 여부를 확인하므로 커널 주소 입력으로 인한 ASSERT 위험이 줄었다.

또한 `uint8_t *`를 사용해 포인터 이동 단위를 1바이트로 맞췄다. `PGSIZE`만큼 이동하면 실제로 4096바이트씩 이동한다.

마지막으로 버퍼 범위를 `buffer`부터 `buffer + size - 1`까지로 정의해 실제 접근하는 마지막 바이트를 기준으로 검사한다.

#### 남은 문제

이 설계는 페이지가 매핑되어 있는지는 확인하지만, 읽기 가능한지와 쓰기 가능한지를 구분하지 않는다.

예를 들어 `write(fd, buffer, size)`는 커널이 유저 버퍼를 읽는 syscall이다. 반면 `read(fd, buffer, size)`는 커널이 유저 버퍼에 쓰는 syscall이다. VM이나 page permission까지 고려하면 writable 여부를 확인하는 설계가 필요하다.

또한 문자열 검증은 아직 별도 함수가 필요하다. `open(file)`이나 `exec(cmd_line)`은 길이 `size`가 주어지지 않고, `\0`을 만날 때까지 검사해야 한다.

#### 가능한 대안

대안 A: `is_valid_user_buffer()`는 매핑 여부만 검사하고, 읽기/쓰기 검사는 나중에 별도 함수로 둔다.

장점은 지금 단계에서 단순하다. 단점은 `read`처럼 쓰기 권한이 중요한 syscall에서 검증 의미가 약해질 수 있다.

대안 B: 버퍼 검증 함수에 접근 방향을 인자로 추가한다.

```c
enum user_access {
    USER_ACCESS_READ,
    USER_ACCESS_WRITE
};

static bool is_valid_user_buffer(const void *buffer, unsigned size,
                                 enum user_access access);
```

장점은 syscall별 의도를 함수 호출에서 명확히 표현할 수 있다. 단점은 writable bit 확인을 위해 PTE 접근 helper가 추가로 필요할 수 있다.

대안 C: `is_readable_user_buffer()`와 `is_writable_user_buffer()`로 함수를 분리한다.

장점은 호출부가 읽기 쉽다. 단점은 내부 반복 로직이 중복될 수 있다.

### v3. 접근 방향과 문자열 검증까지 확장하는 설계

#### 목표

버퍼 검증을 syscall의 실제 접근 방향과 연결하고, 문자열 포인터 검증을 별도로 설계한다.

#### 채택한 방향

처음 구현 단계에서는 대안 A로 시작하고, 이후 `read`와 VM 권한 검사가 필요해지는 시점에 대안 B로 확장하는 것이 좋다.

이유는 현재 핵심 위험이 "커널 주소, NULL, unmapped page"이고, 이 문제는 단일 포인터와 페이지 단위 버퍼 검증만으로도 먼저 잡을 수 있기 때문이다. 함수 시그니처는 나중에 접근 방향을 넣을 수 있게 너무 좁게 설계하지 않는다.

#### 코드 또는 구조

문자열 검증은 버퍼 검증과 별도로 둔다.

```c
static bool
is_valid_user_string(const char *str)
{
    if (!is_valid_user_ptr(str))
        return false;

    for (const char *p = str; ; p++) {
        if (!is_valid_user_ptr(p))
            return false;
        if (*p == '\0')
            return true;
    }
}
```

이 코드는 개념 구조다. 실제 구현에서는 `*p`를 읽기 전에 해당 주소가 유효한지 확인해야 한다는 점이 핵심이다.

접근 방향까지 확장한다면 구조는 다음처럼 잡을 수 있다.

```c
enum user_access {
    USER_ACCESS_READ,
    USER_ACCESS_WRITE
};

static bool
is_valid_user_buffer(const void *buffer, unsigned size,
                     enum user_access access)
{
    /* 페이지 범위 검사 + access에 따른 권한 검사 */
}
```

#### 이전 버전에서 개선된 점

v2가 "버퍼 범위가 매핑되어 있는가"에 집중했다면, v3는 syscall마다 메모리를 어떻게 사용하는지까지 설계에 포함한다.

`write`는 유저 버퍼를 읽고, `read`는 유저 버퍼에 쓴다. 문자열 syscall은 크기가 정해진 버퍼가 아니라 `\0`까지 이어지는 동적 범위다. 이 차이를 함수 이름과 인자에 반영하면 호출부에서 실수를 줄일 수 있다.

#### 남은 문제

문자열 검증은 `\0`이 없는 경우 오래 순회할 수 있다. 과제 요구사항이나 테스트 범위에 따라 최대 길이 제한을 둘지 고민해야 한다.

또한 `is_valid_user_string()`은 문자를 실제로 읽는다. 이때 검증과 실제 사용 사이에 페이지 상태가 바뀔 수 있는지, lock이나 page fault 처리 정책과 어떤 관계가 있는지도 이후 VM 단계에서 다시 생각해야 한다.

## 4. 현재 코드에 대한 설계적 평가

현재 접근은 방향 자체가 틀리지는 않았다.

좋은 점은 다음과 같다.

- 단일 포인터 검증과 버퍼 검증을 분리했다.
- `pml4_get_page()`로 매핑 여부를 확인하려고 했다.
- 버퍼가 페이지 경계를 넘을 수 있다는 문제를 인식했다.

다만 구현 전에 설계상 바로잡아야 할 기준이 있다.

- `is_user_vaddr()`를 먼저 검사한 뒤 `pml4_get_page()`를 호출해야 한다.
- 버퍼 검증은 `buffer`와 `buffer + size - 1` 사이의 모든 페이지를 검사해야 한다.
- 주소 이동에는 `uint8_t *` 또는 `char *`를 사용해야 한다.
- `size == 0`일 때의 정책을 정해야 한다.
- 문자열 검증은 버퍼 검증과 별도로 설계해야 한다.
- 이후에는 읽기 검증과 쓰기 검증을 구분할 수 있어야 한다.

## 5. 배운 점

이번 설계에서 배운 핵심은 "주소값 하나를 검사하는 것"과 "메모리 범위를 검사하는 것"이 다르다는 점이다.

`is_user_vaddr()`는 주소가 유저 영역인지 알려주지만, 실제 매핑 여부는 알려주지 않는다. `pml4_get_page()`는 매핑 여부를 확인할 수 있지만, 유저 주소라는 전제가 먼저 만족되어야 한다.

또한 버퍼는 시작 주소 하나가 아니라 범위다. 범위는 여러 페이지를 걸칠 수 있으므로, 페이지 단위로 생각해야 한다. 이때 `PGSIZE`, `pg_round_down()`, `buffer + size - 1` 같은 개념이 필요해진다.

## 6. 발표용 요약

처음에는 user pointer 하나가 유효한지 확인하는 함수와 버퍼 전체를 확인하는 함수를 나누는 방식으로 접근했다.

그 과정에서 `pml4_get_page()`는 매핑 여부 확인에 적합하지만, 내부에서 유저 주소라는 ASSERT를 전제한다는 점을 알게 되었다. 그래서 커널 주소를 먼저 거르는 순서가 중요하다는 것을 배웠다.

또한 버퍼는 시작 주소만 검사하면 안 된다. 버퍼가 페이지 경계를 넘으면 중간 페이지가 unmapped일 수 있기 때문에, `buffer`부터 `buffer + size - 1`까지 포함하는 모든 페이지를 검사하는 방식으로 설계를 개선했다.

최종적으로 이 기능은 단일 포인터 검증, 버퍼 범위 검증, 문자열 검증, 접근 방향 검증으로 확장될 수 있는 구조로 정리했다.

